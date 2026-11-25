import { Injectable, computed, signal } from '@angular/core';
import { FirebaseApp, initializeApp } from 'firebase/app';
import { Auth, User, getAuth, onAuthStateChanged, signInWithEmailAndPassword, signOut } from 'firebase/auth';
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
//   /perfis/{uid}       { nome, email }             leitura pública
//   /config             { sensorInstalado, tempoCursoMs }
//   /comandos/{id}      { uid, nome, acao: 'acionar', ts }   <- este app
//   /estado             { visto, rssi, sensorFechado? }      <- ESP32
//   /historico/{id}     { nome, acao, ts, resultado, comandoId? } <- ESP32
// ============================================================

export interface Perfil {
  uid: string;
  nome: string;
  email: string;
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

  // ---------- estado bruto ----------
  readonly perfis = signal<Perfil[]>([]);
  readonly perfisCarregados = signal(false);
  /** undefined = ainda verificando se já havia alguém logado. */
  readonly usuario = signal<User | null | undefined>(undefined);
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

  readonly perfilAtual = computed(() => {
    const u = this.usuario();
    return u ? (this.perfis().find((p) => p.uid === u.uid) ?? null) : null;
  });

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

    onValue(
      ref(db, 'perfis'),
      (s) => {
        const lista: Perfil[] = [];
        s.forEach((filho) => {
          const v = filho.val() ?? {};
          if (typeof v.nome === 'string' && typeof v.email === 'string') {
            lista.push({ uid: filho.key!, nome: v.nome, email: v.email });
          }
        });
        lista.sort((a, b) => a.nome.localeCompare(b.nome, 'pt-BR'));
        this.perfis.set(lista);
        this.perfisCarregados.set(true);
        this.validarPerfil();
      },
      () => this.perfisCarregados.set(true),
    );

    onAuthStateChanged(this.auth, (u) => {
      this.usuario.set(u);
      this.pararEscutas();
      if (u) this.escutarLogado(db);
      this.validarPerfil();
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

  /** Conta logada que não está em /perfis (ex.: a do ESP32) não usa o app. */
  private validarPerfil(): void {
    const u = this.usuario();
    if (u && this.perfisCarregados() && !this.perfilAtual()) {
      this.erroLogin.set('Este usuário não tem perfil no app.');
      void this.sair();
    }
  }

  /** Devolve null se entrou, ou a mensagem de erro. */
  async entrar(perfil: Perfil, pin: string): Promise<string | null> {
    if (!this.auth) return 'Firebase não configurado.';
    this.erroLogin.set(null);
    try {
      await signInWithEmailAndPassword(this.auth, perfil.email, pin);
      return null;
    } catch (e: unknown) {
      const codigo = (e as { code?: string })?.code ?? '';
      if (codigo === 'auth/too-many-requests') return 'Muitas tentativas. Aguarde alguns minutos.';
      if (codigo === 'auth/network-request-failed') return 'Sem internet. Tente de novo.';
      if (codigo === 'auth/user-disabled') return 'Este perfil está desativado.';
      return 'PIN incorreto';
    }
  }

  async sair(): Promise<void> {
    if (this.auth) await signOut(this.auth);
  }

  /** Grava um comando "acionar" (um pulso no BOT). */
  async acionar(): Promise<void> {
    const u = this.usuario();
    const p = this.perfilAtual();
    if (!this.db || !u || !p || this.pendente()) return;
    const novo = push(ref(this.db, 'comandos'));
    this.erroComando.set(null);
    this.pendente.set({ id: novo.key!, desde: Date.now() });
    try {
      await set(novo, { uid: u.uid, nome: p.nome, acao: 'acionar', ts: Date.now() + this.offset() });
    } catch {
      this.pendente.set(null);
      this.erroComando.set('O servidor recusou o comando. Confira a internet e tente de novo.');
    }
  }
}
