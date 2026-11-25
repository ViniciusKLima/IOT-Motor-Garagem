import { Injectable, computed, signal } from '@angular/core';
import { FirebaseApp, initializeApp } from 'firebase/app';
import {
  Auth,
  GoogleAuthProvider,
  User,
  getAuth,
  getRedirectResult,
  onAuthStateChanged,
  signInWithPopup,
  signInWithRedirect,
  signOut,
} from 'firebase/auth';
import {
  Database,
  Unsubscribe,
  getDatabase,
  limitToLast,
  onValue,
  orderByChild,
  push,
  query,
  ref,
  set,
} from 'firebase/database';
import { environment } from '../environments/environment';
import { AcaoPulso, Fase, aberturaEm, faseEm, reproduzir } from './estado-portao';

// ============================================================
// Estrutura do banco (ver .claude/docs/app-e-firebase.md):
//   /permitidos/{email} { nome }   e-mail com ',' no lugar de '.'
//                       (lista de quem pode abrir; editada no console)
//   /config             { sensorInstalado, tempoCursoMs }
//   /comandos/{id}      { uid, nome, acao: 'acionar', ts }   <- este app
//   /estado             { visto, rssi, sensorFechado? }      <- ESP32
//   /historico/{id}     { nome, acao, ts, resultado, comandoId? } <- ESP32
// ============================================================

export interface Acesso {
  nome: string;
}

/** Chave de /permitidos: e-mail em minúsculas com ',' no lugar de '.' (o RTDB não aceita '.'). */
export function chaveEmail(email: string): string {
  return email.trim().toLowerCase().replaceAll('.', ',');
}

export interface EstadoEsp {
  /** Timestamp do servidor gravado pelo ESP32 a cada ~5 s. */
  visto?: number;
  rssi?: number;
  sensorFechado?: boolean;
}

export interface ConfigApp {
  sensorInstalado: boolean;
  tempoCursoMs: number;
}

export interface ItemHistorico {
  id: string;
  nome: string;
  acao: string;
  ts: number;
  /** 'ok' = o pulso foi dado. Qualquer outro texto = recusado (motivo). */
  resultado?: string;
  comandoId?: string;
}

/** ESP32 sem dar sinal por mais que isso = offline. */
const OFFLINE_APOS_MS = 15000;
/** Tempo máximo esperando o ESP32 confirmar um comando no /historico. */
const ESPERA_CONFIRMACAO_MS = 12000;
const CONFIG_PADRAO: ConfigApp = { sensorInstalado: false, tempoCursoMs: 15000 };

@Injectable({ providedIn: 'root' })
export class PortaoService {
  /** false enquanto o environment.ts ainda tem os valores de exemplo. */
  readonly configurado = !environment.firebase.apiKey.startsWith('COLE');

  private auth?: Auth;
  private db?: Database;
  private escutasLogado: Unsubscribe[] = [];
  private pararAcesso?: Unsubscribe;

  // ---------- estado bruto ----------
  /** undefined = ainda verificando se já havia alguém logado. */
  readonly usuario = signal<User | null | undefined>(undefined);
  /** undefined = conferindo a lista /permitidos; null = sem acesso. */
  readonly acesso = signal<Acesso | null | undefined>(undefined);
  readonly entrando = signal(false);
  /** Navegador conectado ao Firebase (.info/connected). */
  readonly conectado = signal(false);
  private readonly offset = signal(0);
  private readonly relogio = signal(Date.now());
  readonly estado = signal<EstadoEsp | null>(null);
  readonly config = signal<ConfigApp>(CONFIG_PADRAO);
  readonly historico = signal<ItemHistorico[]>([]);
  readonly historicoCarregado = signal(false);
  readonly pendente = signal<{ id: string; desde: number } | null>(null);
  readonly erroComando = signal<string | null>(null);
  readonly erroLogin = signal<string | null>(null);

  // ---------- derivados ----------
  /** "Agora" no relógio do servidor (não depende da hora do celular). */
  readonly agoraServidor = computed(() => this.relogio() + this.offset());

  /** Logado E na lista de permitidos. */
  readonly liberado = computed(() => !!this.usuario() && !!this.acesso());

  readonly espOnline = computed(() => {
    const visto = this.estado()?.visto;
    return typeof visto === 'number' && this.agoraServidor() - visto < OFFLINE_APOS_MS;
  });

  private readonly cursoMs = computed(() => Math.max(1000, this.config().tempoCursoMs || CONFIG_PADRAO.tempoCursoMs));

  /** Só confia no sensor se ele estiver instalado E o ESP32 estiver online (leitura recente). */
  readonly sensorConfiavel = computed(
    () => this.config().sensorInstalado && this.espOnline() && typeof this.estado()?.sensorFechado === 'boolean',
  );

  /** Reproduz os pulsos executados (do mais antigo ao mais novo). */
  private readonly reproducao = computed(() => {
    const pulsos = this.historico()
      .filter((h) => h.resultado === 'ok')
      .slice()
      .sort((a, b) => a.ts - b.ts);
    return reproduzir(pulsos, this.cursoMs());
  });

  /** O que cada item do histórico fez (abrir / fechar / parar). */
  readonly acoes = computed<Map<string, AcaoPulso>>(() => this.reproducao().acoes);

  readonly fase = computed<Fase>(() => {
    const t = this.agoraServidor();
    let f = faseEm(this.reproducao().m, t, this.cursoMs());
    // Com o sensor reed instalado, ele manda quando o portão está parado.
    const sensor = this.estado()?.sensorFechado;
    if (this.sensorConfiavel() && typeof sensor === 'boolean' && f !== 'abrindo' && f !== 'fechando') {
      if (sensor) f = 'fechado';
      else if (f === 'fechado') f = 'aberto';
    }
    return f;
  });

  /** 0 = fechado, 1 = aberto (para a ilustração). */
  readonly abertura = computed(() => {
    const f = this.fase();
    if (f === 'fechado') return 0;
    if (f === 'aberto') return 1;
    return aberturaEm(this.reproducao().m, this.agoraServidor(), this.cursoMs());
  });

  /** Destino do último movimento (para o rótulo de "Parado no meio"). */
  readonly ultimoDestino = computed(() => this.reproducao().m.destino);

  constructor() {
    if (!this.configurado) return;
    const app: FirebaseApp = initializeApp(environment.firebase);
    this.auth = getAuth(app);
    this.db = getDatabase(app);
    const db = this.db;

    onValue(ref(db, '.info/connected'), (s) => this.conectado.set(s.val() === true));
    onValue(ref(db, '.info/serverTimeOffset'), (s) => this.offset.set(Number(s.val()) || 0));

    // Volta do login por redirecionamento (celular / app instalado)
    getRedirectResult(this.auth).catch((e) => this.erroLogin.set(this.mensagemErro(e)));

    onAuthStateChanged(this.auth, (u) => {
      this.usuario.set(u);
      this.pararEscutas();
      this.pararAcesso?.();
      this.pararAcesso = undefined;
      this.acesso.set(u ? undefined : null);
      if (!u) return;

      const email = u.email ?? '';
      this.pararAcesso = onValue(
        ref(db, `permitidos/${chaveEmail(email)}`),
        (s) => {
          const nome = s.val()?.nome;
          if (typeof nome === 'string' && nome) {
            const novo = !this.acesso();
            this.acesso.set({ nome });
            this.erroLogin.set(null);
            if (novo) this.escutarLogado(db);
          } else {
            this.semAcesso(email);
          }
        },
        () => this.semAcesso(email),
      );
    });

    // Relógio da tela (anima o portão e o chip online/offline).
    setInterval(() => {
      this.relogio.set(Date.now());
      const p = this.pendente();
      if (p && Date.now() - p.desde > ESPERA_CONFIRMACAO_MS) {
        this.pendente.set(null);
        this.erroComando.set('O ESP32 não confirmou o comando. Confira se o portão mexeu.');
      }
    }, 250);
  }

  private escutarLogado(db: Database): void {
    this.escutasLogado.push(
      onValue(ref(db, 'estado'), (s) => this.estado.set(s.val())),
      onValue(ref(db, 'config'), (s) => this.config.set({ ...CONFIG_PADRAO, ...(s.val() ?? {}) })),
      onValue(query(ref(db, 'historico'), orderByChild('ts'), limitToLast(50)), (s) => {
        const itens: ItemHistorico[] = [];
        s.forEach((filho) => {
          const v = filho.val() ?? {};
          itens.push({
            id: filho.key!,
            nome: String(v.nome ?? '?'),
            acao: String(v.acao ?? ''),
            ts: Number(v.ts) || 0,
            resultado: v.resultado,
            comandoId: v.comandoId,
          });
        });
        this.historico.set(itens);
        this.historicoCarregado.set(true);
        const p = this.pendente();
        if (p && itens.some((i) => i.comandoId === p.id)) this.pendente.set(null);
      }),
    );
  }

  private pararEscutas(): void {
    this.escutasLogado.forEach((parar) => parar());
    this.escutasLogado = [];
    this.estado.set(null);
    this.historico.set([]);
    this.historicoCarregado.set(false);
    this.pendente.set(null);
    this.erroComando.set(null);
  }

  private semAcesso(email: string): void {
    this.acesso.set(null);
    this.erroLogin.set(`O e-mail ${email} ainda não tem acesso ao portão. Peça para liberarem.`);
    void this.sair(false);
  }

  private mensagemErro(e: unknown): string {
    const codigo = (e as { code?: string })?.code ?? '';
    if (codigo === 'auth/network-request-failed') return 'Sem internet. Tente de novo.';
    if (codigo === 'auth/user-disabled') return 'Esta conta está desativada.';
    if (codigo === 'auth/unauthorized-domain') return 'Este endereço não está autorizado no Firebase.';
    return 'Não foi possível entrar com o Google. Tente de novo.';
  }

  /** App instalado na tela inicial (PWA): popup costuma falhar, usa redirecionamento. */
  private emModoApp(): boolean {
    return (
      window.matchMedia?.('(display-mode: standalone)').matches ||
      (navigator as Navigator & { standalone?: boolean }).standalone === true
    );
  }

  /** Entra com a conta Google. */
  async entrar(): Promise<void> {
    if (!this.auth || this.entrando()) return;
    this.erroLogin.set(null);
    this.entrando.set(true);
    const provedor = new GoogleAuthProvider();
    provedor.setCustomParameters({ prompt: 'select_account' });
    try {
      if (this.emModoApp()) {
        await signInWithRedirect(this.auth, provedor);
        return; // a página sai e volta logada
      }
      await signInWithPopup(this.auth, provedor);
    } catch (e: unknown) {
      const codigo = (e as { code?: string })?.code ?? '';
      if (codigo === 'auth/popup-blocked' || codigo === 'auth/operation-not-supported-in-this-environment') {
        await signInWithRedirect(this.auth, provedor);
        return;
      }
      if (codigo !== 'auth/popup-closed-by-user' && codigo !== 'auth/cancelled-popup-request') {
        this.erroLogin.set(this.mensagemErro(e));
      }
    } finally {
      this.entrando.set(false);
    }
  }

  /** limparErro = false mantém a mensagem de "sem acesso" na tela. */
  async sair(limparErro = true): Promise<void> {
    if (limparErro) this.erroLogin.set(null);
    if (this.auth) await signOut(this.auth);
  }

  /** Grava um comando "acionar" (um pulso no BOT). */
  async acionar(): Promise<void> {
    const u = this.usuario();
    const a = this.acesso();
    if (!this.db || !u || !a || this.pendente()) return;
    const novo = push(ref(this.db, 'comandos'));
    this.erroComando.set(null);
    this.pendente.set({ id: novo.key!, desde: Date.now() });
    try {
      await set(novo, { uid: u.uid, nome: a.nome, acao: 'acionar', ts: Date.now() + this.offset() });
    } catch {
      this.pendente.set(null);
      this.erroComando.set('O servidor recusou o comando. Confira a internet e tente de novo.');
    }
  }
}
