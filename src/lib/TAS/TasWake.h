#pragma once

#include <stdint.h>
#include "TasCrypto.h"

// Wake-on-signal deep sleep option.
//
// The RX sleeps with the MCU in deep sleep and the radio duty-cycled in CAD
// (channel activity detection) mode: once per TAS_WAKE_POLL_S the radio wakes,
// scans one symbol window for a LoRa preamble, and returns to sleep unless
// activity is detected. The TX broadcasts a keyed wake token repeatedly for
// long enough that any polling window catches it. Only the paired TX can
// produce valid tokens, so jamming noise or foreign transmitters never wake
// the receiver.
//
// Power budget (SX127x-class radio, measured-typical currents):
//   average_I ~= (t_scan * i_radio_active) / poll_period + i_deepsleep
//   poll=3s   => ~2.7 mA  => 3000 mAh pack lasts ~46 days, 5000 mAh ~77 days.
// Worst-case wake latency equals the poll period.

#ifndef TAS_WAKE_TOKEN_LEN
#define TAS_WAKE_TOKEN_LEN 8
#endif

#ifndef TAS_WAKE_POLL_MS
#define TAS_WAKE_POLL_MS 3000
#endif

#ifndef TAS_WAKE_SCAN_MS
#define TAS_WAKE_SCAN_MS 100 // radio active time per poll
#endif

#ifndef TAS_WAKE_RADIO_MA
#define TAS_WAKE_RADIO_MA 80 // current draw while scanning
#endif

// Derive the wake-domain key from session material (domain separated from
// the RC-data keystream key).
void TasWakeDeriveKey(
    const uint8_t masterKey[TAS_KEY_LEN],
    uint8_t outWakeKey[TAS_KEY_LEN]);

// Compute the 64-bit wake token broadcast by the TX.
void TasWakeMakeToken(
    const uint8_t wakeKey[TAS_KEY_LEN],
    uint8_t outToken[TAS_WAKE_TOKEN_LEN]);

// Constant-time token comparison used by the sleeping RX upon CAD activity.
bool TasWakeTokenValid(
    const uint8_t wakeKey[TAS_KEY_LEN],
    const uint8_t candidate[TAS_WAKE_TOKEN_LEN]);

// Battery endurance estimate in whole days for the sleeping state.
uint32_t TasWakeEstimateDays(
    uint32_t battery_mAh,
    uint32_t pollMs,
    uint32_t deepSleepUa);

// Platform bring-up hooks. The portable core above is fully testable on the
// host; entering real deep sleep requires per-target HAL work (ESP32 ext0/
// timer wakeup from DIO1-CAD GPIO, STM32 standby + WKUP pin). Targets that
// enable TAS_WAKE_ON_SIGNAL without providing these hooks fail the build.
#if defined(TAS_WAKE_ON_SIGNAL)
#  if !defined(TAS_WAKE_HAS_PLATFORM_HOOKS)
#    error "TAS_WAKE_ON_SIGNAL requires platform hooks (see TAS_README.md, Faza B)"
#  endif
void TasSleepEnter(void);      // configure radio CAD + MCU deep sleep entry
void TasWakeSendBeacon(void);  // TX-side beacon loop (blocking, menu-invoked)
#endif
