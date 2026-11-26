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

## Log técnico e diagnóstico (firmware 1.1.0)
- **LED da placa desligado** (`LED_STATUS = false`; mude para `true` para o LED mostrar o estado).
- Eventos vão para o Serial e para **`/logs/{0..199}`** (anel com os últimos 200; `{ n, ts, nivel: info|aviso|erro, msg, up }`). Sem internet, até 40 eventos ficam na memória e são enviados quando voltar. O contador `n` fica na memória flash (sobrevive a reinício).
- O que é registrado: ligou + motivo do reinício (falta de luz, fonte fraca, travamento); quanto tempo ficou sem sinal antes de ligar; Wi-Fi conectou/caiu (motivo)/voltou (tempo fora); "ainda sem Wi-Fi" a cada 5 min; sinal fraco (< −80 dBm, a cada 30 min); hora sincronizada; conectou no Firebase; erros do Firebase (resumidos a cada 5 min); escuta de comandos caiu; cada comando **executado (PULSO dado)** ou **recusado (motivo)**; pulso de teste pelo USB; reinício pedido pelo próprio ESP32 (10 min sem Firebase, OTA).
- Com sensor reed: se o portão estava fechado e continua fechado 5 s depois do pulso → erro "pulso dado, mas o portão continuou fechado".
- `/estado` também traz: `fw`, `uptimeS`, `reinicio`, `quedasWifi`, `foraWifiS`, `pulsos`, `ultimoPulsoTs`, `errosFirebase`, `heap`, `ip`.
- No site: tocar no chip **"ESP32 online/offline"** → tela **Diagnóstico** (situação, ficha técnica, ajuda "o portão não abriu?" e o log com filtro "Avisos e erros").

## Wi-Fi de emergência (firmware 1.2.0)
- Se ficar **1 min sem Wi-Fi** (inclusive logo ao ligar), o ESP32 cria a rede **`Portao-ESP32`** (senha = `OTA_PASSWORD` do `secrets.h`).
- No celular, conectar nela e abrir **http://192.168.4.1**: estado (Wi-Fi, sinal, Firebase, hora, quedas, pulsos, reinício) e os **últimos 60 eventos**. Atualiza a cada 5 s. **Somente leitura** (não aciona o portão).
- Desliga sozinho depois de **5 min** conectado com Firebase ok. Com internet, a mesma página abre pelo IP da casa (aparece no Diagnóstico do site).
- Limitação: enquanto o ESP32 procura o roteador, ele pode trocar de canal e o celular cair dessa rede por alguns segundos; é só reconectar.

## Ferramentas
Arduino IDE (placa "ESP32 Dev Module", Serial 115200). O PlatformIO também serve se o Vinícius preferir.
