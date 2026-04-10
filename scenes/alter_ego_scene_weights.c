#include "../alter_ego_app.h"
#include "../amiibo_smash.h"

#include <gui/canvas.h>
#include <input/input.h>
#include <furi.h>

#define TAG "SceneWeights"

// ─── Display constants ────────────────────────────────────────────────────────

#define ROWS_VISIBLE    4     // weight rows shown at once
#define LABEL_COL       0
#define LABEL_W         74    // pixels for the move name column
#define VALUE_COL       76
#define VALUE_W         50
#define ROW_H           12    // pixels per row
#define HEADER_H        11    // top header bar height
#define WEIGHT_STEP     50    // how much ±each button press changes a weight

// ─── Draw callback ────────────────────────────────────────────────────────────

static void weights_draw_cb(Canvas* canvas, void* ctx) {
    AlterEgoApp* app = ctx;
    SmashFigure* f   = &app->figure;

    canvas_clear(canvas);

    // ── Header ────────────────────────────────────────────────────────────
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_box(canvas, 0, 0, 128, HEADER_H);
    canvas_invert_color(canvas);
    canvas_draw_str_aligned(canvas, 64, 1, AlignCenter, AlignTop, "Move Weights");
    canvas_invert_color(canvas);

    // ── Column headers ────────────────────────────────────────────────────
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, LABEL_COL + 1, HEADER_H + 8, "Move");
    canvas_draw_str(canvas, VALUE_COL + 2, HEADER_H + 8, "Weight");
    canvas_draw_line(canvas, 0, HEADER_H + 9, 127, HEADER_H + 9);

    int y_start = HEADER_H + 11;

    // ── Visible rows ──────────────────────────────────────────────────────
    for(int row = 0; row < ROWS_VISIBLE; row++) {
        int idx = app->weight_offset + row;
        if(idx >= SSBU_WEIGHT_COUNT) break;

        int y = y_start + row * ROW_H;
        bool selected = (idx == app->weight_selected);

        if(selected) {
            canvas_draw_box(canvas, 0, y - 1, 128, ROW_H);
            canvas_invert_color(canvas);
        }

        // Move label — truncate name to fit in the 74-px label column
        char lbl[32];
        char name[13];
        snprintf(name, sizeof(name), "%s", ssbu_weight_label((uint8_t)idx));
        snprintf(lbl, sizeof(lbl), "%02d:%-12s", idx, name);
        canvas_draw_str(canvas, LABEL_COL + 1, y + 8, lbl);

        // Weight value
        char val[10];
        snprintf(val, sizeof(val), "%+d", (int)f->weights[idx]);
        canvas_draw_str_aligned(canvas, 127, y + 8, AlignRight, AlignBottom, val);

        if(selected) canvas_invert_color(canvas);
    }

    // ── Scroll indicator ──────────────────────────────────────────────────
    int sb_height = (SSBU_WEIGHT_COUNT > 0) ?
        (ROWS_VISIBLE * (64 - y_start)) / SSBU_WEIGHT_COUNT : 0;
    int sb_y = y_start + app->weight_offset * (64 - y_start) / SSBU_WEIGHT_COUNT;
    if(sb_height < 4) sb_height = 4;
    canvas_draw_box(canvas, 126, sb_y, 2, sb_height);

    // ── Bottom hint ───────────────────────────────────────────────────────
    // (shown only when not occupying the last row)
    if(y_start + ROWS_VISIBLE * ROW_H < 64) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 63, AlignCenter, AlignBottom,
                                "L/R: +/-50  OK: reset sel");
    }
}

// ─── Input callback ───────────────────────────────────────────────────────────

static bool weights_input_cb(InputEvent* event, void* ctx) {
    AlterEgoApp* app = ctx;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat)
        return false;

    SmashFigure* f = &app->figure;
    bool consumed  = true;

    switch(event->key) {
    case InputKeyUp:
        if(app->weight_selected > 0) {
            app->weight_selected--;
            if(app->weight_selected < app->weight_offset)
                app->weight_offset = app->weight_selected;
        }
        break;

    case InputKeyDown:
        if(app->weight_selected < SSBU_WEIGHT_COUNT - 1) {
            app->weight_selected++;
            if(app->weight_selected >= app->weight_offset + ROWS_VISIBLE)
                app->weight_offset = app->weight_selected - ROWS_VISIBLE + 1;
        }
        break;

    case InputKeyRight:
        f->weights[app->weight_selected] =
            (int16_t)CLAMP(f->weights[app->weight_selected] + WEIGHT_STEP,
                           SSBU_WEIGHT_MAX, SSBU_WEIGHT_MIN);
        f->dirty = true;
        break;

    case InputKeyLeft:
        f->weights[app->weight_selected] =
            (int16_t)CLAMP(f->weights[app->weight_selected] - WEIGHT_STEP,
                           SSBU_WEIGHT_MAX, SSBU_WEIGHT_MIN);
        f->dirty = true;
        break;

    case InputKeyOk:
        // Reset selected weight to 0
        f->weights[app->weight_selected] = 0;
        f->dirty = true;
        break;

    case InputKeyBack:
        consumed = false;  // let scene manager handle
        break;

    default:
        consumed = false;
        break;
    }

    // Returning true signals ViewDispatcher to redraw
    return consumed;
}

// ─── Scene ────────────────────────────────────────────────────────────────────

void alter_ego_scene_weights_on_enter(void* ctx) {
    AlterEgoApp* app = ctx;

    // Move Weights is only supported for SSBU — pop back immediately if a
    // non-SSBU tag reaches this scene (should not happen via normal UI flow,
    // but serves as a belt-and-suspenders guard).
    if(!smash_is_weights_supported(app->figure.game)) {
        FURI_LOG_W(TAG, "Weights scene reached for non-SSBU tag (%s) — aborting",
                   smash_game_name(app->figure.game));
        scene_manager_previous_scene(app->scene_manager);
        return;
    }

    view_set_draw_callback(app->weights_view, weights_draw_cb);
    view_set_input_callback(app->weights_view, weights_input_cb);
    view_set_context(app->weights_view, app);

    /* restore scroll position if coming back */
    // (weight_offset and weight_selected persist in app struct)

    view_dispatcher_switch_to_view(app->view_dispatcher, AlterEgoViewWeights);
}

bool alter_ego_scene_weights_on_event(void* ctx, SceneManagerEvent e) {
    UNUSED(ctx);
    UNUSED(e);
    return false;  // all input handled by view callback
}

void alter_ego_scene_weights_on_exit(void* ctx) {
    UNUSED(ctx);
    // weight_offset/selected stay in app struct for next visit
}
