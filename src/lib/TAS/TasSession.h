#pragma once

#include <stdint.h>
#include "TasCrypto.h"

// Rekey policy: keystream counter space is (fhssChannel:8 | otaNonce:8) per
// epoch key. The (channel,nonce) pair repeats at worst every 256*freqCount
// frames; freqCount can be as low as 3 (433MHz domains) => period as low as
// 768 frames. TAS_REKEY_FRAMES MUST stay below that bound; default 512
// (~1s @ 500Hz) guarantees zero keystream reuse within an epoch.
#ifndef TAS_REKEY_FRAMES
#define TAS_REKEY_FRAMES 512
#endif

#define TAS_EPOCH_OVERLAP 32 // accept previous epoch key this many frames into a new epoch

typedef struct {
    uint8_t masterKey[TAS_KEY_LEN];   // HKDF(bindingPhrase || UID)
    uint8_t epochKey[TAS_KEY_LEN];    // current epoch key
    uint8_t prevEpochKey[TAS_KEY_LEN];// previous epoch key (overlap window)
    uint32_t epochIndex;
    uint32_t frameCounter;            // absolute frames since bind/sync
} TasSessionCtx_s;

typedef enum {
    TAS_OK = 0,
    TAS_REPLAY_REJECT = 1,
    TAS_RESYNC_ACCEPT = 2
} TasReplayResult_e;

void TasSessionDeriveMaster(
    TasSessionCtx_s *ctx,
    const uint8_t uid[6],
    uint32_t uidSeed);

void TasSessionAdvance(
    TasSessionCtx_s *ctx,
    uint32_t frames);

void TasSessionGetKeystreamKey(
    const TasSessionCtx_s *ctx,
    uint8_t outKey[TAS_KEY_LEN]);

uint32_t TasSessionMacInit(
    const TasSessionCtx_s *ctx,
    uint32_t fhssIndex,
    uint8_t otaNonce);

TasReplayResult_e TasReplayCheck(
    uint8_t incomingNonce,
    uint8_t *lastNonce,
    bool *seenWindow,
    uint8_t windowBits);
