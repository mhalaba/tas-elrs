# TAS-RX-900-C3 — pinout y contrato con el firmware

El mapa de pines tiene **tres fuentes de verdad** que deben cambiarse juntas en un solo commit:
1. este archivo,
2. `src/hardware/RX/TAS RX 900 C3.json` (hardware-def grabado vía Configurator),
3. el esquemático KiCad.

| Función firmware | GPIO C3 | Red KiCad | Notas |
|---|---|---|---|
| CRSF UART RX | 20 | UART_RX | al FC |
| CRSF UART TX | 21 | UART_TX | al FC |
| SX1276 SCK | 6 | SPI_SCK | |
| SX1276 MOSI | 7 | SPI_MOSI | |
| SX1276 MISO | 4 | SPI_MISO | |
| SX1276 NSS | 5 | SPI_NSS | |
| SX1276 RST | 2 | RF_RST | |
| SX1276 DIO0 | 3 | RF_DIO0 | RX/TX done |
| SX1276 DIO1 | 10 | RF_DIO1 | CAD/CRC err |
| LED estado | 8 | — | vía R1 1k |
| Botón bind | 9 | — | BOOT, pull-up interno, botón a GND |

## Por qué estos GPIO
- **18/19 libres** → USB-C nativo (flash/OTA sin adaptador; `ARDUINO_USB_MODE` en el target).
- **11–17 omitidos** → reservados por el flash interno del módulo WROOM-02.
- **9 = BOOT**: doble función como botón de bind, igual que las placas C3 stock.
- Potencia: `power_values [120,124,127]` = registros PA del SX1276 idénticos a `Generic 900.json` (≤ ~+13 dBm e.i.r.p. con antena ≤2 dBi — verifica la ley local).

## Reglas obligatorias de layout RF
1. Micropista ANT_FEED 50 Ω — **3,06 mm** en FR4 de 1,6 mm εr 4,4 (2 capas). Para 1,0 mm: ~2,2 mm.
2. Traza de antena lo más corta posible, **sin vias** en la línea de señal.
3. Masa completa en F.Cu y B.Cu; valla de vias cada ≤ 2,5 mm a lo largo de la línea de antena.
4. Zona de antena del módulo E28 (borde con antena PCB): **sin cobre**, keepout 8×5 mm.
5. LDO: C1 junto a VIN, C2 junto a VOUT; tierra del LDO con haz de vias a GND.
6. SMA de borde: pad central sobresale ~1 mm más allá de Edge.Cuts (estándar edge-SMA).

## Estado del CAD
- Esquemático: lógicamente completo (redes coinciden con el JSON anterior).
- PCB: **esqueleto** — contorno, footprints, zona GND; **enrutado manual + DRC + revisión por segunda persona antes de pedir.**
