#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// ─── Constants ────────────────────────────────────────────────────────────────

#define AMIIBO_DUMP_SIZE    540   // Full NTAG215 dump (135 pages × 4 bytes)
#define AMIIBO_USER_OFFSET  16    // page 0x04 * 4 bytes — start of user data
#define AMIIBO_USER_SIZE    508   // pages 0x04..0x82 (127 pages × 4)
#define AMIIBO_UID_SIZE     7     // 7 meaningful UID bytes

// Within the 508-byte user/data area (byte 0 = dump[16]):
#define AMIIBO_OFF_FLAGS        0x000  // 2 bytes — init flags
#define AMIIBO_OFF_HASH_TAG     0x008  // 32 bytes — HMAC tag hash
#define AMIIBO_OFF_SETTINGS     0x028  // 8 bytes  — settings/counter
#define AMIIBO_OFF_APPID        0x030  // 4 bytes  — application game ID (BE u32)
#define AMIIBO_OFF_HASH_DATA    0x038  // 32 bytes — HMAC data hash
#define AMIIBO_OFF_ENCRYPTED    0x058  // 40 bytes — AES-CTR encrypted header
#define AMIIBO_OFF_GAME_DATA    0x0DC  // up to 360 bytes — game specific area

// key_retail.bin path on Flipper SD
#define KEYS_PATH  "/ext/amiibo/key_retail.bin"

// ─── Structs ──────────────────────────────────────────────────────────────────

/**
 * One 80-byte master key from key_retail.bin.
 * Two of these are packed consecutively (data key first, tag key second).
 *
 * Layout matches amiitool's keygen.c structure.
 */
typedef struct __attribute__((packed)) {
    uint8_t hmac_key[16];      // HMAC-SHA256 master key for seed derivation
    char    type_string[14];   // "unfixed infos" or "locked secret"
    uint8_t reserved;          // padding / rfu
    uint8_t magic_size;        // Length of valid bytes in magic_bytes (usually 16)
    uint8_t magic_bytes[16];   // Keygen magic bytes
    uint8_t xor_pad[32];       // Applied to derived HMAC key (XOR pad)
} AmiiboMasterKey;             // 80 bytes total

/** Both master keys packed from key_retail.bin (160 bytes) */
typedef struct __attribute__((packed)) {
    AmiiboMasterKey data;  // bytes 0–79   — "unfixed infos"
    AmiiboMasterKey tag;   // bytes 80–159 — "locked secret"
} AmiiboKeyRetail;

/** 48 bytes of key material derived per operation */
typedef struct {
    uint8_t aes_key[16];   // AES-128 key
    uint8_t aes_iv[16];    // AES-128 CTR initial value
    uint8_t hmac_key[16];  // HMAC-SHA256 key (first 16 of 48-byte stream)
} AmiiboDerivedKey;

// ─── API ──────────────────────────────────────────────────────────────────────

/**
 * Load key_retail.bin from the Flipper SD card.
 * @param out  Destination struct — filled on success.
 * @return     true if the 160-byte file was read correctly.
 */
bool amiibo_crypto_load_keys(AmiiboKeyRetail* out);

/**
 * Extract the 7-byte UID from a raw 540-byte NTAG215 dump.
 * @param dump  Raw tag bytes.
 * @param uid   Output buffer (7 bytes).
 */
void amiibo_crypto_extract_uid(const uint8_t* dump, uint8_t uid[AMIIBO_UID_SIZE]);

/**
 * Derive a 48-byte key pair (aes_key + aes_iv + hmac_key) from a master key
 * and the tag UID.  Follows the algorithm used by amiitool / amiibo.life.
 * @param master  The data or tag master key.
 * @param uid     7-byte UID.
 * @param out     Receives the derived keys.
 */
void amiibo_crypto_derive_key(
    const AmiiboMasterKey* master,
    const uint8_t          uid[AMIIBO_UID_SIZE],
    AmiiboDerivedKey*      out);

/**
 * Decrypt the AES-CTR section of an amiibo dump in-place.
 * The encrypted region is bytes [AMIIBO_USER_OFFSET + 0x58 ..
 *                                AMIIBO_USER_OFFSET + 0x7F] (40 bytes).
 * Additionally, the game-specific block is NOT encrypted;  this function
 * decrypts only the header section using the data-derived key.
 *
 * @param dump  Raw 540-byte dump (modified in place — decrypted output goes
 *              to the same buffer for the header section).
 * @param keys  Loaded key_retail.bin.
 * @return      true if HMAC verification passes.
 */
bool amiibo_crypto_decrypt(uint8_t dump[AMIIBO_DUMP_SIZE], const AmiiboKeyRetail* keys);

/**
 * Re-encrypt and re-sign the dump after modification.
 * Must be called before writing back to NFC or saving to SD.
 *
 * @param dump  540-byte buffer (modified in place).
 * @param keys  Loaded key_retail.bin.
 */
void amiibo_crypto_encrypt(uint8_t dump[AMIIBO_DUMP_SIZE], const AmiiboKeyRetail* keys);

/**
 * Convenience: verify only — return true if both HMACs check out.
 */
bool amiibo_crypto_verify(const uint8_t dump[AMIIBO_DUMP_SIZE], const AmiiboKeyRetail* keys);
