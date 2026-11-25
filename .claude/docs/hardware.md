# Hardware

## Motor e central (o que foi visto nas fotos)
- Portão **de correr** com cremalheira; motor **Rossi**, central Rossi com sensor **HALL** de fim de curso. A etiqueta do motor indica 220 V **(a confirmar)**. Modelo exato da central **(a confirmar)**; a etiqueta está parcialmente legível.
- Bornes de **cima** (baixa tensão), da esquerda para a direita:
  `ANT · GND · BOT · TX- · FOTOCÉLULA (2 bornes) · 12V · GND · SIN · FCH`
  Bloco ao lado: `FF · CM · FA`.
- Bornes de **baixo** (fios grossos vermelho, azul, amarelo): motor e rede elétrica. **NÃO MEXER.**
- `ANT`: o fio preto que sai dele é a antena do receptor de controle. Não mexer.
- `BOT`: botoeira de acionamento (um pulso abre/para/fecha). É por aqui que o ESP32 entra.
- `FCH` (botoeira só de fechar) e `SIN` (sinaleira) são prováveis, **a confirmar** no manual.
- `FF/CM/FA`: fim de curso **especial**, são **entradas** (habilitado apertando AJUSTE 8 vezes). **Não servem para o ESP32 ler o estado do portão.** A central detecta o fim de curso por um sensor HALL interno.
- `12V/GND` (saída auxiliar): normalmente alimenta fotocélulas. Como não usam fotocélula, está livre. Corrente disponível **a confirmar**.

## Ligação do relé (sem solda)
| De | Para |
|---|---|
| Relé **COM** | borne **GND** (2º da esquerda, vizinho do BOT) |
| Relé **NO** | borne **BOT** (3º da esquerda) |
| ESP32 GPIO **26** | relé **IN** |
| ESP32 5V/VIN | relé **VCC** |
| ESP32 GND | relé **GND** |

- O relé só **fecha um contato seco**; nenhuma tensão vai para a central.
- Pulso de **500 ms** (como apertar o botão), nunca segurar fechado.
- Fios nos bornes da central: fio fino (~0,5 mm²) com ponteira, apertado no parafuso.

### Teste antes de montar
Com o motor ligado, encostar um pedaço de fio entre **BOT** e **GND** por meio segundo. Se o portão mexer, é isso mesmo. Registrar o resultado.

## Cuidados do ESP32 e do relé
- Usar um pino seguro no boot (GPIO 26 ou 27; evitar 0, 2, 12, 15).
- Colocar o pino no nível **inativo antes** de configurá-lo como saída, para o relé não dar pulso ao ligar. Módulos podem ser ativo-baixo ou ativo-alto **(a confirmar o do Vinícius)**. Testar tirando e voltando a energia: o portão **não pode mexer**.
- Caixa **plástica** e fechada (tem chuva e poeira). Não deixar o ESP32 dentro de metal; antena para fora (ver `wifi.md`).

## Alimentação do ESP32 (decisão pendente)
- **A) Fonte USB 5 V separada** (carregador de celular antigo). Recomendada: mais estável e isola a central de qualquer problema.
- **B) 12 V da central + step-down 12→5 V** (LM2596 ou MP1584). Possível porque a fotocélula não é usada. Antes, medir os 12 V com multímetro e conferir se a saída aguenta ~300 mA de pico (Wi-Fi + relé).

## Sensor de estado (futuro, ainda não comprado)
- Sensor magnético (reed) de alarme, em caixinha com 2 fios, + ímã na folha do portão.
- Ligação: reed entre um GPIO e o GND, com `INPUT_PULLUP` (sem resistor externo). Ligar em borne de parafuso.
- **Opção A (começar por ela):** 1 sensor perto do motor, no ponto de "fechado". Estados: *Fechado* ou *Não fechado*. "Aberto" vs "parado no meio" pelo tempo desde o último comando.
- **Opção B:** 2 sensores (fechado e aberto), com cabo de alarme de 2 fios (2 a 4 m) até a caixa.
- Enquanto não houver sensor, o app mostra o aviso: "Sensor de fechamento ainda não instalado. O estado mostrado é estimado pelo último comando."

## Controle RF (não clonar)
- Controles originais Rossi são **NTX HCS** (rolling code) e algumas centrais têm anticlonagem. Copiar o sinal não funciona.
- Plano B (só se o relé na central não der certo): ESP32 acionando o botão de **um controle pareado**, em qualquer lugar da casa. Contras: depende do controle, usa pilha e perde o relé direto e o sensor.
- Manter **um controle bom de reserva**: se o Wi-Fi cair, o portão continua abrindo por ele.
