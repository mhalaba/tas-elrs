#pragma once

#include <stdint.h>

// Radio-watchdog decision logic (pure function, natively testable).
// While a link is expected (connected or recently seen), the radio IRQ must
// produce activity; prolonged SPI/radio silence means the silicon wedged and
// requires re-initialization.
#ifndef TAS_RADIO_REINIT_MS
#define TAS_RADIO_REINIT_MS 500
#endif

typedef enum {
    TAS_WD_OK = 0,
    TAS_WD_LINK_LOST = 1,   // normal RF loss path, no action
    TAS_WD_RADIO_WEDGE = 2  // radio silent beyond tolerance => reinit
} TasWatchdogVerdict_e;

TasWatchdogVerdict_e TasWatchdogEvaluate(
    uint32_t nowMs,
    uint32_t lastValidPacketMs,
    bool connectionEstablished);
