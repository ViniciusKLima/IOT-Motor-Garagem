# Portão da garagem IoT (ESP32 + Firebase)

Abrir o portão da garagem de casa (motor Rossi, portão de correr) pelo celular/web, com login por perfil + PIN e registro de quem acionou. Hoje usamos controles genéricos que quebram toda hora.

## Como trabalhar neste projeto
- Responder e comentar o código em português do Brasil.
- Vinícius é desenvolvedor (Angular, Ionic, TypeScript, Firebase), mas é iniciante em eletrônica e ruim de solda: explicar ligações físicas passo a passo e **sem exigir solda** (jumpers e bornes de parafuso).
- Antes de alterar algo no Firebase (regras, usuários, configuração, hosting), anotar o estado anterior completo (com ids) em `docs/mudancas.md`, para poder reverter.
- Segurança primeiro: a parte de 110/220 V da central é perigosa. Só se mexe nos bornes de baixa tensão (BOT/GND), com o motor desligado da tomada.
- Não inventar: o que não foi confirmado está marcado como **(a confirmar)**.

## Estado atual
- [x] Em mãos: ESP32 e módulo relé 5 V (com optoacoplador).
- [ ] Sensor magnético (reed) **ainda não comprado**. Até lá, o estado do portão é **estimado** pelo último comando.
- [x] Medir o sinal de Wi-Fi no ponto do motor (`firmware/portao_teste_wifi/`): −75 a −77 dBm, 0 quedas em 18 min. Suficiente, sem repetidor por ora.
- [ ] Confirmar o borne BOT com o teste do fio (ver `.claude/docs/hardware.md`).
- [x] Site (Angular PWA) pronto em `web/`, falta colar o `firebaseConfig` e publicar na Vercel.
- [x] Regras do banco em `firebase/database.rules.json` (trocar `UID_ESP32`).
- Protótipo visual do app (clicável): https://claude.ai/artifact/5AY6BDTKsBkCRKN25QdbL9

## Decisões fechadas
1. O ESP32 aciona a **botoeira (BOT) da central Rossi por um relé**, com contato seco. Não copiar o sinal de RF (controles Rossi são rolling code) e não soldar no controle.
2. **Sem solda**: jumpers, bornes de parafuso, protoboard ou shield com bornes.
3. **Sem fotocélula** (não usam e não querem). Cuidado extra ao fechar pelo app, sem sensor de obstáculo.
4. Login simples: **tela com seleção de perfil + PIN numérico**. Sem cadastro; os usuários são criados direto no Firebase. Objetivo: log de **quem** abriu.
5. O sensor reed fica para depois (comprar). Quando chegar: 1 sensor perto do motor (portão fechado), 2º opcional.
6. Tudo o que for possível fica junto do motor, numa caixa plástica fechada.

## Arquitetura (resumo)
```
App/Web (Angular PWA, Vercel) --Firebase Auth (perfil+PIN)--> Firebase RTDB
                                                                |  /comandos
ESP32 (no motor) <------ escuta /comandos, valida ---------------+
   |-- GPIO26 -> módulo relé -> COM→GND, NO→BOT (central Rossi)
   |-- (futuro) GPIO reed -> estado fechado/não fechado
   `-- escreve /estado e /historico (quem, quando, rssi)
```

## Mapa dos documentos
- @.claude/docs/hardware.md — motor, placa, ligação do relé, alimentação, sensor, cuidados.
- @.claude/docs/app-e-firebase.md — login, dados, regras, segurança dos comandos, telas.
- @.claude/docs/design-do-app.md — especificação visual detalhada do app (cores, tamanhos, estados). Reconstruir **igual ao protótipo** (`design/prototipo/Main.dc.html`).
- @.claude/docs/wifi.md — o problema do sinal no portão, como medir, soluções. Resultado do teste: estável em −75 a −77 dBm, 0 quedas em 18 min; suficiente, sem repetidor por ora.
- @.claude/docs/firmware.md — plano do firmware e o sketch de teste de Wi-Fi.
- @.claude/docs/roadmap.md — fases, pendências e perguntas em aberto.
