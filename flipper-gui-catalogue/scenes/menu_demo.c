#define FURI_LOG_TAG "GuiExamples"
#include "gui_examples.h"

static void menu_callback(void* context, uint32_t index) {
    GlobalVariableStruct* app = context;
    FURI_LOG_D("GuiExamples", "menu_callback BEGIN: index=%lu", index);

    if(index == 0) {
        // Option 1 - stay on this scene
        FURI_LOG_D("GuiExamples", "Option 1 selected");
    } else if(index == 1) {
        // Option 2 - navigate back to main
        scene_manager_next_scene(app->scene_manager, SceneMain);
    }
    FURI_LOG_D("GuiExamples", "menu_callback END");
}

void scene_menu_on_enter(void* context) {
    GlobalVariableStruct* app = context;
    FURI_LOG_D("GuiExamples", "scene_menu_on_enter BEGIN");

    FURI_LOG_D("GuiExamples", "Menu scene entered");

    // Reset and configure menu
    menu_reset(app->menu);
    menu_set_selected_item(app->menu, 0);

    // Add menu items (using NULL icon for simplicity)
    menu_add_item(app->menu, "Option 1", NULL, 0, menu_callback, app);
    menu_add_item(app->menu, "Back", NULL, 1, menu_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, SceneMenu);
    FURI_LOG_D("GuiExamples", "scene_menu_on_enter END");
}

bool scene_menu_on_event(void* context, SceneManagerEvent event) {
    GlobalVariableStruct* app = context;
    FURI_LOG_D("GuiExamples", "scene_menu_on_event BEGIN: type=%d", event.type);

    if(event.type == SceneManagerEventTypeBack) {
        FURI_LOG_D("GuiExamples", "Menu back pressed - returning to main");
        scene_manager_next_scene(app->scene_manager, SceneMain);
        FURI_LOG_D("GuiExamples", "scene_menu_on_event END: consumed (back)");
        return true;
    }
    FURI_LOG_D("GuiExamples", "scene_menu_on_event END: not consumed");
    return false;
}

void scene_menu_on_exit(void* context) {
    GlobalVariableStruct* app = context;
    FURI_LOG_D("GuiExamples", "scene_menu_on_exit BEGIN");
    menu_reset(app->menu);
    FURI_LOG_D("GuiExamples", "scene_menu_on_exit END");
}
