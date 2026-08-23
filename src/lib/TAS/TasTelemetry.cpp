#include "TasTelemetry.h"

static uint8_t milJamStatus;
static uint8_t milNoisyFraction;
static uint8_t milWdReinits;

void TasTelemetrySet(uint8_t jamStatus, uint8_t noisyFraction)
{
    milJamStatus = jamStatus;
    milNoisyFraction = noisyFraction;
}

void TasTelemetryCountReinit(void)
{
    ++milWdReinits; // wraps naturally at 255
}

uint8_t TasTelemetryBuildPayload(uint8_t out[7])
{
    out[0] = milJamStatus;
    out[1] = milNoisyFraction;
    out[2] = milWdReinits;
    // epoch placeholder (Faza B)
    out[3] = 0;
    out[4] = 0;
    out[5] = 0;
    out[6] = 1;
    return 7;
}
