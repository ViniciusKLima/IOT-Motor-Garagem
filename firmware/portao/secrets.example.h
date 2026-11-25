// ======================================================
// MODELO DE CREDENCIAIS
// Copie este arquivo como "secrets.h" (mesma pasta) e preencha.
// O secrets.h fica fora do git (.gitignore): as senhas não vão pro GitHub.
// ======================================================

#pragma once

// Rede Wi-Fi (o ESP32 só conecta em 2,4 GHz)
#define WIFI_SSID "NOME_DA_REDE"
#define WIFI_PASSWORD "SENHA_DA_REDE"

// Firebase (Console > Configurações do projeto)
#define API_KEY "COLE_A_API_KEY"
#define DATABASE_URL "https://iot-motor-garagem-default-rtdb.firebaseio.com"

// Usuário PRÓPRIO do ESP32 no Firebase Auth (o UID dele está nas regras)
#define ESP_EMAIL "esp32@portao.local"
#define ESP_PASSWORD "SENHA_LONGA_DO_ESP32"

// Senha para atualizar o código pelo Wi-Fi (OTA), escolha uma qualquer
#define OTA_PASSWORD "TROQUE_ESTA_SENHA"
