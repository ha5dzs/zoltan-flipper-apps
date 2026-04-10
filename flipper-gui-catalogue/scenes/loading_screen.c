#define FURI_LOG_TAG "GuiExamples"
#include "gui_examples.h"

// This scene shows a loading animation briefly then returns to main
// Uses a timeout approach instead of blocking delay

void scene_loading_on_enter(void* context) {
    GlobalVariableStruct* app = context;
    FURI_LOG_D("GuiExamples", "scene_loading_on_enter BEGIN");

    FURI_LOG_D("GuiExamples", "Loading scene entered");

    // Show loading indicator for 2 seconds, then auto-return
    // The scene_manager_next_scene will be called after the delay
    // We use furi_delay_ms here since it's a simple demo
    // For a production app, use a timer-based approach instead
    furi_delay_ms(2000);

    // Navigate back to main menu
    FURI_LOG_D("GuiExamples", "Loading complete, navigating to main");
    scene_manager_next_scene(app->scene_manager, SceneMain);
    FURI_LOG_D("GuiExamples", "scene_loading_on_enter END (after navigation)");
}

bool scene_loading_on_event(void* context, SceneManagerEvent event) {
    GlobalVariableStruct* app = context;
    FURI_LOG_D("GuiExamples", "scene_loading_on_event BEGIN: type=%d", event.type);

    // Allow back to cancel and return to main immediately
    if(event.type == SceneManagerEventTypeBack) {
        FURI_LOG_D("GuiExamples", "Loading back pressed - returning to main");
        scene_manager_next_scene(app->scene_manager, SceneMain);
        FURI_LOG_D("GuiExamples", "scene_loading_on_event END: consumed (back)");
        return true;
    }
    FURI_LOG_D("GuiExamples", "scene_loading_on_event END: not consumed");
    return false;
}

void scene_loading_on_exit(void* context) {
    FURI_LOG_D("GuiExamples", "scene_loading_on_exit BEGIN");
    UNUSED(context);
    FURI_LOG_D("GuiExamples", "scene_loading_on_exit END");
}
