#ifndef GUI_EXAMPLES_H
#define GUI_EXAMPLES_H

#include <furi.h>
#include <stdbool.h>
#include <stdio.h>

#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>
#include <gui/modules/dialog_ex.h>
#include <gui/modules/popup.h>
#include <gui/modules/text_input.h>
#include <gui/modules/byte_input.h>
#include <gui/modules/number_input.h>
#include <gui/modules/menu.h>
#include <gui/modules/button_menu.h>

// Global state structure
typedef struct {
    uint8_t byteinput_buffer[4];
    char single_line_text_input_value[64];
    SceneManager* scene_manager;
    ViewDispatcher* view_dispatcher;
    Submenu* submenu;
    Widget* widget;
    DialogEx* dialog;
    Popup* popup;
    TextInput* text_input;
    ByteInput* byte_input;
    Widget* about_widget;
    NumberInput* number_input;
    Menu* menu;
    ButtonMenu* button_menu;
    int32_t current_number;
    bool success;
} GlobalVariableStruct;

// Scene definitions
typedef enum {
    SceneMain,
    SceneDialog,
    ScenePopup,
    SceneByteInput,
    SceneWidget,
    SceneTextInput,
    SceneAbout,
    SceneLoading,
    SceneNumberInput,
    SceneMenu,
    SceneButtonMenu,
    SceneNum,
} SceneIndex;

// Scene callbacks
// Main menu scene
void scene_main_on_enter(void* context);
bool scene_main_on_event(void* context, SceneManagerEvent event);
void scene_main_on_exit(void* context);

// Dialog scene
void scene_dialog_on_enter(void* context);
bool scene_dialog_on_event(void* context, SceneManagerEvent event);
void scene_dialog_on_exit(void* context);

// Popup scene
void scene_popup_on_enter(void* context);
bool scene_popup_on_event(void* context, SceneManagerEvent event);
void scene_popup_on_exit(void* context);

// ByteInput scene
void scene_byteinput_on_enter(void* context);
bool scene_byteinput_on_event(void* context, SceneManagerEvent event);
void scene_byteinput_on_exit(void* context);

// Widget scene
void scene_widget_on_enter(void* context);
bool scene_widget_on_event(void* context, SceneManagerEvent event);
void scene_widget_on_exit(void* context);

// TextInput scene
void scene_text_input_on_enter(void* context);
bool scene_text_input_on_event(void* context, SceneManagerEvent event);
void scene_text_input_on_exit(void* context);

// About scene
void scene_about_on_enter(void* context);
bool scene_about_on_event(void* context, SceneManagerEvent event);
void scene_about_on_exit(void* context);

// Loading scene
void scene_loading_on_enter(void* context);
bool scene_loading_on_event(void* context, SceneManagerEvent event);
void scene_loading_on_exit(void* context);

// NumberInput scene
void scene_number_input_on_enter(void* context);
bool scene_number_input_on_event(void* context, SceneManagerEvent event);
void scene_number_input_on_exit(void* context);

// Menu scene
void scene_menu_on_enter(void* context);
bool scene_menu_on_event(void* context, SceneManagerEvent event);
void scene_menu_on_exit(void* context);

// ButtonMenu scene
void scene_button_menu_on_enter(void* context);
bool scene_button_menu_on_event(void* context, SceneManagerEvent event);
void scene_button_menu_on_exit(void* context);

// Application main entry point
int32_t gui_examples_main(void* p);

#endif // GUI_EXAMPLES_H
