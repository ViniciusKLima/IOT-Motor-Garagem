import { Component, computed, inject } from '@angular/core';
import { PortaoService } from './portao.service';

const COR = {
  verde: '#5EE0A0',
  azul: '#7AB8FF',
  ambar: '#FFC24D',
  coral: '#FF8A7A',
};

/** Tela 2: Portão (estado, ilustração e botão principal). */
@Component({
  selector: 'app-portao',
  template: `
    <main class="portao">
      <section class="cartao-estado" aria-live="polite">
        <div class="estado-textos">
          <div class="rotulo">ESTADO</div>
          <div class="estado-grande" [style.color]="vista().cor">{{ vista().texto }}</div>
          <div class="apoio">{{ vista().apoio }}</div>
        </div>
        <div class="ilustracao" aria-hidden="true">
          <div class="trilho"></div>
          <div class="folha" [style.transform]="transformFolha()" [style.transition]="transicaoFolha()"></div>
          <div class="motor"><span>MOTOR</span></div>
        </div>
      </section>

      <div class="area-botao">
        <button
          type="button"
          class="botao-principal"
          [style.background]="vista().corBotao"
          [disabled]="bloqueio() !== null"
          (click)="srv.acionar()"
        >
          {{ vista().botao }}
        </button>
        <div class="dica" [class.erro]="!!srv.erroComando()" aria-live="polite">{{ dica() }}</div>
      </div>

      @if (!srv.config().sensorInstalado) {
        <div class="aviso">
          <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="#FFC24D" stroke-width="2"
               stroke-linecap="round" stroke-linejoin="round" aria-hidden="true">
            <circle cx="12" cy="12" r="9" /><path d="M12 8v5M12 16.5v.01" />
          </svg>
          <div>Sensor de fechamento ainda não instalado. O estado mostrado é estimado pelo último comando enviado.</div>
        </div>
      }
    </main>
  `,
})
export class Portao {
  protected readonly srv = inject(PortaoService);

  protected readonly vista = computed(() => {
    const apoioEstimado = this.srv.config().sensorInstalado ? 'Lido pelo sensor' : 'Estimado pelo último comando';
    switch (this.srv.fase()) {
      case 'abrindo':
        return { texto: 'Abrindo…', cor: COR.ambar, apoio: 'Toque para parar', botao: 'Parar', corBotao: COR.coral };
      case 'fechando':
        return { texto: 'Fechando…', cor: COR.ambar, apoio: 'Toque para parar', botao: 'Parar', corBotao: COR.coral };
      case 'aberto':
        return { texto: 'Aberto', cor: COR.azul, apoio: apoioEstimado, botao: 'Fechar portão', corBotao: COR.azul };
      case 'parado':
        return {
          texto: 'Parado no meio',
          cor: COR.coral,
          apoio: 'O portão não chegou ao fim',
          botao: this.srv.ultimoDestino() === 'aberto' ? 'Fechar portão' : 'Abrir portão',
          corBotao: COR.azul,
        };
      default:
        return { texto: 'Fechado', cor: COR.verde, apoio: apoioEstimado, botao: 'Abrir portão', corBotao: COR.azul };
    }
  });

  /** Motivo para o botão estar desabilitado (null = pode tocar). */
  protected readonly bloqueio = computed(() => {
    if (!this.srv.conectado()) return 'Sem conexão com a internet.';
    if (!this.srv.espOnline()) return 'ESP32 offline. Use o controle remoto.';
    if (this.srv.pendente()) return 'Enviando comando…';
    return null;
  });

  protected readonly dica = computed(() => {
    const erro = this.srv.erroComando();
    if (erro) return erro;
    const b = this.bloqueio();
    if (b) return b;
    if (this.vista().botao === 'Fechar portão') return 'Sem fotocélula: confira se o caminho está livre.';
    return '';
  });

  protected readonly transformFolha = computed(() => `translateX(${(-250 * this.srv.abertura()).toFixed(1)}px)`);

  protected readonly transicaoFolha = computed(() => {
    const f = this.srv.fase();
    return f === 'abrindo' || f === 'fechando' ? 'transform 250ms linear' : 'transform 400ms ease-out';
  });
}
