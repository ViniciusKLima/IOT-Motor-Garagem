# Design do app (especificação do protótipo)

Reconstruir o app **igual ao protótipo**. Referência visual em código: `design/prototipo/Main.dc.html` (formato do editor de design; não roda sozinho, serve só para consultar estilos e estrutura). Idioma da interface: **português do Brasil**. Tema **escuro**, pensado para celular (390 × 844), mas deve funcionar em qualquer largura (PWA).

## Tokens
| Uso | Valor |
|---|---|
| Fundo da tela | `#12161C` |
| Cartões / botões do teclado / barra inferior | `#1B212A` |
| Linhas, bordas, botão da aba ativa, pontinho vazio do PIN | `#2A3340` |
| Texto principal | `#E8ECF1` |
| Texto secundário / rótulos | `#9AA6B2` |
| Texto de apoio em cartões | `#C3CCD6` |
| **Azul** (ação principal, "Aberto", links, PIN preenchido) | `#7AB8FF` |
| **Verde** ("Fechado", ESP32 online) | `#5EE0A0` |
| **Âmbar** (em movimento, aviso) | `#FFC24D` |
| **Vermelho/coral** ("Parar", "Parado no meio") | `#FF8A7A` |
| Texto sobre botão colorido | `#0B1014` |
- Fonte: **Inter** (400, 500, 600, 700). Ícones: traço simples de 2 px (SVG), sem emoji.
- Rótulos em caixa alta: 12 px, peso 600, `letter-spacing: .08em`, cor `#9AA6B2`.
- Cantos: cartões grandes 20 px; cartões médios 16 px; botões de aba 14 px; botões do teclado 18 px.
- Alvos de toque de pelo menos 44 px. Usar `<button>`, `<select>`, `<label>` de verdade e `aria-label` nos botões só com ícone.

## Cabeçalho (sempre visível)
- Esquerda: rótulo "MINHA CASA" e título **"Portão da garagem"** (24 px, 700).
- Direita: pílula `#1B212A` com bolinha verde de 8 px + **"ESP32 online"** (12 px, 500). Ela deve refletir `/estado/online` do Firebase (verde = online; cinza/coral = offline).
- Depois de entrar, abaixo do título: **"Entrou como {Nome} · Sair"** (13 px; "Sair" em azul, 600).

## Tela 1: Entrar
- Título **"Entrar"** (28 px, 700) e subtítulo "Escolha quem você é e digite seu PIN." (14 px, secundário).
- **Select de perfil** (rótulo "PERFIL"): altura 52, borda 1 px `#2A3340`, fundo `#1B212A`, raio 14, fonte 16. Primeira opção "Escolher perfil". Os perfis vêm do Firebase (`/perfis`).
- **PIN** (rótulo "PIN"): 6 pontinhos de 16 px, espaçamento 14; vazio `#2A3340`, preenchido azul. Abaixo, texto de ajuda de 13 px: padrão **"PIN de 6 dígitos"**; se digitar sem escolher o perfil: **"Escolha o perfil primeiro"**.
- **Teclado numérico**: grade de 3 colunas, espaçamento 12; botões de 68 px de altura, fundo `#1B212A`, número 26 px (600). Última linha: espaço vazio, **0**, **←** (apagar, `aria-label="Apagar"`). Teclado grudado embaixo da tela.
- Ao completar o 6º dígito, espera ~250 ms e tenta entrar (Firebase Auth com o e-mail do perfil + PIN). Erro de PIN: limpar os pontinhos e mostrar o texto de ajuda em coral ("PIN incorreto").

## Tela 2: Portão
1. **Cartão de estado** (fundo `#1B212A`, raio 20, padding 20): rótulo "ESTADO", texto grande de estado (32 px, 700, na cor do estado) e uma linha de apoio (13 px). Dentro, a **ilustração do portão** (altura 84, raio 12, fundo `#12161C`, `overflow: hidden`):
   - trilho: linha de 3 px `#2A3340` a 10 px do fundo;
   - **folha do portão** 300 × 56, raio 6, listras verticais (`repeating-linear-gradient`: 6 px `#3A4656` e 14 px `#232B36`), a 12 px do topo;
   - **bloco do motor** à esquerda: 36 × 84, `#232B36`, com a palavra "MOTOR" na vertical (10 px, 600, `#9AA6B2`), por cima da folha.
   - A folha desliza: **fechado** `translateX(0)`, **aberto** `translateX(-250px)`, **parado** `translateX(-125px)`. Transição de 6 s linear no movimento e 0,4 s ease-out ao parar.
2. **Botão principal**: círculo de 168 px, centralizado, com anel `box-shadow: 0 0 0 10px #1B212A`, texto de 18 px (700) em `#0B1014`. Azul normalmente; **coral** quando o rótulo é "Parar".
3. **Cartão de aviso** (raio 16, padding 14/16, ícone de alerta âmbar de 20 px + texto de 13 px): *"Sensor de fechamento ainda não instalado. O estado mostrado é estimado pelo último comando enviado."* **Só aparece enquanto o reed não existir** (flag de configuração).

### Estados e textos
| Estado | Texto | Cor | Linha de apoio | Botão |
|---|---|---|---|---|
| Fechado | Fechado | verde | Estimado pelo último comando | Abrir portão (azul) |
| Abrindo | Abrindo… | âmbar | Toque para parar | Parar (coral) |
| Fechando | Fechando… | âmbar | Toque para parar | Parar (coral) |
| Aberto | Aberto | azul | Estimado pelo último comando | Fechar portão (azul) |
| Parado no meio | Parado no meio | coral | O portão não chegou ao fim | rótulo oposto ao último destino (azul) |

### Transições (protótipo)
Fechado → toque → Abrindo → (6 s) → Aberto → toque → Fechando → (6 s) → Fechado. Toque durante o movimento → Parado no meio. Toque em Parado no meio → volta a mover para o destino **oposto** ao último.

### No app de verdade
- Cada toque grava um comando `"acionar"` em `/comandos` (a central alterna). O app **não** decide abrir/fechar; só escolhe o rótulo.
- Enquanto não houver sensor, o estado é estimado: o tempo de curso (6 s no protótipo) deve ser **configurável** com o tempo real do portão.
- Com o reed instalado, o estado "Fechado" vem de `/estado/sensorFechado` e o aviso some.
- Dar um feedback rápido no toque (botão desabilitado por ~1 s) para evitar toques duplos.

## Tela 3: Histórico
- Rótulo **"QUEM ACIONOU O PORTÃO"**. Lista de cartões (raio 16, padding 14/16): à esquerda a ação (15 px, 600, ex.: "Comando: abrir") e abaixo **"por {nome}"** (13 px, secundário); à direita o horário (13 px, secundário).
- Vazio: *"Nenhum comando ainda. Volte à aba Portão e toque no botão."*
- No app de verdade, vem de `/historico` (mais novo primeiro, com limite e rolagem). O protótipo mostra até 12 itens, com data/hora do servidor.

## Barra inferior (só depois de entrar)
Fundo `#1B212A`, borda superior 1 px `#2A3340`, padding 8/12/20. Duas abas lado a lado, cada uma com ícone de 22 px e rótulo de 12 px (600), altura mínima 52, raio 14: **Portão** (ícone de casa) e **Histórico** (ícone de lista). Aba ativa: fundo `#2A3340`, texto `#E8ECF1`; inativa: transparente, `#9AA6B2`.

## Observações
- Os nomes dos perfis no protótipo (Vinícius, Pai, Mãe, Visitante) são **exemplos**; os reais vêm do Firebase.
- Qualquer PIN de 6 dígitos entra no protótipo; no app de verdade a validação é do Firebase Auth.
