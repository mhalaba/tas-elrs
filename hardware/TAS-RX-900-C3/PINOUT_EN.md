# TAS-RX-900-C3 — pinout & firmware contract

The pin map has **three sources of truth** that must change together in one commit:
1. this file,
2. `src/hardware/RX/TAS RX 900 C3.json` (hardware-def flashed via Configurator),
3. the KiCad schematic.

| Firmware function | C3 GPIO | KiCad net | Notes |
|---|---|---|---|
| CRSF UART RX | 20 | UART_RX | to FC |
| CRSF UART TX | 21 | UART_TX | to FC |
| SX1276 SCK | 6 | SPI_SCK | |
| SX1276 MOSI | 7 | SPI_MOSI | |
| SX1276 MISO | 4 | SPI_MISO | |
| SX1276 NSS | 5 | SPI_NSS | |
| SX1276 RST | 2 | RF_RST | |
| SX1276 DIO0 | 3 | RF_DIO0 | RX/TX done |
| SX1276 DIO1 | 10 | RF_DIO1 | CAD/CRC err |
| Status LED | 8 | — | via R1 1k |
| Bind button | 9 | — | BOOT, internal pull-up, button to GND |

## Why these GPIOs
- **18/19 left free** → native USB-C (flash/OTA without an adapter; `ARDUINO_USB_MODE` in the target).
- **11–17 skipped** → reserved by the WROOM-02 module's internal flash.
- **9 = BOOT**: dual role as bind button, same as stock C3 boards.
- Power: `power_values [120,124,127]` = SX1276 PA registers identical to `Generic 900.json` (≤ ~+13 dBm e.i.r.p. with ≤2 dBi antenna — verify local law).

## Mandatory RF layout rules
1. ANT_FEED microstrip 50 Ω — **3.06 mm** width on 1.6 mm FR4 εr 4.4 (2 layers). For 1.0 mm: ~2.2 mm.
2. Shortest possible antenna trace, **no vias** in the signal line.
3. Full ground pour on F.Cu and B.Cu; via fence every ≤ 2.5 mm along the antenna line.
4. E28 module antenna area (module PCB-antenna edge): **no copper**, keepout 8×5 mm.
5. LDO: C1 close to VIN, C2 close to VOUT; LDO ground via bundle to GND.
6. Edge SMA: center pad extends ~1 mm beyond Edge.Cuts (edge-SMA standard).

## CAD status
- Schematic: logically complete (nets match the JSON above).
- PCB: **skeleton** — outline, footprints, GND zone; **manual routing + DRC + second-person review before ordering.**
