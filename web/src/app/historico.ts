import { Component, computed, inject } from '@angular/core';
import { ItemHistorico, PortaoService } from './portao.service';

/** Tela 3: Histórico (quem acionou o portão). */
@Component({
  selector: 'app-historico',
  template: `
    <main class="historico">
      <h2 class="rotulo">QUEM ACIONOU O PORTÃO</h2>
      @if (!srv.historicoCarregado()) {
        <div class="cartao-vazio">Carregando…</div>
      } @else if (itens().length === 0) {
        <div class="cartao-vazio">Nenhum comando ainda. Volte à aba Portão e toque no botão.</div>
      } @else {
        <ul class="lista">
          @for (item of itens(); track item.id) {
            <li class="item">
              <div class="item-textos">
                <span class="item-acao">{{ item.texto }}</span>
                <span class="item-quem">por {{ item.nome }}</span>
              </div>
              <span class="item-hora">{{ item.hora }}</span>
            </li>
          }
        </ul>
      }
    </main>
  `,
})
export class Historico {
  protected readonly srv = inject(PortaoService);

  private readonly hora = new Intl.DateTimeFormat('pt-BR', { hour: '2-digit', minute: '2-digit', second: '2-digit' });
  private readonly dataHora = new Intl.DateTimeFormat('pt-BR', {
    day: '2-digit',
    month: '2-digit',
    hour: '2-digit',
    minute: '2-digit',
  });

  protected readonly itens = computed(() => {
    const acoes = this.srv.acoes();
    const hoje = new Date().toDateString();
    return this.srv
      .historico()
      .slice()
      .sort((a, b) => b.ts - a.ts)
      .map((h) => ({
        id: h.id,
        nome: h.nome,
        texto: this.texto(h, acoes.get(h.id)),
        hora: new Date(h.ts).toDateString() === hoje ? this.hora.format(h.ts) : this.dataHora.format(h.ts),
      }));
  });

  private texto(h: ItemHistorico, acao?: string): string {
    if (h.resultado !== 'ok') return `Não executado${h.resultado ? ` (${h.resultado})` : ''}`;
    if (acao === 'abrir') return 'Comando: abrir';
    if (acao === 'fechar') return 'Comando: fechar';
    if (acao === 'parar') return 'Comando: parar';
    return 'Comando: acionar';
  }
}
