// ============================================================
// ESTIMATIVA DO ESTADO DO PORTÃO (sem sensor)
//
// A central Rossi alterna a cada pulso no BOT:
//   fechado --pulso--> abrindo --(tempo de curso)--> aberto
//   aberto  --pulso--> fechando --(tempo de curso)--> fechado
//   em movimento --pulso--> parado no meio
//   parado no meio --pulso--> vai para o lado OPOSTO ao último destino
//
// O app reproduz essa máquina a partir do /historico (pulsos que o ESP32
// realmente executou, de qualquer pessoa). Começa supondo "fechado".
// Controles remotos não aparecem no histórico, por isso é só uma ESTIMATIVA
// até o sensor reed ser instalado.
// ============================================================

export type Destino = 'aberto' | 'fechado';
export type AcaoPulso = 'abrir' | 'fechar' | 'parar';
export type Fase = 'fechado' | 'abrindo' | 'fechando' | 'aberto' | 'parado';

export interface Movimento {
  /** 0 = fechado, 1 = aberto, medido no instante `desde`. */
  abertura: number;
  /** Para onde está indo (ou para onde ia quando parou). */
  destino: Destino;
  movendo: boolean;
  /** Timestamp (ms, relógio do servidor) em que este trecho começou. */
  desde: number;
}

export const INICIAL: Movimento = { abertura: 0, destino: 'fechado', movendo: false, desde: 0 };

function limitar(v: number): number {
  return Math.min(1, Math.max(0, v));
}

/** Abertura (0..1) no instante t. */
export function aberturaEm(m: Movimento, t: number, cursoMs: number): number {
  if (!m.movendo) return m.abertura;
  const sentido = m.destino === 'aberto' ? 1 : -1;
  return limitar(m.abertura + (sentido * Math.max(0, t - m.desde)) / cursoMs);
}

/** Se o movimento já terminou no instante t, devolve o portão parado no fim. */
export function assentar(m: Movimento, t: number, cursoMs: number): Movimento {
  if (!m.movendo) return m;
  const alvo = m.destino === 'aberto' ? 1 : 0;
  const restante = Math.abs(alvo - m.abertura) * cursoMs;
  if (t - m.desde >= restante) {
    return { abertura: alvo, destino: m.destino, movendo: false, desde: m.desde + restante };
  }
  return m;
}

/** Aplica um pulso no BOT no instante t. */
export function pulso(m: Movimento, t: number, cursoMs: number): { m: Movimento; acao: AcaoPulso } {
  const s = assentar(m, t, cursoMs);
  if (s.movendo) {
    return {
      m: { abertura: aberturaEm(s, t, cursoMs), destino: s.destino, movendo: false, desde: t },
      acao: 'parar',
    };
  }
  let destino: Destino;
  if (s.abertura <= 0) destino = 'aberto';
  else if (s.abertura >= 1) destino = 'fechado';
  else destino = s.destino === 'aberto' ? 'fechado' : 'aberto';
  return {
    m: { abertura: s.abertura, destino, movendo: true, desde: t },
    acao: destino === 'aberto' ? 'abrir' : 'fechar',
  };
}

export function faseEm(m: Movimento, t: number, cursoMs: number): Fase {
  const s = assentar(m, t, cursoMs);
  if (s.movendo) return s.destino === 'aberto' ? 'abrindo' : 'fechando';
  if (s.abertura <= 0) return 'fechado';
  if (s.abertura >= 1) return 'aberto';
  return 'parado';
}

/** Reproduz uma lista de pulsos (em ordem crescente de tempo). */
export function reproduzir(
  pulsos: { id: string; ts: number }[],
  cursoMs: number,
): { m: Movimento; acoes: Map<string, AcaoPulso> } {
  let m = INICIAL;
  const acoes = new Map<string, AcaoPulso>();
  for (const p of pulsos) {
    const r = pulso(m, p.ts, cursoMs);
    m = r.m;
    acoes.set(p.id, r.acao);
  }
  return { m, acoes };
}
