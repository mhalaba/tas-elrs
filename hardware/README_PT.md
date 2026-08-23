# TAS-ELRS — Hardware (PT)

> **Idiomas:** [Polski](README.md) · [English](README_EN.md) · [Українська](README_UK.md) · [Español](README_ES.md) · **Português** · [العربية](README_AR.md)

Eletrónica de referência para os targets de firmware deste fork.

## TAS-RX-900-C3 (pronto v1.0)

Recetor 868/915 MHz: ESP32-C3 + E28-900M22S (SX1276). Compatível em pinos com o
target `Unified_ESP32C3_900_RX_via_UART` — **zero alterações de firmware**.

- `TAS-RX-900-C3/TAS-RX-900-C3.kicad_sch` — esquemático (KiCad 8)
- `TAS-RX-900-C3/TAS-RX-900-C3.kicad_pcb` — esqueleto PCB 25×35 mm (contorno, footprints, zona GND; encaminhamento manual antes da produção!)
- `TAS-RX-900-C3/PINOUT_PT.md` — pinout e regras de layout RF
- `TAS-RX-900-C3/BOM.csv` — lista de materiais (~12 itens)
- `../src/hardware/RX/TAS RX 900 C3.json` — hardware-def gravado via ELRS Configurator

## Procedimento de arranque

1. Fabrique o PCB conforme PINOUT (após encaminhamento manual e DRC!).
2. Monte conforme BOM; ligue a antena ANTES da primeira alimentação (o PA sem antena mata o SX1276).
3. Flash: Configurator → custom target → carregue o JSON `TAS RX 900 C3.json` → firmware `Unified_ESP32C3_900_RX_via_UART` com `-DTAS_HARDENING`.
4. Teste de failsafe em bancada + teste de alcance conforme a secção 7 do TAS_README principal.

## Regras de design

- Cada componente de ≥2 fontes independentes (lição L4).
- Antena sempre montada antes de ligar; potência dentro da lei local.
- Cada revisão PCB = diretório próprio (`-revB/` etc.), nunca sobrescrever.
