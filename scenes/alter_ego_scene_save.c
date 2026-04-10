#include "../alter_ego_app.h"
#include "../nfc_helper.h"
#include "../amiibo_crypto.h"
#include "../amiibo_smash.h"

#include <gui/modules/dialog_ex.h>
#include <furi.h>

#define TAG "SceneSave"

// ─── Dialog result callback ───────────────────────────────────────────────────

static void save_dialog_cb(DialogExResult result, void* ctx) {
    AlterEgoApp* app = ctx;
    AlterEgoEvent evt;
    switch(result) {
    case DialogExResultLeft:   evt = AlterEgoEventSaveNfc; break;
    case DialogExResultCenter: evt = AlterEgoEventSaveSD;  break;
    default:                   evt = AlterEgoEventBack;    break;
    }
    view_dispatcher_send_custom_event(app->view_dispatcher, evt);
}

// ─── Scene ────────────────────────────────────────────────────────────────────

void alter_ego_scene_save_on_enter(void* ctx) {
    AlterEgoApp* app = ctx;

    dialog_ex_reset(app->dialog);
    dialog_ex_set_header(app->dialog, "Save Changes", 64, 0, AlignCenter, AlignTop);
    dialog_ex_set_text(app->dialog,
                       "How would you like\nto save?",
                       64, 28, AlignCenter, AlignCenter);
    dialog_ex_set_left_button_text(app->dialog,   "Write NFC");
    dialog_ex_set_center_button_text(app->dialog, "Save .nfc");
    dialog_ex_set_right_button_text(app->dialog,  "Discard");
    dialog_ex_set_result_callback(app->dialog, save_dialog_cb);
    dialog_ex_set_context(app->dialog, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, AlterEgoViewDialog);
}

bool alter_ego_scene_save_on_event(void* ctx, SceneManagerEvent e) {
    AlterEgoApp* app = ctx;
    bool consumed = false;

    if(e.type == SceneManagerEventTypeCustom) {
        switch((AlterEgoEvent)e.event) {

        // ── Save to SD card ───────────────────────────────────────────────
        case AlterEgoEventSaveSD: {
            // Serialize figure player data back into dump
            ssbu_serialize(&app->figure, app->dump);
            // Re-encrypt and sign
            amiibo_crypto_encrypt(app->dump, &app->keys);
            app->data_dirty = false;
            app->figure.dirty = false;

            // Build save path: /ext/amiibo/alterego_<uid>.nfc
            uint8_t uid[AMIIBO_UID_SIZE];
            amiibo_crypto_extract_uid(app->dump, uid);
            char path[256];
            snprintf(path, sizeof(path),
                     "/ext/amiibo/alterego_%02X%02X%02X%02X%02X%02X%02X.nfc",
                     uid[0], uid[1], uid[2], uid[3],
                     uid[4], uid[5], uid[6]);
            strncpy(app->save_path, path, sizeof(app->save_path) - 1);

            if(nfc_save_file(app->save_path, app->dump)) {
                // Show confirmation via text_box then go back to menu
                text_box_reset(app->text_box);
                text_box_set_text(app->text_box,
                    "Saved!\n\nUse Flipper NFC app\nto write to physical\namiibo.");
                text_box_set_font(app->text_box, TextBoxFontText);
                view_dispatcher_switch_to_view(app->view_dispatcher, AlterEgoViewError);
                // After user presses Back, scene_error will pop us to menu
            } else {
                alter_ego_show_error(app, "SD save failed.");
            }
            consumed = true;
            break;
        }

        // ── Write directly to NFC ──────────────────────────────────────────
        case AlterEgoEventSaveNfc:
            ssbu_serialize(&app->figure, app->dump);
            amiibo_crypto_encrypt(app->dump, &app->keys);
            // NFC write not yet implemented — fall through to informational error
            alter_ego_show_error(app,
                "Direct NFC write\nnot yet supported.\n\nUse 'Save .nfc' then\nwrite via NFC app.");
            consumed = true;
            break;

        // ── Discard ───────────────────────────────────────────────────────
        case AlterEgoEventBack:
            app->figure.dirty = false;
            app->data_dirty   = false;
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, AlterEgoSceneMainMenu);
            consumed = true;
            break;

        default:
            break;
        }
    }

    return consumed;
}

void alter_ego_scene_save_on_exit(void* ctx) {
    AlterEgoApp* app = ctx;
    dialog_ex_reset(app->dialog);
}
