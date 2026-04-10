#include "../pocsag_pager_app_i.h"
#include "../views/pocsag_pager_receiver.h"

void pocsag_pager_scene_receiver_info_callback(PCSGCustomEvent event, void* context) {
    furi_assert(context);
    POCSAGPagerApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, event);
}

static void pocsag_pager_scene_receiver_info_add_to_history_callback(
    SubGhzReceiver* receiver,
    SubGhzProtocolDecoderBase* decoder_base,
    void* context) {
    furi_assert(context);
    POCSAGPagerApp* app = context;

    // idx_menu_chosen is display position (0=top=newest), convert to history index
    uint16_t history_count = pcsg_history_get_item(app->txrx->history);
    uint16_t history_idx = 0;
    if(history_count > 0 && app->txrx->idx_menu_chosen < history_count) {
        history_idx = history_count - 1 - app->txrx->idx_menu_chosen;
    }

    if(pcsg_history_add_to_history(app->txrx->history, decoder_base, app->txrx->preset) ==
       PCSGHistoryStateAddKeyUpdateData) {
        pcsg_view_receiver_info_update(
            app->pcsg_receiver_info,
            pcsg_history_get_raw_data(app->txrx->history, history_idx));
        subghz_receiver_reset(receiver);

        notification_message(app->notifications, &sequence_blink_green_10);
        app->txrx->rx_key_state = PCSGRxKeyStateAddKey;
    }
}

void pocsag_pager_scene_receiver_info_on_enter(void* context) {
    POCSAGPagerApp* app = context;

    // idx_menu_chosen is display position (0=top=newest), convert to history index
    uint16_t history_count = pcsg_history_get_item(app->txrx->history);
    uint16_t history_idx = 0;
    if(history_count > 0 && app->txrx->idx_menu_chosen < history_count) {
        // Display position 0 = newest = last history item
        history_idx = history_count - 1 - app->txrx->idx_menu_chosen;
    }

    subghz_receiver_set_rx_callback(
        app->txrx->receiver, pocsag_pager_scene_receiver_info_add_to_history_callback, app);
    pcsg_view_receiver_info_update(
        app->pcsg_receiver_info,
        pcsg_history_get_raw_data(app->txrx->history, history_idx));
    view_dispatcher_switch_to_view(app->view_dispatcher, POCSAGPagerViewReceiverInfo);
}

// Scene event handler for receiver info - CURRENTLY UNUSED
// The ReceiverInfo scene is a simple display-only view that doesn't
// process events - all interaction happens in the Receiver scene
// This function exists to satisfy the scene framework but just returns false
bool pocsag_pager_scene_receiver_info_on_event(void* context, SceneManagerEvent event) {  // SCENE_UNUSED
    POCSAGPagerApp* app = context;
    bool consumed = false;
    UNUSED(app);
    UNUSED(event);
    return consumed;
}

void pocsag_pager_scene_receiver_info_on_exit(void* context) {  // SCENE_UNUSED - no cleanup needed
    UNUSED(context);  // No resources to free
}
