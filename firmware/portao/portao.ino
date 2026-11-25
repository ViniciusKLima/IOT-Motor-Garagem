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
  - Publica /estado (visto, rssi) a cada 5 s: é o "ESP32 online" do site.
  - Atualização pelo Wi-Fi (OTA) com senha.

  Placa: "ESP32 Dev Module". Serial Monitor: 115200.
  Bibliotecas: "Firebase Arduino Client Library for ESP8266 and ESP32"
  (Mobizt, 4.4.x) e "ArduinoJson" (7.x).

  TESTE SEM FIREBASE: no Serial Monitor, digite  p  + Enter para dar
  um pulso no relé, e  s  + Enter para ver o estado.

  Contrato com o site: .claude/docs/app-e-firebase.md
  ======================================================
*/

#include <WiFi.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include <time.h>
#include <sys/time.h>
#include <esp_system.h>

#include "secrets.h"  // WIFI_SSID, WIFI_PASSWORD, API_KEY, DATABASE_URL, ESP_EMAIL, ESP_PASSWORD, OTA_PASSWORD

// ======================================================
// CONFIGURAÇÃO (confira com o seu hardware)
// ======================================================

// Relé que fecha BOT com GND na central
const int RELE_PIN = 26;

// Nível que LIGA o relé. A maioria dos módulos com optoacoplador é
// ativo-BAIXO (LOW liga). Se o relé ficar ligado direto ao energizar,
// troque para HIGH. TESTE: tire e volte a energia; o portão NÃO pode mexer.
const int RELE_LIGA = LOW;
const int RELE_DESLIGA = (RELE_LIGA == LOW) ? HIGH : LOW;

const unsigned long PULSO_MS = 500;        // como apertar o botão
const unsigned long INTERVALO_MIN_MS = 2000; // entre dois pulsos
const long long IDADE_MAX_MS = 10000;      // comando mais velho que isso é recusado
const long long FUTURO_MAX_MS = 5000;      // tolerância de relógio para o futuro

// Sensor reed (FUTURO). Deixe -1 enquanto não estiver instalado.
// Ligação: reed entre o pino e o GND (usa INPUT_PULLUP, sem resistor).
// Ex.: const int SENSOR_PIN = 27;
const int SENSOR_PIN = -1;

// LED da placa (status)
#ifndef LED_BUILTIN
#define LED_BUILTIN 2
#endif
const int LED_PIN = LED_BUILTIN;

// Tempos
const unsigned long ESTADO_INTERVALO_MS = 5000;   // "estou vivo" no banco
const unsigned long CHECAGEM_INTERVALO_MS = 30000; // relê /comandos mesmo sem aviso do stream
const unsigned long WIFI_RETRY_MS = 20000;        // força nova tentativa de Wi-Fi
const unsigned long SEM_FIREBASE_REINICIA_MS = 10UL * 60UL * 1000UL; // 10 min sem sucesso = reinicia

// ======================================================
// ESTADO
// ======================================================

FirebaseData fbdo;    // leituras e escritas
FirebaseData stream;  // escuta /comandos
FirebaseAuth auth;
FirebaseConfig config;

bool firebaseIniciado = false;
bool streamIniciado = false;
bool precisaChecar = true;
bool ntpIniciado = false;

unsigned long ultimoPulso = 0;
bool jaPulsou = false;
unsigned long ultimoEstado = 0;
unsigned long ultimaChecagem = 0;
unsigned long ultimaTentativaWifi = 0;
unsigned long ultimoSucessoFirebase = 0;

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
  Serial.println(">>> PULSO no relé (BOT)");
  digitalWrite(RELE_PIN, RELE_LIGA);
  delay(PULSO_MS);
  digitalWrite(RELE_PIN, RELE_DESLIGA);
  ultimoPulso = millis();
  jaPulsou = true;
}

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

// ======================================================
// LED DE STATUS
//   pisca rápido        = sem Wi-Fi
//   pisca a cada 1 s    = Wi-Fi ok, esperando hora/Firebase
//   piscada curta a 2 s = tudo funcionando
// ======================================================

void atualizarLed() {
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
    case 201: return "rede NAO encontrada (nome errado, longe ou so 5 GHz)";
    case 202: return "autenticacao falhou (senha incorreta?)";
    case 204: return "handshake expirou";
    default:  return "outro motivo";
  }
}

void eventoWifi(WiFiEvent_t evento, WiFiEventInfo_t info) {
  if (evento == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
    Serial.printf("[wifi] conectado, IP %s, sinal %d dBm\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
  } else if (evento == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
    int r = info.wifi_sta_disconnected.reason;
    Serial.printf("[wifi] caiu. Motivo %d: %s\n", r, motivoWifi(r));
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
  Serial.printf("[wifi] conectando em \"%s\"...\n", WIFI_SSID);
}

void cuidarWifi() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!ntpIniciado) {
      configTime(0, 0, "pool.ntp.org", "time.google.com");
      ntpIniciado = true;
      Serial.println("[hora] sincronizando (NTP)...");
    }
    return;
  }
  if (millis() - ultimaTentativaWifi >= WIFI_RETRY_MS) {
    ultimaTentativaWifi = millis();
    Serial.println("[wifi] sem conexao ha 20 s, tentando do zero...");
    WiFi.disconnect();
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  }
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
    Serial.printf("[sensor] portao %s\n", sensorFechado ? "FECHADO" : "NAO fechado");
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
  Serial.printf("[historico] erro: %s\n", fbdo.errorReason().c_str());
  return false;
}

void apagarComando(const String& id) {
  String caminho = "/comandos/" + id;
  if (!Firebase.RTDB.deleteNode(&fbdo, caminho.c_str()))
    Serial.printf("[comando] erro ao apagar %s: %s\n", id.c_str(), fbdo.errorReason().c_str());
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

  Serial.printf("[comando] %s de %s (idade %lld ms): %s\n", id.c_str(), nome.c_str(), idade, resultado);

  if (strcmp(resultado, "ok") == 0) darPulso();
  gravarHistorico(nome, resultado, id);
  apagarComando(id);
}

bool checarComandos() {
  if (!Firebase.RTDB.getJSON(&fbdo, "/comandos")) {
    String erro = fbdo.errorReason();
    // /comandos vazio (ninguém tocou no botão): não é erro
    if (fbdo.httpCode() == 404 || erro.indexOf("not found") >= 0 || erro.indexOf("path not exist") >= 0) return true;
    Serial.printf("[comandos] erro ao ler: %s\n", erro.c_str());
    return false;
  }

  String payload = fbdo.payload();
  if (payload.length() == 0 || payload == "null") return true;

  JsonDocument doc;
  if (deserializeJson(doc, payload)) {
    Serial.println("[comandos] JSON invalido");
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

bool publicarEstado() {
  FirebaseJson j;
  j.set("visto/.sv", "timestamp");
  j.set("rssi", WiFi.RSSI());
  j.set("ip", WiFi.localIP().toString());
  if (jaPulsou) j.set("ultimoPulsoHaS", (int)((millis() - ultimoPulso) / 1000));
  if (SENSOR_PIN >= 0 && sensorLido) j.set("sensorFechado", sensorFechado);
  if (Firebase.RTDB.updateNode(&fbdo, "/estado", &j)) return true;
  Serial.printf("[estado] erro: %s\n", fbdo.errorReason().c_str());
  return false;
}

void cuidarFirebase() {
  if (!firebaseIniciado) {
    if (WiFi.status() == WL_CONNECTED && horaValida()) iniciarFirebase();
    return;
  }

  // Muito tempo sem conseguir falar com o Firebase: reinicia do zero.
  // (seguro: ao ligar, o relé fica desligado e comandos velhos são recusados)
  if (millis() - ultimoSucessoFirebase > SEM_FIREBASE_REINICIA_MS) {
    Serial.println("[firebase] 10 min sem sucesso, reiniciando o ESP32...");
    delay(200);
    ESP.restart();
  }

  if (!Firebase.ready()) return;

  if (!streamIniciado) {
    if (Firebase.RTDB.beginStream(&stream, "/comandos")) {
      streamIniciado = true;
      precisaChecar = true;
      Serial.println("[firebase] escutando /comandos");
    } else {
      Serial.printf("[firebase] erro no stream: %s\n", stream.errorReason().c_str());
      return;
    }
  }

  if (!Firebase.RTDB.readStream(&stream)) {
    Serial.printf("[firebase] stream caiu: %s\n", stream.errorReason().c_str());
  }
  if (stream.streamTimeout()) {
    Serial.println("[firebase] stream sem resposta, reconectando...");
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
}

// ======================================================
// SERIAL (teste de bancada)
// ======================================================

void cuidarSerial() {
  if (!Serial.available()) return;
  char c = Serial.read();
  if (c == 'p' || c == 'P') {
    Serial.println("[teste] pulso pedido pelo Serial");
    darPulso();
  } else if (c == 's' || c == 'S') {
    Serial.printf("[status] wifi=%s rssi=%d hora=%s firebase=%s stream=%s heap=%u\n",
                  WiFi.status() == WL_CONNECTED ? "ok" : "NAO", WiFi.RSSI(),
                  horaValida() ? "ok" : "NAO", firebaseIniciado && Firebase.ready() ? "ok" : "NAO",
                  streamIniciado ? "ok" : "NAO", ESP.getFreeHeap());
  }
}

// ======================================================
// SETUP / LOOP
// ======================================================

const char* motivoReinicio() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON:  return "ligou na energia";
    case ESP_RST_SW:       return "reiniciou por software";
    case ESP_RST_PANIC:    return "ERRO no codigo";
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT:      return "watchdog (travou)";
    case ESP_RST_BROWNOUT: return "QUEDA DE TENSAO (fonte fraca)";
    default:               return "outro";
  }
}

void setup() {
  releDesligadoSeguro();  // PRIMEIRA coisa: relé desligado
  pinMode(LED_PIN, OUTPUT);
  if (SENSOR_PIN >= 0) pinMode(SENSOR_PIN, INPUT_PULLUP);

  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.println("=== Portao da garagem (ESP32) ===");
  Serial.printf("Ultimo reinicio: %s\n", motivoReinicio());
  Serial.println("Teste: digite p (pulso) ou s (status) no Serial Monitor.");

  iniciarWifi();
}

void loop() {
  atualizarLed();
  cuidarSerial();
  cuidarSensor();
  cuidarWifi();
  cuidarOta();
  cuidarFirebase();
}
