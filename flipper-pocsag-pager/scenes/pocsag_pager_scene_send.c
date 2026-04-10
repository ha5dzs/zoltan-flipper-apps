#include "../pocsag_pager_app_i.h"

typedef enum {
    SubmenuIndexSendRIC,
    SubmenuIndexSendMessage,
    SubmenuIndexSendTransmit,
    SubmenuIndexSendErrorOk,  // Used for error dialog OK button
} SubmenuIndexSend;

static void pocsag_pager_scene_send_submenu_callback(void* context, uint32_t index) {
    POCSAGPagerApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

// Widget callback for error dialogs - USED for error display in Send scene
// This callback is called when user presses OK on error dialogs shown
// in the Send scene (RIC not set, message not set errors)
// The result parameter is ignored - we send a fixed value for all widget buttons
static void pocsag_pager_scene_send_widget_callback(GuiButtonType result, InputType type, void* context) {  // SCENE_ERROR_HANDLER
    POCSAGPagerApp* app = context;
    if(type == InputTypeShort) {
        // Use SubmenuIndexSendErrorOk (value 4) to distinguish from submenu items
        // result parameter is ignored - we send a fixed value for all widget buttons
        (void)result;
        view_dispatcher_send_custom_event(app->view_dispatcher, SubmenuIndexSendErrorOk);
    }
}

void pocsag_pager_scene_send_on_enter(void* context) {
    POCSAGPagerApp* app = context;
    Submenu* submenu = app->submenu;

    submenu_add_item(
        submenu,
        "(1) Send to RIC",
        SubmenuIndexSendRIC,
        pocsag_pager_scene_send_submenu_callback,
        app);
    submenu_add_item(
        submenu,
        "(2) Edit message",
        SubmenuIndexSendMessage,
        pocsag_pager_scene_send_submenu_callback,
        app);
    submenu_add_item(
        submenu,
        "(3) Transmit!",
        SubmenuIndexSendTransmit,
        pocsag_pager_scene_send_submenu_callback,
        app);

    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, POCSAGPagerSceneSend));

    view_dispatcher_switch_to_view(app->view_dispatcher, POCSAGPagerViewSubmenu);
}

bool pocsag_pager_scene_send_on_event(void* context, SceneManagerEvent event) {
    POCSAGPagerApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case SubmenuIndexSendRIC:
            scene_manager_next_scene(app->scene_manager, POCSAGPagerSceneSendRIC);
            consumed = true;
            break;
        case SubmenuIndexSendMessage:
            scene_manager_next_scene(app->scene_manager, POCSAGPagerSceneSendMessage);
            consumed = true;
            break;
        case SubmenuIndexSendTransmit:
            // Validate we have RIC and message before transmitting
            if(!pcsg_validate_ric(app->send_ric)) {
                // Show error - RIC not set
                widget_reset(app->widget);
                widget_add_text_box_element(
                    app->widget,
                    0,
                    0,
                    128,
                    64,
                    AlignCenter,
                    AlignCenter,
                    "RIC not set!\nPlease enter RIC first.",
                    false);
                widget_add_button_element(
                    app->widget, GuiButtonTypeCenter, "OK", pocsag_pager_scene_send_widget_callback, app);
                view_dispatcher_switch_to_view(app->view_dispatcher, POCSAGPagerViewWidget);
            } else if(strlen(app->send_message) == 0) {
                // Show error - message not set (allow blank messages)
                widget_reset(app->widget);
                widget_add_text_box_element(
                    app->widget,
                    0,
                    0,
                    128,
                    64,
                    AlignCenter,
                    AlignCenter,
                    "Message not set!\nPlease enter message first.",
                    false);
                widget_add_button_element(
                    app->widget, GuiButtonTypeCenter, "OK", pocsag_pager_scene_send_widget_callback, app);
                view_dispatcher_switch_to_view(app->view_dispatcher, POCSAGPagerViewWidget);
            } else {
                // Start transmission scene
                scene_manager_next_scene(app->scene_manager, POCSAGPagerSceneSendTransmit);
            }
            consumed = true;
            break;
        case SubmenuIndexSendErrorOk:
            // User pressed OK on error dialog
            scene_manager_next_scene(app->scene_manager, POCSAGPagerSceneSend);
            consumed = true;
            break;
        }
        scene_manager_set_scene_state(app->scene_manager, POCSAGPagerSceneSend, event.event);
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, POCSAGPagerSceneStart);
        consumed = true;
    }

    return consumed;
}

void pocsag_pager_scene_send_on_exit(void* context) {
    POCSAGPagerApp* app = context;
    submenu_reset(app->submenu);
}
