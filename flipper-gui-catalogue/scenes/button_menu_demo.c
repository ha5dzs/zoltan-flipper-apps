#define FURI_LOG_TAG "GuiExamples"
#include "gui_examples.h"

static void button_menu_callback(void* context, int32_t index, InputType type) {
    GlobalVariableStruct* app = context;
    if(type == InputTypeShort) {
        if(index == 0) {
            // Option 1 - just stay on this scene
            FURI_LOG_D("GuiExamples", "Option 1 selected");
        } else if(index == 1) {
            // navigate back to main menu
            FURI_LOG_D("GuiExamples", "Button menu: navigate back");
            scene_manager_next_scene(app->scene_manager, SceneMain);
        }
    }
}

void scene_button_menu_on_enter(void* context) {
    GlobalVariableStruct* app = context;
    FURI_LOG_D("GuiExamples", "scene_button_menu_on_enter BEGIN");

    FURI_LOG_D("GuiExamples", "ButtonMenu scene entered");

    // Reset and configure button menu
    button_menu_reset(app->button_menu);
    button_menu_set_header(app->button_menu, "Button Menu Demo");

    // Add buttons with common type to ensure proper behavior
    button_menu_add_item(
        app->button_menu,
        "Option 1",
        0,
        button_menu_callback,
        ButtonMenuItemTypeCommon,
        app);
    button_menu_add_item(
        app->button_menu,
        "Back",
        1,
        button_menu_callback,
        ButtonMenuItemTypeCommon,
        app);

    view_dispatcher_switch_to_view(app->view_dispatcher, SceneButtonMenu);
    FURI_LOG_D("GuiExamples", "scene_button_menu_on_enter END");
}

bool scene_button_menu_on_event(void* context, SceneManagerEvent event) {
    GlobalVariableStruct* app = context;
    if(event.type == SceneManagerEventTypeBack) {
        scene_manager_next_scene(app->scene_manager, SceneMain);
        return true;
    }
    return false;
}

void scene_button_menu_on_exit(void* context) {
    GlobalVariableStruct* app = context;
    FURI_LOG_D("GuiExamples", "scene_button_menu_on_exit BEGIN");
    button_menu_reset(app->button_menu);
    FURI_LOG_D("GuiExamples", "scene_button_menu_on_exit END");
}