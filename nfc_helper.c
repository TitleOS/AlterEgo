#include "nfc_helper.h"

#include <string.h>
#include <furi.h>
#include <furi_hal.h>
#include <storage/storage.h>

// Momentum NFC API (based on post-refactor Flipper firmware)
#include <nfc/nfc.h>
#include <nfc/nfc_device.h>
#include <nfc/nfc_poller.h>
#include <nfc/protocols/mf_ultralight/mf_ultralight.h>
#include <nfc/protocols/mf_ultralight/mf_ultralight_poller.h>

#define TAG "NfcHelper"

// ─── Flat dump ↔ MfUltralightData conversion ─────────────────────────────────

void nfc_mfu_to_dump(const MfUltralightData* mfu, uint8_t dump[AMIIBO_DUMP_SIZE]) {
    furi_assert(mfu);
    furi_assert(dump);
    memset(dump, 0, AMIIBO_DUMP_SIZE);

    uint16_t total = mfu->pages_total < 135 ? mfu->pages_total : 135;
    uint16_t read  = mfu->pages_read;
    FURI_LOG_I(TAG, "MFU pages_total=%u pages_read=%u", total, read);
    if(read < 17) {
        FURI_LOG_W(
            TAG,
            "Only %u pages read — App ID (page 16) may be missing!",
            read);
    }
    for(uint16_t i = 0; i < total; i++) {
        memcpy(&dump[i * 4], mfu->page[i].data, 4);
    }
}

void nfc_dump_to_mfu(const uint8_t dump[AMIIBO_DUMP_SIZE], MfUltralightData* mfu) {
    furi_assert(dump);
    furi_assert(mfu);

    uint16_t pages = mfu->pages_total < 135 ? mfu->pages_total : 135;
    for(uint16_t i = 4; i < pages; i++) {
        memcpy(mfu->page[i].data, &dump[i * 4], 4);
    }
}

// ─── NFC file I/O (Flipper .nfc format) ──────────────────────────────────────

bool nfc_load_file(const char* path, uint8_t dump[AMIIBO_DUMP_SIZE]) {
    furi_assert(path);
    furi_assert(dump);

    NfcDevice* device = nfc_device_alloc();
    bool ok = nfc_device_load(device, path);

    if(ok) {
        if(nfc_device_get_protocol(device) == NfcProtocolMfUltralight) {
            const MfUltralightData* mfu =
                (const MfUltralightData*)nfc_device_get_data(device, NfcProtocolMfUltralight);
            nfc_mfu_to_dump(mfu, dump);
            FURI_LOG_I(TAG, "Loaded %s (%u pages)", path, mfu->pages_total);
        } else {
            FURI_LOG_E(TAG, "Not an MF Ultralight tag: %s", path);
            ok = false;
        }
    } else {
        FURI_LOG_E(TAG, "Failed to load %s", path);
    }

    nfc_device_free(device);
    return ok;
}

bool nfc_save_file(const char* path, const uint8_t dump[AMIIBO_DUMP_SIZE]) {
    furi_assert(path);
    furi_assert(dump);

    // Allocate a fresh MfUltralightData and populate it from the flat dump
    MfUltralightData* mfu = mf_ultralight_alloc();
    mfu->type        = MfUltralightTypeNTAG215;
    mfu->pages_total = 135;
    mfu->pages_read  = 135;

    for(uint16_t i = 0; i < 135; i++)
        memcpy(mfu->page[i].data, &dump[i * 4], 4);

    // Set UID from dump bytes — NTAG215 layout: 0-2, skip 3 (BCC0), 4-7
    uint8_t uid[7] = {
        dump[0], dump[1], dump[2],
        dump[4], dump[5], dump[6], dump[7],
    };
    mf_ultralight_set_uid(mfu, uid, sizeof(uid));

    NfcDevice* device = nfc_device_alloc();
    nfc_device_set_data(device, NfcProtocolMfUltralight, (const NfcDeviceData*)mfu);

    bool ok = nfc_device_save(device, path);
    if(ok)
        FURI_LOG_I(TAG, "Saved to %s", path);
    else
        FURI_LOG_E(TAG, "Failed to save %s", path);

    nfc_device_free(device);
    mf_ultralight_free(mfu);
    return ok;
}

// ─── NFC scan worker ─────────────────────────────────────────────────────────

typedef struct {
    NfcPoller*    poller;
    uint8_t*      dump;
    volatile bool done;
    bool          success;
} ScanCtx;

static NfcCommand scan_poller_callback(NfcGenericEvent event, void* context) {
    ScanCtx* ctx = context;

    if(event.protocol == NfcProtocolMfUltralight) {
        const MfUltralightPollerEvent* mfu_evt =
            (const MfUltralightPollerEvent*)event.event_data;

        if(mfu_evt->type == MfUltralightPollerEventTypeReadSuccess) {
            const MfUltralightData* mfu =
                (const MfUltralightData*)nfc_poller_get_data(ctx->poller);
            if(mfu) {
                nfc_mfu_to_dump(mfu, ctx->dump);
                ctx->success = true;
            }
            ctx->done = true;
            return NfcCommandStop;
        }

        if(mfu_evt->type == MfUltralightPollerEventTypeReadFailed) {
            ctx->done    = true;
            ctx->success = false;
            return NfcCommandStop;
        }
    }
    return NfcCommandContinue;
}

bool nfc_scan_tag(uint8_t dump[AMIIBO_DUMP_SIZE], volatile bool* stop_flag) {
    furi_assert(dump);
    furi_assert(stop_flag);

    Nfc* nfc = nfc_alloc();

    ScanCtx ctx = {
        .dump    = dump,
        .done    = false,
        .success = false,
    };

    ctx.poller = nfc_poller_alloc(nfc, NfcProtocolMfUltralight);
    nfc_poller_start(ctx.poller, scan_poller_callback, &ctx);

    // Spin until done or cancelled
    while(!ctx.done && !(*stop_flag)) {
        furi_delay_ms(50);
    }

    nfc_poller_stop(ctx.poller);
    nfc_poller_free(ctx.poller);
    nfc_free(nfc);

    return ctx.success && !(*stop_flag);
}

// ─── NFC write ────────────────────────────────────────────────────────────────

bool nfc_write_tag(const uint8_t dump[AMIIBO_DUMP_SIZE], volatile bool* stop_flag) {
    furi_assert(dump);
    furi_assert(stop_flag);

    // TODO: use mf_ultralight_poller_write_page() in a poller callback loop
    FURI_LOG_W(TAG, "nfc_write_tag: not yet implemented — use Save to SD");
    UNUSED(stop_flag);
    return false;
}
