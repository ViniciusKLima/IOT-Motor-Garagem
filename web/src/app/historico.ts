import { Component, DestroyRef, computed, effect, inject, signal } from '@angular/core';
import { ItemHistorico, PortaoService } from './portao.service';

interface Linha {
  id: string;
  nome: string;
  texto: string;
  hora: string;
  falhou: boolean;
}

/** Tela 3: Histórico (quem acionou e quando), com filtro por pessoa e por dia. */
@Component({
  selector: 'app-historico',
  template: `
    <main class="historico">
      <h2 class="historico-titulo">Quem acionou o portão</h2>

      <div class="filtros">
        <label class="filtro">
          <span class="filtro-rotulo">Pessoa</span>
          <select class="campo-filtro" [value]="pessoa()" (change)="pessoa.set($any($event.target).value)">
            <option value="">Todas</option>
            @for (n of nomes(); track n) {
              <option [value]="n">{{ n }}</option>
            }
          </select>
        </label>
        <label class="filtro">
          <span class="filtro-rotulo">Dia</span>
          <input type="date" class="campo-filtro" [value]="dia()" [max]="hoje"
                 (change)="dia.set($any($event.target).value)" />
        </label>
      </div>

      <div class="resumo">
        <span>{{ resumo() }}</span>
        @if (pessoa() || dia()) {
          <button type="button" class="link" (click)="limpar()">Limpar filtros</button>
        }
      </div>

      @if (!srv.historicoTelaCarregado()) {
        <div class="cartao-vazio">Carregando…</div>
      } @else if (grupos().length === 0) {
        <div class="cartao-vazio">
          @if (pessoa() || dia()) {
            Nenhum acionamento com esses filtros. Troque a pessoa ou o dia.
          } @else {
            Nenhum comando ainda. Volte ao controle e toque no botão.
          }
        </div>
      } @else {
        @for (g of grupos(); track g.chave) {
          <section class="grupo-dia">
            <h3 class="dia-titulo">{{ g.titulo }}</h3>
            <ul class="lista">
              @for (l of g.linhas; track l.id) {
                <li class="item" [class.falhou]="l.falhou">
                  <div class="item-textos">
                    <span class="item-nome">{{ l.nome }}</span>
                    <span class="item-acao">{{ l.texto }}</span>
                  </div>
                  <span class="item-hora">{{ l.hora }}</span>
                </li>
              }
            </ul>
          </section>
        }
      }
    </main>
  `,
})
export class Historico {
  protected readonly srv = inject(PortaoService);

  protected readonly pessoa = signal('');
  protected readonly dia = signal('');
  protected readonly hoje = this.chaveDia(Date.now());

  private readonly fmtHora = new Intl.DateTimeFormat('pt-BR', { hour: '2-digit', minute: '2-digit' });
  private readonly fmtDia = new Intl.DateTimeFormat('pt-BR', { weekday: 'long', day: 'numeric', month: 'long' });
  private readonly fmtDiaAno = new Intl.DateTimeFormat('pt-BR', {
    weekday: 'long',
    day: 'numeric',
    month: 'long',
    year: 'numeric',
  });

  constructor() {
    // Troca de dia = nova consulta no banco (o filtro de pessoa é feito aqui mesmo).
    effect(() => this.srv.filtrarHistorico(this.dia() || null));
    inject(DestroyRef).onDestroy(() => this.srv.pararFiltroHistorico());
  }

  /** Nomes que aparecem no histórico carregado (para o filtro). */
  protected readonly nomes = computed(() =>
    [...new Set(this.srv.historicoTela().map((h) => h.nome))].sort((a, b) => a.localeCompare(b, 'pt-BR')),
  );

  private readonly filtrados = computed(() => {
    const p = this.pessoa();
    return this.srv
      .historicoTela()
      .filter((h) => !p || h.nome === p)
      .sort((a, b) => b.ts - a.ts);
  });

  protected readonly resumo = computed(() => {
    const n = this.filtrados().length;
    if (n === 0) return '';
    const base = n === 1 ? '1 acionamento' : `${n} acionamentos`;
    return this.dia() ? base : `${base} recentes`;
  });

  protected readonly grupos = computed(() => {
    const acoes = this.srv.acoes();
    const grupos: { chave: string; titulo: string; linhas: Linha[] }[] = [];
    for (const h of this.filtrados()) {
      const chave = this.chaveDia(h.ts);
      let g = grupos[grupos.length - 1];
      if (!g || g.chave !== chave) {
        g = { chave, titulo: this.tituloDia(h.ts, chave), linhas: [] };
        grupos.push(g);
      }
      g.linhas.push({
        id: h.id,
        nome: h.nome,
        texto: this.texto(h, acoes.get(h.id)),
        hora: this.fmtHora.format(h.ts),
        falhou: h.resultado !== 'ok',
      });
    }
    return grupos;
  });

  protected limpar(): void {
    this.pessoa.set('');
    this.dia.set('');
  }

  private chaveDia(ts: number): string {
    const d = new Date(ts);
    const m = String(d.getMonth() + 1).padStart(2, '0');
    const dd = String(d.getDate()).padStart(2, '0');
    return `${d.getFullYear()}-${m}-${dd}`;
  }

  private tituloDia(ts: number, chave: string): string {
    if (chave === this.hoje) return 'Hoje';
    if (chave === this.chaveDia(Date.now() - 86400000)) return 'Ontem';
    const mesmoAno = new Date(ts).getFullYear() === new Date().getFullYear();
    const t = (mesmoAno ? this.fmtDia : this.fmtDiaAno).format(ts);
    return t.charAt(0).toUpperCase() + t.slice(1);
  }

  private texto(h: ItemHistorico, acao?: string): string {
    if (h.resultado !== 'ok') return `Não executado${h.resultado ? `: ${h.resultado}` : ''}`;
    if (acao === 'abrir') return 'Abriu o portão';
    if (acao === 'fechar') return 'Fechou o portão';
    if (acao === 'parar') return 'Parou o portão';
    return 'Acionou o portão';
  }
}
