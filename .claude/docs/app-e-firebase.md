# App e Firebase

## Login (sem cadastro)
- Tela **Entrar**: um **select de perfil** + **teclado numérico de PIN com 6 dígitos**. Entra automaticamente ao completar o PIN.
- Os usuários são criados **direto no console do Firebase** (Authentication, e-mail/senha). Um e-mail fictício por perfil (ex.: `nome@portao.local`), e o **PIN é a senha**.
- O Firebase Auth exige senha de **no mínimo 6 caracteres**; por isso o PIN tem 6 dígitos. O Firebase limita tentativas erradas.
- O PIN **não** é guardado no banco, só no Auth.
- A lista de perfis vem de `/perfis/{uid}` = `{ nome, email }`, com **leitura pública** (o app precisa do e-mail **antes** de logar). O ESP32 **não** fica em `/perfis`.
- **Criação de contas pelo cliente desligada** (Authentication → Configurações → Ações do usuário → desmarcar "Ativar criação"). Sem isso, qualquer um cria conta pela API com a chave pública.
- Botão **Sair** depois de entrar.

## Dados (Realtime Database)
```
/perfis/{uid}        { nome, email }                                   <- console (leitura pública)
/config              { sensorInstalado: false, tempoCursoMs }          <- console
/comandos/{pushId}   { uid, nome, acao: "acionar", ts }                <- app (ts = relógio do servidor)
/estado              { visto, rssi, ultimoComandoTs, sensorFechado? }  <- ESP32
/historico/{pushId}  { nome, acao, ts, resultado }                     <- ESP32 ao executar
```
- A ação é sempre **"acionar"** (um pulso no BOT), porque a central **alterna** abre/para/fecha. O app não diz "abrir" ou "fechar" ao ESP32; isso é só rótulo de tela.
- **Online/offline** (mesmo padrão do ControleIOT-WEB): o ESP32 grava `/estado/visto` com o timestamp do servidor a cada ~5 s. O app compara com o relógio do servidor (`.info/serverTimeOffset`); mais de ~15 s sem sinal = offline.
- `ts` do comando: o app grava `Date.now() + serverTimeOffset` (não depende do relógio do celular).

## Segurança dos comandos (importante)
- Regras **publicadas a partir de** `firebase/database.rules.json` (trocar `UID_ESP32` pelo UID real em todos os lugares). Publicar pelo console ou `firebase deploy --only database`.
- Só usuário com perfil em `/perfis` cria comando; `uid` e `nome` precisam ser os dele (não dá para falsificar o "quem"); `acao` só `"acionar"`; comando não pode ser editado depois de criado.
- O **servidor recusa** `ts` com mais de 10 s (cobre o celular sem internet que envia o toque guardado minutos depois). O app também desabilita o botão quando `.info/connected` é falso.
- O ESP32 **também ignora comandos antigos** (> ~10 s, via NTP; sem hora sincronizada, **recusa tudo**), executa cada comando **uma vez** (por id) com **cooldown de ~2 s**, e **apaga** o comando depois de executar.
- O ESP32 tem **usuário próprio** (senha longa, não PIN): é o único que lê `/comandos`, apaga comandos e escreve `/estado` e `/historico`.
- Registrar no `docs/mudancas.md` o estado anterior das regras antes de qualquer alteração.

## Passo a passo para criar o banco
1. Console Firebase → novo projeto (ex.: `portao-garagem`), sem Analytics.
2. Realtime Database → criar → **us-central1** → **modo bloqueado**. Anotar a URL.
3. Authentication → E-mail/senha ativado → Configurações → desmarcar **criação de contas** e manter a proteção contra enumeração de e-mail.
4. Criar os usuários (um por pessoa, PIN de 6 dígitos; `esp32@portao.local` com senha longa). Copiar os UIDs.
5. Importar `/perfis` e `/config` (JSON) com os UIDs reais.
6. Publicar `firebase/database.rules.json` com o UID do ESP32.
7. Testar no Rules Playground: anônimo grava comando (negado); perfil grava comando correto (permitido); nome trocado (negado); perfil escreve em `/historico` (negado).
8. Registrar app Web e copiar o `firebaseConfig` para `web/src/environments/environment.ts`.
9. Anotar em `docs/mudancas.md`: id do projeto, URL, e-mails e UIDs, data das regras. **Nunca** PINs/senhas.

## Telas (protótipo clicável já feito)
Link: https://claude.ai/artifact/5AY6BDTKsBkCRKN25QdbL9 (tema escuro, 390×844). Cópia local: `design/prototipo/Main.dc.html`. Especificação completa (cores, tamanhos, estados): `design-do-app.md`.
1. **Entrar:** select de perfil, 6 pontinhos de PIN, teclado numérico.
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
