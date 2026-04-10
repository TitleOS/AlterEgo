#pragma once
#include <stdint.h>
#include <stddef.h>

// AES-128 key expansion and block encryption (ECB)
// Used by the AES-CTR mode below.

typedef struct {
    uint32_t rk[44]; // 11 round keys x 4 words
} Aes128Ctx;

void aes128_set_encrypt_key(Aes128Ctx* ctx, const uint8_t key[16]);
void aes128_encrypt_block(const Aes128Ctx* ctx, const uint8_t in[16], uint8_t out[16]);

// AES-128-CTR — nonce is 16 bytes (big-endian counter in last 4 bytes)
void aes128_ctr(
    const uint8_t key[16],
    const uint8_t nonce[16],
    const uint8_t* input,
    uint8_t* output,
    size_t length);
