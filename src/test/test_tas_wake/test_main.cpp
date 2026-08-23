// Native tests: wake-token auth + sleep budget + watchdog verdicts.
#include "TasWake.h"
#include "TasFailsafe.h"
#include "TasSession.h"
#include <stdio.h>
#include <string.h>

static int failures = 0;
#define CHECK(cond, name) do { \
    if (!(cond)) { printf("FAIL %s\n", name); failures++; } \
    else { printf("PASS %s\n", name); } \
} while(0)

int main()
{
    TasSessionCtx_s session;
    uint8_t uid[6] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
    TasSessionDeriveMaster(&session, uid, 0xCAFEF00D);

    // --- token roundtrip ---
    {
        uint8_t wakeKey[TAS_KEY_LEN], tokA[TAS_WAKE_TOKEN_LEN], tokB[TAS_WAKE_TOKEN_LEN];
        TasWakeDeriveKey(session.masterKey, wakeKey);
        TasWakeMakeToken(wakeKey, tokA);
        CHECK(TasWakeTokenValid(wakeKey, tokA), "wake: valid token accepted");

        // deterministic
        TasWakeMakeToken(wakeKey, tokB);
        CHECK(memcmp(tokA, tokB, TAS_WAKE_TOKEN_LEN) == 0, "wake: token deterministic");

        // foreign battery (different bind) rejected
        TasSessionCtx_s other;
        uint8_t uid2[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
        TasSessionDeriveMaster(&other, uid2, 0x11223344);
        uint8_t otherKey[TAS_KEY_LEN];
        TasWakeDeriveKey(other.masterKey, otherKey);
        CHECK(!TasWakeTokenValid(otherKey, tokA), "wake: foreign key rejected");
        // domain separation: wake key differs from RC keystream key
        uint8_t rcKey[TAS_KEY_LEN];
        TasSessionGetKeystreamKey(&session, rcKey);
        CHECK(memcmp(rcKey, wakeKey, TAS_KEY_LEN) != 0, "wake: domain separated from RC key");
    }

    // --- endurance: month requirement ---
    {
        uint32_t days3000 = TasWakeEstimateDays(3000, 3000, 150);
        printf("INFO 3000mAh @3s poll => %u days\n", days3000);
        CHECK(days3000 > 30, "wake: 3000mAh lasts > 30 days");
        uint32_t days5000 = TasWakeEstimateDays(5000, 3000, 150);
        printf("INFO 5000mAh @3s poll => %u days\n", days5000);
        CHECK(days5000 > days3000, "wake: bigger pack lasts longer");
        // tighter polling costs endurance but still viable
        uint32_t d1s = TasWakeEstimateDays(3000, 1000, 150);
        CHECK(d1s > 10 && d1s < days3000, "wake: 1s poll tradeoff sane");
    }

    // --- watchdog ---
    CHECK(TasWatchdogEvaluate(10000, 9900, true) == TAS_WD_OK, "wd: healthy link ok");
    CHECK(TasWatchdogEvaluate(10000, 9000, false) == TAS_WD_LINK_LOST, "wd: bench loss no action");
    CHECK(TasWatchdogEvaluate(10000, 9400, true) == TAS_WD_RADIO_WEDGE, "wd: silence>500ms while connected");
    CHECK(TasWatchdogEvaluate(10000, 8000, false) == TAS_WD_RADIO_WEDGE, "wd: was-connected wedge after 2s");
    CHECK(TasWatchdogEvaluate(200000, 10000, true) == TAS_WD_RADIO_WEDGE, "wd: long stall while connected");

    printf(failures ? "\n== FAILED (%d) ==\n" : "\n== PASSED ==\n", failures);
    return failures ? 1 : 0;
}
