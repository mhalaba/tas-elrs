# TAS-ELRS v2 — Enlace C2 endurecido basado en ExpressLRS 4.1.0

> **Licencia:** el fork completo es **GPL-3.0** (heredado de ExpressLRS) · nuestros archivos totalmente originales (`src/lib/TAS/*`, docs, tests) están además bajo **MIT** — detalles en `NOTICE` y la sección *Licencia* más abajo.

**¿Qué es TAS-ELRS?** Un enlace de radio de código abierto y endurecido para el control de drones, diseñado para entornos de guerra electrónica: mandos RC cifrados (ChaCha20), anti-replay, detección de interferencias reportada al controlador de vuelo, rotación determinista de canales FHSS y modo de espera de meses con despertar autenticado. Nació como fork de ExpressLRS 4.1.0.

> **Idiomas:** [Polski](../TAS_README.md) · [English](TAS_README_EN.md) · [Українська](TAS_README_UK.md) · **Español** · [Português](TAS_README_PT.md) · [العربية](TAS_README_AR.md)

> Documento escrito desde la perspectiva de un equipo de ingeniería que trabaja
> con drones y guerra electrónica en Ucrania desde 2022: RF/firmware,
> criptografía, formación de operadores, reparaciones de campo. Cada decisión
> de diseño de este repositorio deriva de una lección de campo concreta
> (sección 1), con referencia al código que la implementa.

**Alcance del proyecto: únicamente la capa de comunicaciones (enlace C2 + telemetría).**
Sin cargas útiles, sin armamento, sin guiado ni localización de objetivos.
El cumplimiento normativo de radio (ETSI EN 300 220/328, FCC Part 15) se hereda
de ExpressLRS y sigue siendo obligatorio; la selección de región es permanente.

---

## 1. Lecciones de campo 2022–2026 → requisitos → código

### L1. La detección mata más rápido que la interferencia
Lo peligroso no es el jammer: es el detector de emisiones junto con
radiolocalización y artillería/drones kamikaze. Un operador que permanece
demasiado tiempo en una posición con el transmisor activo se convierte en
objetivo. **Cada segundo de emisión tiene un precio.**

Implementación:
- Perfil STEALTH (sección 5): telemetría OFF, potencia mínima que mantenga el
  enlace, preámbulos cortos;
- Sin campos constantes en texto claro por el aire;
- Verificado upstream: el TX no emite sin handset conectado
  (`tx_main.cpp`: `UARTdisconnected() → hwTimer::stop()`).

### L2. Popularidad = firma registrada
ExpressLRS es el protocolo hobby más popular del mundo; su sync-word, su
preámbulo LoRa y su estructura de tramas llevan años en las tablas de cualquier
escáner. El ELRS de serie se presenta solo antes de hacer nada. Tener FHSS no
basta: el adversario interfiere selectivamente los protocolos que reconoce.

Implementación: MAC con clave en lugar del semilla CRC estática derivada del
UID; cifrado de todo el payload RC; sin bytes constantes identificables;
dominio `"wake"` separado del dominio de datos (`TasWake.cpp`, `TasOta.cpp`).

### L3. La interferencia es selectiva o de banda ancha — planifica ambas
Clases descritas públicamente: barridos de banda ancha sobre bandas completas,
jammers barredores (sweep) y jammers orientados a protocolos conocidos.
Sub-GHz suele estar más limpio pero ofrece menos canales; 2.4 GHz está
saturado por drones propios y Wi-Fi.

Implementación: clasificación CLEAN/SUSPECT/WIDEBAND/SWEEP reportada al FC
(`TasAfh.cpp`, trama CRSF 0x34); recorte determinista por época de canales FHSS
(anti-follow: quien aprendió tu secuencia la pierde en cada época);
dual-band failover LR1121 — Fase B.

### L4. El hardware es fungible. La logística vence al rendimiento
Los drones no vuelven. Los RX se compran a granel por voluntarios. Si tu BOM
necesita piezas con seis semanas de envío, el proyecto está muerto.

Implementación: objetivos BOM (RX < $12, TX < $20 @ 100 uds.), mínimo dos
fuentes independientes por componente, compilación sobre placas ELRS existentes
(sin hardware nuevo para v1).

### L5. El invierno dura más que el calendario
−20 °C con viento, LiPo perdiendo 30–40 % de capacidad, condensación al entrar
en calor. El brownout a 3,0 V/celda debe ser seguro, no aleatorio.

Implementación: criterios de aceptación −25…+55 °C (sección 7), reinicio
seguro del radio ante brownout (watchdog), pruebas de cámara como puerta de
Fase B.

### L6. El operador tuvo dos días de formación y no leerá un manual en una trinchera
La configuración debe ser por presets (QR/archivo), offline, determinista, en
polaco y ucraniano. Nada de "ajustes avanzados" en el campo.

Implementación: presets operativos (sección 5) como especificación para el
portal/Lua; diagnóstico por códigos LED (pegatina en la carcasa) — GUI Fase B.

### L7. Hay veinte drones en el aire y todos usan ELRS
La planificación del espectro y el aislamiento de sesiones son requisitos:
el cross-talk entre pares pierde ambos aparatos.

Implementación: clave de sesión única por pareja derivada del material de
binding (HKDF sobre UID); MAC con clave que ata cada trama a su canal — las
tramas de otro TX fallan la validación (test `tas: cross-slot injection rejected`).
Plan de espectro por unidad — Fase B.

### L8. El momento más peligroso de una misión es perder el enlace con FPV activo
Un acelerador congelado es un regalo para el adversario. El RX no debe
"recordar" los últimos valores más de 150 ms; el failsafe lo decide el FC.

Implementación: semántica failsafe de serie preservada + anti-replay (los
duplicados rechazados nunca refrescan el estado de mandos) + watchdog del
radio (`TasFailsafe.cpp`): silencio > 500 ms con enlace vivo → reinit del chip,
contador visible en telemetría del FC.

### L9. No hay internet. Las actualizaciones viajan en pendrive y BLE de móvil
Implementación: pipeline PlatformIO de serie por target + requisito de
artefactos firmados — Fase B (manifiestos ed25519). v1: distribución offline
(sección 10).

### L10. Un dron puede esperar semanas hasta que haga falta
La batería de un dron en espera muere antes que su electrónica. La opción
"dormir hasta un mes, despertar solo con la señal explícita del TX emparejado"
es una necesidad real de almacén.

Implementación: deep sleep del RX + CAD con ciclo de trabajo + token de
despertar autenticado en el dominio `"wake"` (`TasWake.cpp`). Presupuestos:
3000 mAh → ~44 días, 5000 mAh → ~73 días (poll 3 s). HAL por placa — Fase B
(`#error` deliberado sin hooks para que nadie envíe una función de papel).

---

## 2. Estado de implementación

| Función | Estado | Código |
|---|---|---|
| Cifrado ChaCha20 (RFC 8439) del payload RC | ✅ v1 | `TasCrypto.cpp`, `TasOta.cpp` |
| MAC por trama con clave (canal+nonce+época) | ✅ v1 | `TasSession.cpp` |
| HKDF-SHA256 desde material de binding | ✅ v1 | `TasSession.cpp` |
| Anti-replay (ventana monótona de nonce) | ✅ v1 | `TasSession.cpp`, `rx_main.cpp` |
| Clasificación de interferencias → FC | ✅ v1 | `TasAfh.cpp`, `TasTelemetry.cpp` |
| Recorte FHSS determinista (anti-follow) | ✅ núcleo | `TasAfh.cpp` |
| Watchdog del radio (reinit tras >500 ms de silencio) | ✅ v1 | `TasFailsafe.cpp` |
| Telemetría TAS_STATUS 0x34 | ✅ v1 | `TasTelemetry.cpp` |
| Deep sleep + token de despertar | ✅ lógica / ⚠ HAL | `TasWake.cpp` |
| Rotación de claves en vuelo | 🔜 B (contador de época en 4 bytes libres SYNC OTA8) | — |
| AFH por medición TX↔RX, dual-band failover | 🔜 B | — |
| OTA firmado (ed25519), secure boot | 🔜 B | — |
| Relay capa PHY | 🔜 C | — |

## 3. Modelo de amenazas

| # | Amenaza | Impacto sin mitigación | Mitigación v1 | Riesgo residual |
|---|---|---|---|---|
| T1 | Escáner de firmas → DF → fuego sobre la posición del operador | bajas | perfiles L1, sin campos constantes, todo con clave | la emisión RF siempre es físicamente detectable |
| T2 | Jammer consciente del protocolo ELRS | pérdida de enlace | recorte por época, watchdog, failsafe FC | un barrido total vence a cualquier protocolo |
| T3 | Barrido/wideband | pérdida de enlace | clasificación + informe (cambia banda/posición) | ídem |
| T4 | Replay/inyección de tramas | toma de control | MAC atado a slot + ventana nonce | — |
| T5 | Spoofing de SYNC | desincronización | SYNC sin cifrar pero poco sensible; datos validados | posible pérdida breve de lock |
| T6 | Captura física del RX | extracción de claves | rotación Fase B; rebind tras pérdida | v1: clave estática en el dispositivo |

## 4. Propiedades criptográficas v1 — honestamente

1. Payload RCDATA cifrado con ChaCha20; byte de tipo y CRC quedan en claro
   (framing); SYNC sin cifrar (necesario para resincronizar tras pérdida total).
2. MAC = SHA-256 de material de clave de época ligado al canal FHSS y al nonce,
   truncado en el campo CRC. **No es Poly1305**: el presupuesto de bytes OTA4/8
   no admite un tag completo. La falsificación offline queda prácticamente
   excluida; la demostrabilidad formal es menor que AEAD — compromiso consciente,
   mejorado en Fase B.
3. Anti-replay: todo paso atrás o duplicado se rechaza; la recuperación va por
   SYNC, nunca aceptando tramas viejas.
4. Clave estática durante la vida de la versión (sin campo de época en el aire).
   Los pares (canal, nonce) se repiten como mucho cada ~768 tramas (dominios de
   3 canales): fuga = XOR de dos payloads separados por un múltiplo del período.
   **Donde el adversario archiva emisiones masivamente, se requiere Fase B antes.**
5. El token de despertar autentica al transmisor (comparación const-time), no
   la frescura; despertar no omite ninguna autorización de vuelo.

## 5. Presets operativos (spec portal/Lua, v0)

| Preset | Packet rate | TLM | Potencia | Recorte | Uso |
|---|---|---|---|---|---|
| STANDARD | 250–500 Hz | 1:16 | auto | on | trabajo por defecto |
| EW_HEAVY | 125–250 Hz | 1:32 | máxima legal | on, agresivo | interferencia intensa |
| STEALTH | 100–150 Hz | **off** | mínima que sostenga LQ | on | cerca del frente, L1 |
| MAX_RANGE | 25–50 Hz | 1:64 | máxima legal | off | reconocimiento lejano |
| LONG_WATCH | 50 Hz | 1:128 | baja | off | vigilancia + deep sleep |
| TRAINING/BENCH | cualquiera | 1:8 | ≤25 mW | off | trabajo legal en banco |

## 6. Despertar por señal — detalles

Ciclo: MCU deep sleep → cada `TAS_WAKE_POLL_MS` un escaneo CAD → actividad →
recepción del token → comparación const-time → arranque completo del enlace.
El beacon del TX emite el token ≥1 periodo de poll en el canal de sync.

Presupuestos (`TasWakeEstimateDays`, 80 mA / 100 ms por escaneo):

| Batería | Poll | Corriente media | Espera |
|---|---|---|---|
| 3000 mAh | 3 s | ~2,9 mA | ~44 días |
| 5000 mAh | 3 s | ~2,9 mA | ~73 días |
| 3000 mAh | 1 s | ~8,2 mA | ~15 días |

Procedimiento de almacén: flashear LONG_WATCH → dormir → despertar solo con el
beacon propio. Pérdida de un RX = procedimiento de rebind (clave por pareja;
la rotación de Fase B cierra el tema definitivamente). HAL (ESP32 ext0/timer +
DIO1-CAD; STM32 standby/WKUP) — Fase B por placa; `-DTAS_WAKE_ON_SIGNAL` sin
hooks termina el build con `#error`.

## 7. Criterios de aceptación

Hecho (nativo, 58 aserciones PASS — `src/test/run_tas_tests.sh`): vectores RFC
(ChaCha20/HKDF/HMAC/SHA-256), determinismo y límites del recorte (incluidos
dominios EU868 de 13 canales y protección de slots SYNC), roundtrip TAS sobre
el `OTA.cpp` real (ciphertext ≠ plaintext, tamper/cross-slot/replay rechazados,
SYNC pasa, OTA8 funciona), regresión stock sin flag, presupuestos de sueño
>30 días @3000 mAh, veredictos del watchdog.

Pendiente en hardware (bloqueantes de despliegue):
1. HIL con jammer SDR (sweep+wideband): curvas LQ vs potencia, re-lock <2 s
   (STANDARD) / <5 s (low-signal) tras 30 s de barrido.
2. Test de alcance según protocolo stock con margen −3 dB por crypto/FEC.
3. Soak 24 h: cero pérdidas de sincronización, free-heap plano.
4. Cámara −25/+55 °C: boot y brownout a 3,0 V/celda.
5. Consumo real medido en deep sleep (nuestros números son modelo, no medidor).
6. Matriz de builds PIO por target — ¡nuestros tests son nativos y no sustituyen
   la compilación del firmware!

## 8. Proceso

- Cualquier cambio en rutas criptográficas: PR + actualización de vectores aquí.
- Revisión por dos personas para `TasCrypto/TasSession/TasOta`.
- Sin metadatos de ubicación en commits; nomenclatura neutral.
- Fase B (orden): rotación de claves vía SYNC OTA8 → AFH por medición →
  dual-band failover → OTA firmado → HAL wake por placa.
- Fase C: relay PHY (sin descifrado en el relay).

## 9. Límites y cumplimiento

Este proyecto no contiene funciones de armamento, guiado ni localización de
objetivos — esos PR se rechazarán. Los equipos deben operar dentro de la
normativa de radio nacional; exportación y transferencia pueden estar sujetas
a controles de doble uso — revisión legal antes de distribuir fuera del proyecto.

## 10. Guía rápida para el constructor voluntario

1. Firmware: pipeline PlatformIO de serie; en `user_defines.txt` descomenta
   `-DTAS_HARDENING` (¡TX **y** RX siempre juntos! Stock y TAS nunca hablan entre sí).
2. Compila y ejecuta la suite: `bash src/test/run_tas_tests.sh` — debe dar PASS.
3. Bind con la frase clásica; verifica en el portal que `TAS_STATUS` llega al FC.
4. Test de failsafe en banco: apaga el TX con gas armado en el soporte —
   gas 0 en ≤150 ms, acción según configuración del FC.
5. Sleep/wake: solo cuando lleguen los hooks de tu placa (Fase B).

---

## Licencia y atribución

- **El fork en conjunto permanece en GPL-3.0** (archivo `LICENSE`) — requisito
  legal heredado de ExpressLRS. Cambios sobre archivos upstream y obras
  derivadas: solo GPL-3.0.
- **Nuestros archivos totalmente originales** — `src/lib/TAS/*`,
  `docs/TAS_README_*`, `src/test/test_tas_*`, `src/test/run_tas_tests.sh` —
  están además **bajo licencia MIT** (Copyright © 2026 TAS-ELRS Team).
- **Atribución:** el equipo y comunidad de
  [ExpressLRS](https://github.com/ExpressLRS/ExpressLRS) (GPL-3.0) por la base
  de código, la arquitectura FHSS/OTA y años de trabajo abierto.

## Apoya el proyecto ☕

Si este proyecto te ayudó: [**invítanos a un café → buycoffee.to/maha**](https://buycoffee.to/maha)
