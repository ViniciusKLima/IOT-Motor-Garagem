import { Component, DestroyRef, computed, inject, output, signal } from '@angular/core';
import { PortaoService } from './portao.service';

/** Tela de Diagnóstico: situação do ESP32, ficha técnica e log técnico (/logs). */
@Component({
  selector: 'app-diagnostico',
  template: `
    <main class="diagnostico">
      <button type="button" class="voltar" (click)="voltar.emit()">
        <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"
             stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M15 18l-6-6 6-6" /></svg>
        Voltar
      </button>
      <h2 class="historico-titulo">Diagnóstico do ESP32</h2>

      <section class="diag-situacao" [style.--cor]="situacao().cor" aria-live="polite">
        <span class="diag-bolinha" aria-hidden="true"></span>
        <div>
          <div class="diag-titulo">{{ situacao().titulo }}</div>
          <div class="diag-sub">{{ situacao().sub }}</div>
        </div>
      </section>
      @if (situacao().dica) {
        <p class="diag-dica">{{ situacao().dica }}</p>
      }

      @if (srv.estado(); as e) {
        <dl class="ficha">
          <div><dt>Sinal Wi-Fi</dt><dd>{{ sinal() }}</dd></div>
          <div><dt>Ligado há</dt><dd>{{ ligadoHa() }}</dd></div>
          <div class="largo"><dt>Último reinício</dt><dd>{{ e.reinicio ?? '—' }}</dd></div>
          <div><dt>Quedas de Wi-Fi</dt><dd>{{ quedas() }}</dd></div>
          <div><dt>Pulsos no relé</dt><dd>{{ pulsos() }}</dd></div>
          <div><dt>Erros do Firebase</dt><dd>{{ e.errosFirebase ?? '—' }}</dd></div>
          <div><dt>Firmware</dt><dd>{{ e.fw ?? 'antigo (sem log)' }}</dd></div>
          <div><dt>Memória livre</dt><dd>{{ e.heap ? kb(e.heap) : '—' }}</dd></div>
          <div><dt>IP na rede</dt><dd>{{ e.ip ?? '—' }}</dd></div>
        </dl>
      }

      <details class="diag-ajuda">
        <summary>O portão não abriu?</summary>
        <p>
          Procure no log o horário do toque. Se aparece <strong>"PULSO dado no relé"</strong>, o ESP32 fez a parte
          dele: confira os fios no BOT e no GND da central, se o relé estala e se o motor está na tomada.
        </p>
        <p>
          Se o comando <strong>nem aparece</strong>, o problema é antes: internet do celular, Wi-Fi da casa ou o
          ESP32 sem energia (veja a situação acima).
        </p>
      </details>

      <div class="log-cabecalho">
        <h3 class="dia-titulo">Log técnico</h3>
        <div class="log-filtro" role="group" aria-label="Filtrar log">
          <button type="button" [class.ativo]="!soProblemas()" (click)="soProblemas.set(false)">Tudo</button>
          <button type="button" [class.ativo]="soProblemas()" (click)="soProblemas.set(true)">Avisos e erros</button>
        </div>
      </div>

      @if (!srv.logsCarregados()) {
        <div class="cartao-vazio">Carregando…</div>
      } @else if (linhas().length === 0) {
        <div class="cartao-vazio">
          @if (soProblemas() && srv.logs().length) {
            Nenhum aviso ou erro registrado.
          } @else {
            O log aparece aqui quando o ESP32 estiver com o firmware 1.1.0 ou mais novo.
          }
        </div>
      } @else {
        <ol class="log-lista">
          @for (l of linhas(); track l.n) {
            <li class="log-item" [class]="'log-item nivel-' + l.nivel">
              <span class="log-hora">{{ l.quando }}</span>
              <span class="log-msg">{{ l.msg }}</span>
            </li>
          }
        </ol>
      }
    </main>
  `,
})
export class Diagnostico {
  protected readonly srv = inject(PortaoService);
  readonly voltar = output<void>();
  protected readonly soProblemas = signal(false);

  private readonly fmtHora = new Intl.DateTimeFormat('pt-BR', { hour: '2-digit', minute: '2-digit' });
  private readonly fmtDiaHora = new Intl.DateTimeFormat('pt-BR', {
    day: '2-digit',
    month: '2-digit',
    hour: '2-digit',
    minute: '2-digit',
  });
  private readonly fmtLog = new Intl.DateTimeFormat('pt-BR', {
    day: '2-digit',
    month: '2-digit',
    hour: '2-digit',
    minute: '2-digit',
    second: '2-digit',
  });

  constructor() {
    this.srv.escutarLogs();
    inject(DestroyRef).onDestroy(() => this.srv.pararEscutaLogs());
  }

  /** Segundos desde o último sinal do ESP32 (relógio do servidor). */
  private readonly semSinalS = computed(() => {
    const visto = this.srv.estado()?.visto;
    return typeof visto === 'number' ? Math.max(0, (this.srv.agoraServidor() - visto) / 1000) : null;
  });

  protected readonly situacao = computed(() => {
    if (!this.srv.conectado()) {
      return {
        cor: '#9AA6B2',
        titulo: 'Este aparelho está sem internet',
        sub: 'Não dá para saber o estado do ESP32 agora.',
        dica: '',
      };
    }
    const s = this.semSinalS();
    if (s === null) {
      return {
        cor: '#9AA6B2',
        titulo: 'O ESP32 nunca deu sinal',
        sub: 'Nenhum registro de estado no banco.',
        dica: 'Ligue o ESP32 e confira pelo cabo USB (Serial Monitor) se ele conecta no Wi-Fi.',
      };
    }
    if (this.srv.espOnline()) {
      return {
        cor: '#5EE0A0',
        titulo: 'Funcionando',
        sub: `Último sinal há ${Math.round(s)} s. Pronto para receber comandos.`,
        dica: '',
      };
    }
    const visto = this.srv.estado()!.visto!;
    return {
      cor: '#FF8A7A',
      titulo: `Sem sinal há ${this.duracao(s)}`,
      sub: `Último sinal: ${this.diaHora(visto)}.`,
      dica:
        'O ESP32 está desligado ou sem internet. Confira se a fonte está na tomada perto do motor e se o Wi-Fi da ' +
        'casa está funcionando. Quando ele voltar, o log mostra quanto tempo ficou fora e o motivo do reinício.',
    };
  });

  protected readonly sinal = computed(() => {
    const r = this.srv.estado()?.rssi;
    if (typeof r !== 'number') return '—';
    const q = r > -70 ? 'bom' : r >= -80 ? 'no limite' : 'fraco';
    return `${r} dBm (${q})`;
  });

  protected readonly ligadoHa = computed(() => {
    const e = this.srv.estado();
    const s = this.semSinalS();
    if (!e?.uptimeS || s === null || !this.srv.espOnline()) return '—';
    return this.duracao(e.uptimeS + s);
  });

  protected readonly quedas = computed(() => {
    const e = this.srv.estado();
    if (e?.quedasWifi === undefined) return '—';
    if (e.quedasWifi === 0) return 'nenhuma';
    return `${e.quedasWifi} (${this.duracao(e.foraWifiS ?? 0)} fora)`;
  });

  protected readonly pulsos = computed(() => {
    const e = this.srv.estado();
    if (e?.pulsos === undefined) return '—';
    if (!e.ultimoPulsoTs) return String(e.pulsos);
    return `${e.pulsos} (último ${this.diaHora(e.ultimoPulsoTs)})`;
  });

  protected readonly linhas = computed(() =>
    this.srv
      .logs()
      .filter((l) => !this.soProblemas() || l.nivel !== 'info')
      .map((l) => ({ ...l, quando: l.ts ? this.fmtLog.format(l.ts) : '—' })),
  );

  private diaHora(ts: number): string {
    const d = new Date(ts);
    const hoje = new Date();
    return d.toDateString() === hoje.toDateString()
      ? `hoje às ${this.fmtHora.format(ts)}`
      : this.fmtDiaHora.format(ts).replace(', ', ' às ');
  }

  protected kb(bytes: number): string {
    return `${Math.round(bytes / 1024)} KB`;
  }

  private duracao(s: number): string {
    s = Math.round(s);
    if (s < 60) return `${s} s`;
    if (s < 3600) return `${Math.floor(s / 60)} min`;
    if (s < 86400) return `${Math.floor(s / 3600)} h ${Math.floor((s % 3600) / 60)} min`;
    return `${Math.floor(s / 86400)} d ${Math.floor((s % 86400) / 3600)} h`;
  }
}
