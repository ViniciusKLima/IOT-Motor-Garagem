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
