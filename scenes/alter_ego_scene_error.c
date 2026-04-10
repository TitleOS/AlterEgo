#include "../alter_ego_app.h"

#include <gui/modules/text_box.h>
#include <furi.h>

#define TAG "SceneError"

void alter_ego_scene_error_on_enter(void* ctx) {
    AlterEgoApp* app = ctx;

    text_box_reset(app->text_box);
    text_box_set_text(app->text_box, app->error_msg);
    text_box_set_font(app->text_box, TextBoxFontText);

    view_dispatcher_switch_to_view(app->view_dispatcher, AlterEgoViewError);
}

bool alter_ego_scene_error_on_event(void* ctx, SceneManagerEvent e) {
    AlterEgoApp* app = ctx;
    bool consumed = false;

    if(e.type == SceneManagerEventTypeBack) {
        // Pop the error scene and go back to wherever we came from
        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    }

    return consumed;
}

void alter_ego_scene_error_on_exit(void* ctx) {
    AlterEgoApp* app = ctx;
    text_box_reset(app->text_box);
    // Clear error message
    app->error_msg[0] = '\0';
}
