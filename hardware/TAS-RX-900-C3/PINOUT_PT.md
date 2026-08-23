# TAS-RX-900-C3 — pinout e contrato com o firmware

O mapa de pinos tem **três fontes de verdade** que mudam juntas num único commit:
1. este ficheiro,
2. `src/hardware/RX/TAS RX 900 C3.json`,
3. o esquemático KiCad.

| Função firmware | GPIO C3 | Rede KiCad | Notas |
|---|---|---|---|
| CRSF UART RX | 20 | UART_RX | ao FC |
| CRSF UART TX | 21 | UART_TX | ao FC |
| SX1276 SCK | 6 | SPI_SCK | |
| SX1276 MOSI | 7 | SPI_MOSI | |
| SX1276 MISO | 4 | SPI_MISO | |
| SX1276 NSS | 5 | SPI_NSS | |
| SX1276 RST | 2 | RF_RST | |
| SX1276 DIO0 | 3 | RF_DIO0 | RX/TX done |
| SX1276 DIO1 | 10 | RF_DIO1 | CAD/CRC err |
| LED de estado | 8 | — | via R1 1k |
| Botão bind | 9 | — | BOOT, pull-up interno, botão para GND |

## Porque estes GPIO
- **18/19 livres** → USB-C nativo (flash/OTA sem adaptador).
- **11–17 ignorados** → reservados pelo flash interno do módulo WROOM-02.
- **9 = BOOT**: dupla função como botão bind, igual às placas C3 stock.
- Potência: `power_values [120,124,127]` = registos PA do SX1276 idênticos a
  `Generic 900.json` (≤ ~+13 dBm e.i.r.p. com antena ≤2 dBi — verifique a lei local).

## Regras obrigatórias de layout RF
1. Micropista ANT_FEED 50 Ω — **3,06 mm** em FR4 1,6 mm εr 4,4 (2 camadas). Para 1,0 mm: ~2,2 mm.
2. Traço de antena o mais curto possível, **sem vias** na linha de sinal.
3. Massa integral em F.Cu e B.Cu; cerca de vias cada ≤ 2,5 mm ao longo da linha da antena.
4. Zona da antena do módulo E28 (borda com antena PCB): **sem cobre**, keepout 8×5 mm.
5. LDO: C1 junto ao VIN, C2 junto ao VOUT; massa do LDO com feixe de vias para GND.
6. SMA de borda: pad central estende-se ~1 mm além de Edge.Cuts.

## Estado do CAD
- Esquemático: logicamente completo (redes coincidem com o JSON acima).
- PCB: **esqueleto** — contorno, footprints, zona GND; **encaminhamento manual +
  DRC + revisão por segunda pessoa antes de encomendar.**
