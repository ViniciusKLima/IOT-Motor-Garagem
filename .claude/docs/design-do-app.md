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
> Atualizada em 25/11/2026: login com **Google** (o protótipo ainda mostra perfil + PIN).
- No topo, a **ilustração do portão em tamanho grande** (148 px, raio 20), fechada. Ao abrir a tela ela "espia" **uma vez** (abre ~30% e fecha em 2,6 s); desligado com `prefers-reduced-motion`.
- Título **"Abra o portão pelo celular"** (30 px, 700) e texto de apoio (15 px, secundário).
- Botão **"Continuar com Google"** no rodapé: branco, altura 60, raio 18, sombra; logo do Google num quadrado `#F1F3F4` de 44 px à esquerda (vira um indicador girando enquanto abre o Google).
- Ajuda de 13 px abaixo: "Só quem foi liberado consegue entrar."; erros em coral.

## Tela 2: Portão
1. **Cartão de estado** (fundo `#1B212A`, raio 20, padding 20): rótulo "ESTADO", texto grande de estado (32 px, 700, na cor do estado) e uma linha de apoio (13 px). Dentro, a **ilustração do portão** (altura 84, raio 12, fundo `#12161C`, `overflow: hidden`):
   - trilho: linha de 3 px `#2A3340` a 10 px do fundo; **coluna** de 10 px à direita (`#2F3A48`), onde o portão encosta fechado;
   - **folha do portão** ocupa da borda esquerda até a coluna (largura acompanha a tela), raio 6, listras verticais (`repeating-linear-gradient`: 6 px `#3A4656` e 14 px `#232B36`);
   - **bloco do motor** à esquerda: 36 px, `#232B36`, com "MOTOR" na vertical, por cima da folha.
   - A folha desliza proporcional à abertura: `translateX(calc(abertura * (-100% + 56px)))` (aberto sobra só uma ponta depois do motor). Componente `ilustracao-portao.ts`.
   - **Curso real: 9 s** (`/config/tempoCursoMs = 9000`). Tocar durante o movimento para a folha onde ela estiver.
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
Fechado → toque → Abrindo → (9 s) → Aberto → toque → Fechando → (9 s) → Fechado. Toque durante o movimento → Parado no meio. Toque em Parado no meio → volta a mover para o destino **oposto** ao último.

### No app de verdade
- Cada toque grava um comando `"acionar"` em `/comandos` (a central alterna). O app **não** decide abrir/fechar; só escolhe o rótulo.
- Enquanto não houver sensor, o estado é estimado: o tempo de curso (6 s no protótipo) deve ser **configurável** com o tempo real do portão.
- Com o reed instalado, o estado "Fechado" vem de `/estado/sensorFechado` e o aviso some.
- Dar um feedback rápido no toque (botão desabilitado por ~1 s) para evitar toques duplos.

## Tela 3: Histórico
- Título "Quem acionou o portão" (22 px, 700).
- **Filtros** lado a lado: **Pessoa** (select, "Todas" + nomes do histórico) e **Dia** (campo de data). Trocar o dia faz uma consulta no banco só daquele dia; sem dia, mostra os 200 mais recentes. "Limpar filtros" aparece quando há filtro.
- Resumo: "N acionamentos (recentes)".
- **Agrupado por dia** ("Hoje", "Ontem", "Segunda-feira, 24 de novembro"). Cada grupo é um cartão com linhas: **nome** (15 px, 600) e a ação ("Abriu/Fechou/Parou o portão"; "Não executado: motivo" em coral) à esquerda, **hora** (HH:mm, números tabulares) à direita.
- Vazio: "Nenhum comando ainda. Volte ao controle e toque no botão." (ou mensagem de filtro sem resultado).

## Barra de navegação (só depois de entrar)
- **Pílula flutuante centralizada** no rodapé (fixa, 18 px da borda + área segura), fundo `#1B212A` translúcido com desfoque, borda `#2A3340`, sombra.
- Dois botões só com ícone (68 × 50): **controle remoto** (tela do portão) e **relógio** (histórico), com `aria-label`.
- Um **destaque azul** desliza por trás do botão ativo (0,28 s; sem animação com `prefers-reduced-motion`). Ícone ativo em `#0B1014`.

## Observações
- Os nomes no protótipo (Vinícius, Pai, Mãe, Visitante) são **exemplos**; os reais vêm de `/permitidos`.
