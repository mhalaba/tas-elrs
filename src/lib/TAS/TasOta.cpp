#include "TasOta.h"

#ifdef TAS_HARDENING

#include "TasCrypto.h"
#include "TasSession.h"
#include "FHSS.h"
#include <string.h>

// v1 session model:
//  - One static link key derived from the binding secret (HKDF over UID).
//    In-flight rotation is deferred to Faza B: OTA8 SYNC packets carry four
//    free bytes which will host an epoch counter, allowing drift-free key
//    rotation. Rotating without an air-interface epoch field desynchronizes
//    TX/RX keys permanently after any packet loss burst.
//  - Keystream reuse bound: (channel, nonce) pairs recur at worst every
//    256*freqCount frames (freqCount >= 3 => >= 768). Quantified residual
//    risk documented in TAS_README.md.
static TasSessionCtx_s milSession;
static uint8_t milLastNonce = 0;

void TasOtaInitFromUid()
{
    milLastNonce = 0;
    TasSessionDeriveMaster(&milSession, UID, OtaGetUidSeed());
}

void TasOtaAdvanceFrame()
{
    TasSessionAdvance(&milSession, 1);
}

uint16_t TasOtaMacInitializer(uint8_t nonceValidator)
{
    // Keyed per-frame check value bound to channel + frame nonce. Replaces
    // the static UID-derived OtaCrcInitializer for data frames so captured
    // frames cannot be replayed into other slots nor forged offline.
    uint8_t ch = 0;
    if (!FHSSuseDualBand)
    {
        ch = FHSSsequence[FHSSgetCurrIndex()];
    }
    else
    {
        ch = FHSSsequence_DualBand[FHSSgetCurrIndex()];
    }
    uint32_t mac = TasSessionMacInit(&milSession, ch, nonceValidator);
    return (uint16_t)(mac ^ (mac >> 16));
}

// Symmetric keystream transform over the RC payload region. Type bits (low
// two bits of byte 0) and CRC fields stay clear; everything between is
// ciphertext. SYNC packets are never encrypted so resync after total loss
// stays possible.
void TasOtaCryptRcData(OTA_Packet_s *otaPktPtr)
{
    if (otaPktPtr->std.type != PACKET_TYPE_RCDATA)
    {
        return;
    }

    uint8_t nonce[TAS_NONCE_LEN];
    nonce[0] = 'M'; nonce[1] = 'R'; nonce[2] = 'C'; nonce[3] = 'D';
    nonce[4] = 0; nonce[5] = 0;
    nonce[6] = 0; nonce[7] = 1; // epoch placeholder (Faza B)
    nonce[8] = OtaNonce;
    nonce[9] = !FHSSuseDualBand ? FHSSsequence[FHSSgetCurrIndex()]
                                : FHSSsequence_DualBand[FHSSgetCurrIndex()];
    nonce[10] = 0x4D; // domain separation constant
    nonce[11] = OTA_VERSION_ID;

    uint8_t key[TAS_KEY_LEN];
    TasSessionGetKeystreamKey(&milSession, key);

    uint8_t *payload = ((uint8_t *)otaPktPtr) + 1;
    size_t len = OtaIsFullRes ? (OTA8_PACKET_SIZE - 3)   // rc header + channels
                              : (OTA4_PACKET_SIZE - 2);  // channels + switches

    uint32_t counter = ((uint32_t)OTA_VERSION_ID << 24) | 0x00524344; // 'RCD'
    TasChaCha20Xor(key, counter, nonce, payload, payload, len);

    memset(key, 0, sizeof(key));
    memset(nonce, 0, sizeof(nonce));
}

bool TasOtaReplayAccept(uint8_t incomingNonce, bool isSyncPacket)
{
    if (isSyncPacket)
    {
        // SYNC carries the authoritative phase; adopt it wholesale.
        milLastNonce = incomingNonce;
        return true;
    }
    return TasReplayCheck(incomingNonce, &milLastNonce, nullptr, 0) != TAS_REPLAY_REJECT;
}

#endif // TAS_HARDENING
