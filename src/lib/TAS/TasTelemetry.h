#pragma once

#include <stdint.h>
#include "TasAfh.h"

// TAS status exposed to the flight controller over CRSF frame 0x34.
// Payload layout (7 bytes, plain octets, no endianness traps):
//   [0] jamStatus   (TasJamStatus_e)
//   [1] noisyFraction (0-255)
//   [2] wdReinits   (radio watchdog reinit counter, wraps at 255)
//   [3..6] epochIndex placeholder for Faza B key rotation

void TasTelemetrySet(uint8_t jamStatus, uint8_t noisyFraction);
void TasTelemetryCountReinit(void);
uint8_t TasTelemetryBuildPayload(uint8_t out[7]);
