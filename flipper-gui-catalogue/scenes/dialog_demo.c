#define FURI_LOG_TAG "GuiExamples"
#include "gui_examples.h"

static void dialog_callback(DialogExResult result, void* context) {
    GlobalVariableStruct* app = context;
    FURI_LOG_D("GuiExamples", "dialog_callback BEGIN: result=%d", result);
    UNUSED(result);
    // Navigate back to main menu when any dialog button is pressed
    scene_manager_next_scene(app->scene_manager, SceneMain);
    FURI_LOG_D("GuiExamples", "dialog_callback END");
}

void scene_dialog_on_enter(void* context) {
    GlobalVariableStruct* app = context;
    FURI_LOG_D("GuiExamples", "scene_dialog_on_enter BEGIN");

    dialog_ex_reset(app->dialog);
    dialog_ex_set_header(app->dialog, "Confirm Action", 64, 16, AlignCenter, AlignTop);
    dialog_ex_set_text(app->dialog, "This demonstrates the dialog\nmodule with multiple buttons.", 64, 28, AlignCenter, AlignTop);
    dialog_ex_set_left_button_text(app->dialog, "Cancel");
    dialog_ex_set_center_button_text(app->dialog, "Maybe");
    dialog_ex_set_right_button_text(app->dialog, "OK");
    dialog_ex_set_result_callback(app->dialog, dialog_callback);
    dialog_ex_set_context(app->dialog, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, SceneDialog);
    FURI_LOG_D("GuiExamples", "scene_dialog_on_enter END");
}

bool scene_dialog_on_event(void* context, SceneManagerEvent event) {
    GlobalVariableStruct* app = context;
    FURI_LOG_D("GuiExamples", "scene_dialog_on_event BEGIN: type=%d", event.type);

    if(event.type == SceneManagerEventTypeBack) {
        FURI_LOG_D("GuiExamples", "Dialog back pressed - returning to main");
        // Go back to main menu on back press
        scene_manager_next_scene(app->scene_manager, SceneMain);
        FURI_LOG_D("GuiExamples", "scene_dialog_on_event END: consumed (back)");
        return true;
    }
    FURI_LOG_D("GuiExamples", "scene_dialog_on_event END: not consumed");
    return false;
}

void scene_dialog_on_exit(void* context) {
    GlobalVariableStruct* app = context;
    FURI_LOG_D("GuiExamples", "scene_dialog_on_exit BEGIN");
    // Reset dialog on exit to prevent callback after free
    dialog_ex_reset(app->dialog);
    FURI_LOG_D("GuiExamples", "scene_dialog_on_exit END");
}
