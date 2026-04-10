#define FURI_LOG_TAG "GuiExamples"
#include "gui_examples.h"

typedef enum {
    MainMenuItemDialog,
    MainMenuItemPopup,
    MainMenuItemByteInput,
    MainMenuItemWidget,
    MainMenuItemTextInput,
    MainMenuItemAbout,
    MainMenuItemLoading,
    MainMenuItemNumberInput,
    MainMenuItemMenu,
    MainMenuItemButtonMenu,
    MainMenuItemCount,
} MainMenuItem;

static void main_menu_callback(void* context, uint32_t index) {
    GlobalVariableStruct* app = context;
    FURI_LOG_D("GuiExamples", "main_menu_callback: index=%lu", index);

    // Navigate directly based on menu selection
    scene_manager_next_scene(app->scene_manager, (SceneIndex)(SceneDialog + index));
}

void scene_main_on_enter(void* context) {
    GlobalVariableStruct* app = context;

    FURI_LOG_D("GuiExamples", "scene_main_on_enter BEGIN");
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "GUI Examples");

    submenu_add_item(app->submenu, "Dialog Demo", MainMenuItemDialog, main_menu_callback, app);
    submenu_add_item(app->submenu, "Popup Demo", MainMenuItemPopup, main_menu_callback, app);
    submenu_add_item(app->submenu, "ByteInput Demo", MainMenuItemByteInput, main_menu_callback, app);
    submenu_add_item(app->submenu, "Widget Demo", MainMenuItemWidget, main_menu_callback, app);
    submenu_add_item(app->submenu, "Text Input", MainMenuItemTextInput, main_menu_callback, app);
    submenu_add_item(app->submenu, "About", MainMenuItemAbout, main_menu_callback, app);
    submenu_add_item(app->submenu, "Loading Demo", MainMenuItemLoading, main_menu_callback, app);
    submenu_add_item(app->submenu, "Number Input", MainMenuItemNumberInput, main_menu_callback, app);
    submenu_add_item(app->submenu, "Menu Demo", MainMenuItemMenu, main_menu_callback, app);
    submenu_add_item(app->submenu, "Button Menu", MainMenuItemButtonMenu, main_menu_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, SceneMain);
    FURI_LOG_D("GuiExamples", "scene_main_on_enter END");
}

bool scene_main_on_event(void* context, SceneManagerEvent event) {
    GlobalVariableStruct* app = context;
    FURI_LOG_D("GuiExamples", "scene_main_on_event BEGIN: type=%d, event=%lu", event.type, (unsigned long)event.event);
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        consumed = true;
        FURI_LOG_D("GuiExamples", "Custom event: %lu", (unsigned long)event.event);
        switch(event.event) {
        case MainMenuItemDialog:
            FURI_LOG_D("GuiExamples", "Selecting Dialog Demo");
            scene_manager_next_scene(app->scene_manager, SceneDialog);
            break;
        case MainMenuItemPopup:
            FURI_LOG_D("GuiExamples", "Selecting Popup Demo");
            scene_manager_next_scene(app->scene_manager, ScenePopup);
            break;
        case MainMenuItemByteInput:
            FURI_LOG_D("GuiExamples", "Selecting ByteInput Demo");
            scene_manager_next_scene(app->scene_manager, SceneByteInput);
            break;
        case MainMenuItemWidget:
            FURI_LOG_D("GuiExamples", "Selecting Widget Demo");
            scene_manager_next_scene(app->scene_manager, SceneWidget);
            break;
        case MainMenuItemTextInput:
            FURI_LOG_D("GuiExamples", "Selecting Text Input Demo");
            scene_manager_next_scene(app->scene_manager, SceneTextInput);
            break;
        case MainMenuItemAbout:
            FURI_LOG_D("GuiExamples", "Selecting About");
            scene_manager_next_scene(app->scene_manager, SceneAbout);
            break;
        case MainMenuItemLoading:
            FURI_LOG_D("GuiExamples", "Selecting Loading Demo");
            scene_manager_next_scene(app->scene_manager, SceneLoading);
            break;
        case MainMenuItemNumberInput:
            FURI_LOG_D("GuiExamples", "Selecting Number Input Demo");
            scene_manager_next_scene(app->scene_manager, SceneNumberInput);
            break;
        case MainMenuItemMenu:
            FURI_LOG_D("GuiExamples", "Selecting Menu Demo");
            scene_manager_next_scene(app->scene_manager, SceneMenu);
            break;
        case MainMenuItemButtonMenu:
            FURI_LOG_D("GuiExamples", "Selecting Button Menu Demo");
            scene_manager_next_scene(app->scene_manager, SceneButtonMenu);
            break;
        default:
            consumed = false;
            break;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        // On back from main menu, exit the app
        FURI_LOG_D("GuiExamples", "Back pressed - initiating exit");
        consumed = true;
        view_dispatcher_stop(app->view_dispatcher);
        FURI_LOG_D("GuiExamples", "view_dispatcher_stop called");
    }
    FURI_LOG_D("GuiExamples", "scene_main_on_event END: consumed=%d", consumed);
    return consumed;
}

void scene_main_on_exit(void* context) {
    GlobalVariableStruct* app = context;
    FURI_LOG_D("GuiExamples", "scene_main_on_exit BEGIN");
    submenu_reset(app->submenu);
    FURI_LOG_D("GuiExamples", "scene_main_on_exit END");
}
