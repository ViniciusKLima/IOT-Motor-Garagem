# Mudanças em plataformas

Anotar aqui o estado ANTERIOR (com ids) antes de qualquer alteração no Firebase, para poder reverter.

## 25/11/2026: troca do login perfil + PIN por login com Google

### Estado ANTERIOR (para reverter)
- **Regras do RTDB:** as de `firebase/database.rules.json` no commit `8db493a` (com `/perfis` de leitura pública), com `UID_ESP32` trocado pelo UID real do ESP32 **(a confirmar no console)**.
- **Authentication:** provedor **E-mail/senha** ativo; **criação de contas desligada** (Configurações → Ações do usuário). Usuários:
  - `Ku6wtAkAeqMxk59RaWYFkjhMG9k2`: Vinícius (e-mail/senha, PIN como senha)
  - `T7kcYLtTk8gcKW0MRCBcm1OcVP02`: Fia (e-mail/senha, PIN como senha)
  - `go4gdu12mLhwfm6LmPXeY3d9Zuy1`: Amauri (e-mail/senha, PIN como senha)
  - ESP32: `esp32@portao.local` (UID **a confirmar** no console; é o mesmo das regras).
- **Dados:** `/perfis/{uid}` = `{ nome, email }` dos 3 usuários acima; `/config` = `{ sensorInstalado: false, tempoCursoMs: 15000 }`.
- **Site:** `authDomain` = `iot-motor-garagem.firebaseapp.com`.

### Mudança
- Ativar provedor **Google**; **religar** a criação de contas (senão contas Google novas não entram). Quem pode abrir é decidido pela lista `/permitidos`.
- Importar `/permitidos` (arquivo local `permitidos.json`, fora do git) e publicar as novas regras (trocar `UID_ESP32` de novo).
- Adicionar `iot-motor-garagem.vercel.app` em Authentication → Configurações → Domínios autorizados, e o redirect URI do Google (ver `app-e-firebase.md`).
- Depois de testado: apagar `/perfis` e os 3 usuários e-mail/senha acima. **Manter** o usuário do ESP32 e o provedor E-mail/senha (o ESP32 usa).

### Estado REAL encontrado no console (25/11/2026, antes das alterações pelo Claude)
Diferente do anotado acima (aquilo era o planejado). Conta Google dona do projeto: a 2ª conta do Chrome (`/u/1/`).
- **Authentication:** nenhum usuário. Provedores: **E-mail/senha desativado**, **Google ativado**. (Os UIDs listados acima não existem mais.)
- **Dados (raiz do RTDB):** só 3 nós soltos na raiz (chaves = e-mails Google de Amauri, Fia e Vinícius, com `,` no lugar de `.`), de um import feito na raiz; cada um `{ nome }`. Não havia `/perfis` nem `/config`.
- **Regras publicadas:** a 1ª versão (commit `2d7cf92`: `/perfis` com leitura pública, `config`/`estado` com `auth != null`), com o texto literal `UID_ESP32` (nunca trocado).

### Alterações feitas pelo Claude (25/11/2026)
- **Regras do RTDB:** publicada a versão do commit `17ad388` (`/permitidos`, só login Google). `UID_ESP32` continua literal: o usuário do ESP32 ainda não existe. Conferido: sem login, `/perfis`, `/permitidos` e `/config` dão "Permission denied".
- **Google Cloud → cliente OAuth "Web client (auto created by Google Service)":** antes tinha origens `http://localhost`, `http://localhost:5000`, `https://iot-motor-garagem.firebaseapp.com` e redirect só `https://iot-motor-garagem.firebaseapp.com/__/auth/handler`. **Adicionados** a origem `https://iot-motor-garagem.vercel.app` e o redirect `https://iot-motor-garagem.vercel.app/__/auth/handler`.
- **Sem mudança (já estavam certos):** provedor Google ativado; criação de contas ligada; domínio `iot-motor-garagem.vercel.app` autorizado.
- **Feito pelo Vinícius:** importou `banco-inicial.json` (`/permitidos` + `/config`); login com Google testado e funcionando (25/11/2026).
- **Feito pelo Vinícius:** ativou E-mail/senha e criou `esp32@portao.local` (UID `VH2QhwXbGwhya40QFhiY58C6iEV2`).
- **Claude:** republicou as regras trocando só `UID_ESP32` por esse UID (6 lugares). O arquivo do repositório já tem o UID real.
