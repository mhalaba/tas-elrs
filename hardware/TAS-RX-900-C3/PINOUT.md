# TAS-RX-900-C3 — pinout i kontrakt z firmware

Mapa pinów jest **jedynym źródłem prawdy** dla obu stron:
1. ten plik (dokumentacja),
2. `src/hardware/RX/TAS RX 900 C3.json` (hardware-def flashowany przez ELRS Configurator),
3. schemat KiCad.

Zmiana pinu = zmiana WSZYSTKICH trzech miejsc w jednym commicie.

| Funkcja firmware | GPIO C3 | Sieć KiCad | Uwagi |
|---|---|---|---|
| CRSF UART RX | 20 | UART_RX | do FC |
| CRSF UART TX | 21 | UART_TX | do FC |
| SX1276 SCK | 6 | SPI_SCK | |
| SX1276 MOSI | 7 | SPI_MOSI | |
| SX1276 MISO | 4 | SPI_MISO | |
| SX1276 NSS | 5 | SPI_NSS | |
| SX1276 RST | 2 | RF_RST | |
| SX1276 DIO0 | 3 | RF_DIO0 | RX/TX done |
| SX1276 DIO1 | 10 | RF_DIO1 | CAD/CRC err |
| LED status | 8 | — | przez R1 1k |
| Bind button | 9 | — | BOOT, pull-up wewn., przycisk do GND |

## Dlaczego te GPIO
- **18/19 wolne** → native USB-C (flash/OTA bez przejściówki; `ARDUINO_USB_MODE` w targecie).
- **11–17 pominięte** → rezerwowane pod wewnętrzny flash modułu WROOM-02.
- **9** = BOOT: podwójna rola bind-button jak w stockowych płytach C3.
- Moc: `power_values [120,124,127]` = rejestry PA SX1276 identyczne jak `Generic 900.json` (≤ ~+13 dBm e.i.r.p. z anteną ≤2 dBi — sprawdź lokalne prawo).

## Zasady layoutu RF (obowiązkowe)
1. ANT_FEED: mikropask 50 Ω — **3,06 mm** szerokości na FR4 1,6 mm εr 4,4 (2 warstwy). Przy 1,0 mm: ~2,2 mm.
2. Ścieżka antenowa jak najkrótsza, **bez via** w linii sygnałowej.
3. Pełny ground pour F.Cu i B.Cu; szynek via-fence co ≤ 2,5 mm wzdłuż linii antenowej.
4. Moduł E28: obszar pod anteną modułu (krawędź z PCB-antną) — **bez miedzi**, keepout 8×5 mm.
5. LDO: C1 blisko VIN, C2 blisko VOUT; masa LDO osobnym via-bundle do GND.
6. Krawędź SMA: pad centralny wystaje poza Edge.Cuts o ~1 mm (standard edge-SMA).

## Status CAD
- Schemat: kompletny logicznie (netlisty zgodne z JSON-em powyżej).
- PCB: **szkielet** — outline 25×35 mm, footprints krytyczne umieszczone, GND zone; **routing ręczny przed produkcją + DRC + przegląd drugiej osoby**. Nie zamawiaj bez rewizji!
