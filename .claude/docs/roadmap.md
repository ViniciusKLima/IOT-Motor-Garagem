# Roadmap e pendências

## Fases
0. ~~**Medir o Wi-Fi no portão**~~ feito: −75 a −77 dBm, 0 quedas em 18 min. Sem repetidor por ora (ver `wifi.md`).
1. **Validar o BOT:** teste do fio entre BOT e GND. Depois ligar o relé (COM→GND, NO→BOT) e acionar por um botão ou comando serial.
2. **Firmware + Firebase:** Auth do ESP32, escutar `/comandos`, pulso no relé, `/historico`, NTP, regras de segurança.
3. **App (Ionic + Angular, PWA):** Entrar (perfil + PIN), Portão, Histórico, conforme o protótipo.
4. **Sensor reed:** comprar; 1 sensor (fechado) perto do motor; estado real no app. 2º sensor opcional.
5. **Acabamento:** caixa plástica fechada, fixação, OTA, alimentação definitiva, uso diário com a família.

## Perguntas em aberto
- Alimentação: fonte USB ou 12 V da central com step-down? (medir os 12 V antes)
- Módulo relé: ativo-baixo ou ativo-alto? (testar religando a energia)
- Confirmar no manual: `FCH`, `SIN`, tensão do motor (110/220 V) e modelo da central.
- Realtime Database ou Firestore (a sugestão é RTDB)? Hospedagem do PWA (Firebase Hosting?).
- Quais perfis existirão e com quais nomes (o protótipo usa exemplos)?

## Fora do escopo (por decisão)
- Copiar o sinal de RF dos controles.
- Fotocélula.
- Soldar o ESP32 em qualquer coisa.
- Tela de cadastro de usuários.
