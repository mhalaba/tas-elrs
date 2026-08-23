// Native audit test: RFC vectors + session/replay behaviour.
#include "TasCrypto.h"
#include "TasSession.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int failures = 0;
#define CHECK(cond, name) do { \
    if (!(cond)) { printf("FAIL %s\n", name); failures++; } \
    else { printf("PASS %s\n", name); } \
} while(0)

static void hex2bin(const char *hex, uint8_t *out, size_t len)
{
    for (size_t i = 0; i < len; i++)
    {
        unsigned v; sscanf(hex + i*2, "%2x", &v);
        out[i] = (uint8_t)v;
    }
}

int main()
{
    // --- SHA-256 FIPS vector "abc" ---
    {
        uint8_t out[32];
        TasSha256((const uint8_t *)"abc", 3, out);
        const char *expect = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
        uint8_t e[32]; hex2bin(expect, e, 32);
        CHECK(memcmp(out, e, 32) == 0, "SHA256(abc) FIPS vector");
    }

    // --- HMAC-SHA256 RFC 4231 TC2 ---
    {
        uint8_t out[32];
        TasHmacSha256((const uint8_t *)"Jefe", 4,
            (const uint8_t *)"what do ya want for nothing?", 28, out);
        const char *expect = "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843";
        uint8_t e[32]; hex2bin(expect, e, 32);
        CHECK(memcmp(out, e, 32) == 0, "HMAC-SHA256 RFC4231 TC2");
    }

    // --- HKDF-SHA256 RFC 5869 Test Case 1 ---
    {
        uint8_t ikm[22]; memset(ikm, 0x0b, 22);
        uint8_t salt[13]; for (unsigned i=0;i<13;i++) salt[i]=(uint8_t)i;
        uint8_t info[10]; for (unsigned i=0;i<10;i++) info[i]=(uint8_t)(0xf0+i);
        uint8_t prk[32], okm[42];
        TasHkdfExtract(salt, 13, ikm, 22, prk);
        TasHkdfExpand(prk, info, 10, okm, 42);
        const char *expect =
            "3cb25f25faacd57a90434f64d0362f2a2d2d0a90cf1a5a4c5db02d56ecc4c5bf34007208d5b887185865";
        uint8_t e[42]; hex2bin(expect, e, 42);
        CHECK(memcmp(okm, e, 42) == 0, "HKDF RFC5869 TC1");
    }

    // --- ChaCha20 keystream RFC 8439 2.4.2 ---
    {
        uint8_t key[32], nonce[12], block[64];
        for (unsigned i=0;i<32;i++) key[i]=(uint8_t)i;
        hex2bin("000000090000004a00000000", nonce, 12);
        TasChaCha20Block(key, 1, nonce, block);
        const char *expect = "10f1e7e4d13b5915500fdd1fa32071c4";
        uint8_t e[16]; hex2bin(expect, e, 16);
        CHECK(memcmp(block, e, 16) == 0, "ChaCha20 keystream RFC8439 2.4.2");
    }

    // --- CRC32 check value ---
    CHECK(TasCrc32((const uint8_t *)"123456789", 9, 0) == 0xCBF43926u, "CRC32 check val");

    // --- Session: master derivation deterministic, epoch rotation advances ---
    {
        TasSessionCtx_s a, b;
        uint8_t uid[6] = {0x11,0x22,0x33,0x44,0x55,0x66};
        TasSessionDeriveMaster(&a, uid, 0xDEADBEEF);
        TasSessionDeriveMaster(&b, uid, 0xDEADBEEF);
        CHECK(memcmp(a.masterKey, b.masterKey, 32) == 0, "master deterministic");

        uint8_t k0[32]; memcpy(k0, a.epochKey, 32);
        TasSessionAdvance(&a, TAS_REKEY_FRAMES + 1);
        CHECK(a.epochIndex == 1, "epoch advanced");
        CHECK(memcmp(a.epochKey, k0, 32) != 0, "epoch key rotated");
        CHECK(memcmp(a.prevEpochKey, k0, 32) == 0, "prev epoch kept");

        // different seed -> different master
        TasSessionDeriveMaster(&b, uid, 0xDEADBEEC);
        CHECK(memcmp(a.masterKey, b.masterKey, 32) != 0, "master binds seed");
    }

    // --- Replay window ---
    {
        uint8_t last = 250;
        bool seen[16] = {};
        CHECK(TasReplayCheck(251, &last, seen, 4) == TAS_OK, "replay: forward ok");
        CHECK(TasReplayCheck(252, &last, seen, 4) == TAS_OK, "replay: forward ok2");
        CHECK(TasReplayCheck(252, &last, seen, 4) == TAS_REPLAY_REJECT, "replay: dup rejected");
        CHECK(TasReplayCheck(253, &last, seen, 4) == TAS_OK, "replay: wrap ok");
        CHECK(TasReplayCheck(252, &last, seen, 4) == TAS_REPLAY_REJECT, "replay: stale rejected");
        uint8_t last2 = 100;
        bool seen2[16] = {};
        CHECK(TasReplayCheck(90, &last2, seen2, 4) == TAS_REPLAY_REJECT, "replay: far-past rejected");
        CHECK(TasReplayCheck(101, &last2, seen2, 4) == TAS_OK, "replay: resync forward ok");
    }

    // --- Const-time equal ---
    {
        uint8_t a[8] = {1,2,3,4,5,6,7,8}, b[8] = {1,2,3,4,5,6,7,9};
        CHECK(TasConstTimeEqual(a,a,8) && !TasConstTimeEqual(a,b,8), "const-time cmp");
    }

    printf(failures ? "\n== AUDIT FAILED (%d) ==\n" : "\n== AUDIT PASSED ==\n", failures);
    return failures ? 1 : 0;
}
