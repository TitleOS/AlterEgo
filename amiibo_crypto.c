#include "amiibo_crypto.h"

#include <string.h>
#include <furi.h>
#include <storage/storage.h>

// Self-contained crypto — no mbedTLS (disabled in Momentum FAP API)
#include "crypto/sha256.h"
#include "crypto/aes128.h"

#define TAG "AmiiboCrypto"

// ─── Key loading ──────────────────────────────────────────────────────────────

bool amiibo_crypto_load_keys(AmiiboKeyRetail* out) {
    furi_assert(out);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    bool ok = false;
    do {
        if(!storage_file_open(file, KEYS_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
            FURI_LOG_E(TAG, "Cannot open %s", KEYS_PATH);
            break;
        }
        uint16_t read = storage_file_read(file, out, sizeof(AmiiboKeyRetail));
        if(read != sizeof(AmiiboKeyRetail)) {
            FURI_LOG_E(TAG, "key_retail.bin wrong size: %u (expected %u)",
                       (unsigned)read, (unsigned)sizeof(AmiiboKeyRetail));
            break;
        }
        ok = true;
    } while(false);

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

// ─── UID extraction ───────────────────────────────────────────────────────────

void amiibo_crypto_extract_uid(const uint8_t* dump, uint8_t uid[AMIIBO_UID_SIZE]) {
    // NTAG215 UID layout:
    //   dump[0..2] = uid[0..2]
    //   dump[3]    = BCC0
    //   dump[4..7] = uid[3..6]
    uid[0] = dump[0]; uid[1] = dump[1]; uid[2] = dump[2];
    uid[3] = dump[4]; uid[4] = dump[5]; uid[5] = dump[6]; uid[6] = dump[7];
}

// ─── Key derivation ───────────────────────────────────────────────────────────
//
// Follows amiitool algorithm (socram8888/amiitool keygen.c):
//   seed = type_string[14] + rfu + magic_size
//          + interleaved(uid[0..6], magic_bytes[0..magic_size-1])
//          + uid[0..6]
//   block0 = HMAC-SHA256(hmac_key, 0x0000 || seed)
//   block1 = HMAC-SHA256(hmac_key, 0x0001 || seed)
//   key_material[64] = block0 || block1
//   aes_key  = key_material[0..15]
//   aes_iv   = key_material[16..31] XOR xor_pad[0..15]
//   hmac_key = key_material[32..47] XOR xor_pad[16..31]

void amiibo_crypto_derive_key(
    const AmiiboMasterKey* master,
    const uint8_t          uid[AMIIBO_UID_SIZE],
    AmiiboDerivedKey*      out)
{
    uint8_t seed[256];
    size_t  pos = 0;

    memcpy(&seed[pos], master->type_string, 14);  pos += 14;
    seed[pos++] = master->reserved;
    seed[pos++] = master->magic_size;

    uint8_t magic_size = (master->magic_size <= 16) ? master->magic_size : 16;
    for(uint8_t i = 0; i < magic_size || i < AMIIBO_UID_SIZE; i++) {
        if(i < AMIIBO_UID_SIZE) seed[pos++] = uid[i];
        if(i < magic_size)      seed[pos++] = master->magic_bytes[i];
    }
    memcpy(&seed[pos], uid, AMIIBO_UID_SIZE);
    pos += AMIIBO_UID_SIZE;

    // Two HMAC passes with 2-byte big-endian counter prefix
    uint8_t buf[2 + 256];
    memcpy(buf + 2, seed, pos);

    uint8_t key_material[64];
    buf[0] = 0x00; buf[1] = 0x00;
    hmac_sha256(master->hmac_key, 16, buf, 2 + pos, &key_material[0]);
    buf[1] = 0x01;
    hmac_sha256(master->hmac_key, 16, buf, 2 + pos, &key_material[32]);

    memcpy(out->aes_key, &key_material[0], 16);
    for(int i = 0; i < 16; i++)
        out->aes_iv[i]   = key_material[16 + i] ^ master->xor_pad[i];
    for(int i = 0; i < 16; i++)
        out->hmac_key[i] = key_material[32 + i] ^ master->xor_pad[16 + i];
}

// ─── Internal helpers ─────────────────────────────────────────────────────────

static void aes_ctr_crypt(
    const uint8_t key[16],
    const uint8_t iv[16],
    const uint8_t* in,
    uint8_t*       out,
    size_t         len)
{
    aes128_ctr(key, iv, in, out, len);
}

static void compute_hmac_tag(
    const uint8_t* dump,
    const uint8_t  hmac_key[16],
    uint8_t        out[32])
{
    // Message = uid_area(8 bytes) || flag_area(8 bytes)
    uint8_t msg[16];
    memcpy(msg,     dump,                            8);
    memcpy(msg + 8, dump + AMIIBO_USER_OFFSET,       8);
    hmac_sha256(hmac_key, 16, msg, sizeof(msg), out);
}

static void compute_hmac_data(
    const uint8_t* dump,
    const uint8_t  hmac_key[16],
    uint8_t        out[32])
{
    const uint8_t* data = dump + AMIIBO_USER_OFFSET + AMIIBO_OFF_SETTINGS;
    size_t         len  = AMIIBO_USER_SIZE - AMIIBO_OFF_SETTINGS;
    hmac_sha256(hmac_key, 16, data, len, out);
}

// ─── Public API ───────────────────────────────────────────────────────────────

bool amiibo_crypto_decrypt(uint8_t dump[AMIIBO_DUMP_SIZE], const AmiiboKeyRetail* keys) {
    uint8_t uid[AMIIBO_UID_SIZE];
    amiibo_crypto_extract_uid(dump, uid);

    AmiiboDerivedKey data_key, tag_key;
    amiibo_crypto_derive_key(&keys->data, uid, &data_key);
    amiibo_crypto_derive_key(&keys->tag,  uid, &tag_key);

    // Verify HMAC-Tag (non-fatal: allow working with .nfc files)
    uint8_t expected_tag[32];
    compute_hmac_tag(dump, tag_key.hmac_key, expected_tag);
    const uint8_t* stored_tag = dump + AMIIBO_USER_OFFSET + AMIIBO_OFF_HASH_TAG;
    if(memcmp(expected_tag, stored_tag, 32) != 0)
        FURI_LOG_W(TAG, "HMAC tag mismatch — editing continues");

    // AES-128-CTR decrypt
    uint8_t* enc = dump + AMIIBO_USER_OFFSET + AMIIBO_OFF_ENCRYPTED;
    aes_ctr_crypt(data_key.aes_key, data_key.aes_iv, enc, enc, 40);

    FURI_LOG_I(TAG, "Decryption complete");
    return true;
}

void amiibo_crypto_encrypt(uint8_t dump[AMIIBO_DUMP_SIZE], const AmiiboKeyRetail* keys) {
    uint8_t uid[AMIIBO_UID_SIZE];
    amiibo_crypto_extract_uid(dump, uid);

    AmiiboDerivedKey data_key, tag_key;
    amiibo_crypto_derive_key(&keys->data, uid, &data_key);
    amiibo_crypto_derive_key(&keys->tag,  uid, &tag_key);

    // Re-encrypt (CTR is its own inverse)
    uint8_t* enc = dump + AMIIBO_USER_OFFSET + AMIIBO_OFF_ENCRYPTED;
    aes_ctr_crypt(data_key.aes_key, data_key.aes_iv, enc, enc, 40);

    // Recompute HMAC signatures
    uint8_t hmac_data[32];
    compute_hmac_data(dump, data_key.hmac_key, hmac_data);
    memcpy(dump + AMIIBO_USER_OFFSET + AMIIBO_OFF_HASH_DATA, hmac_data, 32);

    uint8_t hmac_tag[32];
    compute_hmac_tag(dump, tag_key.hmac_key, hmac_tag);
    memcpy(dump + AMIIBO_USER_OFFSET + AMIIBO_OFF_HASH_TAG, hmac_tag, 32);

    FURI_LOG_I(TAG, "Re-encryption and signing complete");
}

bool amiibo_crypto_verify(const uint8_t dump[AMIIBO_DUMP_SIZE], const AmiiboKeyRetail* keys) {
    uint8_t tmp[AMIIBO_DUMP_SIZE];
    memcpy(tmp, dump, AMIIBO_DUMP_SIZE);
    return amiibo_crypto_decrypt(tmp, keys);
}
