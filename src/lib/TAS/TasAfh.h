#pragma once

#include <stdint.h>

#define TAS_MAX_CHANNELS 80

// Fixed-point EMA: value stored as <<8
#define TAS_EMA_SHIFT 8
#ifndef TAS_NOISE_THRESHOLD_DBM
// RSSI register units below which channel counts as noisy (tunable)
#define TAS_NOISE_THRESHOLD_DBM 128 // raw register scale (half-dB typical)
#endif

typedef enum {
    TAS_JAM_CLEAN = 0,
    TAS_JAM_SUSPECT = 1,
    TAS_JAM_WIDEBAND = 2,
    TAS_JAM_SWEEP = 3
} TasJamStatus_e;

typedef struct {
    uint16_t noiseEma[TAS_MAX_CHANNELS]; // <<8 fixed point
    uint16_t sampleCount[TAS_MAX_CHANNELS];
    uint16_t prevSpikeEma;               // previous classification cycle mean
} TasAfhCtx_s;

// Classify jam state from link health (pure function, natively testable).
// Strong-but-lossy signal is the saturation signature of an active barrage;
// weak-and-lossy is just range. lqRaw: 0-100, rssiAbs: |RSSI| in dB.
TasJamStatus_e TasAfhJamFromLink(uint8_t lqRaw, uint8_t rssiAbs);

void TasAfhInit(TasAfhCtx_s *ctx, uint8_t channelCount);

// Record one instantaneous noise-floor observation for a channel.
void TasAfhNote(TasAfhCtx_s *ctx, uint8_t channelIdx, int16_t rssiRaw);

// Classify current RF environment from accumulated per-channel noise.
TasJamStatus_e TasAfhClassify(const TasAfhCtx_s *ctx, uint8_t channelCount);

// Fraction (0-255) of channels currently considered noisy.
uint8_t TasAfhNoisyFraction(const TasAfhCtx_s *ctx, uint8_t channelCount);

// Deterministic per-epoch channel prune mask derived from session material.
// Same (seed, epoch, count) on TX and RX => identical mask => no desync.
// Returns number of pruned channels; outMask has channelCount bits.
uint8_t TasAfhEpochPrune(
    uint32_t seed,
    uint32_t epoch,
    uint8_t channelCount,
    uint8_t *outMask,
    uint8_t maxPrune);

// Rewrite an FHSS sequence in place so no pruned channel is ever visited.
// Each block of channelCount entries keeps its SYNC slot untouched; every
// entry carrying a pruned channel index is replaced deterministically from
// the surviving-channel list. TX and RX run this with identical inputs.
void TasAfhRemapSequence(
    uint8_t *sequence,
    uint16_t seqLen,
    uint8_t channelCount,
    uint8_t syncChannel,
    const uint8_t *mask);
