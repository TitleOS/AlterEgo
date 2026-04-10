#include "alter_ego_app.h"
#include "nfc_helper.h"
#include "amiibo_crypto.h"
#include "amiibo_smash.h"

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/menu.h>
#include <gui/modules/dialog_ex.h>
#include <gui/modules/text_box.h>
#include <gui/modules/variable_item_list.h>
#include <storage/storage.h>
#include <dialogs/dialogs.h>

#define TAG "AlterEgo"

// ─── Scene Handler Table ──────────────────────────────────────────────────────

static const AppSceneOnEnterCallback alter_ego_on_enter_handlers[AlterEgoSceneCount] = {
    [AlterEgoSceneMainMenu] = alter_ego_scene_main_menu_on_enter,
    [AlterEgoSceneReading]  = alter_ego_scene_reading_on_enter,
    [AlterEgoSceneEditor]   = alter_ego_scene_editor_on_enter,
    [AlterEgoSceneWeights]  = alter_ego_scene_weights_on_enter,
    [AlterEgoSceneSave]     = alter_ego_scene_save_on_enter,
    [AlterEgoSceneError]    = alter_ego_scene_error_on_enter,
};

static const AppSceneOnEventCallback alter_ego_on_event_handlers[AlterEgoSceneCount] = {
    [AlterEgoSceneMainMenu] = alter_ego_scene_main_menu_on_event,
    [AlterEgoSceneReading]  = alter_ego_scene_reading_on_event,
    [AlterEgoSceneEditor]   = alter_ego_scene_editor_on_event,
    [AlterEgoSceneWeights]  = alter_ego_scene_weights_on_event,
    [AlterEgoSceneSave]     = alter_ego_scene_save_on_event,
    [AlterEgoSceneError]    = alter_ego_scene_error_on_event,
};

static const AppSceneOnExitCallback alter_ego_on_exit_handlers[AlterEgoSceneCount] = {
    [AlterEgoSceneMainMenu] = alter_ego_scene_main_menu_on_exit,
    [AlterEgoSceneReading]  = alter_ego_scene_reading_on_exit,
    [AlterEgoSceneEditor]   = alter_ego_scene_editor_on_exit,
    [AlterEgoSceneWeights]  = alter_ego_scene_weights_on_exit,
    [AlterEgoSceneSave]     = alter_ego_scene_save_on_exit,
    [AlterEgoSceneError]    = alter_ego_scene_error_on_exit,
};

const SceneManagerHandlers alter_ego_scene_handlers = {
    .on_enter_handlers = alter_ego_on_enter_handlers,
    .on_event_handlers = alter_ego_on_event_handlers,
    .on_exit_handlers  = alter_ego_on_exit_handlers,
    .scene_num         = AlterEgoSceneCount,
};

// ─── View navigation callback ─────────────────────────────────────────────────

static bool alter_ego_nav_event_callback(void* context) {
    AlterEgoApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static bool alter_ego_custom_event_callback(void* context, uint32_t event) {
    AlterEgoApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

// ─── Custom view draw callbacks ───────────────────────────────────────────────

// Progress / scanning animation
static void progress_view_draw(Canvas* canvas, void* ctx) {
    UNUSED(ctx);
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignCenter, "AlterEgo");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 36, AlignCenter, AlignCenter,
                            "Hold amiibo to back...");

    // Animated dots using tick counter
    static uint8_t tick = 0;
    tick = (tick + 1) % 4;
    char dots[5] = "    ";
    for(uint8_t i = 0; i < tick; i++) dots[i] = '.';
    canvas_draw_str_aligned(canvas, 64, 50, AlignCenter, AlignCenter, dots);

    // NFC icon area (simple brackets)
    canvas_draw_frame(canvas, 48, 4, 32, 12);
    canvas_draw_str_aligned(canvas, 64, 10, AlignCenter, AlignCenter, "NFC");
}

// ─── Error helper ─────────────────────────────────────────────────────────────

void alter_ego_show_error(AlterEgoApp* app, const char* msg) {
    furi_assert(app);
    furi_assert(msg);
    strncpy(app->error_msg, msg, sizeof(app->error_msg) - 1);
    app->error_msg[sizeof(app->error_msg) - 1] = '\0';
    scene_manager_next_scene(app->scene_manager, AlterEgoSceneError);
}

// ─── App allocation ───────────────────────────────────────────────────────────

static AlterEgoApp* alter_ego_app_alloc(void) {
    AlterEgoApp* app = malloc(sizeof(AlterEgoApp));
    furi_assert(app);
    memset(app, 0, sizeof(AlterEgoApp));

    app->gui      = furi_record_open(RECORD_GUI);
    app->dialogs  = furi_record_open(RECORD_DIALOGS);

    // Scene manager
    app->scene_manager = scene_manager_alloc(&alter_ego_scene_handlers, app);

    // View dispatcher
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, alter_ego_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, alter_ego_nav_event_callback);

    // ── Standard widgets ──────────────────────────────────────────────────
    app->menu = menu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, AlterEgoViewMainMenu, menu_get_view(app->menu));

    app->var_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, AlterEgoViewEditor,
        variable_item_list_get_view(app->var_list));

    app->dialog = dialog_ex_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, AlterEgoViewDialog, dialog_ex_get_view(app->dialog));

    app->text_box = text_box_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, AlterEgoViewError, text_box_get_view(app->text_box));

    // ── Custom views ──────────────────────────────────────────────────────
    app->progress_view = view_alloc();
    view_set_draw_callback(app->progress_view, progress_view_draw);
    view_set_context(app->progress_view, app);
    view_dispatcher_add_view(
        app->view_dispatcher, AlterEgoViewProgress, app->progress_view);

    app->weights_view = view_alloc();
    view_set_context(app->weights_view, app);
    // draw/input callbacks are set in scene_weights_on_enter
    view_dispatcher_add_view(
        app->view_dispatcher, AlterEgoViewWeights, app->weights_view);

    // Save scene uses dialog widget
    view_dispatcher_add_view(
        app->view_dispatcher, AlterEgoViewSave, dialog_ex_get_view(app->dialog));

    view_dispatcher_attach_to_gui(
        app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    // ── Try to load keys ──────────────────────────────────────────────────
    app->keys_loaded = amiibo_crypto_load_keys(&app->keys);
    if(!app->keys_loaded)
        FURI_LOG_W(TAG, "key_retail.bin not found at %s", KEYS_PATH);

    return app;
}

// ─── App free ─────────────────────────────────────────────────────────────────

static void alter_ego_app_free(AlterEgoApp* app) {
    furi_assert(app);

    // Stop NFC thread if running
    if(app->nfc_thread) {
        app->nfc_stop_flag = true;
        furi_thread_join(app->nfc_thread);
        furi_thread_free(app->nfc_thread);
        app->nfc_thread = NULL;
    }

    // Remove views
    view_dispatcher_remove_view(app->view_dispatcher, AlterEgoViewMainMenu);
    view_dispatcher_remove_view(app->view_dispatcher, AlterEgoViewEditor);
    view_dispatcher_remove_view(app->view_dispatcher, AlterEgoViewProgress);
    view_dispatcher_remove_view(app->view_dispatcher, AlterEgoViewWeights);
    view_dispatcher_remove_view(app->view_dispatcher, AlterEgoViewDialog);
    view_dispatcher_remove_view(app->view_dispatcher, AlterEgoViewError);
    // AlterEgoViewSave shares dialog widget — already removed above

    // Free widgets
    menu_free(app->menu);
    variable_item_list_free(app->var_list);
    dialog_ex_free(app->dialog);
    text_box_free(app->text_box);
    view_free(app->progress_view);
    view_free(app->weights_view);

    // Free framework objects
    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    furi_record_close(RECORD_DIALOGS);
    furi_record_close(RECORD_GUI);

    free(app);
}

// ─── Entry point ─────────────────────────────────────────────────────────────

int32_t alter_ego_app(void* p) {
    UNUSED(p);
    FURI_LOG_I(TAG, "Starting AlterEgo");

    AlterEgoApp* app = alter_ego_app_alloc();

    scene_manager_next_scene(app->scene_manager, AlterEgoSceneMainMenu);
    view_dispatcher_run(app->view_dispatcher);

    alter_ego_app_free(app);

    FURI_LOG_I(TAG, "AlterEgo done");
    return 0;
}
