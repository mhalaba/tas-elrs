#pragma once

#include <stdint.h>
#include "OTA.h"

// TAS hardening glue between the OTA framing layer and the TAS crypto core.
// All functions are no-ops (static inline) when TAS_HARDENING is not defined.

#ifdef TAS_HARDENING

void TasOtaInitFromUid();
void TasOtaAdvanceFrame();
uint16_t TasOtaMacInitializer(uint8_t nonceValidator);
void TasOtaCryptRcData(OTA_Packet_s *otaPktPtr);
bool TasOtaReplayAccept(uint8_t incomingNonce, bool isSyncPacket);

#else

static inline void TasOtaInitFromUid() {}
static inline void TasOtaAdvanceFrame() {}
static inline uint16_t TasOtaMacInitializer(uint8_t nonceValidator) { (void)nonceValidator; return OtaCrcInitializer; }
static inline void TasOtaCryptRcData(OTA_Packet_s *) {}
static inline bool TasOtaReplayAccept(uint8_t, bool) { return true; }

#endif
