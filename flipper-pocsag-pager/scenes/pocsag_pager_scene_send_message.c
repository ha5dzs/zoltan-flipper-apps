#include "../pocsag_pager_app_i.h"
#include "../helpers/pocsag_config.h"
#include <furi.h>

void pocsag_pager_scene_send_message_text_input_callback(void* context) {
    POCSAGPagerApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, 1);  // OK pressed
}

// Widget callback for error dialog - UNUSED
// The send message scene uses direct callback handling instead of widget buttons
static void pocsag_pager_scene_send_message_widget_callback(GuiButtonType result, InputType type, void* context) {  // SCENE_UNUSED
    POCSAGPagerApp* app = context;
    if(type == InputTypeShort) {
        view_dispatcher_send_custom_event(app->view_dispatcher, (uint32_t)result);
    }
}

void pocsag_pager_scene_send_message_on_enter(void* context) {
    POCSAGPagerApp* app = context;
    TextInput* text_input = app->text_input;

    text_input_set_header_text(text_input, "Enter message (max 120 chars):");

    text_input_set_result_callback(
        text_input,
        pocsag_pager_scene_send_message_text_input_callback,
        app,
        app->send_message,
        PCSG_MAX_MESSAGE_LENGTH,
        true);  // clear_default_text = true to show default

    view_dispatcher_switch_to_view(app->view_dispatcher, POCSAGPagerViewTextInput);
}

bool pocsag_pager_scene_send_message_on_event(void* context, SceneManagerEvent event) {
    POCSAGPagerApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == 1) {  // OK pressed
            size_t msg_len = strlen(app->send_message);

            // Validate message length (0 is valid for blank message)
            if(msg_len <= 120) {
                app->send_message_valid = true;
                scene_manager_next_scene(app->scene_manager, POCSAGPagerSceneSend);
                consumed = true;
            } else if(msg_len > 120) {
                // Message too long, show error using widget
                widget_reset(app->widget);
                widget_add_text_box_element(
                    app->widget,
                    0,
                    0,
                    128,
                    64,
                    AlignCenter,
                    AlignCenter,
                    "Message too long!\nMax 120 characters",
                    false);
                widget_add_button_element(
                    app->widget, GuiButtonTypeCenter, "OK", pocsag_pager_scene_send_message_widget_callback, app);
                view_dispatcher_switch_to_view(app->view_dispatcher, POCSAGPagerViewWidget);
                consumed = true;
            }
        } else if(event.event == GuiButtonTypeCenter) {
            // User pressed OK on error dialog
            scene_manager_next_scene(app->scene_manager, POCSAGPagerSceneSendMessage);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, POCSAGPagerSceneSend);
        consumed = true;
    }

    return consumed;
}

void pocsag_pager_scene_send_message_on_exit(void* context) {
    POCSAGPagerApp* app = context;
    text_input_reset(app->text_input);
}
