/* gui_examples.c */
#include<furi.h>
#include<furi_hal.h>
#include<input/input.h>
#include<gui/gui.h>
#include<gui/view_dispatcher.h>
#include<gui/scene_manager.h>
#include<gui/modules/submenu.h>
#include<gui/modules/widget.h>
#include<gui/modules/dialog_ex.h>
#include<gui/modules/popup.h>
#include<gui/modules/text_input.h>
#include<gui/modules/byte_input.h>
#include<gui/modules/number_input.h>
#include<gui/modules/menu.h>
#include<gui/modules/button_menu.h>

#include "gui_examples.h"
#include "helper_functions.h"

 // Custom event callback - dispatches menu selections to scene manager
static bool app_custom_callback(void* context, uint32_t custom_event) {
    GlobalVariableStruct* app = context;
    FURI_LOG_D("GuiExamples", "app_custom_callback: event=%lu", custom_event);
    // Safety check: scene_manager must be valid
    if(app == NULL || app->scene_manager == NULL) {
        FURI_LOG_D("GuiExamples", "app_custom_callback: NULL context or scene_manager");
        return false;
    }
    // Custom events are menu selections - let scene manager handle them
    bool result = scene_manager_handle_custom_event(app->scene_manager, custom_event);
    FURI_LOG_D("GuiExamples", "app_custom_callback END: result=%d", result);
    return result;
}

// Navigation event callback - handles the Back button
static bool app_navigation_callback(void* context) {
    GlobalVariableStruct* app = context;
    FURI_LOG_D("GuiExamples", "app_navigation_callback: Back pressed");
    if(app == NULL || app->scene_manager == NULL) {
        return false;
    }
    return scene_manager_handle_back_event(app->scene_manager);
}

// Main entry point
int32_t gui_examples_main(void* p) {
    UNUSED(p);

    FURI_LOG_D("GuiExamples", "gui_examples_main BEGIN");

    // Allocate global context
    GlobalVariableStruct* context = malloc(sizeof(GlobalVariableStruct));
    memset(context, 0, sizeof(GlobalVariableStruct));
    FURI_LOG_D("GuiExamples", "Context allocated");

    // Setup scene manager and view dispatcher
    static void (*const scene_on_enter_handlers[])(void*) = {
        scene_main_on_enter,
        scene_dialog_on_enter,
        scene_popup_on_enter,
        scene_byteinput_on_enter,
        scene_widget_on_enter,
        scene_text_input_on_enter,
        scene_about_on_enter,
        scene_loading_on_enter,
        scene_number_input_on_enter,
        scene_menu_on_enter,
        scene_button_menu_on_enter,
    };
    static bool (*const scene_on_event_handlers[])(void*, SceneManagerEvent) = {
        scene_main_on_event,
        scene_dialog_on_event,
        scene_popup_on_event,
        scene_byteinput_on_event,
        scene_widget_on_event,
        scene_text_input_on_event,
        scene_about_on_event,
        scene_loading_on_event,
        scene_number_input_on_event,
        scene_menu_on_event,
        scene_button_menu_on_event,
    };
    static void (*const scene_on_exit_handlers[])(void*) = {
        scene_main_on_exit,
        scene_dialog_on_exit,
        scene_popup_on_exit,
        scene_byteinput_on_exit,
        scene_widget_on_exit,
        scene_text_input_on_exit,
        scene_about_on_exit,
        scene_loading_on_exit,
        scene_number_input_on_exit,
        scene_menu_on_exit,
        scene_button_menu_on_exit,
    };
    static const SceneManagerHandlers scene_handlers = {
        .on_enter_handlers = scene_on_enter_handlers,
        .on_event_handlers = scene_on_event_handlers,
        .on_exit_handlers = scene_on_exit_handlers,
        .scene_num = SceneNum,
    };

    context->scene_manager = scene_manager_alloc(&scene_handlers, context);
    FURI_LOG_D("GuiExamples", "Scene manager allocated");

    context->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(context->view_dispatcher, context);
    view_dispatcher_set_custom_event_callback(context->view_dispatcher, app_custom_callback);
    view_dispatcher_set_navigation_event_callback(context->view_dispatcher, app_navigation_callback);
    // No navigation event callback - handled by scene manager and input callbacks
    FURI_LOG_D("GuiExamples", "View dispatcher configured");

    // Add views - Submenu for main menu
    context->submenu = submenu_alloc();
    view_dispatcher_add_view(context->view_dispatcher, SceneMain, submenu_get_view(context->submenu));

    // Add Widget view for Widget demo
    context->widget = widget_alloc();
    view_dispatcher_add_view(context->view_dispatcher, SceneWidget, widget_get_view(context->widget));

    // Add Dialog view for Dialog demo
    context->dialog = dialog_ex_alloc();
    view_dispatcher_add_view(context->view_dispatcher, SceneDialog, dialog_ex_get_view(context->dialog));

    // Add Popup view for Popup demo
    context->popup = popup_alloc();
    view_dispatcher_add_view(context->view_dispatcher, ScenePopup, popup_get_view(context->popup));

    // Add Widget view for About scene
    context->about_widget = widget_alloc();
    view_dispatcher_add_view(context->view_dispatcher, SceneAbout, widget_get_view(context->about_widget));

    // Add ByteInput view for ByteInput demo
    context->byte_input = byte_input_alloc();
    view_dispatcher_add_view(context->view_dispatcher, SceneByteInput, byte_input_get_view(context->byte_input));

    // Add TextInput view for Text Input demo
    context->text_input = text_input_alloc();
    view_dispatcher_add_view(context->view_dispatcher, SceneTextInput, text_input_get_view(context->text_input));

    // Add NumberInput view for NumberInput demo
    context->number_input = number_input_alloc();
    view_dispatcher_add_view(context->view_dispatcher, SceneNumberInput, number_input_get_view(context->number_input));

    // Add Menu view for Menu demo
    context->menu = menu_alloc();
    view_dispatcher_add_view(context->view_dispatcher, SceneMenu, menu_get_view(context->menu));

    // Add ButtonMenu view for ButtonMenu demo
    context->button_menu = button_menu_alloc();
    view_dispatcher_add_view(context->view_dispatcher, SceneButtonMenu, button_menu_get_view(context->button_menu));

    FURI_LOG_D("GuiExamples", "All views added");

    // Create title viewport
    ViewPort* title_viewport = view_port_alloc();
    view_port_set_orientation(title_viewport, ViewPortOrientationHorizontal);
    view_port_draw_callback_set(title_viewport, title_screen_draw_callback, NULL);

    // Start GUI
    Gui* gui = furi_record_open(RECORD_GUI);

    // Add viewport and show title
    gui_add_view_port(gui, title_viewport, GuiLayerFullscreen);
    view_port_update(title_viewport);
    furi_delay_ms(2000);

    // Remove title screen
    gui_remove_view_port(gui, title_viewport);
    view_port_free(title_viewport);

    FURI_LOG_D("GuiExamples", "Title screen removed, navigating to main");

    // Navigate to main menu
    scene_manager_next_scene(context->scene_manager, SceneMain);
    view_dispatcher_attach_to_gui(context->view_dispatcher, gui, ViewDispatcherTypeFullscreen);
    FURI_LOG_D("GuiExamples", "View dispatcher attached, starting dispatcher");

    view_dispatcher_run(context->view_dispatcher);

    FURI_LOG_D("GuiExamples", "view_dispatcher_run returned, cleaning up");

    // Cleanup - view_dispatcher must have all views removed before freeing
    // Order: remove view -> free view -> repeat -> free dispatcher -> free scene manager

    // Remove and free Submenu (SceneMain)
    view_dispatcher_remove_view(context->view_dispatcher, SceneMain);
    submenu_free(context->submenu);

    // Remove and free Widget (SceneWidget)
    view_dispatcher_remove_view(context->view_dispatcher, SceneWidget);
    widget_free(context->widget);

    // Remove and free DialogEx (SceneDialog)
    view_dispatcher_remove_view(context->view_dispatcher, SceneDialog);
    dialog_ex_free(context->dialog);

    // Remove and free Popup (ScenePopup)
    view_dispatcher_remove_view(context->view_dispatcher, ScenePopup);
    popup_free(context->popup);

    // Remove and free ByteInput (SceneByteInput)
    view_dispatcher_remove_view(context->view_dispatcher, SceneByteInput);
    if(context->byte_input) byte_input_free(context->byte_input);

    // Remove and free TextInput (SceneTextInput)
    view_dispatcher_remove_view(context->view_dispatcher, SceneTextInput);
    text_input_free(context->text_input);

    // Remove and free About Widget (SceneAbout)
    view_dispatcher_remove_view(context->view_dispatcher, SceneAbout);
    widget_free(context->about_widget);

    // SceneLoading has no view added to dispatcher (it's a bug in original code)
    // Skip removing/freeing for SceneLoading

    // Remove and free NumberInput (SceneNumberInput)
    view_dispatcher_remove_view(context->view_dispatcher, SceneNumberInput);
    if(context->number_input) number_input_free(context->number_input);

    // Remove and free Menu (SceneMenu)
    view_dispatcher_remove_view(context->view_dispatcher, SceneMenu);
    if(context->menu) menu_free(context->menu);

    // Remove and free ButtonMenu (SceneButtonMenu)
    view_dispatcher_remove_view(context->view_dispatcher, SceneButtonMenu);
    if(context->button_menu) button_menu_free(context->button_menu);

    // Free the view dispatcher (now empty)
    FURI_LOG_D("GuiExamples", "Cleanup: free view dispatcher");
    view_dispatcher_free(context->view_dispatcher);

    // Free the scene manager
    FURI_LOG_D("GuiExamples", "Cleanup: free scene manager");
    scene_manager_free(context->scene_manager);

    // Free the context
    FURI_LOG_D("GuiExamples", "Cleanup: free context");
    free(context);

    // Close the GUI record
    FURI_LOG_D("GuiExamples", "Cleanup: close gui record");
    furi_record_close(RECORD_GUI);

    FURI_LOG_D("GuiExamples", "gui_examples_main END");

    return 0;
}