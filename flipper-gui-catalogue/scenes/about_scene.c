#define FURI_LOG_TAG "GuiExamples"
#include "gui_examples.h"

void scene_about_on_enter(void* context) {
    GlobalVariableStruct* app = context;
    FURI_LOG_D("GuiExamples", "scene_about_on_enter BEGIN");

    const char* about_text =
        "This application demonstrates various Flipper Zero GUI\n"
        "components including:\n\n"
        "- Dialogs: Modal windows for user interaction\n"
        "- Popups: Simple alerts and notifications\n"
        "- ByteInput: Hexadecimal input fields\n"
        "- Widgets: Custom layouts with text and buttons\n"
        "- Text Input: Standard text entry fields\n"
        "- Number Input: Numeric entry fields\n"
        "- Menu: Classic Flipper list menus\n"
        "- Button Menu: Menu with custom button behaviors\n\n"
        "This About screen now uses a Widget with a scrolling\n"
        "text element, which allows you to scroll through the\n"
        "content using the Up and Down buttons while still\n"
        "allowing the Back button to return to the main menu.\n\n"
        "Press Back to return to the main menu.";

    widget_reset(app->about_widget);
    widget_add_string_multiline_element(app->about_widget, 64, 10, AlignCenter, AlignTop, FontPrimary, "About");
    widget_add_text_scroll_element(app->about_widget, 0, 25, 128, 35, about_text);

    view_dispatcher_switch_to_view(app->view_dispatcher, SceneAbout);
    FURI_LOG_D("GuiExamples", "scene_about_on_enter END");
}

bool scene_about_on_event(void* context, SceneManagerEvent event) {
    GlobalVariableStruct* app = context;
    if(event.type == SceneManagerEventTypeBack) {
        scene_manager_next_scene(app->scene_manager, SceneMain);
        return true;
    }
    return false;
}

void scene_about_on_exit(void* context) {
    GlobalVariableStruct* app = context;
    FURI_LOG_D("GuiExamples", "scene_about_on_exit BEGIN");
    widget_reset(app->about_widget);
    FURI_LOG_D("GuiExamples", "scene_about_on_exit END");
}
