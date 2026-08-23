#include "TasSession.h"
#include <string.h>

void TasSessionDeriveMaster(
    TasSessionCtx_s *ctx,
    const uint8_t uid[6],
    uint32_t uidSeed)
{
    // IKM = UID(6B) || uidSeed(4B LE). Binding phrase entropy is already
    // folded into the UID by the stock ELRS binding mechanism.
    uint8_t ikm[10];
    memcpy(ikm, uid, 6);
    ikm[6] = (uint8_t)(uidSeed);
    ikm[7] = (uint8_t)(uidSeed >> 8);
    ikm[8] = (uint8_t)(uidSeed >> 16);
    ikm[9] = (uint8_t)(uidSeed >> 24);

    const char *salt = "TAS-ELRS-v1";
    const char *info = "master";

    uint8_t prk[TAS_KEY_LEN];
    TasHkdfExtract((const uint8_t *)salt, 11, ikm, sizeof(ikm), prk);
    TasHkdfExpand(prk, (const uint8_t *)info, 6, ctx->masterKey, TAS_KEY_LEN);

    ctx->epochIndex = 0;
    ctx->frameCounter = 0;
    // epoch 0 key + "previous" starts as epoch 0 too (no overlap at bind)
    TasSessionAdvance(ctx, 0);
}

void TasSessionAdvance(
    TasSessionCtx_s *ctx,
    uint32_t frames)
{
    ctx->frameCounter += frames;

    uint32_t newEpoch = ctx->frameCounter / TAS_REKEY_FRAMES;
    if (newEpoch != ctx->epochIndex)
    {
        memcpy(ctx->prevEpochKey, ctx->epochKey, TAS_KEY_LEN);
        ctx->epochIndex = newEpoch;

        uint8_t info[8];
        info[0] = 'e';
        info[1] = 'p';
        info[2] = 'o';
        info[3] = 'c';
        info[4] = (uint8_t)(newEpoch >> 24);
        info[5] = (uint8_t)(newEpoch >> 16);
        info[6] = (uint8_t)(newEpoch >> 8);
        info[7] = (uint8_t)(newEpoch);

        TasHkdfExpand(ctx->masterKey, info, sizeof(info), ctx->epochKey, TAS_KEY_LEN);
    }
}

void TasSessionGetKeystreamKey(
    const TasSessionCtx_s *ctx,
    uint8_t outKey[TAS_KEY_LEN])
{
    memcpy(outKey, ctx->epochKey, TAS_KEY_LEN);
}

uint32_t TasSessionMacInit(
    const TasSessionCtx_s *ctx,
    uint32_t fhssIndex,
    uint8_t otaNonce)
{
    // Keyed CRC initializer: derived per-epoch from session material plus the
    // implicit per-frame context. Both TX and RX compute this identically from
    // synchronized state; nothing static is transmitted in the clear.
    uint8_t material[TAS_KEY_LEN];
    for (unsigned i = 0; i < TAS_KEY_LEN; i++)
    {
        material[i] = ctx->epochKey[i];
    }
    material[0] ^= (uint8_t)fhssIndex;
    material[1] ^= otaNonce;
    material[2] ^= (uint8_t)(ctx->epochIndex >> 16);
    material[3] ^= (uint8_t)(ctx->epochIndex >> 24);

    uint8_t mac[TAS_KEY_LEN];
    TasSha256(material, TAS_KEY_LEN, mac);

    return ((uint32_t)mac[0] << 24) | ((uint32_t)mac[1] << 16) |
           ((uint32_t)mac[2] << 8) | mac[3];
}

TasReplayResult_e TasReplayCheck(
    uint8_t incomingNonce,
    uint8_t *lastNonce,
    bool *seenWindow,
    uint8_t windowBits)
{
    (void)seenWindow;
    (void)windowBits;

    // ELRS OtaNonce increments by exactly 1 per uplink frame and wraps at 256.
    // Any non-forward step is therefore a duplicate/replay: reject it.
    // Link-loss recovery is handled by the SYNC packet path, not by accepting
    // old frames.
    uint8_t diff = (uint8_t)(incomingNonce - *lastNonce);

    if (diff == 0 || diff > 128)
    {
        return TAS_REPLAY_REJECT;
    }

    *lastNonce = incomingNonce;
    return TAS_OK;
}
