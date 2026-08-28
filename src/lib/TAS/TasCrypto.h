#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define TAS_KEY_LEN     32
#define TAS_SALT_MAX    32
#define TAS_NONCE_LEN   12

typedef struct {
    uint32_t state[8];
    uint8_t  buffer[64];
    uint64_t byteCount;
    uint8_t  bufferIdx;
} TasSha256Ctx_s;

void TasSha256Init(TasSha256Ctx_s *ctx);
void TasSha256Update(TasSha256Ctx_s *ctx, const uint8_t *data, size_t len);
void TasSha256Final(TasSha256Ctx_s *ctx, uint8_t out[TAS_KEY_LEN]);
void TasSha256(const uint8_t *data, size_t len, uint8_t out[TAS_KEY_LEN]);

void TasHmacSha256(
    const uint8_t *key, size_t keyLen,
    const uint8_t *data, size_t dataLen,
    uint8_t out[TAS_KEY_LEN]);

void TasHkdfExtract(
    const uint8_t *salt, size_t saltLen,
    const uint8_t *ikm, size_t ikmLen,
    uint8_t prk[TAS_KEY_LEN]);

void TasHkdfExpand(
    const uint8_t prk[TAS_KEY_LEN],
    const uint8_t *info, size_t infoLen,
    uint8_t *okm, size_t okmLen);

void TasChaCha20Block(
    const uint8_t key[TAS_KEY_LEN],
    uint32_t counter,
    const uint8_t nonce[TAS_NONCE_LEN],
    uint8_t outBlock[64]);

void TasChaCha20Xor(
    const uint8_t key[TAS_KEY_LEN],
    uint32_t counter,
    const uint8_t nonce[TAS_NONCE_LEN],
    const uint8_t *in, uint8_t *out, size_t len);

uint32_t TasCrc32(const uint8_t *data, size_t len, uint32_t crc);

bool TasConstTimeEqual(const void *a, const void *b, size_t len);
