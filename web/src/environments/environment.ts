// Configuração do Firebase (Console → Configurações do projeto → Seus apps → Web).
// Esses valores NÃO são segredo: ficam públicos no site de qualquer jeito.
// A proteção está nas regras do banco (firebase/database.rules.json) e no
// Auth com a criação de contas desligada.
export const environment = {
  firebase: {
    apiKey: 'AIzaSyBd2HXNvDiICJo9EZk1LNxlwS9aNXeWMn8',
    authDomain: 'iot-motor-garagem.firebaseapp.com',
    projectId: 'iot-motor-garagem',
    storageBucket: 'iot-motor-garagem.firebasestorage.app',
    messagingSenderId: '786078542912',
    appId: '1:786078542912:web:aa2e9dd46296fbaaad43a5',
  },
};
