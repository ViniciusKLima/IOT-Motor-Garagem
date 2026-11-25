import { Component, computed, inject, signal } from '@angular/core';
import { SwUpdate } from '@angular/service-worker';
import { Diagnostico } from './diagnostico';
import { Entrar } from './entrar';
import { Historico } from './historico';
import { Portao } from './portao';
import { PortaoService } from './portao.service';

@Component({
  selector: 'app-root',
  imports: [Entrar, Portao, Historico, Diagnostico],
  template: `
    <div class="tela">
      <header class="cabecalho">
        <div>
          <div class="rotulo">MINHA CASA</div>
          <h1 class="titulo">Portão da garagem</h1>
        </div>
        @if (srv.liberado()) {
          <button type="button" class="pilula" (click)="aba.set('diagnostico')"
                  [attr.aria-label]="chip().texto + '. Abrir diagnóstico'" title="Diagnóstico do ESP32">
            <span class="bolinha" [style.background]="chip().cor"></span>
            <span>{{ chip().texto }}</span>
            <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.4"
                 stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M9 18l6-6-6-6" /></svg>
          </button>
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
        } @else if (aba() === 'historico') {
          <app-historico />
        } @else {
          <app-diagnostico (voltar)="aba.set('portao')" />
        }

        <nav class="barra-flutuante" aria-label="Navegação">
          <span class="barra-destaque" [class.direita]="aba() === 'historico'" [class.oculto]="aba() === 'diagnostico'"
                aria-hidden="true"></span>
          <button type="button" class="aba" [class.ativa]="aba() === 'portao'" (click)="aba.set('portao')"
                  aria-label="Controle do portão" title="Controle do portão"
                  [attr.aria-current]="aba() === 'portao' ? 'page' : null">
            <svg width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"
                 stroke-linecap="round" stroke-linejoin="round" aria-hidden="true">
              <rect x="7" y="2" width="10" height="20" rx="3" />
              <circle cx="12" cy="8" r="2" />
              <path d="M10 14h.01M14 14h.01M10 17.5h.01M14 17.5h.01" />
            </svg>
          </button>
          <button type="button" class="aba" [class.ativa]="aba() === 'historico'" (click)="aba.set('historico')"
                  aria-label="Histórico" title="Histórico"
                  [attr.aria-current]="aba() === 'historico' ? 'page' : null">
            <svg width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"
                 stroke-linecap="round" stroke-linejoin="round" aria-hidden="true">
              <circle cx="12" cy="12" r="9" />
              <path d="M12 7v5l3 2" />
            </svg>
          </button>
        </nav>
      }
    </div>
  `,
})
export class App {
  protected readonly srv = inject(PortaoService);
  protected readonly aba = signal<'portao' | 'historico' | 'diagnostico'>('portao');

  constructor() {
    // App instalado (PWA): quando sai versão nova do site, recarrega sozinho
    // (sem esperar a pessoa fechar e abrir de novo). Não recarrega no meio de um comando.
    const sw = inject(SwUpdate);
    if (!sw.isEnabled) return;
    sw.versionUpdates.subscribe((ev) => {
      if (ev.type === 'VERSION_READY' && !this.srv.pendente()) document.location.reload();
    });
    document.addEventListener('visibilitychange', () => {
      if (document.visibilityState === 'visible') void sw.checkForUpdate().catch(() => undefined);
    });
  }

  protected readonly chip = computed(() => {
    if (!this.srv.conectado()) return { cor: '#9AA6B2', texto: 'Sem internet' };
    if (this.srv.estado() === null) return { cor: '#9AA6B2', texto: 'ESP32 …' };
    return this.srv.espOnline()
      ? { cor: '#5EE0A0', texto: 'ESP32 online' }
      : { cor: '#FF8A7A', texto: 'ESP32 offline' };
  });
}
