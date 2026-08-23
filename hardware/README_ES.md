# TAS-ELRS — Залізо (ES)

> **Idiomas:** [Polski](README.md) · [English](README_EN.md) · [Українська](README_UK.md) · **Español** · [Português](README_PT.md) · [العربية](README_AR.md)

Electrónica de referencia para los targets de firmware de este fork.

## TAS-RX-900-C3 (listo v1.0)

Receptor 868/915 MHz: ESP32-C3 + E28-900M22S (SX1276). Compatible en pines con
el target `Unified_ESP32C3_900_RX_via_UART` — **cero cambios de firmware**.

- `TAS-RX-900-C3/TAS-RX-900-C3.kicad_sch` — esquemático (KiCad 8)
- `TAS-RX-900-C3/TAS-RX-900-C3.kicad_pcb` — esqueleto PCB 25×35 mm
- `TAS-RX-900-C3/PINOUT_ES.md` — pinout y reglas de layout RF
- `TAS-RX-900-C3/BOM.csv` — lista de materiales (~12 ítems)
- `../src/hardware/RX/TAS RX 900 C3.json` — hardware-def para Configurator

## Puesta en marcha

1. Fabrica el PCB según PINOUT (tras enrutado manual y DRC!).
2. Monta según BOM; conecta la antena ANTES del primer encendido (el PA sin antena mata el SX1276).
3. Flash: Configurator → custom target → carga el JSON `TAS RX 900 C3.json` → firmware `Unified_ESP32C3_900_RX_via_UART` con `-DTAS_HARDENING`.
4. Test failsafe de banco + test de alcance según la sección 7 del TAS_README principal.

## Reglas de diseño

- Cada componente de ≥2 fuentes independientes (lección L4).
- Antena siempre puesta antes de encender; potencia dentro de la ley local.
- Cada revisión PCB = directorio propio (`-revB/` etc.), nunca sobrescribir.
