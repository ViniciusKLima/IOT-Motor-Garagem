# Wi-Fi no portão

## O problema
O roteador fica no meio da casa e, lá fora, o sinal é fraco e às vezes cai. O ESP32 precisa de Wi-Fi para receber comandos, esteja ele no motor ou junto de um controle. Só usa **2,4 GHz**. O roteador atual é um modelo genérico do provedor (`192.168.15.1`); se o 2,4 e o 5 GHz tiverem o mesmo nome, conferir em qual o celular está conectado.

## Como medir (pendente)
O iPhone **não** mostra o dBm de verdade no WiFiman sem equipamento UniFi, então a medição é feita **pelo próprio ESP32**:
1. Editar `WIFI_SSID` e `WIFI_PASS` em `firmware/portao_teste_wifi/portao_teste_wifi.ino` e enviar para o ESP32 (placa "ESP32 Dev Module", monitor serial 115200).
2. Ligar numa fonte USB ou power bank, **no ponto exato do portão**, por 10 a 15 minutos.
3. No celular, conectar no Wi-Fi do próprio ESP32 (**PortaoTeste**, senha `12345678`) e abrir `http://192.168.4.1`. Funciona mesmo sem conectar no roteador. `/log` mostra o log completo em texto.
4. Medir com o **portão fechado e aberto**.
5. Registrar o resultado na seção abaixo.

### Como ler o dBm
- mais forte que −70: bom · −70 a −80: no limite · abaixo de −80: precisa de reforço.

## Soluções, da mais confiável para a menos
1. **Levar o Wi-Fi até lá:** repetidor, segundo roteador em modo AP ou nó de mesh perto da garagem (cerca de R$ 80 a 200). Melhor ainda se der para puxar um cabo de rede.
2. **Dois ESP32 com ESP-NOW:** um no meio da casa (perto do roteador) fala com o Firebase; outro no motor recebe o comando direto dele, sem passar pelo roteador. Alcance de dezenas de metros.
3. **Antena melhor:** placa com conector de antena externa (ESP32-WROOM-32U com IPEX) e antena 2,4 GHz de 5 a 8 dBi. Ganho pequeno. O que mais atrapalha é o ESP32 dentro de caixa metálica ou colado no motor; usar caixa plástica e antena para fora.
4. **Plano B:** ESP32 junto do roteador acionando o botão de um controle pareado (ver `hardware.md`).

## Resultados (preencher)
| Local / condição | dBm (mín / máx) | Quedas | Observações |
|---|---|---|---|
| No motor (condição do portão **a confirmar**) | −77 / −75 | 0 em 18 min | Estável. Faixa "no limite", mas suficiente; sem repetidor por ora. |
| Portão fechado |  |  |  |
| Portão aberto |  |  |  |
