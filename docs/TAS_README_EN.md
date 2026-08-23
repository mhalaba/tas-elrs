# TAS-ELRS v2 — Hardened C2 link based on ExpressLRS 4.1.0

> **License:** the fork as a whole is **GPL-3.0** (inherited from ExpressLRS) · our fully original files (`src/lib/TAS/*`, docs, tests) are additionally **MIT** — see `NOTICE` and the *License* section below.

**What is TAS-ELRS?** An open-source, hardened radio link for drone control designed for electronic-warfare environments: encrypted RC commands (ChaCha20), anti-replay, jamming detection reported to the flight controller, deterministic FHSS channel rotation, and months-long standby with authenticated wake-up. It started as a fork of ExpressLRS 4.1.0.

> **Languages / Języki:** [Polski](../TAS_README.md) · **English** · [Українська](TAS_README_UK.md) · [Español](TAS_README_ES.md) · [Português](TAS_README_PT.md) · [العربية](TAS_README_AR.md)

> This document is written from the perspective of an engineering team that has
> worked on drones and electronic warfare in Ukraine since 2022: RF/firmware,
> cryptography, operator training, field repairs. Every design decision in this
> repository derives from a specific field lesson — each one is listed in
> section 1 with a pointer to the code implementing it.

**Project scope: the communications layer only (C2 link + telemetry).**
No payloads, no armament, no targeting of any kind. Radio regulatory compliance
(ETSI EN 300 220/328, FCC Part 15) is inherited from stock ExpressLRS and
remains mandatory; region selection is permanent.

---

## 1. Field lessons 2022–2026 → requirements → code

### L1. Detection kills faster than jamming
The scariest thing is not the jammer. It is the emission detector paired with
direction finding and artillery/LM drones. An operator sitting too long in one
spot with an active transmitter becomes a target. **Every second of RF
emission has a price.**

Implementation:
- STEALTH profile (section 5): telemetry OFF, minimum power sustaining link,
  short preambles;
- no constant plaintext fields over the air (L2);
- verified upstream: TX does not emit without a connected handset
  (`tx_main.cpp`: `UARTdisconnected() → hwTimer::stop()`).

### L2. Popularity equals a registered signature
ExpressLRS is the most popular hobby protocol in the world. Its sync word,
LoRa preamble and frame layout have long been in every scanner's tables.
Stock ELRS on a modern battlefield announces itself before doing anything.
"Tasitary FHSS" alone is not enough — adversaries selectively jam protocols
they recognise.

Implementation: keyed MAC instead of the static UID-derived CRC seed;
encryption of the entire RC payload; no identifiable constant bytes;
the `"wake"` domain separated from the data domain (`TasWake.cpp`, `TasOta.cpp`).

### L3. Jamming is either selective or wideband — plan for both
Publicly described threat classes: wideband barrages covering whole bands,
sweep jammers, and protocol-aware jammers targeted at known systems.
Sub-GHz bands are often cleaner but offer fewer channels; 2.4 GHz is crowded
by friendly drones plus Wi-Fi.

Implementation: CLEAN/SUSPECT/WIDEBAND/SWEEP classification reported to the FC
(`TasAfh.cpp`, CRSF frame 0x34); deterministic per-epoch FHSS channel pruning
(anti-follow: an adversary who learned your sequence loses it every epoch);
LR1121 dual-band failover — Phase B.

### L4. Hardware is expendable. Logistics beat performance
Drones do not come back. RX units are bought in bulk by volunteers. If your
BOM needs parts with six-week shipping, the project is dead.

Implementation: BOM targets (RX < $12, TX < $20 @ 100 pcs), at least two
independent sources per part, builds on existing ELRS boards (no new hardware
for v1).

### L5. Winter lasts longer than the calendar season
−20 °C in the wind, LiPo losing 30–40 % capacity, condensation on warming up.
Brownout at 3.0 V/cell must be safe, not random.

Implementation: −25…+55 °C acceptance criteria (section 7), brownout-safe radio
restart (watchdog), chamber tests as Phase B gate.

### L6. The operator had two days of training and will not read a manual in a trench
Configuration must be preset-based (QR/file), offline, deterministic, in Polish
and Ukrainian. No "advanced settings" in the field.

Implementation: operational presets (section 5) as portal/Lua spec; LED
diagnostic codes (sticker on the case) — Phase B GUI.

### L7. Twenty drones overhead, all running ELRS
Spectrum planning and session isolation are requirements, not luxuries:
cross-talk between pairs loses both aircraft.

Implementation: unique per-pair session key derived from binding material
(HKDF over UID); keyed MAC binding a frame to its channel — another TX's frames
fail validation (test `tas: cross-slot injection rejected`). Unit-level spectrum
plan — Phase B.

### L8. The most dangerous moment of a mission is link loss with active FPV
Frozen throttle commands are a gift to the adversary. The RX must not "remember"
last throttle values longer than 150 ms; failsafe decisions belong to the FC.

Implementation: stock failsafe semantics preserved + anti-replay (rejected
duplicates never refresh command state) + radio watchdog (`TasFailsafe.cpp`):
silence > 500 ms while link was alive → silicon reinit, counter visible in FC
telemetry.

### L9. There is no internet. Updates travel by pendrive and phone BLE
Implementation: stock PlatformIO pipeline per target + signed artifact
requirement — Phase B (ed25519 manifests). v1: offline distribution process
(section 10).

### L10. A drone may wait weeks until it is needed
A waiting drone's battery dies faster than its electronics. A "sleep for a month,
wake on explicit signal from the paired TX" option is a real depot requirement.

Implementation: RX deep sleep + duty-cycled CAD + token authenticated in the
`"wake"` key domain (`TasWake.cpp`). Budgets: 3000 mAh → ~44 days, 5000 mAh →
~73 days (3 s poll). Per-board HAL — Phase B (deliberate `#error` without hooks
so nobody ships a paper feature).

---

## 2. Implementation status (code mapping)

| Feature | State | Code |
|---|---|---|
| ChaCha20 (RFC 8439) RC payload encryption | ✅ v1 | `TasCrypto.cpp`, `TasOta.cpp` |
| Keyed per-frame MAC (channel+nonce+epoch material) | ✅ v1 | `TasSession.cpp` |
| HKDF-SHA256 from binding material | ✅ v1 | `TasSession.cpp` |
| Anti-replay (monotonic nonce window) | ✅ v1 | `TasSession.cpp`, `rx_main.cpp` |
| Jamming classification → FC | ✅ v1 | `TasAfh.cpp`, `TasTelemetry.cpp` |
| Deterministic FHSS pruning (anti-follow) | ✅ core | `TasAfh.cpp` |
| Radio watchdog (reinit after >500 ms silence) | ✅ v1 | `TasFailsafe.cpp` |
| TAS_STATUS telemetry 0x34 | ✅ v1 | `TasTelemetry.cpp` |
| Deep sleep + wake token | ✅ logic / ⚠ HAL | `TasWake.cpp` |
| In-flight key rotation | 🔜 B (epoch counter in 4 free OTA8 SYNC bytes) | — |
| Measurement-driven AFH, dual-band failover | 🔜 B | — |
| Signed OTA (ed25519), secure boot | 🔜 B | — |
| PHY-layer relay | 🔜 C | — |

## 3. Threat model

| # | Threat | Impact unmitigated | v1 mitigation | Residual risk |
|---|---|---|---|---|
| T1 | Signature scanner → DF → fire on operator position | casualties | L1 profiles, no constant fields, everything keyed | RF emission is always physically detectable |
| T2 | Protocol-aware ELRS jammer | link loss | per-epoch pruning, watchdog, FC failsafe | total barrage defeats any protocol |
| T3 | Sweep/wideband barrage | link loss | classification + report (operator changes band/position) | as above |
| T4 | Frame replay/injection | command takeover | slot-bound keyed MAC + nonce window | — |
| T5 | SYNC spoofing | desynchronisation | SYNC unencrypted but low-sensitivity; data still validated | brief lock-loss possible |
| T6 | Physical capture of RX | key extraction | Phase B rotation; rebind procedure after hardware loss | v1: static key on device |

## 4. Crypto properties v1 — honestly, no marketing

1. RCDATA payload encrypted with ChaCha20; type byte and CRC stay clear
   (framing); SYNC unencrypted (required for resync after total loss).
2. MAC = SHA-256 of epoch-key material bound to FHSS channel and frame nonce,
   truncated into the CRC field. **It is not Poly1305** — the OTA4/8 byte budget
   does not fit a full tag. Offline forgery is practically excluded; formal
   provability weaker than AEAD — conscious trade-off, improved in Phase B.
3. Anti-replay: every backward step/duplicate rejected; link recovery goes
   through SYNC, never through accepting old frames.
4. Static key for the firmware version lifetime (no epoch field over the air).
   (channel, nonce) pairs repeat at worst every ~768 frames (3-channel domains):
   leak = XOR of two payloads separated by a multiple of that period.
   **Where the adversary mass-archives emissions, Phase B is required first.**
5. Wake token authenticates the transmitter (const-time compare), not freshness;
   waking bypasses no flight authorisation whatsoever.

## 5. Operational presets (portal/Lua spec, v0)

| Preset | Packet rate | TLM | Power | Pruning | Use |
|---|---|---|---|---|---|
| STANDARD | 250–500 Hz | 1:16 | auto | on | default work |
| EW_HEAVY | 125–250 Hz | 1:32 | max legal | on, aggressive | heavy jamming |
| STEALTH | 100–150 Hz | **off** | minimum sustaining LQ | on | near the front, L1 |
| MAX_RANGE | 25–50 Hz | 1:64 | max legal | off | long-range recon |
| LONG_WATCH | 50 Hz | 1:128 | low | off | loiter + deep sleep |
| TRAINING/BENCH | any | 1:8 | ≤25 mW | off | legal bench work |

## 6. Wake-on-signal details

Cycle: MCU deep sleep → every `TAS_WAKE_POLL_MS` one CAD scan → activity →
token reception → const-time comparison → full link boot. TX beacon transmits
the token for ≥1 poll period on the sync channel.

Budgets (`TasWakeEstimateDays`, 80 mA scan current / 100 ms):

| Battery | Poll | Average current | Standby |
|---|---|---|---|
| 3000 mAh | 3 s | ~2.9 mA | ~44 days |
| 5000 mAh | 3 s | ~2.9 mA | ~73 days |
| 3000 mAh | 1 s | ~8.2 mA | ~15 days |

Depot procedure: flash LONG_WATCH → enter sleep → wake only via own TX beacon.
Field loss of an RX = rebind procedure (per-pair key; Phase B rotation closes
the topic definitively). HAL (ESP32 ext0/timer + DIO1-CAD; STM32 standby/WKUP)
— Phase B per board; `-DTAS_WAKE_ON_SIGNAL` without hooks ends the build with
`#error`.

## 7. Acceptance criteria

Done (native, 60/60 PASS — `src/test/run_mil_tests.sh`): RFC vectors
(ChaCha20/HKDF/HMAC/SHA-256), pruning determinism and bounds (incl. 13-channel
EU868 domains and SYNC-slot protection), TAS roundtrip on real `OTA.cpp`
(ciphertext ≠ plaintext, tamper/cross-slot/replay rejected, SYNC passes, OTA8
works), stock regression without the flag, sleep budgets >30 days @3000 mAh,
watchdog verdicts.

Required on hardware (deployment blockers):
1. HIL with SDR jammer (sweep+wideband): LQ vs power curves, re-lock <2 s
   (STANDARD) / <5 s (low-signal) after a 30 s barrage.
2. Range test per stock protocol with a −3 dB margin for crypto/FEC.
3. 24 h soak: zero synchronisation loss, flat free-heap trend.
4. −25/+55 °C chamber: boot and brownout at 3.0 V/cell.
5. Measured deep-sleep current draw (our numbers are a model, not a meter).
6. PIO build matrix per target — our tests are native and do not replace
   firmware compilation!

## 8. Process

- Any change to crypto paths: PR + update vectors in this README.
- Two-person review for `TasCrypto/TasSession/TasOta`.
- No location metadata in commits; neutral naming.
- Phase B (order): key rotation via OTA8 SYNC → measurement-driven AFH →
  dual-band failover → signed OTA → per-board wake HAL.
- Phase C: PHY-layer relay (no decryption at the relay).

## 9. Boundaries and compliance

This project contains no armament, guidance or targeting functions — such PRs
will be rejected. Devices must operate within national radio regulations;
export and transfer may be subject to dual-use controls — legal review before
distribution outside the project.

## 10. Volunteer builder quick start

1. Firmware: stock PlatformIO pipeline; in `user_defines.txt` uncomment
   `-DTAS_HARDENING` (TX **and** RX always together! Stock and TAS never talk).
2. Build and run the suite: `bash src/test/run_mil_tests.sh` — must PASS.
3. Bind with the classic binding phrase; check in the portal that `TAS_STATUS`
   reaches the FC.
4. Bench failsafe test: kill TX with a armed-throttle rig on the stand —
   throttle 0 within ≤150 ms, action per FC config.
5. Sleep/wake: only after your board's hooks land (Phase B).

---

## License and attribution

- **The fork as a whole remains GPL-3.0** (`LICENSE` file) — a legal requirement
  inherited from ExpressLRS. Changes to upstream files and derivative works are
  GPL-3.0 only.
- **Our fully original files** — `src/lib/TAS/*`, `docs/TAS_README_*`,
  `src/test/test_tas_*`, `src/test/run_tas_tests.sh` — are **additionally
  licensed under MIT** (Copyright © 2026 TAS-ELRS Team) for independent use.
- **Attribution:** the ExpressLRS team and community
  ([ExpressLRS/ExpressLRS](https://github.com/ExpressLRS/ExpressLRS), GPL-3.0)
  for the code base, FHSS/OTA architecture and years of open work.

## Support the project ☕

If this project helped you: [**buy us a coffee → buycoffee.to/maha**](https://buycoffee.to/maha)

## Changelog

**v2.1** — v2 self-audit fixes:
- jam classification wired end-to-end (RX → LQ×RSSI heuristic → `TAS_STATUS` to FC; previously dead code),
- hardened binding-phrase KDF: 10 000 × SHA-256 when `-DTAS_HARDENING` is set (stock md5 otherwise); both sides must be built from the same user_defines,
- property test: all 54 single-bit payload/header flips rejected,
- fixed PIO library build for TAS (library.json + test_ignore) — upstream ELRS CI passes again,
- dedicated CI workflow (`tas-tests`) + KDF selftest in the suite.

## Hardware
Reference electronics (KiCad, BOM, hardware-def): [`hardware/`](../hardware/README.md) — TAS-RX-900-C3 v1.0, pin-compatible with the `Unified_ESP32C3_900_RX` target.
