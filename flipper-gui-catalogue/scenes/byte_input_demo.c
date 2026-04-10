#define FURI_LOG_TAG "GuiExamples"
#include "gui_examples.h"

static void byteinput_changed_callback(void* context) {
    GlobalVariableStruct* app = context;
    FURI_LOG_D("GuiExamples", "byteinput_changed_callback BEGIN");

    // Log the current byte values when changed
    FURI_LOG_D(
        "GuiExamples",
        "ByteInput changed: %02X %02X %02X %02X",
        app->byteinput_buffer[0],
        app->byteinput_buffer[1],
        app->byteinput_buffer[2],
        app->byteinput_buffer[3]);
    FURI_LOG_D("GuiExamples", "byteinput_changed_callback END");
}

static void byteinput_result_callback(void* context) {
    GlobalVariableStruct* app = context;
    FURI_LOG_D("GuiExamples", "byteinput_result_callback BEGIN");

    // Navigate back directly from callback
    scene_manager_next_scene(app->scene_manager, SceneMain);
    FURI_LOG_D("GuiExamples", "byteinput_result_callback END");
}

void scene_byteinput_on_enter(void* context) {
    GlobalVariableStruct* app = context;
    FURI_LOG_D("GuiExamples", "scene_byteinput_on_enter BEGIN");

    // Initialize buffer with default value
    app->byteinput_buffer[0] = 0x12;
    app->byteinput_buffer[1] = 0x34;
    app->byteinput_buffer[2] = 0x56;
    app->byteinput_buffer[3] = 0x78;

    // Configure byte input with callbacks - pass app directly as context
    byte_input_set_result_callback(
        app->byte_input,
        byteinput_result_callback,
        byteinput_changed_callback,
        app,
        app->byteinput_buffer,
        4);

    byte_input_set_header_text(app->byte_input, "32-bit Hex Input:");

    view_dispatcher_switch_to_view(app->view_dispatcher, SceneByteInput);
    FURI_LOG_D("GuiExamples", "scene_byteinput_on_enter END");
}

bool scene_byteinput_on_event(void* context, SceneManagerEvent event) {
    GlobalVariableStruct* app = context;
    FURI_LOG_D("GuiExamples", "scene_byteinput_on_event BEGIN: type=%d", event.type);

    if(event.type == SceneManagerEventTypeBack) {
        FURI_LOG_D("GuiExamples", "ByteInput back pressed - returning to main");
        scene_manager_next_scene(app->scene_manager, SceneMain);
        FURI_LOG_D("GuiExamples", "scene_byteinput_on_event END: consumed (back)");
        return true;
    }
    FURI_LOG_D("GuiExamples", "scene_byteinput_on_event END: not consumed");
    return false;
}

void scene_byteinput_on_exit(void* context) {
    FURI_LOG_D("GuiExamples", "scene_byteinput_on_exit BEGIN");
    UNUSED(context);
    FURI_LOG_D("GuiExamples", "scene_byteinput_on_exit END");
}
