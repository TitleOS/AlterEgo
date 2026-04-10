#include "../alter_ego_app.h"
#include "../amiibo_smash.h"

#include <gui/modules/variable_item_list.h>
#include <furi.h>

#define TAG "SceneEditor"

// ─── Field descriptors ────────────────────────────────────────────────────────

// Each editable field has a getter/setter and a label.
// We use VariableItemList: each item shows current value as a string.

#define LEVEL_STEP  1
#define STAT_STEP   100   // 100 = 1.0 display unit

// Encoded value range for VariableItemList (which uses uint8_t index).
// We remap signed stat range –5000..+5000 (101 steps of 100) to index 0..100.
#define STAT_INDICES   101   // 0=–5000, 50=0, 100=+5000

static const char* PERSONA_LABELS[SsbuPersonaCount] = {
    "Normal", "Aggressive", "Calm", "Careless", "Cautious", "Tricky", "Joyful",
};

// ─── VariableItemList callbacks ───────────────────────────────────────────────

static void level_changed(VariableItem* item) {
    AlterEgoApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->figure.level = SSBU_LEVEL_MIN + idx;  // idx 0 = level 1
    app->figure.dirty = true;

    char buf[8];
    snprintf(buf, sizeof(buf), "%u", app->figure.level);
    variable_item_set_current_value_text(item, buf);
}

static void attack_changed(VariableItem* item) {
    AlterEgoApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->figure.attack = (int16_t)(SSBU_STAT_MIN + (int32_t)idx * STAT_STEP);
    app->figure.dirty  = true;

    char buf[10];
    snprintf(buf, sizeof(buf), "%+d", (int)app->figure.attack);
    variable_item_set_current_value_text(item, buf);
}

static void defense_changed(VariableItem* item) {
    AlterEgoApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->figure.defense = (int16_t)(SSBU_STAT_MIN + (int32_t)idx * STAT_STEP);
    app->figure.dirty   = true;

    char buf[10];
    snprintf(buf, sizeof(buf), "%+d", (int)app->figure.defense);
    variable_item_set_current_value_text(item, buf);
}

static void speed_changed(VariableItem* item) {
    AlterEgoApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->figure.speed = (int16_t)(SSBU_STAT_MIN + (int32_t)idx * STAT_STEP);
    app->figure.dirty = true;

    char buf[10];
    snprintf(buf, sizeof(buf), "%+d", (int)app->figure.speed);
    variable_item_set_current_value_text(item, buf);
}

static void persona_changed(VariableItem* item) {
    AlterEgoApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->figure.persona = idx;
    app->figure.dirty   = true;
    variable_item_set_current_value_text(item, PERSONA_LABELS[idx]);
}

// ─── Enter callback (OK on an item) ──────────────────────────────────────────

static void editor_enter_callback(void* ctx, uint32_t index) {
    AlterEgoApp* app = ctx;
    // The "Move Weights →" entry is only present for SSBU; when present it sits
    // at index AlterEgoFieldCount (0-based after the 5 stat fields).
    // The "Game" read-only label is index 0, stat fields are 1..5.
    // Weights entry (when present) is index 6 = AlterEgoFieldCount + 1 (label).
    // We compare against the total VariableItemList count minus 1.
    if(smash_is_weights_supported(app->figure.game) &&
       index == (uint32_t)(AlterEgoFieldCount + 1)) {
        view_dispatcher_send_custom_event(app->view_dispatcher, AlterEgoEventEditWeights);
    }
}

// ─── Scene ────────────────────────────────────────────────────────────────────

void alter_ego_scene_editor_on_enter(void* ctx) {
    AlterEgoApp* app = ctx;
    VariableItemList* vl = app->var_list;

    variable_item_list_reset(vl);
    variable_item_list_set_enter_callback(vl, editor_enter_callback, app);

    SmashFigure* f = &app->figure;

    // ── Game label (read-only, 1 value) ───────────────────────────────────
    {
        VariableItem* item = variable_item_list_add(
            vl, "Game", 1, NULL, app);
        variable_item_set_current_value_text(item, smash_game_name(f->game));
        UNUSED(item);
    }

    // ── Level ─────────────────────────────────────────────────────────────
    {
        VariableItem* item = variable_item_list_add(
            vl, "Level", SSBU_LEVEL_MAX - SSBU_LEVEL_MIN + 1,
            level_changed, app);
        uint8_t idx = (uint8_t)(f->level - SSBU_LEVEL_MIN);
        variable_item_set_current_value_index(item, idx);
        char buf[8];
        snprintf(buf, sizeof(buf), "%u", f->level);
        variable_item_set_current_value_text(item, buf);
    }

    // ── Attack ────────────────────────────────────────────────────────────
    {
        VariableItem* item = variable_item_list_add(
            vl, "Attack", STAT_INDICES, attack_changed, app);
        uint8_t idx = (uint8_t)((f->attack - SSBU_STAT_MIN) / STAT_STEP);
        variable_item_set_current_value_index(item, idx);
        char buf[10];
        snprintf(buf, sizeof(buf), "%+d", (int)f->attack);
        variable_item_set_current_value_text(item, buf);
    }

    // ── Defense ───────────────────────────────────────────────────────────
    {
        VariableItem* item = variable_item_list_add(
            vl, "Defense", STAT_INDICES, defense_changed, app);
        uint8_t idx = (uint8_t)((f->defense - SSBU_STAT_MIN) / STAT_STEP);
        variable_item_set_current_value_index(item, idx);
        char buf[10];
        snprintf(buf, sizeof(buf), "%+d", (int)f->defense);
        variable_item_set_current_value_text(item, buf);
    }

    // ── Speed ─────────────────────────────────────────────────────────────
    {
        VariableItem* item = variable_item_list_add(
            vl, "Speed", STAT_INDICES, speed_changed, app);
        uint8_t idx = (uint8_t)((f->speed - SSBU_STAT_MIN) / STAT_STEP);
        variable_item_set_current_value_index(item, idx);
        char buf[10];
        snprintf(buf, sizeof(buf), "%+d", (int)f->speed);
        variable_item_set_current_value_text(item, buf);
    }

    // ── Persona ───────────────────────────────────────────────────────────
    {
        VariableItem* item = variable_item_list_add(
            vl, "Persona", SsbuPersonaCount, persona_changed, app);
        variable_item_set_current_value_index(item, f->persona);
        variable_item_set_current_value_text(item, PERSONA_LABELS[f->persona]);
    }

    // ── "Move Weights →" entry — SSBU only ────────────────────────────────
    if(smash_is_weights_supported(f->game)) {
        VariableItem* item = variable_item_list_add(
            vl, "Move Weights", 1, NULL, app);
        variable_item_set_current_value_text(item, ">");
        UNUSED(item);
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, AlterEgoViewEditor);
}

bool alter_ego_scene_editor_on_event(void* ctx, SceneManagerEvent e) {
    AlterEgoApp* app = ctx;
    bool consumed = false;

    if(e.type == SceneManagerEventTypeCustom) {
        if((AlterEgoEvent)e.event == AlterEgoEventEditWeights) {
            scene_manager_next_scene(app->scene_manager, AlterEgoSceneWeights);
            consumed = true;
        }
    } else if(e.type == SceneManagerEventTypeBack) {
        // Back from editor → go to Save scene if dirty, else directly to menu
        if(app->figure.dirty) {
            scene_manager_next_scene(app->scene_manager, AlterEgoSceneSave);
        } else {
            scene_manager_previous_scene(app->scene_manager);
        }
        consumed = true;
    }

    return consumed;
}

void alter_ego_scene_editor_on_exit(void* ctx) {
    AlterEgoApp* app = ctx;
    variable_item_list_reset(app->var_list);
}
