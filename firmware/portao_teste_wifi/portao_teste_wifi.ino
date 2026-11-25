/*
  TESTE DE WI-FI DO PORTAO (ESP32)
  - Mostra o estado da conexao, o sinal (dBm) e um log de eventos.
  - LED da placa:  piscada curta a cada 2 s  = conectado no Wi-Fi
                   pisca rapido (sempre)      = sem Wi-Fi / tentando conectar
                   LED apagado direto         = travado ou desligado
  - Funciona sem notebook: o ESP32 cria o proprio Wi-Fi "PortaoTeste".
    Conecte o celular nele (senha 12345678) e abra  http://192.168.4.1
    (se estiver conectado na rede da casa, abra o IP dele, que aparece no log).
  - Serial Monitor: 115200.
*/

#include <WiFi.h>
#include <WebServer.h>
#include <esp_system.h>
#include <stdarg.h>

// ======== CONFIGURE AQUI ========
const char* WIFI_SSID = "";
const char* WIFI_PASS = "";
const char* AP_SSID   = "PortaoTeste";
const char* AP_PASS   = "12345678";   // minimo 8 caracteres
// =================================

#ifndef LED_BUILTIN
#define LED_BUILTIN 2
#endif
const int LED_PIN = LED_BUILTIN;      // se o LED nao piscar, troque (ex.: 2, 5, 13)

enum State { ST_CONNECTING, ST_CONNECTED };
volatile State state = ST_CONNECTING;

WebServer server(80);

// ---------- Log (guarda os ultimos 40 eventos) ----------
const int LOG_MAX = 40;
String logBuf[LOG_MAX];
int logCount = 0;

String fmtTime(unsigned long ms) {
  unsigned long s = ms / 1000;
  char b[24];
  snprintf(b, sizeof(b), "%luh %02lum %02lus", s / 3600, (s / 60) % 60, s % 60);
  return String(b);
}

void addLog(const char* fmt, ...) {
  char msg[170];
  va_list args;
  va_start(args, fmt);
  vsnprintf(msg, sizeof(msg), fmt, args);
  va_end(args);
  unsigned long s = millis() / 1000;
  char line[210];
  snprintf(line, sizeof(line), "[%02lu:%02lu:%02lu] %s", s / 3600, (s / 60) % 60, s % 60, msg);
  Serial.println(line);
  logBuf[logCount % LOG_MAX] = String(line);
  logCount++;
}

// ---------- Contadores ----------
unsigned long stateSince = 0, lastRetry = 0, lastSample = 0, lastPrint = 0;
int drops = 0;          // quedas (estava conectado e caiu)
int failedAttempts = 0; // tentativas que nao conectaram
int lastReason = 0;
int rssiMin = 0, rssiMax = 0;
bool rssiHas = false;

const char* reasonText(int r) {
  switch (r) {
    case 1:   return "motivo nao especificado";
    case 2:   return "autenticacao expirou";
    case 8:   return "conexao encerrada";
    case 15:  return "falha no handshake (senha errada ou sinal fraco)";
    case 200: return "perdeu o sinal do roteador (longe/sinal fraco)";
    case 201: return "rede NAO encontrada (nome errado, fora de alcance ou so 5 GHz)";
    case 202: return "autenticacao falhou (senha incorreta?)";
    case 203: return "falha ao associar";
    case 204: return "handshake expirou";
    case 205: return "falha de conexao";
    default:  return "outro motivo";
  }
}

const char* resetText(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON:  return "ligou na energia";
    case ESP_RST_SW:       return "reiniciou por software";
    case ESP_RST_PANIC:    return "ERRO/TRAVAMENTO no codigo";
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT:      return "watchdog (travou e reiniciou)";
    case ESP_RST_BROWNOUT: return "QUEDA DE TENSAO (fonte fraca)";
    case ESP_RST_EXT:      return "botao reset";
    case ESP_RST_DEEPSLEEP:return "saiu do deep sleep";
    default:               return "desconhecido";
  }
}

// ---------- Eventos do Wi-Fi ----------
void onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
      addLog("Associou ao roteador (canal %d)", info.wifi_sta_connected.channel);
      break;
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      state = ST_CONNECTED;
      stateSince = millis();
      addLog("CONECTADO! IP %s | sinal %d dBm", WiFi.localIP().toString().c_str(), WiFi.RSSI());
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED: {
      lastReason = info.wifi_sta_disconnected.reason;
      if (state == ST_CONNECTED) {
        drops++;
        state = ST_CONNECTING;
        stateSince = millis();
        addLog("CAIU (queda %d). Motivo %d: %s", drops, lastReason, reasonText(lastReason));
      } else {
        failedAttempts++;
        addLog("Tentativa %d falhou. Motivo %d: %s", failedAttempts, lastReason, reasonText(lastReason));
      }
      break;
    }
    default:
      break;
  }
}

void scanNetworks() {
  digitalWrite(LED_PIN, HIGH);
  addLog("Procurando redes Wi-Fi (2,4 GHz)...");
  int n = WiFi.scanNetworks();
  bool found = false;
  for (int i = 0; i < n; i++) {
    bool alvo = (WiFi.SSID(i) == WIFI_SSID);
    if (alvo) found = true;
    if (alvo || i < 5) {
      addLog("  %s%s | canal %d | %d dBm", alvo ? "[SUA REDE] " : "", WiFi.SSID(i).c_str(), WiFi.channel(i), WiFi.RSSI(i));
    }
  }
  if (!found) {
    addLog("ATENCAO: nao encontrei '%s'. Confira o nome (maiusculas) ou se a rede e so 5 GHz.", WIFI_SSID);
  }
  WiFi.scanDelete();
}

// ---------- LED ----------
void updateLed() {
  unsigned long t = millis();
  bool on;
  if (state == ST_CONNECTED) on = (t % 2000) < 100;   // piscada curta a cada 2 s
  else on = ((t / 150) % 2) == 0;                      // pisca rapido
  digitalWrite(LED_PIN, on ? HIGH : LOW);
}

// ---------- Pagina web ----------
String page() {
  bool ok = (state == ST_CONNECTED);
  String s = "<!doctype html><html><head><meta charset='utf-8'>"
             "<meta name='viewport' content='width=device-width,initial-scale=1'>"
             "<meta http-equiv='refresh' content='2'><title>Teste Wi-Fi</title>"
             "<style>body{font-family:system-ui,sans-serif;background:#12161c;color:#e8ecf1;margin:0;padding:16px}"
             "h1{font-size:30px;margin:0 0 4px}.big{font-size:64px;font-weight:700;margin:4px 0}"
             ".card{background:#1b212a;border-radius:14px;padding:14px;margin:12px 0}"
             ".k{color:#9aa6b2;font-size:13px}pre{white-space:pre-wrap;font-size:12px;line-height:1.5;margin:0}"
             "</style></head><body>";
  s += "<h1 style='color:" + String(ok ? "#5ee0a0" : "#ff8a7a") + "'>" +
       String(ok ? "CONECTADO" : "SEM WI-FI (tentando...)") + "</h1>";
  s += "<div class='card'><div class='k'>SINAL</div><div class='big'>" +
       (ok ? String(WiFi.RSSI()) + " dBm" : String("--")) + "</div>";
  if (rssiHas) s += "<div class='k'>minimo " + String(rssiMin) + " dBm | maximo " + String(rssiMax) + " dBm</div>";
  s += "</div><div class='card'>";
  s += "Rede: " + String(WIFI_SSID) + "<br>";
  s += "IP: " + (ok ? WiFi.localIP().toString() : String("--")) + "<br>";
  s += "Canal: " + (ok ? String(WiFi.channel()) : String("--")) + "<br>";
  s += "Ligado ha: " + fmtTime(millis()) + "<br>";
  s += String(ok ? "Conectado ha: " : "Sem Wi-Fi ha: ") + fmtTime(millis() - stateSince) + "<br>";
  s += "Quedas: " + String(drops) + " | Tentativas falhas: " + String(failedAttempts) + "<br>";
  s += "Ultimo motivo de queda: " + String(lastReason) + " (" + reasonText(lastReason) + ")<br>";
  s += "Memoria livre: " + String(ESP.getFreeHeap()) + " bytes";
  s += "</div><div class='card'><div class='k'>LOG (mais novo primeiro)</div><pre>";
  int first = logCount - 25;
  if (first < 0) first = 0;
  for (int n = logCount - 1; n >= first && n >= logCount - LOG_MAX; n--) {
    s += logBuf[n % LOG_MAX] + "\n";
  }
  s += "</pre></div><div class='k'>Log completo em texto: /log</div></body></html>";
  return s;
}

String fullLog() {
  String s = "";
  int first = logCount - LOG_MAX;
  if (first < 0) first = 0;
  for (int n = first; n < logCount; n++) s += logBuf[n % LOG_MAX] + "\n";
  s += "\nEstado: " + String(state == ST_CONNECTED ? "CONECTADO" : "SEM WI-FI");
  s += " | sinal atual: " + (state == ST_CONNECTED ? String(WiFi.RSSI()) : String("--")) + " dBm";
  if (rssiHas) s += " | min " + String(rssiMin) + " / max " + String(rssiMax) + " dBm";
  s += " | quedas: " + String(drops) + " | tentativas falhas: " + String(failedAttempts);
  s += " | ligado ha: " + fmtTime(millis()) + "\n";
  return s;
}

// ---------- setup / loop ----------
void setup() {
  Serial.begin(115200);
  delay(300);
  pinMode(LED_PIN, OUTPUT);

  addLog("=== Teste de Wi-Fi do portao ===");
  addLog("Ultimo reinicio: %s", resetText(esp_reset_reason()));

  WiFi.persistent(false);
  WiFi.setHostname("portao-esp32");
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_SSID, AP_PASS);
  addLog("Wi-Fi proprio '%s' ligado -> http://%s", AP_SSID, WiFi.softAPIP().toString().c_str());

  WiFi.onEvent(onWiFiEvent);
  scanNetworks();

  WiFi.setAutoReconnect(true);
  addLog("Conectando em '%s'...", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  WiFi.setSleep(false);
  stateSince = millis();
  lastRetry = millis();

  server.on("/", []() { server.send(200, "text/html; charset=utf-8", page()); });
  server.on("/log", []() { server.send(200, "text/plain; charset=utf-8", fullLog()); });
  server.begin();
}

void loop() {
  server.handleClient();
  updateLed();
  unsigned long now = millis();

  if (now - lastSample >= 1000) {
    lastSample = now;
    if (state == ST_CONNECTED) {
      int r = WiFi.RSSI();
      if (!rssiHas || r < rssiMin) rssiMin = r;
      if (!rssiHas || r > rssiMax) rssiMax = r;
      rssiHas = true;
    }
  }

  if (now - lastPrint >= 5000) {
    lastPrint = now;
    if (state == ST_CONNECTED) {
      Serial.printf("[estado] CONECTADO | %d dBm | quedas %d | IP %s\n",
                    WiFi.RSSI(), drops, WiFi.localIP().toString().c_str());
    } else {
      Serial.printf("[estado] SEM WI-FI ha %lu s | tentativas falhas %d | ultimo motivo %d\n",
                    (now - stateSince) / 1000, failedAttempts, lastReason);
    }
  }

  // Se ficar 20 s sem conectar, forca uma nova tentativa do zero
  if (state != ST_CONNECTED && now - stateSince > 20000 && now - lastRetry > 20000) {
    lastRetry = now;
    addLog("20 s sem conectar. Forcando nova tentativa...");
    WiFi.disconnect();
    WiFi.begin(WIFI_SSID, WIFI_PASS);
  }
}
