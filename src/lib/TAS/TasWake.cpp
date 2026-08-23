#include "TasWake.h"
#include <string.h>

void TasWakeDeriveKey(
    const uint8_t masterKey[TAS_KEY_LEN],
    uint8_t outWakeKey[TAS_KEY_LEN])
{
    const char *info = "wake";
    // Expand-only from the session master: cheap, deterministic, domain
    // separated from the RC keystream key.
    uint8_t prk[TAS_KEY_LEN];
    memcpy(prk, masterKey, TAS_KEY_LEN);
    TasHkdfExpand(prk, (const uint8_t *)info, 4, outWakeKey, TAS_KEY_LEN);
}

void TasWakeMakeToken(
    const uint8_t wakeKey[TAS_KEY_LEN],
    uint8_t outToken[TAS_WAKE_TOKEN_LEN])
{
    uint8_t nonce[TAS_NONCE_LEN] = {'W', 'K', 'T', '0', 0, 0, 0, 0, 0, 0, 0, 0};
    uint8_t stream[64];
    TasChaCha20Block(wakeKey, 1, nonce, stream);
    memcpy(outToken, stream, TAS_WAKE_TOKEN_LEN);
}

bool TasWakeTokenValid(
    const uint8_t wakeKey[TAS_KEY_LEN],
    const uint8_t candidate[TAS_WAKE_TOKEN_LEN])
{
    uint8_t expected[TAS_WAKE_TOKEN_LEN];
    TasWakeMakeToken(wakeKey, expected);
    bool ok = TasConstTimeEqual(expected, candidate, TAS_WAKE_TOKEN_LEN);

    // Wipe material from RAM as soon as the decision is made.
    memset(expected, 0, sizeof(expected));
    return ok;
}

uint32_t TasWakeEstimateDays(
    uint32_t battery_mAh,
    uint32_t pollMs,
    uint32_t deepSleepUa)
{
    if (pollMs == 0)
    {
        pollMs = TAS_WAKE_POLL_MS;
    }
    // average current in uA = scan duty share + deep sleep floor
    uint64_t dutyUa = (uint64_t)TAS_WAKE_SCAN_MS * TAS_WAKE_RADIO_MA * 1000ULL / pollMs;
    uint64_t avgUa = dutyUa + deepSleepUa;
    if (avgUa == 0)
    {
        return 0;
    }
    return (uint32_t)((uint64_t)battery_mAh * 1000ULL / avgUa / 24ULL);
}
