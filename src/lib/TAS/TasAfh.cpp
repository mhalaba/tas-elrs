#include "TasAfh.h"
#include "TasCrypto.h"
#include <string.h>

TasJamStatus_e TasAfhJamFromLink(uint8_t lqRaw, uint8_t rssiAbs)
{
    // Saturation signature: the receiver hears a LOT of energy yet loses
    // frames — characteristic of barrage/jamming, not of range falloff
    // (range presents as weak RSSI with graceful LQ decay).
    const uint8_t STRONG = 55;   // |RSSI| dB: closer than ~55 dB loss
    const uint8_t LQ_BAD = 30;

    if (lqRaw < LQ_BAD && rssiAbs > STRONG)
    {
        return TAS_JAM_WIDEBAND;
    }
    if (lqRaw < 60 && rssiAbs > STRONG)
    {
        return TAS_JAM_SUSPECT;
    }
    return TAS_JAM_CLEAN;
}

void TasAfhInit(TasAfhCtx_s *ctx, uint8_t channelCount)
{
    memset(ctx, 0, sizeof(*ctx));
    (void)channelCount;
}

void TasAfhNote(TasAfhCtx_s *ctx, uint8_t channelIdx, int16_t rssiRaw)
{
    if (channelIdx >= TAS_MAX_CHANNELS)
    {
        return;
    }
    // RSSI register values are negative dBm scaled; clamp to unsigned domain
    // so higher == noisier for the EMA.
    int32_t v = rssiRaw;
    if (v < 0)
    {
        v = 0;
    }
    if (v > 255)
    {
        v = 255;
    }
    uint16_t sample = (uint16_t)v << TAS_EMA_SHIFT;

    uint16_t *ema = &ctx->noiseEma[channelIdx];
    if (ctx->sampleCount[channelIdx] == 0)
    {
        *ema = sample;
    }
    else
    {
        // EMA with alpha ~= 1/8
        *ema = (uint16_t)(*ema - (*ema >> 3) + (sample >> 3));
    }
    if (ctx->sampleCount[channelIdx] < 0xFFFF)
    {
        ctx->sampleCount[channelIdx]++;
    }
}

TasJamStatus_e TasAfhClassify(const TasAfhCtx_s *ctx, uint8_t channelCount)
{
    if (channelCount > TAS_MAX_CHANNELS)
    {
        channelCount = TAS_MAX_CHANNELS;
    }

    // Need a minimum number of observations to speak
    uint32_t totalSamples = 0;
    for (uint8_t i = 0; i < channelCount; i++)
    {
        totalSamples += ctx->sampleCount[i];
    }
    if (totalSamples < (uint32_t)channelCount * 2)
    {
        return TAS_JAM_CLEAN;
    }

    // Mean of per-channel EMA
    uint32_t sum = 0;
    uint16_t maxEma = 0;
    for (uint8_t i = 0; i < channelCount; i++)
    {
        sum += ctx->noiseEma[i];
        if (ctx->noiseEma[i] > maxEma)
        {
            maxEma = ctx->noiseEma[i];
        }
    }
    uint16_t mean = (uint16_t)(sum / channelCount);

    // Absolute noise floor per channel
    const uint16_t FLOOR = (uint16_t)TAS_NOISE_THRESHOLD_DBM << TAS_EMA_SHIFT;
    uint8_t noisyCount = 0;
    for (uint8_t i = 0; i < channelCount; i++)
    {
        if (ctx->noiseEma[i] > FLOOR)
        {
            noisyCount++;
        }
    }

    // Temporal delta of band mean => energy moving through band => sweep
    uint16_t delta = mean > ctx->prevSpikeEma ? mean - ctx->prevSpikeEma : ctx->prevSpikeEma - mean;
    ((TasAfhCtx_s *)ctx)->prevSpikeEma = mean;

    if (noisyCount > channelCount / 2)
    {
        return TAS_JAM_WIDEBAND;
    }
    if (delta > (mean >> 1) && maxEma > FLOOR)
    {
        return TAS_JAM_SWEEP;
    }
    if (noisyCount > 0)
    {
        return TAS_JAM_SUSPECT;
    }
    return TAS_JAM_CLEAN;
}

uint8_t TasAfhNoisyFraction(const TasAfhCtx_s *ctx, uint8_t channelCount)
{
    if (channelCount > TAS_MAX_CHANNELS)
    {
        channelCount = TAS_MAX_CHANNELS;
    }
    uint32_t sum = 0;
    for (uint8_t i = 0; i < channelCount; i++)
    {
        sum += ctx->noiseEma[i];
    }
    uint16_t mean = (uint16_t)(sum / channelCount);
    uint8_t hot = 0;
    for (uint8_t i = 0; i < channelCount; i++)
    {
        if (ctx->noiseEma[i] > mean + (mean >> 1))
        {
            hot++;
        }
    }
    return (uint8_t)((hot * 255u) / channelCount);
}

uint8_t TasAfhEpochPrune(
    uint32_t seed,
    uint32_t epoch,
    uint8_t channelCount,
    uint8_t *outMask,
    uint8_t maxPrune)
{
    memset(outMask, 0, (channelCount + 7) / 8);

    // PRF stream from ChaCha20 keyed by session seed mixed with epoch.
    // Deterministic across TX/RX; unpredictable to an observer who does not
    // know the binding material.
    uint8_t key[TAS_KEY_LEN];
    uint8_t nonce[TAS_NONCE_LEN] = {'p', 'r', 'n', 'g', 0, 0, 0, 0, 0, 0, 0, 0};
    nonce[4] = (uint8_t)(seed);
    nonce[5] = (uint8_t)(seed >> 8);
    nonce[6] = (uint8_t)(seed >> 16);
    nonce[7] = (uint8_t)(seed >> 24);
    nonce[8] = (uint8_t)(epoch);
    nonce[9] = (uint8_t)(epoch >> 8);
    nonce[10] = (uint8_t)(epoch >> 16);
    nonce[11] = (uint8_t)(epoch >> 24);
    TasSha256(&nonce[4], 8, key);

    // Fisher-Yates over channel indexes using PRF bytes; prune the tail.
    uint8_t idx[TAS_MAX_CHANNELS];
    for (uint8_t i = 0; i < channelCount; i++)
    {
        idx[i] = i;
    }

    uint8_t ks[64];
    uint32_t blockCounter = 0;
    uint8_t ksPos = 64;
    uint8_t pruned = 0;

    for (uint8_t i = channelCount - 1; i > 0; i--)
    {
        if (ksPos >= 64)
        {
            TasChaCha20Block(key, blockCounter++, nonce, ks);
            ksPos = 0;
        }
        uint16_t r = ks[ksPos++];
        uint8_t j = (uint8_t)((r * (i + 1)) >> 8); // 0..i
        uint8_t tmp = idx[i];
        idx[i] = idx[j];
        idx[j] = tmp;
    }

    // Prune only makes sense when the band has enough channels to lose a
    // quarter of them; tiny domains (e.g. 3ch 433MHz) skip pruning entirely.
    if (channelCount < 12)
    {
        return 0;
    }

    uint8_t limit = maxPrune < channelCount / 4 ? maxPrune : channelCount / 4;
    if (limit > channelCount - 8)
    {
        limit = channelCount - 8; // keep at least 8 usable channels
    }

    for (uint8_t k = 0; k < limit; k++)
    {
        uint8_t ch = idx[channelCount - 1 - k];
        outMask[ch >> 3] |= (1 << (ch & 7));
        pruned++;
    }

    return pruned;
}

void TasAfhRemapSequence(
    uint8_t *sequence,
    uint16_t seqLen,
    uint8_t channelCount,
    uint8_t syncChannel,
    const uint8_t *mask)
{
    // Build the sorted list of surviving channels (excludes sync channel:
    // it must stay reachable and is never pruned).
    uint8_t allowed[TAS_MAX_CHANNELS];
    uint8_t allowedCount = 0;
    for (uint8_t c = 0; c < channelCount; c++)
    {
        if (c == syncChannel)
        {
            continue;
        }
        if (!(mask[c >> 3] & (1 << (c & 7))))
        {
            allowed[allowedCount++] = c;
        }
    }
    if (allowedCount == 0 || channelCount == 0)
    {
        return;
    }

    for (uint16_t p = 0; p < seqLen; p++)
    {
        if (p % channelCount == 0)
        {
            continue; // SYNC slot
        }
        uint8_t ch = sequence[p];
        if (mask[ch >> 3] & (1 << (ch & 7)))
        {
            // Deterministic round-robin over survivors; block index varies
            // the pick so consecutive replacements spread across the band.
            uint16_t block = p / channelCount;
            sequence[p] = allowed[(p * 7u + block * 13u) % allowedCount];
        }
    }
}
