// Native integration: real OTA.cpp + TasOta.cpp + TAS crypto.
// Roundtrip TX->RX, tamper, cross-slot injection, replay gate, SYNC passthrough.
#include "OTA.h"
#include "TasOta.h"
#include <stdio.h>
#include "FHSS.h"
#include <string.h>

static int failures = 0;
#define CHECK(cond, name) do { \
    if (!(cond)) { printf("FAIL %s\n", name); failures++; } \
    else { printf("PASS %s\n", name); } \
} while(0)

// FHSS test double: TasOta reads only sequence globals + current index.
uint8_t FHSSsequence[FHSS_SEQUENCE_LEN];
uint8_t FHSSsequence_DualBand[FHSS_SEQUENCE_LEN];
uint8_t volatile FHSSptr = 0;
bool FHSSusePrimaryFreqBand = true;
bool FHSSuseDualBand = false;
uint16_t primaryBandCount = 240;
uint16_t secondaryBandCount = 80;

int main()
{
    for (unsigned i = 0; i < FHSS_SEQUENCE_LEN; i++)
    {
        FHSSsequence[i] = (uint8_t)((i * 37) % 80);
    }

#ifdef TAS_HARDENING
    OtaUpdateCrcInitFromUid();
#endif
    OtaUpdateSerializers(smHybridOr16ch, OTA4_PACKET_SIZE);

#ifdef TAS_HARDENING
    uint32_t channels[16];
    for (unsigned i = 0; i < 16; i++) channels[i] = 988 + i * 27;

    // --- roundtrip ---
    {
        OTA_Packet_s txPkt, rxPkt;
        memset(&txPkt, 0, sizeof(txPkt));
        OtaNonce = 42;
        OtaPackChannelData(&txPkt, channels, false);
        OTA_Packet_s plainBeforeCrc = txPkt; // packed, not yet encrypted
        OtaGeneratePacketCrc(&txPkt);

        rxPkt = txPkt;
        bool ok = OtaValidatePacketCrc(&rxPkt);
        CHECK(ok, "tas: rcdata validates");
        CHECK(memcmp(rxPkt.std.rc.ch.raw, plainBeforeCrc.std.rc.ch.raw, 5) == 0,
              "tas: payload decrypted matches plaintext");
        CHECK(rxPkt.std.rc.switches == plainBeforeCrc.std.rc.switches, "tas: switches match");

        bool encDiff = memcmp(plainBeforeCrc.std.rc.ch.raw, txPkt.std.rc.ch.raw, 6) != 0;
        CHECK(encDiff, "tas: on-air payload is ciphertext");
    }

    // --- tamper ---
    {
        OTA_Packet_s txPkt, rxPkt;
        memset(&txPkt, 0, sizeof(txPkt));
        OtaNonce = 100;
        OtaPackChannelData(&txPkt, channels, false);
        OtaGeneratePacketCrc(&txPkt);
        rxPkt = txPkt;
        rxPkt.std.rc.ch.raw[0] ^= 0x01;
        CHECK(!OtaValidatePacketCrc(&rxPkt), "tas: tampered frame rejected");
    }

    // --- cross-slot injection ---
    {
        OTA_Packet_s txPkt, rxPkt;
        memset(&txPkt, 0, sizeof(txPkt));
        OtaNonce = 55;
        OtaPackChannelData(&txPkt, channels, false);
        OtaGeneratePacketCrc(&txPkt);
        rxPkt = txPkt;
        OtaNonce = 56;
        CHECK(!OtaValidatePacketCrc(&rxPkt), "tas: cross-slot injection rejected");
    }

    // --- duplicate delivery / replay gate ---
    CHECK(TasOtaReplayAccept(60, false), "gate: first accept");
    CHECK(!TasOtaReplayAccept(60, false), "gate: duplicate rejected");
    CHECK(TasOtaReplayAccept(61, false), "gate: forward ok");
    CHECK(TasOtaReplayAccept(62, true), "gate: sync adopts nonce");

    // --- SYNC passthrough in foreign slot ---
    {
        OTA_Packet_s syncPkt;
        memset(&syncPkt, 0, sizeof(syncPkt));
        syncPkt.std.type = PACKET_TYPE_SYNC;
        syncPkt.std.sync.fhssIndex = 5;
        syncPkt.std.sync.nonce = 200;
        OtaGeneratePacketCrc(&syncPkt);
        OtaNonce = 201;
        CHECK(OtaValidatePacketCrc(&syncPkt), "tas: sync validates in foreign slot");
    }

    // --- property: every single-bit flip in the payload region is rejected ---
    {
        OTA_Packet_s base, ref, rx;
        memset(&base, 0, sizeof(base));
        OtaNonce = 77;
        OtaPackChannelData(&base, channels, false);
        ref = base;
        OtaGeneratePacketCrc(&ref);

        unsigned rejected = 0, total = 0;
        for (unsigned byte = 1; byte < 7; ++byte)          // encrypted region (OTA4)
        {
            for (unsigned bit = 0; bit < 8; ++bit)
            {
                rx = ref;
                ((uint8_t *)&rx)[byte] ^= (uint8_t)(1 << bit);
                if (!OtaValidatePacketCrc(&rx)) rejected++;
                total++;
            }
        }
        CHECK(rejected == total, "tas: all 48 payload bit-flips rejected");

        // header type byte flip must not produce a valid DATA frame either
        for (unsigned bit = 2; bit < 8; ++bit)
        {
            rx = ref;
            ((uint8_t *)&rx)[0] ^= (uint8_t)(1 << bit);
            if (!OtaValidatePacketCrc(&rx)) rejected++;
            total++;
        }
        CHECK(total == 54 && rejected == total, "tas: header-bit flips also rejected");
    }

    // --- OTA8 fullres ---
    {
        OtaUpdateSerializers(smWideOr8ch, OTA8_PACKET_SIZE);
        OTA_Packet_s txPkt, rxPkt;
        memset(&txPkt, 0, sizeof(txPkt));
        OtaNonce = 90;
        OtaPackChannelData(&txPkt, channels, false);
        OtaGeneratePacketCrc(&txPkt);
        rxPkt = txPkt;
        CHECK(OtaValidatePacketCrc(&rxPkt), "tas: ota8 validates");
        OtaUpdateSerializers(smHybridOr16ch, OTA4_PACKET_SIZE);
    }
#else
    // stock regression: pack -> crc -> validate must still work
    {
        OTA_Packet_s txPkt, rxPkt;
        memset(&txPkt, 0, sizeof(txPkt));
        uint32_t ch[16];
        for (unsigned i = 0; i < 16; i++) ch[i] = 988 + i * 27;
        OtaNonce = 42;
        OtaPackChannelData(&txPkt, ch, false);
        OtaGeneratePacketCrc(&txPkt);
        rxPkt = txPkt;
        CHECK(OtaValidatePacketCrc(&rxPkt), "stock: rcdata validates");
        CHECK(memcmp(rxPkt.std.rc.ch.raw, txPkt.std.rc.ch.raw, 5) == 0, "stock: payload clear");
    }
#endif

    printf(failures ? "\n== INTEGRATION FAILED (%d) ==\n" : "\n== INTEGRATION PASSED ==\n", failures);
    return failures ? 1 : 0;
}
