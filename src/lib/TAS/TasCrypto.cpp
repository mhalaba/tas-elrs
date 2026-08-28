#include "TasCrypto.h"
#include <string.h>

// ---- SHA-256 (FIPS 180-4) ----

static const uint32_t K256[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

#define ROTR(x,n) (((x) >> (n)) | ((x) << (32-(n))))
#define CH(x,y,z) (((x)&(y)) ^ (~(x)&(z)))
#define MAJ(x,y,z) (((x)&(y)) ^ ((x)&(z)) ^ ((y)&(z)))
#define EP0(x) (ROTR(x,2) ^ ROTR(x,13) ^ ROTR(x,22))
#define EP1(x) (ROTR(x,6) ^ ROTR(x,11) ^ ROTR(x,25))
#define SG0(x) (ROTR(x,7) ^ ROTR(x,18) ^ ((x)>>3))
#define SG1(x) (ROTR(x,17) ^ ROTR(x,19) ^ ((x)>>10))

static void TasSha256Transform(TasSha256Ctx_s *ctx, const uint8_t block[64])
{
    uint32_t w[64];
    for (unsigned i = 0; i < 16; i++)
    {
        w[i] = ((uint32_t)block[i*4] << 24) | ((uint32_t)block[i*4+1] << 16) |
               ((uint32_t)block[i*4+2] << 8) | (uint32_t)block[i*4+3];
    }
    for (unsigned i = 16; i < 64; i++)
    {
        w[i] = SG1(w[i-2]) + w[i-7] + SG0(w[i-15]) + w[i-16];
    }

    uint32_t a=ctx->state[0], b=ctx->state[1], c=ctx->state[2], d=ctx->state[3];
    uint32_t e=ctx->state[4], f=ctx->state[5], g=ctx->state[6], h=ctx->state[7];

    for (unsigned i = 0; i < 64; i++)
    {
        uint32_t t1 = h + EP1(e) + CH(e,f,g) + K256[i] + w[i];
        uint32_t t2 = EP0(a) + MAJ(a,b,c);
        h=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
    }

    ctx->state[0]+=a; ctx->state[1]+=b; ctx->state[2]+=c; ctx->state[3]+=d;
    ctx->state[4]+=e; ctx->state[5]+=f; ctx->state[6]+=g; ctx->state[7]+=h;
}

void TasSha256Init(TasSha256Ctx_s *ctx)
{
    static const uint32_t IV[8] = {
        0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,
        0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19
    };
    memcpy(ctx->state, IV, sizeof(IV));
    ctx->byteCount = 0;
    ctx->bufferIdx = 0;
}

void TasSha256Update(TasSha256Ctx_s *ctx, const uint8_t *data, size_t len)
{
    ctx->byteCount += len;
    while (len > 0)
    {
        size_t space = 64 - ctx->bufferIdx;
        size_t take = len < space ? len : space;
        memcpy(ctx->buffer + ctx->bufferIdx, data, take);
        ctx->bufferIdx += take;
        data += take;
        len -= take;
        if (ctx->bufferIdx == 64)
        {
            TasSha256Transform(ctx, ctx->buffer);
            ctx->bufferIdx = 0;
        }
    }
}

void TasSha256Final(TasSha256Ctx_s *ctx, uint8_t out[TAS_KEY_LEN])
{
    uint64_t bitLen = ctx->byteCount * 8;
    uint8_t pad = 0x80;
    TasSha256Update(ctx, &pad, 1);
    uint8_t zero = 0;
    while (ctx->bufferIdx != 56)
    {
        TasSha256Update(ctx, &zero, 1);
    }
    uint8_t lenBytes[8];
    for (unsigned i = 0; i < 8; i++)
    {
        lenBytes[i] = (uint8_t)(bitLen >> (56 - i*8));
    }
    // append length without recounting padding bytes
    memcpy(ctx->buffer + 56, lenBytes, 8);
    TasSha256Transform(ctx, ctx->buffer);
    ctx->bufferIdx = 0;
    for (unsigned i = 0; i < 8; i++)
    {
        out[i*4]   = (uint8_t)(ctx->state[i] >> 24);
        out[i*4+1] = (uint8_t)(ctx->state[i] >> 16);
        out[i*4+2] = (uint8_t)(ctx->state[i] >> 8);
        out[i*4+3] = (uint8_t)(ctx->state[i]);
    }
}

void TasSha256(const uint8_t *data, size_t len, uint8_t out[TAS_KEY_LEN])
{
    TasSha256Ctx_s ctx;
    TasSha256Init(&ctx);
    TasSha256Update(&ctx, data, len);
    TasSha256Final(&ctx, out);
}

// ---- HMAC-SHA256 (RFC 2104) ----

static void TasHmacPad(const uint8_t *key, size_t keyLen, uint8_t kipad[64], uint8_t kopad[64])
{
    // RFC 2104: key is zero-padded to the 64-byte block before XOR with pads
    uint8_t k[64] {};
    if (keyLen > 64)
    {
        TasSha256(key, keyLen, k);
    }
    else
    {
        memcpy(k, key, keyLen);
    }
    for (unsigned i = 0; i < 64; i++)
    {
        kipad[i] = k[i] ^ 0x36;
        kopad[i] = k[i] ^ 0x5c;
    }
}

void TasHmacSha256(
    const uint8_t *key, size_t keyLen,
    const uint8_t *data, size_t dataLen,
    uint8_t out[TAS_KEY_LEN])
{
    uint8_t kipad[64], kopad[64];
    TasHmacPad(key, keyLen, kipad, kopad);

    TasSha256Ctx_s ctx;
    TasSha256Init(&ctx);
    TasSha256Update(&ctx, kipad, 64);
    TasSha256Update(&ctx, data, dataLen);
    uint8_t inner[TAS_KEY_LEN];
    TasSha256Final(&ctx, inner);

    TasSha256Init(&ctx);
    TasSha256Update(&ctx, kopad, 64);
    TasSha256Update(&ctx, inner, TAS_KEY_LEN);
    TasSha256Final(&ctx, out);
}

// HMAC(PRK, a | b | c) without concatenating into a bounded stack buffer.
// Used by HKDF-Expand so info of any length (RFC 5869 TC2 is 80 bytes) is safe.
static void TasHmacSha256Concat(
    const uint8_t *key, size_t keyLen,
    const uint8_t *a, size_t aLen,
    const uint8_t *b, size_t bLen,
    const uint8_t *c, size_t cLen,
    uint8_t out[TAS_KEY_LEN])
{
    uint8_t kipad[64], kopad[64];
    TasHmacPad(key, keyLen, kipad, kopad);

    TasSha256Ctx_s ctx;
    TasSha256Init(&ctx);
    TasSha256Update(&ctx, kipad, 64);
    if (aLen) TasSha256Update(&ctx, a, aLen);
    if (bLen) TasSha256Update(&ctx, b, bLen);
    if (cLen) TasSha256Update(&ctx, c, cLen);

    uint8_t inner[TAS_KEY_LEN];
    TasSha256Final(&ctx, inner);

    TasSha256Init(&ctx);
    TasSha256Update(&ctx, kopad, 64);
    TasSha256Update(&ctx, inner, TAS_KEY_LEN);
    TasSha256Final(&ctx, out);
}

// ---- HKDF-SHA256 (RFC 5869) ----

void TasHkdfExtract(
    const uint8_t *salt, size_t saltLen,
    const uint8_t *ikm, size_t ikmLen,
    uint8_t prk[TAS_KEY_LEN])
{
    // RFC 5869: salt defaults to HashLen zeros; otherwise it is the HMAC key
    // (HMAC-SHA256 itself hashes keys longer than the 64-byte block).
    static const uint8_t zeros[TAS_KEY_LEN] = {0};
    if (salt == nullptr || saltLen == 0)
    {
        TasHmacSha256(zeros, TAS_KEY_LEN, ikm, ikmLen, prk);
    }
    else
    {
        TasHmacSha256(salt, saltLen, ikm, ikmLen, prk);
    }
}

void TasHkdfExpand(
    const uint8_t prk[TAS_KEY_LEN],
    const uint8_t *info, size_t infoLen,
    uint8_t *okm, size_t okmLen)
{
    uint8_t t[TAS_KEY_LEN];
    size_t tLen = 0;
    size_t pos = 0;
    uint8_t counter = 1;

    while (pos < okmLen)
    {
        // T(i) = HMAC(PRK, T(i-1) | info | i)
        TasHmacSha256Concat(
            prk, TAS_KEY_LEN,
            t, tLen,
            info, infoLen,
            &counter, 1,
            t);
        tLen = TAS_KEY_LEN;

        size_t take = okmLen - pos < TAS_KEY_LEN ? okmLen - pos : TAS_KEY_LEN;
        memcpy(okm + pos, t, take);
        pos += take;
        counter++;
    }
}

// ---- ChaCha20 (RFC 8439) ----

#define ROTL20(x,n) (((x) << (n)) | ((x) >> (32-(n))))
#define QROUND(a,b,c,d) \
    a += b; d ^= a; d = ROTL20(d,16); \
    c += d; b ^= c; b = ROTL20(b,12); \
    a += b; d ^= a; d = ROTL20(d,8);  \
    c += d; b ^= c; b = ROTL20(b,7);

static void TasChaCha20InitState(
    uint32_t st[16],
    const uint8_t key[TAS_KEY_LEN],
    uint32_t counter,
    const uint8_t nonce[TAS_NONCE_LEN])
{
    st[0] = 0x61707865;
    st[1] = 0x3320646e;
    st[2] = 0x79622d32;
    st[3] = 0x6b206574;
    for (unsigned i = 0; i < 8; i++)
    {
        st[4+i] = ((uint32_t)key[i*4]) | ((uint32_t)key[i*4+1] << 8) |
                  ((uint32_t)key[i*4+2] << 16) | ((uint32_t)key[i*4+3] << 24);
    }
    st[12] = counter;
    for (unsigned i = 0; i < 3; i++)
    {
        st[13+i] = ((uint32_t)nonce[i*4]) | ((uint32_t)nonce[i*4+1] << 8) |
                   ((uint32_t)nonce[i*4+2] << 16) | ((uint32_t)nonce[i*4+3] << 24);
    }
}

void TasChaCha20Block(
    const uint8_t key[TAS_KEY_LEN],
    uint32_t counter,
    const uint8_t nonce[TAS_NONCE_LEN],
    uint8_t outBlock[64])
{
    uint32_t st[16], work[16];
    TasChaCha20InitState(st, key, counter, nonce);
    memcpy(work, st, sizeof(work));

    for (unsigned i = 0; i < 10; i++)
    {
        QROUND(work[0], work[4], work[8],  work[12]);
        QROUND(work[1], work[5], work[9],  work[13]);
        QROUND(work[2], work[6], work[10], work[14]);
        QROUND(work[3], work[7], work[11], work[15]);
        QROUND(work[0], work[5], work[10], work[15]);
        QROUND(work[1], work[6], work[11], work[12]);
        QROUND(work[2], work[7], work[8],  work[13]);
        QROUND(work[3], work[4], work[9],  work[14]);
    }

    for (unsigned i = 0; i < 16; i++)
    {
        // RFC 8439: words are serialized little-endian
        uint32_t v = work[i] + st[i];
        outBlock[i*4]   = (uint8_t)v;
        outBlock[i*4+1] = (uint8_t)(v >> 8);
        outBlock[i*4+2] = (uint8_t)(v >> 16);
        outBlock[i*4+3] = (uint8_t)(v >> 24);
    }
}

void TasChaCha20Xor(
    const uint8_t key[TAS_KEY_LEN],
    uint32_t counter,
    const uint8_t nonce[TAS_NONCE_LEN],
    const uint8_t *in, uint8_t *out, size_t len)
{
    uint8_t ks[64];
    while (len > 0)
    {
        TasChaCha20Block(key, counter, nonce, ks);
        size_t take = len < 64 ? len : 64;
        for (size_t i = 0; i < take; i++)
        {
            out[i] = in[i] ^ ks[i];
        }
        in += take;
        out += take;
        len -= take;
        counter++;
    }
}

// ---- CRC32 (ISO-HDLC, reflected) ----

uint32_t TasCrc32(const uint8_t *data, size_t len, uint32_t crc)
{
    crc = ~crc;
    while (len--)
    {
        crc ^= *data++;
        for (unsigned k = 0; k < 8; k++)
        {
            crc = (crc >> 1) ^ (0xEDB88320 & -(int32_t)(crc & 1));
        }
    }
    return ~crc;
}

bool TasConstTimeEqual(const void *a, const void *b, size_t len)
{
    const uint8_t *pa = (const uint8_t *)a;
    const uint8_t *pb = (const uint8_t *)b;
    uint8_t diff = 0;
    for (size_t i = 0; i < len; i++)
    {
        diff |= pa[i] ^ pb[i];
    }
    return diff == 0;
}
