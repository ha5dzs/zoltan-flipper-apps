#include "../pocsag_pager_app_i.h"
#include "../helpers/pocsag_config.h"
#include <notification/notification_messages.h>
#include <furi_hal.h>
#include <gui/view.h>

// Track if user pressed OK on confirmation dialog - UNUSED
// The transmit scene uses enter callback instead of this flag
// Kept for potential future refactoring but current implementation uses view enter callback
static bool pocsag_pager_scene_send_transmit_user_confirmed = false;  // SCENE_UNUSED

// Widget enter callback - called when user presses Enter/OK on the widget - SCENE_UNUSED
// Current implementation uses view enter callback mechanism instead
static void pocsag_pager_scene_send_transmit_widget_enter_callback(void* context) {  // SCENE_UNUSED
    UNUSED(context);
    FURI_LOG_I("POCSAGSend", "Widget Enter callback - user confirmed!");
    pocsag_pager_scene_send_transmit_user_confirmed = true;
}

void pocsag_pager_scene_send_transmit_on_enter(void* context) {
    POCSAGPagerApp* app = context;

    FURI_LOG_I("POCSAGSend", "Starting transmission: RIC=%lu, message='%s'",
               app->send_ric, app->send_message);

    // Validate inputs
    if(!pcsg_validate_ric(app->send_ric)) {
        FURI_LOG_E("POCSAGSend", "Invalid RIC");
        scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, POCSAGPagerSceneStart);
        return;
    }

    if(!app->send_message_valid || strlen(app->send_message) == 0) {
        FURI_LOG_E("POCSAGSend", "Invalid message");
        scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, POCSAGPagerSceneStart);
        return;
    }

    // Get current frequency and modulation
    FuriString* frequency_str = furi_string_alloc();
    FuriString* modulation_str = furi_string_alloc();
    pcsg_get_frequency_modulation(app, frequency_str, modulation_str);

    // Create confirmation dialog using widget
    widget_reset(app->widget);

    FuriString* dialog_text = furi_string_alloc();
    furi_string_printf(
        dialog_text,
        "Send to RIC:\n%07lu\n\nMessage:\n%s\n\nFreq: %s\nMod: %s",
        app->send_ric,
        app->send_message,
        furi_string_get_cstr(frequency_str),
        furi_string_get_cstr(modulation_str));

    widget_add_text_scroll_element(
        app->widget, 0, 0, 128, 64, furi_string_get_cstr(dialog_text));

    // Set up context and enter callback for widget
    View* widget_view = widget_get_view(app->widget);
    view_set_context(widget_view, app);
    view_set_enter_callback(widget_view, pocsag_pager_scene_send_transmit_widget_enter_callback);

    view_dispatcher_switch_to_view(app->view_dispatcher, POCSAGPagerViewWidget);

    furi_string_free(dialog_text);
    furi_string_free(frequency_str);
    furi_string_free(modulation_str);
}

bool pocsag_pager_scene_send_transmit_on_event(void* context, SceneManagerEvent event) {  // SCENE_MAIN_HANDLER
    POCSAGPagerApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        // Custom events from button callbacks - SCENE_UNUSED
        // This branch exists but is not used in current implementation
    } else if(event.type == SceneManagerEventTypeTick) {
        // Check if user confirmed transmission
        if(pocsag_pager_scene_send_transmit_user_confirmed) {
            pocsag_pager_scene_send_transmit_user_confirmed = false;
            FURI_LOG_I("POCSAGSend", "User confirmed transmission");
            pcsg_tx_start(app, app->send_ric, app->send_message);
            consumed = true;
        }
        // Check for TX completion
        else if(pcsg_tx_is_complete()) {
            // TX completed - stop and show success
            FURI_LOG_I("POCSAGSend", "Transmission complete");
            pcsg_tx_stop(app);
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, POCSAGPagerSceneSend);
            consumed = true;
        } else if(app->txrx->txrx_state == PCSGTxRxStateTx) {
            // Blink LED during transmission
            notification_message(app->notifications, &sequence_blink_magenta_10);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        // Stop transmission if backing out
        pcsg_tx_stop(app);
        scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, POCSAGPagerSceneSend);
        consumed = true;
    }

    return consumed;
}

void pocsag_pager_scene_send_transmit_on_exit(void* context) {  // SCENE_EXIT_HANDLER
    POCSAGPagerApp* app = context;

    // Stop transmission
    pcsg_tx_stop(app);

    // Reset widget
    widget_reset(app->widget);

    // Show success message - this is the standard exit flow after TX completes
    widget_add_text_box_element(
        app->widget,
        0,
        0,
        128,
        64,
        AlignCenter,
        AlignCenter,
        "Message sent!",
        false);
    widget_add_button_element(
        app->widget, GuiButtonTypeCenter, "OK", NULL, NULL);
    view_dispatcher_switch_to_view(app->view_dispatcher, POCSAGPagerViewWidget);
}
