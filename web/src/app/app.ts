import { Component, computed, inject, signal } from '@angular/core';
import { Entrar } from './entrar';
import { Historico } from './historico';
import { Portao } from './portao';
import { PortaoService } from './portao.service';

@Component({
  selector: 'app-root',
  imports: [Entrar, Portao, Historico],
  template: `
    <div class="tela">
      <header class="cabecalho">
        <div>
          <div class="rotulo">MINHA CASA</div>
          <h1 class="titulo">Portão da garagem</h1>
        </div>
        @if (srv.liberado()) {
          <div class="pilula" role="status">
            <span class="bolinha" [style.background]="chip().cor"></span>
            <span>{{ chip().texto }}</span>
          </div>
        }
      </header>

      @if (!srv.configurado) {
        <main class="aviso-config">
          <h2 class="titulo-tela">Falta configurar</h2>
          <p>Cole o <code>firebaseConfig</code> do seu projeto em
            <code>web/src/environments/environment.ts</code> e publique de novo.</p>
        </main>
      } @else if (srv.usuario() === undefined || (srv.usuario() && srv.acesso() === undefined)) {
        <main class="carregando">Carregando…</main>
      } @else if (!srv.liberado()) {
        <app-entrar />
      } @else {
        <div class="entrou-como">
          <span>Entrou como <strong>{{ srv.acesso()?.nome }}</strong></span>
          <span aria-hidden="true">·</span>
          <button type="button" class="link" (click)="srv.sair()">Sair</button>
        </div>

        @if (aba() === 'portao') {
          <app-portao />
        } @else {
          <app-historico />
        }

        <nav class="barra" aria-label="Navegação">
          <button type="button" class="aba" [class.ativa]="aba() === 'portao'" (click)="aba.set('portao')"
                  [attr.aria-current]="aba() === 'portao' ? 'page' : null">
            <svg width="22" height="22" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"
                 stroke-linecap="round" stroke-linejoin="round" aria-hidden="true">
              <path d="M3 20V8l9-5 9 5v12M7 20v-7h10v7M7 16.5h10" />
            </svg>
            Portão
          </button>
          <button type="button" class="aba" [class.ativa]="aba() === 'historico'" (click)="aba.set('historico')"
                  [attr.aria-current]="aba() === 'historico' ? 'page' : null">
            <svg width="22" height="22" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"
                 stroke-linecap="round" stroke-linejoin="round" aria-hidden="true">
              <path d="M8 6h13M8 12h13M8 18h13M3.5 6h.01M3.5 12h.01M3.5 18h.01" />
            </svg>
            Histórico
          </button>
        </nav>
      }
    </div>
  `,
})
export class App {
  protected readonly srv = inject(PortaoService);
  protected readonly aba = signal<'portao' | 'historico'>('portao');

  protected readonly chip = computed(() => {
    if (!this.srv.conectado()) return { cor: '#9AA6B2', texto: 'Sem internet' };
    if (this.srv.estado() === null) return { cor: '#9AA6B2', texto: 'ESP32 …' };
    return this.srv.espOnline()
      ? { cor: '#5EE0A0', texto: 'ESP32 online' }
      : { cor: '#FF8A7A', texto: 'ESP32 offline' };
  });
}
