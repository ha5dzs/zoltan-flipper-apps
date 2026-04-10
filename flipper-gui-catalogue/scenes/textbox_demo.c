#define FURI_LOG_TAG "GuiExamples"
#include "gui_examples.h"

static void widget_button_callback(GuiButtonType button, InputType input, void* context) {
    GlobalVariableStruct* app = context;
    FURI_LOG_D("GuiExamples", "widget_button_callback BEGIN: button=%d, input=%d", button, input);
    UNUSED(button);
    if(input == InputTypeShort) {
        scene_manager_next_scene(app->scene_manager, SceneMain);
    }
    FURI_LOG_D("GuiExamples", "widget_button_callback END");
}

void scene_widget_on_enter(void* context) {
    GlobalVariableStruct* app = context;
    FURI_LOG_D("GuiExamples", "scene_widget_on_enter BEGIN");
    UNUSED(context);

    view_dispatcher_switch_to_view(app->view_dispatcher, SceneWidget);

    // Clear widget first
    widget_reset(app->widget);

    // Add string with multiline element
    widget_add_string_multiline_element(
        app->widget, 64, 10, AlignCenter, AlignTop, FontPrimary, "Widget Demo");

    // Add another string
    widget_add_string_multiline_element(
        app->widget, 64, 22, AlignCenter, AlignTop, FontSecondary, "Flipper GUI Demo");

    // Add line separator
    widget_add_line_element(app->widget, 10, 35, 108, 1);

    // Add description text
    widget_add_string_multiline_element(
        app->widget, 64, 42, AlignCenter, AlignTop, FontSecondary, "Shows text and icons");

    widget_add_string_multiline_element(
        app->widget, 64, 50, AlignCenter, AlignTop, FontSecondary, "using Widget module");

    // Add button to go back
    widget_add_button_element(app->widget, GuiButtonTypeLeft, "OK", widget_button_callback, app);
    FURI_LOG_D("GuiExamples", "scene_widget_on_enter END");
}

bool scene_widget_on_event(void* context, SceneManagerEvent event) {
    GlobalVariableStruct* app = context;
    FURI_LOG_D("GuiExamples", "scene_widget_on_event BEGIN: type=%d", event.type);
    UNUSED(context);

    if(event.type == SceneManagerEventTypeBack) {
        FURI_LOG_D("GuiExamples", "Widget back pressed - returning to main");
        scene_manager_next_scene(app->scene_manager, SceneMain);
        FURI_LOG_D("GuiExamples", "scene_widget_on_event END: consumed (back)");
        return true;
    }
    FURI_LOG_D("GuiExamples", "scene_widget_on_event END: not consumed");
    return false;
}

void scene_widget_on_exit(void* context) {
    GlobalVariableStruct* app = context;
    FURI_LOG_D("GuiExamples", "scene_widget_on_exit BEGIN");
    widget_reset(app->widget);
    FURI_LOG_D("GuiExamples", "scene_widget_on_exit END");
}
