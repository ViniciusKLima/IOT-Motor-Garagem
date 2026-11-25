# Site do portão (Angular PWA)

## Rodar no PC
```
npm install
npm start          # http://localhost:4200
```

## Antes de publicar
Colar o `firebaseConfig` do projeto em `src/environments/environment.ts`.

## Vercel
1. Subir o projeto para o GitHub e importar na Vercel.
2. **Root Directory = `web`** (o resto vem do `vercel.json`).

Detalhes do banco e do contrato com o ESP32: `../.claude/docs/app-e-firebase.md`.
