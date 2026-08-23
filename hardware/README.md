# TAS-ELRS — Hardware

> **Wersje:** [English](README_EN.md) · [Українська](README_UK.md) · [Español](README_ES.md) · [Português](README_PT.md) · [العربية](README_AR.md)

Referencyjna elektronika dla targetów firmware z tego forka.

## TAS-RX-900-C3 (gotowe v1.0)

RX 868/915 MHz: ESP32-C3 + E28-900M22S (SX1276). Zgodny z targetem
`Unified_ESP32C3_900_RX_via_UART` — **zero zmian w firmware**.

- `TAS-RX-900-C3/TAS-RX-900-C3.kicad_sch` — schemat (KiCad 8)
- `TAS-RX-900-C3/TAS-RX-900-C3.kicad_pcb` — szkielet PCB 25×35 mm (outline, footprints, GND zone; routing przed produkcją!)
- `TAS-RX-900-C3/PINOUT.md` — pinout + zasady layoutu RF
- `TAS-RX-900-C3/BOM.csv` — materiałówka (~12 pozycji)
- `../src/hardware/RX/TAS RX 900 C3.json` — hardware-def do flashowania unified targetu przez Configurator

## Procedura uruchomienia

1. Wytwórz PCB wg PINOUT.md (po ręcznym routingu i DRC!).
2. Zmontuj BOM; podłącz antenę PRZED pierwszym załączeniem (PA bez anteny = śmierć SX1276).
3. Flash: Configurator → custom target → wczytaj JSON `TAS RX 900 C3.json` → firmware `Unified_ESP32C3_900_RX_via_UART` z `-DTAS_HARDENING`.
4. Bench test failsafe + range test wg sekcji 7 TAS_README.

## Zasady projektu sprzętu

- Części ≥2 niezależnych źródeł (L4 z README głównego).
- Antena zawsze przed włączeniem; moc zgodna z prawem regionu.
- Każda rewizja PCB = osobny katalog `TAS-RX-900-C3-revB/` itd., nigdy nadpisywanie.
