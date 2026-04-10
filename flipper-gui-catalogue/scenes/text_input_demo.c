#define FURI_LOG_TAG "GuiExamples"
#include "gui_examples.h"

static void text_input_callback(void* context) {
    GlobalVariableStruct* app = context;
    FURI_LOG_D("GuiExamples", "text_input_callback BEGIN");

    // Navigate back directly from callback
    scene_manager_next_scene(app->scene_manager, SceneMain);
    FURI_LOG_D("GuiExamples", "text_input_callback END");
}

void scene_text_input_on_enter(void* context) {
    GlobalVariableStruct* app = context;
    FURI_LOG_D("GuiExamples", "scene_text_input_on_enter BEGIN");

    TextInput* text_input = app->text_input;
    view_dispatcher_switch_to_view(app->view_dispatcher, SceneTextInput);

    // Reset and configure text input
    text_input_reset(text_input);
    text_input_set_result_callback(
        text_input,
        text_input_callback,
        app,
        app->single_line_text_input_value,
        sizeof(app->single_line_text_input_value),
        true);
    text_input_set_header_text(text_input, "Enter Text:");
    strlcpy(app->single_line_text_input_value, "Hello Flipper!", sizeof(app->single_line_text_input_value));
    FURI_LOG_D("GuiExamples", "scene_text_input_on_enter END");
}

bool scene_text_input_on_event(void* context, SceneManagerEvent event) {
    GlobalVariableStruct* app = context;
    FURI_LOG_D("GuiExamples", "scene_text_input_on_event BEGIN: type=%d", event.type);

    if(event.type == SceneManagerEventTypeBack) {
        FURI_LOG_D("GuiExamples", "TextInput back pressed - returning to main");
        // Go back to main menu on back press
        scene_manager_next_scene(app->scene_manager, SceneMain);
        FURI_LOG_D("GuiExamples", "scene_text_input_on_event END: consumed (back)");
        return true;
    }
    FURI_LOG_D("GuiExamples", "scene_text_input_on_event END: not consumed");
    return false;
}

void scene_text_input_on_exit(void* context) {
    GlobalVariableStruct* app = context;
    FURI_LOG_D("GuiExamples", "scene_text_input_on_exit BEGIN");

    // Clear text input when exiting
    text_input_reset(app->text_input);
    FURI_LOG_D("GuiExamples", "scene_text_input_on_exit END");
}
