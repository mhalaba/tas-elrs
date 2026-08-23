# TAS-ELRS — Hardware

> **Languages:** [Polski](README.md) · **English** · [Українська](README_UK.md) · [Español](README_ES.md) · [Português](README_PT.md) · [العربية](README_AR.md)

Reference electronics for the firmware targets of this fork.

## TAS-RX-900-C3 (ready v1.0)

868/915 MHz receiver: ESP32-C3 + E28-900M22S (SX1276). Pin-compatible with the
`Unified_ESP32C3_900_RX_via_UART` target — **zero firmware changes needed**.

- `TAS-RX-900-C3/TAS-RX-900-C3.kicad_sch` — schematic (KiCad 8)
- `TAS-RX-900-C3/TAS-RX-900-C3.kicad_pcb` — PCB skeleton 25×35 mm (edge cuts, footprints, GND zone; manual routing before fab!)
- `TAS-RX-900-C3/PINOUT.md` — pinout & RF layout rules
- `TAS-RX-900-C3/BOM.csv` — bill of materials (~12 items)
- `../src/hardware/RX/TAS RX 900 C3.json` — hardware-def flashed via ELRS Configurator

## Bring-up procedure

1. Fabricate the PCB per PINOUT.md (after manual routing and DRC!).
2. Assemble per BOM; connect the antenna BEFORE first power-up (PA without antenna kills the SX1276).
3. Flash: Configurator → custom target → load JSON `TAS RX 900 C3.json` → firmware `Unified_ESP32C3_900_RX_via_UART` built with `-DTAS_HARDENING`.
4. Bench failsafe test + range test per section 7 of the main TAS_README.

## Hardware design rules

- Every part sourced from ≥2 independent vendors (lesson L4).
- Antenna always attached before power-on; TX power within local law.
- Each PCB revision gets its own directory (`TAS-RX-900-C3-revB/` etc.), never overwrite.
