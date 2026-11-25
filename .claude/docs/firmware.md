# Firmware (plano)

## Sketch de teste (já pronto)
`firmware/portao_teste_wifi/portao_teste_wifi.ino`: só testa o Wi-Fi.
- LED da placa: **uma piscada curta a cada 2 s = conectado**; **pisca rápido o tempo todo = sem Wi-Fi**; apagado direto = travado/sem energia. Se o LED não piscar, trocar o pino `LED_PIN` (2, 5, 13...).
- Log em português com o **motivo de cada falha** (senha errada, rede não encontrada, perdeu o sinal...), sinal em dBm com mínimo e máximo, quedas, tentativas falhas, motivo do último reinício (inclui "queda de tensão (fonte fraca)").
- Scan inicial mostrando se a rede aparece e em qual canal (revela rede só de 5 GHz).
- Cria o Wi-Fi próprio `PortaoTeste` para ler o estado no celular sem notebook.
- Reforça a reconexão: depois de 20 s sem conectar, tenta de novo do zero.
- Foi escrito sem compilar (sem toolchain Arduino no ambiente). Se der erro de compilação, corrigir.

## Firmware final (a fazer)
Reaproveitar do sketch de teste: LED de status, eventos do Wi-Fi, log e reconexão forçada.
1. **Wi-Fi:** reconexão automática + watchdog; `WiFi.setSleep(false)`.
2. **Hora:** NTP (necessário para validar a idade dos comandos).
3. **Firebase:** login com usuário próprio do ESP32; escutar `/comandos` (stream ou polling curto).
4. **Executar comando:** validar idade (< ~10 s), id ainda não executado e cooldown (~2 s); dar o **pulso de 500 ms** no relé (GPIO 26); gravar em `/historico` (nome, ts, resultado).
5. **Estado:** publicar `/estado` (`visto` com timestamp do servidor a cada ~5 s, `rssi`). Seguir o contrato em `app-e-firebase.md` ("O que o app espera do ESP32"): `/historico` com `resultado` e `comandoId`, e apagar o comando executado. Quando o reed existir, ler o pino (`INPUT_PULLUP`, com debounce) e publicar `sensorFechado`.
6. **Boot seguro:** relé em nível inativo antes de `pinMode(OUTPUT)`; o portão não pode mexer ao ligar/religar a energia.
7. **OTA (ArduinoOTA):** atualizar o código pelo Wi-Fi, já que o ESP32 vai ficar fixo numa caixa no portão.
8. **Falha de Wi-Fi:** o portão continua funcionando pelos controles; o ESP32 só volta a tentar conectar.

## Ferramentas
Arduino IDE (placa "ESP32 Dev Module", Serial 115200). O PlatformIO também serve se o Vinícius preferir.
