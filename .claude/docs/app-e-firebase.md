# App e Firebase

## Login (com Google, sem cadastro)
- Tela **Entrar**: um botão **"Entrar com Google"**. A sessão fica salva no aparelho (só sai clicando em Sair).
- Quem pode abrir: lista **`/permitidos/{email}` = `{ nome }`**, editada no console. A chave é o e-mail em minúsculas com **`,` no lugar de `.`** (o RTDB não aceita ponto na chave). Ex.: `fulano@gmail.com` → `fulano@gmail,com`.
- Qualquer conta Google consegue logar no Firebase, mas sem estar em `/permitidos` não lê nada nem envia comando; o site mostra "ainda não tem acesso" e desloga.
- A **criação de contas precisa estar ligada** no Auth (senão contas Google novas não entram); a proteção é a lista.
- Celular e app instalado (PWA) usam login por **redirecionamento**; computador usa popup. Para funcionar no Safari/iPhone, o `authDomain` é o domínio do site (`iot-motor-garagem.vercel.app`) e o `web/vercel.json` repassa `/__/auth/*` para `iot-motor-garagem.firebaseapp.com`. O service worker ignora `/__/*`.
- O ESP32 continua com usuário **e-mail/senha** próprio (manter esse provedor ativo).

### Configuração no console (uma vez)
1. Authentication → Método de login → **Google** → ativar (escolher o e-mail de suporte).
2. Authentication → Configurações → Ações do usuário → **marcar "Ativar criação (inscrição)"**.
3. Authentication → Configurações → **Domínios autorizados** → adicionar `iot-motor-garagem.vercel.app`.
4. Google Cloud Console (mesmo projeto) → APIs e serviços → **Credenciais** → cliente OAuth "Web client (auto created by Google Service)" → **URIs de redirecionamento autorizados** → adicionar `https://iot-motor-garagem.vercel.app/__/auth/handler`.
5. Realtime Database → Dados → importar o `permitidos.json` **no nó `/permitidos`** (não na raiz).
6. Realtime Database → Regras → colar `firebase/database.rules.json` e publicar (o arquivo já tem o UID do ESP32).

## Dados (Realtime Database)
```
/permitidos/{email}  { nome }                                          <- console (cada um lê só o seu)
/config              { sensorInstalado: false, tempoCursoMs }          <- console
/comandos/{pushId}   { uid, nome, acao: "acionar", ts }                <- app (ts = relógio do servidor)
/estado              { visto, rssi, ultimoComandoTs, sensorFechado? }  <- ESP32
/historico/{pushId}  { nome, acao, ts, resultado }                     <- ESP32 ao executar
/logs/{0..199}       { n, ts, nivel, msg, up }                         <- ESP32 (log técnico, anel)
```
- A ação é sempre **"acionar"** (um pulso no BOT), porque a central **alterna** abre/para/fecha. O app não diz "abrir" ou "fechar" ao ESP32; isso é só rótulo de tela.
- **Online/offline** (mesmo padrão do ControleIOT-WEB): o ESP32 grava `/estado/visto` com o timestamp do servidor a cada ~5 s. O app compara com o relógio do servidor (`.info/serverTimeOffset`); mais de ~15 s sem sinal = offline.
- `ts` do comando: o app grava `Date.now() + serverTimeOffset` (não depende do relógio do celular).

## Segurança dos comandos (importante)
- Regras **publicadas a partir de** `firebase/database.rules.json` (já com o UID real do ESP32: `VH2QhwXbGwhya40QFhiY58C6iEV2`). Publicar pelo console ou `firebase deploy --only database`.
- Só quem está em `/permitidos` (e-mail Google verificado) cria comando; `uid` e `nome` precisam ser os dele (não dá para falsificar o "quem"); `acao` só `"acionar"`; comando não pode ser editado depois de criado.
- O **servidor recusa** `ts` com mais de 10 s (cobre o celular sem internet que envia o toque guardado minutos depois). O app também desabilita o botão quando `.info/connected` é falso.
- O ESP32 **também ignora comandos antigos** (> ~10 s, via NTP; sem hora sincronizada, **recusa tudo**), executa cada comando **uma vez** (por id) com **cooldown de ~2 s**, e **apaga** o comando depois de executar.
- O ESP32 tem **usuário próprio** (e-mail/senha, senha longa): é o único que lê `/comandos`, apaga comandos e escreve `/estado` e `/historico`.
- Registrar no `docs/mudancas.md` o estado anterior das regras antes de qualquer alteração.

## Passo a passo para criar o banco
Feito em 25/11/2026 (projeto `iot-motor-garagem`, RTDB em us-central1). Para o login com Google, seguir "Configuração no console" acima. Anotar em `docs/mudancas.md` o estado anterior antes de mexer. **Nunca** anotar senhas.

## Telas (protótipo clicável já feito)
Link: https://claude.ai/artifact/5AY6BDTKsBkCRKN25QdbL9 (tema escuro, 390×844). Cópia local: `design/prototipo/Main.dc.html`. Especificação completa (cores, tamanhos, estados): `design-do-app.md`.
1. **Entrar:** botão "Entrar com Google" (substituiu perfil + PIN em 25/11/2026).
2. **Portão:** estado grande (*Fechado, Abrindo…, Aberto, Parado no meio*), ilustração do portão deslizando, botão redondo grande (*Abrir portão / Fechar portão / Parar*), chip "ESP32 online" e o aviso de sensor não instalado.
3. **Histórico:** "Quem acionou o portão", com o nome, a ação e o horário.
4. Barra inferior com duas abas: Portão e Histórico. Mostra "Entrou como X · Sair".

## Stack (decidida)
- **Angular 22 (standalone + signals) como PWA**, com o **SDK modular do Firebase** direto (sem AngularFire, para não depender da compatibilidade de versões). Código em `web/`.
- Sem Ionic: o design é todo próprio (`design-do-app.md`) e os componentes do Ionic não acrescentariam nada. Se um dia quiser app de loja, dá para empacotar com Capacitor.
- **Hospedagem: Vercel** (Root Directory = `web`; o `web/vercel.json` já tem build, pasta de saída e cabeçalhos).
- Online/offline segue o padrão do **ControleIOT-WEB** (`visto` + `.info/serverTimeOffset`).

## O que o app espera do ESP32 (contrato)
- `/estado/visto` = `{".sv": "timestamp"}` a cada ~5 s (senão o app mostra "ESP32 offline" e bloqueia o botão).
- Ao executar ou recusar um comando, gravar em `/historico/{pushId}`: `{ nome, acao: "acionar", ts: {".sv":"timestamp"}, resultado, comandoId }`.
  - `resultado`: `"ok"` quando deu o pulso; qualquer outro texto curto quando recusou (ex.: `"comando antigo"`, `"sem hora"`). Só `"ok"` conta para a estimativa.
  - `comandoId`: a chave do comando em `/comandos`. O app usa para confirmar o toque (sem ela, mostra "O ESP32 não confirmou" depois de 12 s).
- Depois, **apagar** `/comandos/{id}`.
- Estado estimado: o app reproduz os pulsos `"ok"` do histórico (últimos 50), começando de "fechado", com `config/tempoCursoMs` como tempo de curso. Controle remoto não entra; por isso é **estimado** até o reed.
