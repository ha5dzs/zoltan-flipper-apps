#define FURI_LOG_TAG "GuiExamples"
#include "gui_examples.h"

static void popup_callback(void* context) {
    GlobalVariableStruct* app = context;
    FURI_LOG_D("GuiExamples", "popup_callback BEGIN");

    // Navigate back to main menu when popup is dismissed
    scene_manager_next_scene(app->scene_manager, SceneMain);
    FURI_LOG_D("GuiExamples", "popup_callback END");
}

void scene_popup_on_enter(void* context) {
    GlobalVariableStruct* app = context;
    FURI_LOG_D("GuiExamples", "scene_popup_on_enter BEGIN");

    popup_reset(app->popup);
    popup_set_header(app->popup, "Popup Demo", 64, 10, AlignCenter, AlignTop);
    popup_set_text(app->popup, "This popup will auto-dismiss\nafter 3 seconds.", 64, 25, AlignCenter, AlignTop);
    popup_set_callback(app->popup, popup_callback);
    popup_set_context(app->popup, app);
    popup_set_timeout(app->popup, 3000);  // 3 seconds
    popup_enable_timeout(app->popup);

    view_dispatcher_switch_to_view(app->view_dispatcher, ScenePopup);
    FURI_LOG_D("GuiExamples", "scene_popup_on_enter END");
}

bool scene_popup_on_event(void* context, SceneManagerEvent event) {
    GlobalVariableStruct* app = context;
    FURI_LOG_D("GuiExamples", "scene_popup_on_event BEGIN: type=%d", event.type);

    // Allow back to dismiss early and return to main menu
    if(event.type == SceneManagerEventTypeBack) {
        FURI_LOG_D("GuiExamples", "Popup back pressed - returning to main");
        popup_disable_timeout(app->popup);
        popup_reset(app->popup);
        scene_manager_next_scene(app->scene_manager, SceneMain);
        FURI_LOG_D("GuiExamples", "scene_popup_on_event END: consumed (back)");
        return true;
    }
    FURI_LOG_D("GuiExamples", "scene_popup_on_event END: not consumed");
    return false;
}

void scene_popup_on_exit(void* context) {
    GlobalVariableStruct* app = context;
    FURI_LOG_D("GuiExamples", "scene_popup_on_exit BEGIN");

    // Disable timeout and reset popup on exit to prevent callback after free
    popup_disable_timeout(app->popup);
    popup_reset(app->popup);
    FURI_LOG_D("GuiExamples", "scene_popup_on_exit END");
}
