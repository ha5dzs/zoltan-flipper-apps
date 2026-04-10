#define FURI_LOG_TAG "GuiExamples"
#include "gui_examples.h"

static void number_input_callback(void* context, int32_t number) {
    GlobalVariableStruct* app = context;
    FURI_LOG_D("GuiExamples", "number_input_callback BEGIN: number=%ld", number);
    app->current_number = number;

    // Navigate back directly from callback
    scene_manager_next_scene(app->scene_manager, SceneMain);
    FURI_LOG_D("GuiExamples", "number_input_callback END");
}

void scene_number_input_on_enter(void* context) {
    GlobalVariableStruct* app = context;
    FURI_LOG_D("GuiExamples", "scene_number_input_on_enter BEGIN");

    FURI_LOG_D("GuiExamples", "NumberInput scene entered");

    // Initialize current number if not set
    if(app->current_number == 0) {
        app->current_number = 42;
    }

    // Configure number input
    char header_text[50];
    snprintf(header_text, sizeof(header_text), "Set Number (0 - 100)");
    number_input_set_header_text(app->number_input, header_text);
    number_input_set_result_callback(
        app->number_input,
        number_input_callback,
        app,
        app->current_number,
        0,
        100);

    view_dispatcher_switch_to_view(app->view_dispatcher, SceneNumberInput);
    FURI_LOG_D("GuiExamples", "scene_number_input_on_enter END");
}

bool scene_number_input_on_event(void* context, SceneManagerEvent event) {
    GlobalVariableStruct* app = context;
    FURI_LOG_D("GuiExamples", "scene_number_input_on_event BEGIN: type=%d", event.type);

    if(event.type == SceneManagerEventTypeBack) {
        FURI_LOG_D("GuiExamples", "NumberInput back pressed - returning to main");
        scene_manager_next_scene(app->scene_manager, SceneMain);
        FURI_LOG_D("GuiExamples", "scene_number_input_on_event END: consumed (back)");
        return true;
    }
    FURI_LOG_D("GuiExamples", "scene_number_input_on_event END: not consumed");
    return false;
}

void scene_number_input_on_exit(void* context) {
    FURI_LOG_D("GuiExamples", "scene_number_input_on_exit BEGIN");
    UNUSED(context);
    FURI_LOG_D("GuiExamples", "scene_number_input_on_exit END");
}
