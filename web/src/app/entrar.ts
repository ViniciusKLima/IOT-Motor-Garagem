import { Component, inject, signal } from '@angular/core';
import { Perfil, PortaoService } from './portao.service';

/** Tela 1: Entrar (perfil + PIN de 6 dígitos). */
@Component({
  selector: 'app-entrar',
  template: `
    <main class="entrar">
      <div class="titulo-bloco">
        <h2 class="titulo-tela">Entrar</h2>
        <p class="subtitulo">Escolha quem você é e digite seu PIN.</p>
      </div>

      <div class="campo">
        <label for="perfil" class="rotulo">PERFIL</label>
        <select id="perfil" class="select" [value]="perfilUid()" (change)="escolher($any($event.target).value)">
          <option value="">{{ srv.perfisCarregados() ? 'Escolher perfil' : 'Carregando…' }}</option>
          @for (p of srv.perfis(); track p.uid) {
            <option [value]="p.uid">{{ p.nome }}</option>
          }
        </select>
      </div>

      <div class="pin">
        <div class="rotulo">PIN</div>
        <div class="pontos" role="img" [attr.aria-label]="pin().length + ' de 6 dígitos digitados'">
          @for (i of seis; track i) {
            <span class="ponto" [class.cheio]="i < pin().length"></span>
          }
        </div>
        <div class="ajuda" [class.erro]="erro()" aria-live="polite">{{ ajuda() }}</div>
      </div>

      <div class="teclado">
        @for (t of teclas; track $index) {
          @if (t === '') {
            <span></span>
          } @else if (t === 'apagar') {
            <button type="button" class="tecla" aria-label="Apagar" (click)="apagar()" [disabled]="entrando()">←</button>
          } @else {
            <button type="button" class="tecla" [attr.aria-label]="'Dígito ' + t" (click)="digitar(t)" [disabled]="entrando()">
              {{ t }}
            </button>
          }
        }
      </div>
    </main>
  `,
})
export class Entrar {
  protected readonly srv = inject(PortaoService);
  protected readonly seis = [0, 1, 2, 3, 4, 5];
  protected readonly teclas = ['1', '2', '3', '4', '5', '6', '7', '8', '9', '', '0', 'apagar'];

  protected readonly perfilUid = signal('');
  protected readonly pin = signal('');
  protected readonly ajuda = signal(this.srv.erroLogin() ?? 'PIN de 6 dígitos');
  protected readonly erro = signal(!!this.srv.erroLogin());
  protected readonly entrando = signal(false);

  private mensagem(texto: string, erro = false): void {
    this.ajuda.set(texto);
    this.erro.set(erro);
  }

  protected escolher(uid: string): void {
    this.perfilUid.set(uid);
    this.pin.set('');
    this.mensagem('PIN de 6 dígitos');
  }

  protected apagar(): void {
    this.pin.update((p) => p.slice(0, -1));
    this.mensagem('PIN de 6 dígitos');
  }

  protected digitar(d: string): void {
    const perfil = this.srv.perfis().find((p) => p.uid === this.perfilUid());
    if (!perfil) {
      this.mensagem('Escolha o perfil primeiro', true);
      return;
    }
    if (this.pin().length >= 6) return;
    this.pin.update((p) => p + d);
    this.mensagem('PIN de 6 dígitos');
    if (this.pin().length === 6) {
      this.entrando.set(true);
      setTimeout(() => this.tentar(perfil), 250);
    }
  }

  private async tentar(perfil: Perfil): Promise<void> {
    this.mensagem('Entrando…');
    const erro = await this.srv.entrar(perfil, this.pin());
    this.entrando.set(false);
    if (erro) {
      this.pin.set('');
      this.mensagem(erro, true);
    }
  }
}
