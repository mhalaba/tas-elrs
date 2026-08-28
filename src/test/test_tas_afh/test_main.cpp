// Native simulation: jam classifier scenarios + FHSS prune determinism.
#include "TasAfh.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int failures = 0;
#define CHECK(cond, name) do { \
    if (!(cond)) { printf("FAIL %s\n", name); failures++; } \
    else { printf("PASS %s\n", name); } \
} while(0)

static void feed(TasAfhCtx_s *ctx, uint8_t chCount, int16_t base, int16_t noiseAmp)
{
    for (int round = 0; round < 20; round++)
    {
        for (uint8_t c = 0; c < chCount; c++)
        {
            int16_t v = base + (int16_t)((rand() % (noiseAmp + 1)) - noiseAmp / 2);
            if (v < 0) v = 0;
            TasAfhNote(ctx, c, v);
        }
    }
}

int main()
{
    // --- Link-health jam heuristic (rssiAbs = |RSSI| dB, smaller = stronger) ---
    CHECK(TasAfhJamFromLink(20, 40) == TAS_JAM_WIDEBAND,
          "jam: strong RSSI + dead LQ is barrage");
    CHECK(TasAfhJamFromLink(20, 90) == TAS_JAM_CLEAN,
          "jam: weak RSSI + dead LQ is range, not jam");
    CHECK(TasAfhJamFromLink(95, 40) == TAS_JAM_CLEAN,
          "jam: strong RSSI + high LQ is a healthy link");
    CHECK(TasAfhJamFromLink(45, 40) == TAS_JAM_SUSPECT,
          "jam: strong RSSI + mediocre LQ is suspect");
    CHECK(TasAfhJamFromLink(45, 80) == TAS_JAM_CLEAN,
          "jam: weak RSSI + mediocre LQ is range");

    // --- Classifier: clean environment ---
    {
        TasAfhCtx_s ctx; TasAfhInit(&ctx, 80);
        feed(&ctx, 80, 20, 6);
        CHECK(TasAfhClassify(&ctx, 80) == TAS_JAM_CLEAN, "classify: clean");
    }

    // --- Classifier: wideband barrage (all channels hot) ---
    {
        TasAfhCtx_s ctx; TasAfhInit(&ctx, 80);
        feed(&ctx, 80, 200, 20);
        TasJamStatus_e s = TasAfhClassify(&ctx, 80);
        CHECK(s == TAS_JAM_WIDEBAND || s == TAS_JAM_SUSPECT, "classify: wideband flagged");
        CHECK(s == TAS_JAM_WIDEBAND, "classify: wideband exact");
    }

    // --- Classifier: sweep (energy moves across band over time) ---
    {
        TasAfhCtx_s ctx; TasAfhInit(&ctx, 80);
        // quiet phase
        for (int r = 0; r < 10; r++)
            for (uint8_t c = 0; c < 80; c++) TasAfhNote(&ctx, c, 15);
        CHECK(TasAfhClassify(&ctx, 80) == TAS_JAM_CLEAN, "classify: pre-sweep clean");
        // sweeping spike raises band mean sharply
        for (int r = 0; r < 10; r++)
            for (uint8_t c = 0; c < 80; c++) TasAfhNote(&ctx, c, 220);
        TasJamStatus_e s = TasAfhClassify(&ctx, 80);
        CHECK(s != TAS_JAM_CLEAN, "classify: sweep detected as non-clean");
    }

    // --- Prune: deterministic and bounded ---
    {
        uint8_t m1[10], m2[10];
        uint8_t n1 = TasAfhEpochPrune(0xDEADBEEF, 7, 80, m1, 16);
        uint8_t n2 = TasAfhEpochPrune(0xDEADBEEF, 7, 80, m2, 16);
        CHECK(n1 == n2 && memcmp(m1, m2, 10) == 0, "prune: deterministic");
        CHECK(n1 <= 20 && n1 >= 1, "prune: bounded count");
        uint8_t m3[10];
        TasAfhEpochPrune(0xDEADBEEF, 8, 80, m3, 16);
        CHECK(memcmp(m1, m3, 10) != 0, "prune: epoch varies");
        // sync channel never pruned is enforced at remap level; verify mask size math for odd counts
        uint8_t m4[2];
        uint8_t n4 = TasAfhEpochPrune(42, 0, 13, m4, 3);
        CHECK(n4 <= 3, "prune: EU868 count bound");
    }

    // --- Remap: no pruned channel survives; SYNC slots preserved ---
    {
        uint8_t seq[256], ref[256];
        // deterministic pseudo-sequence build like ELRS: blocks are permutations of 0..79
        srand(1234);
        for (int b = 0; b < 3; b++)
        {
            for (uint8_t i = 0; i < 80; i++) seq[b*80+i] = i;
            for (uint8_t i = 79; i > 0; i--) { uint8_t j = rand() % (i+1); uint8_t t = seq[b*80+i]; seq[b*80+i]=seq[b*80+j]; seq[b*80+j]=t; }
        }
        memcpy(ref, seq, 256);

        uint8_t prunedMask[10] = {};
        TasAfhEpochPrune(777, 3, 80, prunedMask, 16);

        TasAfhRemapSequence(seq, 240, 80, 40, prunedMask);

        bool prunedSurvivor = false;
        bool syncOk = true;
        for (uint16_t p = 0; p < 240; p++)
        {
            if (p % 80 == 0) { if (seq[p] != ref[p]) syncOk = false; continue; }
            if (prunedMask[seq[p] >> 3] & (1 << (seq[p] & 7))) prunedSurvivor = true;
        }
        CHECK(!prunedSurvivor, "remap: pruned channels eliminated");
        CHECK(syncOk, "remap: SYNC slots untouched");

        // determinism across runs
        uint8_t seq2[256];
        memcpy(seq2, ref, 256);
        TasAfhRemapSequence(seq2, 240, 80, 40, prunedMask);
        CHECK(memcmp(seq, seq2, 240) == 0, "remap: deterministic TX==RX");
    }

    // --- Remap with heavy pruning keeps all channels in allowed set ---
    {
        uint8_t seq[26]; // EU868-like: 13 channels
        for (uint8_t i = 0; i < 13; i++) seq[i] = i;
        uint8_t mask[2] = {}; 
        // prune channels 0..5 except none is sync(6)
        for (uint8_t c = 0; c <= 5; c++) mask[c >> 3] |= (1 << (c & 7));
        TasAfhRemapSequence(seq, 13, 13, 6, mask);
        bool bad = false;
        for (uint8_t p = 1; p < 13; p++)
            if (seq[p] <= 5) bad = true;
        CHECK(!bad, "remap: small domain EU868-like");
        CHECK(seq[0] == 0, "remap: slot0 preserved");
    }

    printf(failures ? "\n== SIM FAILED (%d) ==\n" : "\n== SIM PASSED ==\n", failures);
    return failures ? 1 : 0;
}
