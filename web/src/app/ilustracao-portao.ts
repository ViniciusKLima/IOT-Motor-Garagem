import { Component, computed, input } from '@angular/core';

/**
 * Desenho do portão de correr.
 * abertura: 0 = fechado (folha encostada na coluna da direita), 1 = aberto.
 * A folha ocupa toda a largura e, aberta, desliza para a esquerda até sobrar
 * só uma ponta depois do motor, em qualquer largura de tela.
 */
@Component({
  selector: 'app-ilustracao-portao',
  template: `
    <div class="ilustracao" [class.grande]="grande()" aria-hidden="true">
      <div class="trilho"></div>
      <div class="folha" [style.transform]="transformacao()" [style.transition]="transicao()"></div>
      <div class="coluna"></div>
      <div class="motor"><span>MOTOR</span></div>
    </div>
  `,
})
export class IlustracaoPortao {
  readonly abertura = input(0);
  readonly movendo = input(false);
  /** Versão maior (tela de login), com a "espiada" ao carregar. */
  readonly grande = input(false);

  protected readonly transformacao = computed(() => {
    const a = Math.min(1, Math.max(0, this.abertura()));
    // aberto: a ponta direita da folha para 56 px depois da borda esquerda
    return `translateX(calc(${a.toFixed(4)} * (-100% + 56px)))`;
  });

  protected readonly transicao = computed(() =>
    this.movendo() ? 'transform 250ms linear' : 'transform 400ms ease-out',
  );
}
