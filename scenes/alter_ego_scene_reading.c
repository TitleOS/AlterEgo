#include "../alter_ego_app.h"
#include "../nfc_helper.h"
#include "../amiibo_crypto.h"
#include "../amiibo_smash.h"

#include <furi.h>
#include <furi_hal.h>

#define TAG "SceneReading"

// ─── NFC worker thread ────────────────────────────────────────────────────────

typedef struct {
    AlterEgoApp* app;
} NfcWorkerArgs;

static int32_t nfc_worker_thread(void* context) {
    NfcWorkerArgs* args = context;
    AlterEgoApp*   app  = args->app;
    free(args);

    bool ok = nfc_scan_tag(app->dump, &app->nfc_stop_flag);

    if(!app->nfc_stop_flag) {
        AlterEgoEvent evt = ok ? AlterEgoEventNfcOk : AlterEgoEventNfcFail;
        view_dispatcher_send_custom_event(app->view_dispatcher, evt);
    }

    return 0;
}

// ─── Scene ────────────────────────────────────────────────────────────────────

void alter_ego_scene_reading_on_enter(void* ctx) {
    AlterEgoApp* app = ctx;

    app->nfc_stop_flag = false;
    view_dispatcher_switch_to_view(app->view_dispatcher, AlterEgoViewProgress);

    // Spawn worker
    NfcWorkerArgs* args = malloc(sizeof(NfcWorkerArgs));
    args->app = app;

    app->nfc_thread = furi_thread_alloc();
    furi_thread_set_name(app->nfc_thread, "AlterEgoNfc");
    furi_thread_set_stack_size(app->nfc_thread, 4096);
    furi_thread_set_callback(app->nfc_thread, nfc_worker_thread);
    furi_thread_set_context(app->nfc_thread, args);
    furi_thread_start(app->nfc_thread);
}

bool alter_ego_scene_reading_on_event(void* ctx, SceneManagerEvent e) {
    AlterEgoApp* app = ctx;
    bool consumed = false;

    if(e.type == SceneManagerEventTypeCustom) {
        switch((AlterEgoEvent)e.event) {

        case AlterEgoEventNfcOk:
            // NFC read succeeded — decrypt and parse
            if(!amiibo_crypto_decrypt(app->dump, &app->keys)) {
                alter_ego_show_error(app, "Decrypt failed.\nWrong keys or bad tag.");
            } else if(!smash_parse(app->dump, &app->figure)) {
                // Show the actual App ID on-screen so the user can report it
                // without needing a serial/log connection.
                static char id_err[48];
                const uint8_t* p = app->dump + AMIIBO_USER_OFFSET + AMIIBO_OFF_APPID;
                uint32_t raw_id = (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 |
                                  (uint32_t)p[2] << 8  |            p[3];
                snprintf(id_err, sizeof(id_err),
                         "Not a Smash tag.\nAppID:%08lX", (unsigned long)raw_id);
                alter_ego_show_error(app, id_err);
            } else {
                app->data_loaded = true;
                app->data_dirty  = false;
                scene_manager_next_scene(app->scene_manager, AlterEgoSceneEditor);
            }
            consumed = true;
            break;

        case AlterEgoEventNfcFail:
            alter_ego_show_error(app, "No amiibo detected.\nTry again.");
            consumed = true;
            break;

        default:
            break;
        }
    } else if(e.type == SceneManagerEventTypeBack) {
        // User pressed Back — cancel NFC scan
        app->nfc_stop_flag = true;
        consumed = true;   // let scene_manager pop naturally after thread finishes
    }

    return consumed;
}

void alter_ego_scene_reading_on_exit(void* ctx) {
    AlterEgoApp* app = ctx;

    // Ensure worker has stopped
    if(app->nfc_thread) {
        app->nfc_stop_flag = true;
        furi_thread_join(app->nfc_thread);
        furi_thread_free(app->nfc_thread);
        app->nfc_thread = NULL;
    }
    app->nfc_stop_flag = false;
}
