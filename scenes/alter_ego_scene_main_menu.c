#include "../alter_ego_app.h"
#include "../nfc_helper.h"

#include <gui/modules/menu.h>
#include <dialogs/dialogs.h>
#include <furi.h>

#define TAG "SceneMainMenu"

// ─── Menu item indices ────────────────────────────────────────────────────────

typedef enum {
    MainMenuItemScan = 0,
    MainMenuItemOpenFile,
    MainMenuItemAbout,
} MainMenuItem;

// ─── Callbacks ────────────────────────────────────────────────────────────────

static void menu_callback(void* ctx, uint32_t index) {
    AlterEgoApp* app = ctx;
    AlterEgoEvent evt = (index == MainMenuItemScan)     ? AlterEgoEventScanNfc :
                        (index == MainMenuItemOpenFile)  ? AlterEgoEventOpenFile :
                                                           AlterEgoEventAbout;
    view_dispatcher_send_custom_event(app->view_dispatcher, evt);
}

// ─── Scene ────────────────────────────────────────────────────────────────────

void alter_ego_scene_main_menu_on_enter(void* ctx) {
    AlterEgoApp* app = ctx;

    menu_reset(app->menu);

    menu_add_item(app->menu, "Scan Amiibo",  NULL, MainMenuItemScan,     menu_callback, app);
    menu_add_item(app->menu, "Open .nfc File", NULL, MainMenuItemOpenFile, menu_callback, app);
    menu_add_item(app->menu, "About",         NULL, MainMenuItemAbout,   menu_callback, app);

    // Restore last selected index
    menu_set_selected_item(
        app->menu,
        scene_manager_get_scene_state(app->scene_manager, AlterEgoSceneMainMenu));

    view_dispatcher_switch_to_view(app->view_dispatcher, AlterEgoViewMainMenu);
}

bool alter_ego_scene_main_menu_on_event(void* ctx, SceneManagerEvent e) {
    AlterEgoApp* app = ctx;
    bool consumed = false;

    if(e.type == SceneManagerEventTypeCustom) {
        switch((AlterEgoEvent)e.event) {

        // ── Scan NFC ──────────────────────────────────────────────────────
        case AlterEgoEventScanNfc:
            if(!app->keys_loaded) {
                alter_ego_show_error(app,
                    "key_retail.bin not found!\n\n"
                    "Copy it to:\n"
                    "/ext/amiibo/key_retail.bin");
            } else {
                scene_manager_next_scene(app->scene_manager, AlterEgoSceneReading);
            }
            consumed = true;
            break;

        // ── Open .nfc file via Flipper file browser ────────────────────────
        case AlterEgoEventOpenFile: {
            if(!app->keys_loaded) {
                alter_ego_show_error(app,
                    "key_retail.bin not found!\n\n"
                    "Copy it to:\n"
                    "/ext/amiibo/key_retail.bin");
                break;
            }

            // Open Flipper file browser dialog
            DialogsFileBrowserOptions opts;
            dialog_file_browser_set_basic_options(
                &opts, ".nfc", NULL);
            opts.base_path = "/ext/nfc";

            FuriString* path = furi_string_alloc_set("/ext/nfc");
            bool picked = dialog_file_browser_show(app->dialogs, path, path, &opts);

            if(picked) {
                strncpy(app->save_path, furi_string_get_cstr(path),
                        sizeof(app->save_path) - 1);

                if(!nfc_load_file(app->save_path, app->dump)) {
                    alter_ego_show_error(app, "Failed to read\n.nfc file.");
                } else if(!amiibo_crypto_decrypt(app->dump, &app->keys)) {
                    alter_ego_show_error(app, "Decrypt failed.\nBad keys or corrupt tag.");
                } else if(!smash_parse(app->dump, &app->figure)) {
                    alter_ego_show_error(app, "Not a Smash Bros.\namiibo.");
                } else {
                    app->data_loaded = true;
                    app->data_dirty  = false;
                    scene_manager_next_scene(app->scene_manager, AlterEgoSceneEditor);
                }
            }
            furi_string_free(path);
            consumed = true;
            break;
        }

        // ── About ─────────────────────────────────────────────────────────
        case AlterEgoEventAbout:
            // Reuse text_box for an about screen
            text_box_reset(app->text_box);
            text_box_set_text(app->text_box,
                "AlterEgo v1.1\n"
                "Smash Bros. Amiibo\n"
                "AI Editor\n\n"
                "Supports SSBU, SSB4-U,\nSSB4-3DS\n\n"
                "Requires key_retail.bin\nat /ext/amiibo/\n\n"
                "Target: Momentum FW");
            text_box_set_font(app->text_box, TextBoxFontText);
            view_dispatcher_switch_to_view(app->view_dispatcher, AlterEgoViewError);
            consumed = true;
            break;

        default:
            break;
        }
    }
    return consumed;
}

void alter_ego_scene_main_menu_on_exit(void* ctx) {
    AlterEgoApp* app = ctx;
    // Save selected index between visits
    scene_manager_set_scene_state(
        app->scene_manager,
        AlterEgoSceneMainMenu,
        menu_get_selected_item(app->menu));
    menu_reset(app->menu);
}
