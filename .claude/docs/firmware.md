# Firmware (plano)

## Sketch de teste (já pronto)
`firmware/portao_teste_wifi/portao_teste_wifi.ino`: só testa o Wi-Fi.
- LED da placa: **uma piscada curta a cada 2 s = conectado**; **pisca rápido o tempo todo = sem Wi-Fi**; apagado direto = travado/sem energia. Se o LED não piscar, trocar o pino `LED_PIN` (2, 5, 13...).
- Log em português com o **motivo de cada falha** (senha errada, rede não encontrada, perdeu o sinal...), sinal em dBm com mínimo e máximo, quedas, tentativas falhas, motivo do último reinício (inclui "queda de tensão (fonte fraca)").
- Scan inicial mostrando se a rede aparece e em qual canal (revela rede só de 5 GHz).
- Cria o Wi-Fi próprio `PortaoTeste` para ler o estado no celular sem notebook.
- Reforça a reconexão: depois de 20 s sem conectar, tenta de novo do zero.
- Compilado em 25/11/2026 (core 3.3.13), sem erros.

## Firmware final (pronto, compilado em 25/11/2026)
`firmware/portao/portao.ino`. Compilado com o core ESP32 3.3.13, Firebase Arduino Client (Mobizt) 4.4.17 e ArduinoJson 7.4.3, **sem erros nem avisos**. Ainda **não testado na placa**.

O que faz: escuta `/comandos` (stream + releitura a cada 30 s), valida (hora NTP, idade < 10 s, id não repetido, 2 s entre pulsos), dá o pulso de 500 ms no GPIO 26, grava `/historico` (`resultado`, `comandoId`) e apaga o comando; publica `/estado` (`visto`, `rssi`, `ip`) a cada 5 s; OTA com senha; reinicia sozinho após 10 min sem conseguir falar com o Firebase; relé desligado **antes** de virar saída.

### Como carregar
1. Copiar `firmware/portao/secrets.example.h` para `firmware/portao/secrets.h` e preencher Wi-Fi, `API_KEY`, e-mail/senha do usuário do ESP32 e uma senha de OTA. O `secrets.h` não vai para o git.
2. Arduino IDE → placa **ESP32 Dev Module** → **Ferramentas → Partition Scheme → "Minimal SPIFFS (1.9MB APP with OTA/190KB SPIFFS)"**. Obrigatório: no esquema padrão o programa ocupa 99% e não sobra espaço para mudanças.
3. Enviar pelo USB e abrir o Serial Monitor (115200).
4. **Teste de bancada (sem ligar na central):** digitar `p` + Enter → o relé deve estalar uma vez (meio segundo). `s` + Enter mostra Wi-Fi, hora e Firebase.
5. **Teste de energia:** tirar e voltar o USB algumas vezes: o relé **não pode** estalar sozinho. Se estalar ou ficar ligado direto, trocar `RELE_LIGA` para `HIGH` (e conferir o jumper/alimentação do módulo, ver `hardware.md`).
6. Pelo site, tocar no botão: o Serial mostra `[comando] ... ok` e `>>> PULSO`, e o Histórico do site mostra o comando.
7. Só então ligar o relé na central (COM→GND, NO→BOT, motor fora da tomada ao mexer nos bornes).
8. Depois de instalado: atualizar pelo Wi-Fi em **Ferramentas → Porta → portao-esp32** (pede a senha de OTA).

### Configurações no topo do código
- `RELE_LIGA` (LOW/HIGH), `PULSO_MS` (500), `INTERVALO_MIN_MS` (2000), `IDADE_MAX_MS` (10000).
- `SENSOR_PIN = -1` até o reed existir; depois colocar o pino (ex.: 27) e, no Firebase, `config/sensorInstalado = true`.

## Ferramentas
Arduino IDE (placa "ESP32 Dev Module", Serial 115200). O PlatformIO também serve se o Vinícius preferir.
