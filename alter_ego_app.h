#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "amiibo_crypto.h"
#include "amiibo_smash.h"

// ─── View IDs ─────────────────────────────────────────────────────────────────

typedef enum {
    AlterEgoViewMainMenu = 0,
    AlterEgoViewProgress,
    AlterEgoViewEditor,
    AlterEgoViewWeights,
    AlterEgoViewSave,
    AlterEgoViewError,
    AlterEgoViewDialog,
    AlterEgoViewCount,
} AlterEgoViewId;

// ─── Scene IDs ────────────────────────────────────────────────────────────────

typedef enum {
    AlterEgoSceneMainMenu = 0,
    AlterEgoSceneReading,
    AlterEgoSceneEditor,
    AlterEgoSceneWeights,
    AlterEgoSceneSave,
    AlterEgoSceneError,
    AlterEgoSceneCount,
} AlterEgoSceneId;

// ─── Custom events ────────────────────────────────────────────────────────────

typedef enum {
    AlterEgoEventScanNfc    = 0,
    AlterEgoEventOpenFile,
    AlterEgoEventAbout,
    AlterEgoEventNfcOk,
    AlterEgoEventNfcFail,
    AlterEgoEventEditWeights,
    AlterEgoEventSaveNfc,
    AlterEgoEventSaveSD,
    AlterEgoEventOk,
    AlterEgoEventBack,
} AlterEgoEvent;

// ─── Editor field order ───────────────────────────────────────────────────────

typedef enum {
    AlterEgoFieldLevel = 0,
    AlterEgoFieldAttack,
    AlterEgoFieldDefense,
    AlterEgoFieldSpeed,
    AlterEgoFieldPersona,
    AlterEgoFieldCount,
} AlterEgoField;

// ─── App struct ───────────────────────────────────────────────────────────────

#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/menu.h>
#include <gui/modules/dialog_ex.h>
#include <gui/modules/text_box.h>
#include <gui/modules/variable_item_list.h>
#include <storage/storage.h>
#include <dialogs/dialogs.h>
#include <furi.h>
#include <furi_hal.h>

typedef struct {
    // ── Flipper subsystems ──
    Gui*               gui;
    ViewDispatcher*    view_dispatcher;
    SceneManager*      scene_manager;
    DialogsApp*        dialogs;

    // ── Stock widgets ──
    Menu*              menu;
    DialogEx*          dialog;
    TextBox*           text_box;
    VariableItemList*  var_list;   // stat editor

    // ── Custom views ──
    View*              progress_view;  // animated NFC scan
    View*              weights_view;   // move weight table

    // ── NFC worker thread ──
    FuriThread*        nfc_thread;
    volatile bool      nfc_stop_flag;

    // ── Raw data ──
    uint8_t  dump[AMIIBO_DUMP_SIZE];   // full 540-byte NTAG215 dump
    bool     keys_loaded;
    bool     data_loaded;
    bool     data_dirty;

    // ── Parsed objects ──
    AmiiboKeyRetail  keys;
    SmashFigure      figure;

    // ── UI state ──
    int   weight_offset;    // first visible row in weight table
    int   weight_selected;  // currently highlighted row
    char  error_msg[160];   // message shown in error scene
    char  save_path[256];   // last save path
} AlterEgoApp;

// ─── Scene handler table (defined in alter_ego_app.c) ────────────────────────

extern const SceneManagerHandlers alter_ego_scene_handlers;

// ─── Per-scene declarations ───────────────────────────────────────────────────

void alter_ego_scene_main_menu_on_enter(void* ctx);
bool alter_ego_scene_main_menu_on_event(void* ctx, SceneManagerEvent e);
void alter_ego_scene_main_menu_on_exit(void* ctx);

void alter_ego_scene_reading_on_enter(void* ctx);
bool alter_ego_scene_reading_on_event(void* ctx, SceneManagerEvent e);
void alter_ego_scene_reading_on_exit(void* ctx);

void alter_ego_scene_editor_on_enter(void* ctx);
bool alter_ego_scene_editor_on_event(void* ctx, SceneManagerEvent e);
void alter_ego_scene_editor_on_exit(void* ctx);

void alter_ego_scene_weights_on_enter(void* ctx);
bool alter_ego_scene_weights_on_event(void* ctx, SceneManagerEvent e);
void alter_ego_scene_weights_on_exit(void* ctx);

void alter_ego_scene_save_on_enter(void* ctx);
bool alter_ego_scene_save_on_event(void* ctx, SceneManagerEvent e);
void alter_ego_scene_save_on_exit(void* ctx);

void alter_ego_scene_error_on_enter(void* ctx);
bool alter_ego_scene_error_on_event(void* ctx, SceneManagerEvent e);
void alter_ego_scene_error_on_exit(void* ctx);

// ─── Helper ───────────────────────────────────────────────────────────────────

/**
 * Set the error message and switch to the error scene.
 */
void alter_ego_show_error(AlterEgoApp* app, const char* msg);
