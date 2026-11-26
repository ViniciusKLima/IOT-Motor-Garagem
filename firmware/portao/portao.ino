/*
  ======================================================
  PORTÃO DA GARAGEM - FIRMWARE DO ESP32
  ======================================================
  O que ele faz:
  - Escuta /comandos no Firebase (o site grava { uid, nome, acao, ts }).
  - Valida cada comando: hora certa (NTP), idade < 10 s, não repetido,
    intervalo mínimo de 2 s entre pulsos.
  - Dá um PULSO de 500 ms no relé (GPIO 26) = apertar a botoeira (BOT)
    da central Rossi. A central alterna abrir / parar / fechar.
  - Grava o resultado em /historico e apaga o comando.
  - Publica /estado a cada 5 s: é o "ESP32 online" do site, com dados
    técnicos (sinal, tempo ligado, motivo do reinício, quedas de Wi-Fi...).
  - LOG TÉCNICO em /logs (últimos 200 eventos): ligou, reiniciou e por quê,
    Wi-Fi caiu/voltou, erros do Firebase, comandos recebidos/recusados,
    pulsos. Sem internet, guarda na memória e envia quando voltar.
    O site mostra em: toque no chip "ESP32 online" → Diagnóstico.
  - Atualização pelo Wi-Fi (OTA) com senha.

  - WI-FI DE EMERGÊNCIA: se ficar 1 min sem internet, cria a rede
    "Portao-ESP32" (senha = OTA_PASSWORD do secrets.h). No celular, conecte
    nela e abra http://192.168.4.1 para ver o estado e o log (só leitura).
    Desliga sozinho depois de 5 min funcionando. Com internet, a mesma
    página abre no IP da casa (aparece no Diagnóstico do site).

  Placa: "ESP32 Dev Module", Partition Scheme "Minimal SPIFFS (1.9MB APP
  with OTA)". Serial Monitor: 115200.
  Bibliotecas: "Firebase Arduino Client Library for ESP8266 and ESP32"
  (Mobizt, 4.4.x) e "ArduinoJson" (7.x).

  TESTE PELO CABO: no Serial Monitor, digite  p  + Enter para dar
  um pulso no relé, e  s  + Enter para ver o estado.

  Contrato com o site: .claude/docs/app-e-firebase.md
  ======================================================
*/

#include <WiFi.h>
#include <ArduinoOTA.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include <time.h>
#include <sys/time.h>
#include <stdarg.h>
#include <esp_system.h>

#include "secrets.h"  // WIFI_SSID, WIFI_PASSWORD, API_KEY, DATABASE_URL, ESP_EMAIL, ESP_PASSWORD, OTA_PASSWORD

const char* FW_VERSAO = "1.2.0";

// ======================================================
// CONFIGURAÇÃO (confira com o seu hardware)
// ======================================================

// Relé que fecha BOT com GND na central
const int RELE_PIN = 26;

// Nível que LIGA o relé (testado: ativo-BAIXO).
// TESTE: tire e volte a energia; o portão NÃO pode mexer.
const int RELE_LIGA = LOW;
const int RELE_DESLIGA = (RELE_LIGA == LOW) ? HIGH : LOW;

const unsigned long PULSO_MS = 500;          // como apertar o botão
const unsigned long INTERVALO_MIN_MS = 2000; // entre dois pulsos
const long long IDADE_MAX_MS = 10000;        // comando mais velho que isso é recusado
const long long FUTURO_MAX_MS = 5000;        // tolerância de relógio para o futuro

// Sensor reed (FUTURO). Deixe -1 enquanto não estiver instalado.
// Ligação: reed entre o pino e o GND (usa INPUT_PULLUP, sem resistor).
// Ex.: const int SENSOR_PIN = 27;
const int SENSOR_PIN = -1;
// Com sensor: se o portão estava fechado e continuar fechado tanto tempo
// depois do pulso, registra "pulso dado mas o portão não saiu do lugar".
const unsigned long SENSOR_CONFIRMA_MS = 5000;

// LED da placa: desligado (pedido do Vinícius). Mude para true se quiser
// o LED mostrando o estado (rápido = sem Wi-Fi; 1 s = conectando; curto a cada 2 s = ok).
const bool LED_STATUS = false;
#ifndef LED_BUILTIN
#define LED_BUILTIN 2
#endif
const int LED_PIN = LED_BUILTIN;

// Tempos
const unsigned long ESTADO_INTERVALO_MS = 5000;    // "estou vivo" no banco
const unsigned long CHECAGEM_INTERVALO_MS = 30000; // relê /comandos mesmo sem aviso do stream
const unsigned long WIFI_RETRY_MS = 20000;         // força nova tentativa de Wi-Fi
const unsigned long SEM_FIREBASE_REINICIA_MS = 10UL * 60UL * 1000UL; // 10 min sem sucesso = reinicia
const unsigned long LOG_REPETIDO_MS = 5UL * 60UL * 1000UL;  // não repetir o mesmo aviso antes disso
const int SINAL_FRACO_DBM = -80;

// Wi-Fi de emergência (página de log no próprio ESP32)
const char* AP_SSID = "Portao-ESP32";            // senha: OTA_PASSWORD (secrets.h)
const unsigned long AP_LIGA_SEM_WIFI_MS = 60000;           // liga após 1 min sem Wi-Fi
const unsigned long AP_DESLIGA_ESTAVEL_MS = 5UL * 60UL * 1000UL;  // desliga após 5 min estável

// ======================================================
// ESTADO
// ======================================================

FirebaseData fbdo;    // leituras e escritas
FirebaseData stream;  // escuta /comandos
FirebaseAuth auth;
FirebaseConfig config;
Preferences prefs;    // memória que sobrevive ao reinício (contador do log, motivo do reinício)

bool firebaseIniciado = false;
bool firebaseConectouUmaVez = false;
bool streamIniciado = false;
bool precisaChecar = true;
bool ntpIniciado = false;
bool horaLogada = false;
bool lacunaVerificada = false;  // já conferiu quanto tempo ficou sem sinal antes de ligar

unsigned long ultimoPulso = 0;
bool jaPulsou = false;
long long ultimoPulsoEpochMs = 0;
int pulsos = 0;
unsigned long ultimoEstado = 0;
unsigned long ultimaChecagem = 0;
unsigned long ultimaTentativaWifi = 0;
unsigned long ultimoSucessoFirebase = 0;

// Wi-Fi (as flags vêm do evento, que roda em outra tarefa; o log é feito no loop)
volatile bool evWifiConectou = false;
volatile bool evWifiCaiu = false;
volatile int evWifiMotivo = 0;
bool wifiConectadoAntes = false;
unsigned long wifiConectadoDesde = 0;
unsigned long wifiForaDesde = 0;
unsigned long wifiForaTotalMs = 0;
int wifiQuedas = 0;
int wifiUltimoMotivo = 0;
unsigned long ultimoAvisoSemWifi = 0;
unsigned long ultimoAvisoSinalFraco = 0;

// Erros do Firebase (contados; o log é resumido para não lotar)
int errosFirebase = 0;
int errosDesdeUltimoLog = 0;
unsigned long ultimoLogErroFirebase = 0;
unsigned long ultimoLogStream = 0;

// ids já processados (evita executar duas vezes se o apagar falhar)
const int IDS_MAX = 16;
String idsFeitos[IDS_MAX];
int idsPos = 0;

// sensor (com debounce)
bool sensorFechado = false;
bool sensorLido = false;
bool sensorBruto = false;
unsigned long sensorMudouEm = 0;
bool sensorMudou = false;
bool conferirSensorAposPulso = false;

// ======================================================
// HORA (NTP)
// ======================================================

bool horaValida() {
  return time(nullptr) > 1700000000;  // depois de nov/2023 = sincronizou
}

long long agoraMs() {
  struct timeval tv;
  gettimeofday(&tv, nullptr);
  return (long long)tv.tv_sec * 1000LL + tv.tv_usec / 1000;
}

String duracao(unsigned long ms) {
  unsigned long s = ms / 1000;
  char b[32];
  if (s < 60) snprintf(b, sizeof(b), "%lus", s);
  else if (s < 3600) snprintf(b, sizeof(b), "%lumin %lus", s / 60, s % 60);
  else if (s < 86400) snprintf(b, sizeof(b), "%luh %lumin", s / 3600, (s / 60) % 60);
  else snprintf(b, sizeof(b), "%lud %luh", s / 86400, (s / 3600) % 24);
  return String(b);
}

// ======================================================
// LOG TÉCNICO
// Cada evento vai para o Serial e para uma fila na memória (40 eventos).
// Quando há Firebase, a fila é enviada para /logs/{0..199} (anel: o 201º
// sobrescreve o 1º). Campos: n (sequência), ts (hora), nivel, msg, up.
// ======================================================

enum NivelLog { INFO, AVISO, ERRO };

const char* nomeNivel(int n) {
  return n == ERRO ? "erro" : (n == AVISO ? "aviso" : "info");
}

struct Evento {
  unsigned long ms;
  long long epochMs;  // 0 = ainda sem hora certa (calculada no envio)
  uint8_t nivel;
  char msg[120];
};

const int EVENTOS_MAX = 40;
const uint32_t LOGS_SLOTS = 200;
Evento eventos[EVENTOS_MAX];
int evIni = 0;
int evQtd = 0;
int eventosPerdidos = 0;
uint32_t logSeq = 0;

// Cópia dos últimos eventos para a página do Wi-Fi de emergência
const int LOCAL_MAX = 60;
char logLocal[LOCAL_MAX][160];
int localPos = 0;
int localQtd = 0;

String carimbo() {
  if (horaValida()) {
    time_t t = time(nullptr) - 3 * 3600;  // horário de Brasília (UTC-3)
    struct tm tmv;
    gmtime_r(&t, &tmv);
    char b[24];
    strftime(b, sizeof(b), "%d/%m %H:%M:%S", &tmv);
    return String(b);
  }
  return "ligado ha " + duracao(millis());
}

void logar(int nivel, const char* fmt, ...) {
  char msg[120];
  va_list args;
  va_start(args, fmt);
  vsnprintf(msg, sizeof(msg), fmt, args);
  va_end(args);
  Serial.printf("[%s] %s\n", nomeNivel(nivel), msg);
  snprintf(logLocal[localPos], sizeof(logLocal[0]), "%s|%s|%s", carimbo().c_str(), nomeNivel(nivel), msg);
  localPos = (localPos + 1) % LOCAL_MAX;
  if (localQtd < LOCAL_MAX) localQtd++;

  if (evQtd == EVENTOS_MAX) {  // fila cheia: descarta o mais antigo
    evIni = (evIni + 1) % EVENTOS_MAX;
    evQtd--;
    eventosPerdidos++;
  }
  Evento& e = eventos[(evIni + evQtd) % EVENTOS_MAX];
  e.ms = millis();
  e.epochMs = horaValida() ? agoraMs() : 0;
  e.nivel = nivel;
  strlcpy(e.msg, msg, sizeof(e.msg));
  evQtd++;
}

void enviarLogs() {
  if (eventosPerdidos > 0) {
    int p = eventosPerdidos;
    eventosPerdidos = 0;
    logar(AVISO, "%d eventos antigos do log foram descartados (muito tempo sem internet)", p);
  }
  int enviados = 0;
  while (evQtd > 0 && enviados < 4) {  // no máximo 4 por volta, para não travar o loop
    Evento& e = eventos[evIni];
    long long ts = e.epochMs ? e.epochMs : agoraMs() - (long long)(millis() - e.ms);

    JsonDocument d;
    d["n"] = logSeq;
    d["ts"] = ts;
    d["nivel"] = nomeNivel(e.nivel);
    d["msg"] = e.msg;
    d["up"] = e.ms / 1000;
    String s;
    serializeJson(d, s);
    FirebaseJson j;
    j.setJsonData(s);

    String caminho = "/logs/" + String(logSeq % LOGS_SLOTS);
    if (!Firebase.RTDB.setJSON(&fbdo, caminho.c_str(), &j)) {
      Serial.printf("[log] nao enviou: %s\n", fbdo.errorReason().c_str());
      return;  // tenta de novo na próxima volta
    }
    logSeq++;
    prefs.putUInt("logSeq", logSeq);
    evIni = (evIni + 1) % EVENTOS_MAX;
    evQtd--;
    enviados++;
  }
}

void erroFirebase(const char* onde, const String& motivo) {
  errosFirebase++;
  errosDesdeUltimoLog++;
  Serial.printf("[firebase] erro em %s: %s\n", onde, motivo.c_str());
  if (ultimoLogErroFirebase == 0 || millis() - ultimoLogErroFirebase > LOG_REPETIDO_MS) {
    logar(ERRO, "Falha no Firebase (%s): %s [%d erro(s) desde o ultimo aviso]", onde, motivo.c_str(),
          errosDesdeUltimoLog);
    errosDesdeUltimoLog = 0;
    ultimoLogErroFirebase = millis();
  }
}

// ======================================================
// RELÉ
// ======================================================

void releDesligadoSeguro() {
  // Define o nível ANTES de virar saída: o relé não dá pulso ao ligar.
  digitalWrite(RELE_PIN, RELE_DESLIGA);
  pinMode(RELE_PIN, OUTPUT);
  digitalWrite(RELE_PIN, RELE_DESLIGA);
}

void darPulso() {
  Serial.println(">>> PULSO no rele (BOT)");
  digitalWrite(RELE_PIN, RELE_LIGA);
  delay(PULSO_MS);
  digitalWrite(RELE_PIN, RELE_DESLIGA);
  ultimoPulso = millis();
  ultimoPulsoEpochMs = horaValida() ? agoraMs() : 0;
  jaPulsou = true;
  pulsos++;
  // Com sensor: se estava fechado, confere se saiu do lugar
  conferirSensorAposPulso = (SENSOR_PIN >= 0 && sensorLido && sensorFechado);
}

// ======================================================
// LED (opcional)
// ======================================================

void atualizarLed() {
  if (!LED_STATUS) return;
  unsigned long t = millis();
  bool acende;
  if (WiFi.status() != WL_CONNECTED) acende = ((t / 150) % 2) == 0;
  else if (!firebaseIniciado || !Firebase.ready() || !streamIniciado) acende = ((t / 500) % 2) == 0;
  else acende = (t % 2000) < 80;
  digitalWrite(LED_PIN, acende ? HIGH : LOW);
}

// ======================================================
// WI-FI
// ======================================================

const char* motivoWifi(int r) {
  switch (r) {
    case 2:   return "autenticacao expirou";
    case 8:   return "conexao encerrada";
    case 15:  return "falha no handshake (senha errada ou sinal fraco)";
    case 200: return "perdeu o sinal do roteador";
    case 201: return "rede nao encontrada (roteador desligado, longe ou nome errado)";
    case 202: return "autenticacao falhou (senha incorreta?)";
    case 204: return "handshake expirou";
    default:  return "outro motivo";
  }
}

// Roda em outra tarefa: só marca flags; o log é feito no loop.
void eventoWifi(WiFiEvent_t evento, WiFiEventInfo_t info) {
  if (evento == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
    evWifiConectou = true;
  } else if (evento == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
    evWifiMotivo = info.wifi_sta_disconnected.reason;
    evWifiCaiu = true;
  }
}

void iniciarWifi() {
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setHostname("portao-esp32");
  WiFi.onEvent(eventoWifi);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  WiFi.setSleep(false);  // resposta mais rápida e conexão mais estável
  ultimaTentativaWifi = millis();
  wifiForaDesde = millis();
  Serial.printf("[wifi] conectando em \"%s\"...\n", WIFI_SSID);
}

void cuidarWifi() {
  // --- eventos vindos da outra tarefa ---
  if (evWifiCaiu) {
    evWifiCaiu = false;
    wifiUltimoMotivo = evWifiMotivo;
    if (wifiConectadoAntes) {
      wifiConectadoAntes = false;
      wifiQuedas++;
      wifiForaDesde = millis();
      ultimoAvisoSemWifi = millis();
      logar(AVISO, "Wi-Fi caiu (queda %d desde que ligou): %s", wifiQuedas, motivoWifi(wifiUltimoMotivo));
    }
  }
  if (evWifiConectou) {
    evWifiConectou = false;
    if (!wifiConectadoAntes) {
      wifiConectadoAntes = true;
      wifiConectadoDesde = millis();
      unsigned long fora = millis() - wifiForaDesde;
      if (wifiQuedas == 0) {
        logar(INFO, "Wi-Fi conectado em %s (sinal %d dBm, canal %d, IP %s)", duracao(fora).c_str(), WiFi.RSSI(),
              WiFi.channel(), WiFi.localIP().toString().c_str());
      } else {
        wifiForaTotalMs += fora;
        logar(INFO, "Wi-Fi voltou depois de %s fora (sinal %d dBm)", duracao(fora).c_str(), WiFi.RSSI());
      }
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    if (!ntpIniciado) {
      configTime(0, 0, "pool.ntp.org", "time.google.com");
      ntpIniciado = true;
      Serial.println("[hora] sincronizando (NTP)...");
    }
    if (!horaLogada && horaValida()) {
      horaLogada = true;
      logar(INFO, "Hora sincronizada pela internet");
    }
    // sinal fraco (aviso no máximo a cada 30 min)
    int rssi = WiFi.RSSI();
    if (rssi < SINAL_FRACO_DBM && (ultimoAvisoSinalFraco == 0 || millis() - ultimoAvisoSinalFraco > 6 * LOG_REPETIDO_MS)) {
      ultimoAvisoSinalFraco = millis();
      logar(AVISO, "Sinal Wi-Fi fraco: %d dBm (abaixo de %d pode cair)", rssi, SINAL_FRACO_DBM);
    }
    return;
  }

  // --- sem Wi-Fi ---
  if (millis() - ultimoAvisoSemWifi > LOG_REPETIDO_MS) {
    ultimoAvisoSemWifi = millis();
    logar(AVISO, "Ainda sem Wi-Fi ha %s (ultimo motivo: %s)", duracao(millis() - wifiForaDesde).c_str(),
          motivoWifi(wifiUltimoMotivo));
  }
  if (millis() - ultimaTentativaWifi >= WIFI_RETRY_MS) {
    ultimaTentativaWifi = millis();
    Serial.println("[wifi] sem conexao ha 20 s, tentando do zero...");
    WiFi.disconnect();
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  }
}

// ======================================================
// WI-FI DE EMERGÊNCIA + PÁGINA DE LOG (somente leitura)
// ======================================================

WebServer servidor(80);
bool servidorIniciado = false;
bool apLigado = false;

String escapar(const char* t) {
  String r;
  for (const char* c = t; *c; c++) {
    if (*c == '<') r += "&lt;";
    else if (*c == '>') r += "&gt;";
    else if (*c == '&') r += "&amp;";
    else r += *c;
  }
  return r;
}

const char* motivoReinicio();

void paginaLog() {
  bool ok = WiFi.status() == WL_CONNECTED;
  String h;
  h.reserve(12000);
  h += "<!doctype html><html lang='pt-BR'><head><meta charset='utf-8'>"
       "<meta name='viewport' content='width=device-width,initial-scale=1'><meta http-equiv='refresh' content='5'>"
       "<title>Portao - log</title><style>body{font-family:system-ui,sans-serif;background:#12161c;color:#e8ecf1;"
       "margin:0;padding:16px}h1{font-size:22px;margin:0 0 12px}.c{background:#1b212a;border-radius:14px;padding:14px;"
       "margin:0 0 12px;font-size:14px;line-height:1.6}.k{color:#9aa6b2}ol{list-style:none;margin:0;padding:0}"
       "li{padding:8px 10px;border-left:3px solid transparent;border-top:1px solid #2a3340;font-size:13px}"
       "li.aviso{border-left-color:#ffc24d}li.erro{border-left-color:#ff8a7a}.t{color:#9aa6b2;margin-right:8px}"
       "</style></head><body><h1>Portao - ESP32</h1><div class='c'>";
  h += "<b style='color:" + String(ok ? "#5ee0a0" : "#ff8a7a") + "'>" +
       String(ok ? "Wi-Fi conectado" : "SEM Wi-Fi (tentando)") + "</b><br>";
  h += "<span class='k'>Sinal:</span> " + (ok ? String(WiFi.RSSI()) + " dBm" : String("--")) + " &nbsp; ";
  h += "<span class='k'>IP:</span> " + (ok ? WiFi.localIP().toString() : String("--")) + "<br>";
  h += "<span class='k'>Firebase:</span> " +
       String(firebaseIniciado && Firebase.ready() ? "ok" : "nao conectado") + " &nbsp; ";
  h += "<span class='k'>Hora:</span> " + String(horaValida() ? "ok" : "sem hora") + "<br>";
  h += "<span class='k'>Ligado ha:</span> " + duracao(millis()) + " &nbsp; ";
  h += "<span class='k'>Quedas de Wi-Fi:</span> " + String(wifiQuedas) + "<br>";
  h += "<span class='k'>Pulsos:</span> " + String(pulsos) + " &nbsp; ";
  h += "<span class='k'>Firmware:</span> " + String(FW_VERSAO) + "<br>";
  h += "<span class='k'>Ultimo reinicio:</span> " + String(motivoReinicio()) + "</div>";
  h += "<div class='c' style='padding:0'><ol>";
  for (int i = 0; i < localQtd; i++) {  // mais novo primeiro
    int idx = (localPos - 1 - i + LOCAL_MAX) % LOCAL_MAX;
    const char* linha = logLocal[idx];
    const char* p1 = strchr(linha, '|');
    const char* p2 = p1 ? strchr(p1 + 1, '|') : nullptr;
    if (!p1 || !p2) continue;
    String quando = String(linha).substring(0, p1 - linha);
    String nivel = String(p1 + 1).substring(0, p2 - p1 - 1);
    h += "<li class='" + nivel + "'><span class='t'>" + quando + "</span>" + escapar(p2 + 1) + "</li>";
  }
  h += "</ol></div><p class='k' style='font-size:12px'>Atualiza a cada 5 s. Somente leitura.</p></body></html>";
  servidor.send(200, "text/html; charset=utf-8", h);
}

void iniciarServidor() {
  if (servidorIniciado) return;
  servidor.on("/", paginaLog);
  servidor.begin();
  servidorIniciado = true;
}

void ligarAp() {
  if (apLigado) return;
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_SSID, OTA_PASSWORD);
  apLigado = true;
  iniciarServidor();
  logar(AVISO, "Wi-Fi de emergencia LIGADO: rede %s, abra http://192.168.4.1", AP_SSID);
}

void desligarAp() {
  if (!apLigado) return;
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  apLigado = false;
  logar(INFO, "Wi-Fi de emergencia desligado (conexao estavel ha 5 min)");
}

void cuidarAp() {
  bool conectado = WiFi.status() == WL_CONNECTED;
  if (conectado) iniciarServidor();  // a página também abre pelo IP da casa
  if (!apLigado && !conectado && millis() - wifiForaDesde > AP_LIGA_SEM_WIFI_MS) ligarAp();
  if (apLigado && conectado && wifiConectadoAntes && Firebase.ready() &&
      millis() - wifiConectadoDesde > AP_DESLIGA_ESTAVEL_MS) {
    desligarAp();
  }
  if (servidorIniciado) servidor.handleClient();
}

// ======================================================
// OTA (atualizar pelo Wi-Fi)
// Arduino IDE > Ferramentas > Porta > "portao-esp32" (porta de rede)
// ======================================================

bool otaIniciado = false;

void cuidarOta() {
  if (WiFi.status() != WL_CONNECTED) return;
  if (!otaIniciado) {
    ArduinoOTA.setHostname("portao-esp32");
    ArduinoOTA.setPassword(OTA_PASSWORD);
    ArduinoOTA.onStart([]() {
      digitalWrite(RELE_PIN, RELE_DESLIGA);
      prefs.putString("motivoRest", "atualizacao do codigo pelo Wi-Fi (OTA)");
      Serial.println("[ota] recebendo codigo novo...");
    });
    ArduinoOTA.onError([](ota_error_t e) { Serial.printf("[ota] erro %u\n", e); });
    ArduinoOTA.begin();
    otaIniciado = true;
    Serial.println("[ota] pronto (porta de rede: portao-esp32)");
  }
  ArduinoOTA.handle();
}

// ======================================================
// SENSOR REED (futuro)
// ======================================================

void cuidarSensor() {
  if (SENSOR_PIN < 0) return;
  bool fechadoAgora = digitalRead(SENSOR_PIN) == LOW;  // ímã perto = contato fechado = LOW
  if (fechadoAgora != sensorBruto) {
    sensorBruto = fechadoAgora;
    sensorMudouEm = millis();
  }
  if (millis() - sensorMudouEm > 300 && (!sensorLido || sensorBruto != sensorFechado)) {
    sensorFechado = sensorBruto;
    sensorLido = true;
    sensorMudou = true;
    logar(INFO, "Sensor: portao %s", sensorFechado ? "FECHADO" : "saiu da posicao fechado");
  }
  if (conferirSensorAposPulso && millis() - ultimoPulso > SENSOR_CONFIRMA_MS) {
    conferirSensorAposPulso = false;
    if (sensorFechado) {
      logar(ERRO, "Pulso dado, mas o portao continuou fechado apos %lus: confira fios BOT/GND, rele e central",
            SENSOR_CONFIRMA_MS / 1000);
    }
  }
}

// ======================================================
// FIREBASE
// ======================================================

void iniciarFirebase() {
  Serial.println("[firebase] iniciando...");
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  config.token_status_callback = tokenStatusCallback;
  config.timeout.serverResponse = 5 * 1000;
  config.timeout.socketConnection = 10 * 1000;

  // Usuário próprio do ESP32 (as regras só deixam ELE ler /comandos)
  auth.user.email = ESP_EMAIL;
  auth.user.password = ESP_PASSWORD;

  fbdo.setBSSLBufferSize(4096, 1024);
  stream.setBSSLBufferSize(2048, 512);
  stream.keepAlive(5, 5, 1);

  Firebase.reconnectNetwork(true);
  Firebase.begin(&config, &auth);
  firebaseIniciado = true;
  ultimoSucessoFirebase = millis();
}

bool jaFeito(const String& id) {
  for (int i = 0; i < IDS_MAX; i++)
    if (idsFeitos[i] == id) return true;
  return false;
}

void marcarFeito(const String& id) {
  idsFeitos[idsPos] = id;
  idsPos = (idsPos + 1) % IDS_MAX;
}

bool gravarHistorico(const String& nome, const char* resultado, const String& comandoId) {
  FirebaseJson j;
  j.set("nome", nome);
  j.set("acao", "acionar");
  j.set("ts/.sv", "timestamp");  // hora do servidor
  j.set("resultado", resultado);
  j.set("comandoId", comandoId);
  if (Firebase.RTDB.pushJSON(&fbdo, "/historico", &j)) return true;
  erroFirebase("gravar historico", fbdo.errorReason());
  return false;
}

void apagarComando(const String& id) {
  String caminho = "/comandos/" + id;
  if (!Firebase.RTDB.deleteNode(&fbdo, caminho.c_str())) erroFirebase("apagar comando", fbdo.errorReason());
}

void processarComando(const String& id, const String& nome, const String& acao, long long ts) {
  if (jaFeito(id)) {  // já executado: só tenta apagar de novo
    apagarComando(id);
    return;
  }
  marcarFeito(id);

  const char* resultado = "ok";
  long long idade = horaValida() ? agoraMs() - ts : 0;

  if (acao != "acionar") resultado = "acao invalida";
  else if (!horaValida()) resultado = "sem hora certa";
  else if (idade > IDADE_MAX_MS) resultado = "comando antigo";
  else if (idade < -FUTURO_MAX_MS) resultado = "relogio adiantado";
  else if (jaPulsou && millis() - ultimoPulso < INTERVALO_MIN_MS) resultado = "toque repetido";

  if (strcmp(resultado, "ok") == 0) {
    darPulso();
    logar(INFO, "Comando de %s: PULSO dado no rele (chegou %lld ms depois do toque)", nome.c_str(), idade);
  } else {
    logar(AVISO, "Comando de %s recusado: %s (idade %lld ms)", nome.c_str(), resultado, idade);
  }
  gravarHistorico(nome, resultado, id);
  apagarComando(id);
}

bool checarComandos() {
  if (!Firebase.RTDB.getJSON(&fbdo, "/comandos")) {
    String erro = fbdo.errorReason();
    // /comandos vazio (ninguém tocou no botão): não é erro
    if (fbdo.httpCode() == 404 || erro.indexOf("not found") >= 0 || erro.indexOf("path not exist") >= 0) return true;
    erroFirebase("ler comandos", erro);
    return false;
  }

  String payload = fbdo.payload();
  if (payload.length() == 0 || payload == "null") return true;

  JsonDocument doc;
  if (deserializeJson(doc, payload)) {
    logar(ERRO, "Comando com JSON invalido no banco (ignorado)");
    return true;
  }

  // Processa do mais antigo para o mais novo (chaves do push são ordenadas)
  for (JsonPair kv : doc.as<JsonObject>()) {
    JsonObject c = kv.value().as<JsonObject>();
    String id = kv.key().c_str();
    String nome = c["nome"] | "?";
    String acao = c["acao"] | "";
    long long ts = (long long)(c["ts"] | 0.0);
    processarComando(id, nome, acao, ts);
  }
  return true;
}

const char* motivoReinicio();

bool publicarEstado() {
  JsonDocument d;
  d["visto"][".sv"] = "timestamp";
  d["rssi"] = WiFi.RSSI();
  d["ip"] = WiFi.localIP().toString();
  d["fw"] = FW_VERSAO;
  d["uptimeS"] = millis() / 1000;
  d["reinicio"] = motivoReinicio();
  d["quedasWifi"] = wifiQuedas;
  d["foraWifiS"] = wifiForaTotalMs / 1000;
  d["pulsos"] = pulsos;
  if (ultimoPulsoEpochMs) d["ultimoPulsoTs"] = ultimoPulsoEpochMs;
  d["errosFirebase"] = errosFirebase;
  d["heap"] = ESP.getFreeHeap();
  if (SENSOR_PIN >= 0 && sensorLido) d["sensorFechado"] = sensorFechado;
  String s;
  serializeJson(d, s);
  FirebaseJson j;
  j.setJsonData(s);
  if (Firebase.RTDB.updateNode(&fbdo, "/estado", &j)) return true;
  erroFirebase("publicar estado", fbdo.errorReason());
  return false;
}

// Na primeira conexão depois de ligar: quanto tempo ficou sem dar sinal?
void verificarLacuna() {
  lacunaVerificada = true;
  if (!Firebase.RTDB.get(&fbdo, "/estado/visto")) return;  // primeira vez na vida: nada a comparar
  long long visto = atoll(fbdo.payload().c_str());
  if (visto <= 0 || !horaValida()) return;
  long long lacuna = agoraMs() - visto;
  if (lacuna > 60000) {
    logar(AVISO, "Ficou %s sem dar sinal antes de ligar (sem energia, sem internet ou reiniciando)",
          duracao((unsigned long)(lacuna > 4000000000LL ? 4000000000LL : lacuna)).c_str());
  }
}

void cuidarFirebase() {
  if (!firebaseIniciado) {
    if (WiFi.status() == WL_CONNECTED && horaValida()) iniciarFirebase();
    return;
  }

  // Muito tempo sem conseguir falar com o Firebase: reinicia do zero.
  // (seguro: ao ligar, o relé fica desligado e comandos velhos são recusados)
  if (millis() - ultimoSucessoFirebase > SEM_FIREBASE_REINICIA_MS) {
    char m[100];
    snprintf(m, sizeof(m), "10 min sem conseguir falar com o Firebase (Wi-Fi %s)",
             WiFi.status() == WL_CONNECTED ? "conectado" : "desconectado");
    prefs.putString("motivoRest", m);
    Serial.println("[firebase] 10 min sem sucesso, reiniciando o ESP32...");
    delay(200);
    ESP.restart();
  }

  if (!Firebase.ready()) return;

  if (!firebaseConectouUmaVez) {
    firebaseConectouUmaVez = true;
    logar(INFO, "Conectado ao Firebase (login do ESP32 ok)");
  }
  if (!lacunaVerificada) verificarLacuna();

  if (!streamIniciado) {
    if (Firebase.RTDB.beginStream(&stream, "/comandos")) {
      streamIniciado = true;
      precisaChecar = true;
      logar(INFO, "Escutando comandos do site");
    } else {
      erroFirebase("iniciar escuta de comandos", stream.errorReason());
      return;
    }
  }

  if (!Firebase.RTDB.readStream(&stream)) {
    if (ultimoLogStream == 0 || millis() - ultimoLogStream > LOG_REPETIDO_MS) {
      ultimoLogStream = millis();
      logar(AVISO, "Escuta de comandos caiu: %s (reconectando sozinho)", stream.errorReason().c_str());
    }
  }
  if (stream.streamAvailable()) precisaChecar = true;

  if (precisaChecar || millis() - ultimaChecagem >= CHECAGEM_INTERVALO_MS) {
    precisaChecar = false;
    ultimaChecagem = millis();
    if (checarComandos()) ultimoSucessoFirebase = millis();
  }

  if (sensorMudou || millis() - ultimoEstado >= ESTADO_INTERVALO_MS) {
    sensorMudou = false;
    ultimoEstado = millis();
    if (publicarEstado()) ultimoSucessoFirebase = millis();
  }

  enviarLogs();
}

// ======================================================
// SERIAL (teste de bancada)
// ======================================================

const char* motivoReinicio();

void cuidarSerial() {
  if (!Serial.available()) return;
  char c = Serial.read();
  if (c == 'p' || c == 'P') {
    darPulso();
    logar(INFO, "Pulso de teste pelo cabo USB (Serial)");
  } else if (c == 's' || c == 'S') {
    Serial.printf("[status] fw=%s wifi=%s rssi=%d hora=%s firebase=%s stream=%s quedas=%d pulsos=%d "
                  "erros=%d logs_na_fila=%d heap=%u ligado_ha=%s\n",
                  FW_VERSAO, WiFi.status() == WL_CONNECTED ? "ok" : "NAO", WiFi.RSSI(), horaValida() ? "ok" : "NAO",
                  firebaseIniciado && Firebase.ready() ? "ok" : "NAO", streamIniciado ? "ok" : "NAO", wifiQuedas,
                  pulsos, errosFirebase, evQtd, ESP.getFreeHeap(), duracao(millis()).c_str());
  }
}

// ======================================================
// SETUP / LOOP
// ======================================================

const char* motivoReinicio() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON:  return "ligou na energia (faltou luz ou foi religado)";
    case ESP_RST_SW:       return "reiniciou por software";
    case ESP_RST_PANIC:    return "TRAVOU (erro no codigo)";
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT:      return "watchdog (travou e reiniciou)";
    case ESP_RST_BROWNOUT: return "QUEDA DE TENSAO (fonte fraca ou mau contato)";
    case ESP_RST_EXT:      return "botao reset";
    default:               return "outro";
  }
}

void setup() {
  releDesligadoSeguro();  // PRIMEIRA coisa: relé desligado
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);  // LED apagado
  if (SENSOR_PIN >= 0) pinMode(SENSOR_PIN, INPUT_PULLUP);

  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.println("=== Portao da garagem (ESP32) ===");
  Serial.println("Teste: digite p (pulso) ou s (status) no Serial Monitor.");

  prefs.begin("portao", false);
  logSeq = prefs.getUInt("logSeq", 0);

  esp_reset_reason_t r = esp_reset_reason();
  bool grave = (r == ESP_RST_PANIC || r == ESP_RST_INT_WDT || r == ESP_RST_TASK_WDT || r == ESP_RST_WDT ||
                r == ESP_RST_BROWNOUT);
  logar(grave ? ERRO : INFO, "ESP32 ligou (firmware %s). Motivo: %s", FW_VERSAO, motivoReinicio());
  String motivoAnterior = prefs.getString("motivoRest", "");
  if (motivoAnterior.length()) {
    logar(AVISO, "Reinicio pedido pelo proprio ESP32: %s", motivoAnterior.c_str());
    prefs.remove("motivoRest");
  }

  iniciarWifi();
}

void loop() {
  atualizarLed();
  cuidarSerial();
  cuidarSensor();
  cuidarWifi();
  cuidarAp();
  cuidarOta();
  cuidarFirebase();
}
