#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "amiibo_crypto.h"
#include "amiibo_smash.h"

// NFC subsystem (Momentum / recent Flipper firmware)
#include <nfc/nfc.h>
#include <nfc/nfc_device.h>
#include <nfc/protocols/mf_ultralight/mf_ultralight.h>
#include <storage/storage.h>

// ─── API ──────────────────────────────────────────────────────────────────────

/**
 * Copy raw NTAG215 page data from an MfUltralightData struct into a
 * flat 540-byte dump buffer (page 0 at buf[0]).
 *
 * @param mfu   Parsed MF Ultralight data from NFC subsystem.
 * @param dump  Destination 540-byte buffer.
 */
void nfc_mfu_to_dump(const MfUltralightData* mfu, uint8_t dump[AMIIBO_DUMP_SIZE]);

/**
 * Write a 540-byte dump back into an MfUltralightData struct
 * (pages 4–130 only; UID pages are not written).
 *
 * @param dump  Source 540-byte dump.
 * @param mfu   Destination struct.
 */
void nfc_dump_to_mfu(const uint8_t dump[AMIIBO_DUMP_SIZE], MfUltralightData* mfu);

/**
 * Load a .nfc file from the Flipper SD card into a flat 540-byte dump.
 *
 * @param path  Absolute path on the SD (e.g. "/ext/nfc/Mario.nfc").
 * @param dump  Destination 540-byte buffer.
 * @return      true on success.
 */
bool nfc_load_file(const char* path, uint8_t dump[AMIIBO_DUMP_SIZE]);

/**
 * Save a 540-byte dump as a Flipper-format .nfc file.
 *
 * @param path  Destination path.
 * @param dump  Source 540-byte buffer.
 * @return      true on success.
 */
bool nfc_save_file(const char* path, const uint8_t dump[AMIIBO_DUMP_SIZE]);

/**
 * Scan for an NTAG215 tag using the Flipper NFC hardware.
 * This is a blocking call — run it from a worker thread.
 * Polls until a tag is found or stop_flag becomes true.
 *
 * @param dump       Destination 540-byte buffer.
 * @param stop_flag  Pointer to a volatile bool; set to true to cancel.
 * @return           true if a tag was successfully read.
 */
bool nfc_scan_tag(uint8_t dump[AMIIBO_DUMP_SIZE], volatile bool* stop_flag);

/**
 * Write pages 4–130 of a 540-byte dump to a physical NTAG215 tag.
 * Blocking — run from a worker thread.
 *
 * @param dump       Source 540-byte buffer.
 * @param stop_flag  Cancellation flag.
 * @return           true on success.
 */
bool nfc_write_tag(const uint8_t dump[AMIIBO_DUMP_SIZE], volatile bool* stop_flag);
