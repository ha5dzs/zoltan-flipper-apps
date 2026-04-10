#include "../pocsag_pager_app_i.h"
#include "../views/pocsag_pager_receiver.h"
#include "../helpers/pocsag_config.h"
#include "../protocols/pocsag.h"
#include <notification/notification_messages.h>
#include <furi_hal_speaker.h>
#include <furi_hal_vibro.h>

#define TAG "POCSAGPagerReceiver"

// Direct alarm beep and vibro using HAL - no notification service to avoid blocking
// This is the correct approach for custom frequency beeps
static void pocsag_alarm_start_beep() {
    // Start vibration
    furi_hal_vibro_on(true);

    // Acquire speaker for 100ms timeout
    if(furi_hal_speaker_acquire(100)) {
        // Start 1kHz tone at full volume
        furi_hal_speaker_start(1000.0f, 1.0f);
    }
}

// Stop alarm beep and vibro - only if we own the speaker
static void pocsag_alarm_stop_beep() {
    // Stop vibration
    furi_hal_vibro_on(false);

    if(furi_hal_speaker_is_mine()) {
        furi_hal_speaker_stop();
        furi_hal_speaker_release();
    }
}

// Check if received page is addressed to owner
// Sets alarm flag and returns true if RIC matches
static bool pocsag_is_page_for_owner(PCSGHistory* history, uint16_t idx, uint32_t own_address, POCSAGPagerApp* app) {
    if(own_address == 0) {
        FURI_LOG_D(TAG, "Alarm disabled: own_address is 0");
        return false;
    }

    uint32_t page_ric = 0;
    if(pcsg_history_get_ric(history, idx, &page_ric)) {
        bool is_match = (page_ric == own_address);
        if(is_match) {
            FURI_LOG_I(TAG, "Page for owner! RIC=%lu, own_address=%lu", page_ric, own_address);
            // Set alarm flag for tick handler to process
            app->txrx->alarm_triggered = true;
        }
        return is_match;
    }
    return false;
}

void pocsag_pager_scene_receiver_callback(PCSGCustomEvent event, void* context) {
    furi_assert(context);
    POCSAGPagerApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, event);
}

static void pocsag_pager_scene_receiver_add_to_history_callback(
    SubGhzReceiver* receiver,
    SubGhzProtocolDecoderBase* decoder_base,
    void* context) {
    furi_assert(context);
    POCSAGPagerApp* app = context;
    FuriString* str_buff;
    str_buff = furi_string_alloc();

    FURI_LOG_I(TAG, "RX callback invoked!");

    if(pcsg_history_add_to_history(app->txrx->history, decoder_base, app->txrx->preset) ==
       PCSGHistoryStateAddKeyNewDada) {
        FURI_LOG_I(TAG, "History added, updating UI");
        furi_string_reset(str_buff);

        pcsg_history_get_text_item_menu(
            app->txrx->history, str_buff, pcsg_history_get_item(app->txrx->history) - 1);
        FURI_LOG_I(TAG, "Adding message to UI: %s", furi_string_get_cstr(str_buff));

        FURI_LOG_I(TAG, "Before add_item_to_menu");
        pcsg_view_receiver_add_item_to_menu(
            app->pcsg_receiver,
            furi_string_get_cstr(str_buff),
            pcsg_history_get_type_protocol(
                app->txrx->history, pcsg_history_get_item(app->txrx->history) - 1));

        // Status bar now updates automatically in draw function

        // Add to log buffer for async CSV logging
        uint32_t ric = 0;
        FuriString* msg = furi_string_alloc();
        uint16_t hist_idx = pcsg_history_get_item(app->txrx->history) - 1;
        if(pcsg_history_get_ric(app->txrx->history, hist_idx, &ric) &&
           pcsg_history_get_message(app->txrx->history, hist_idx, msg)) {
            pocsag_log_buffer_add(&app->txrx->log_buffer, ric, furi_string_get_cstr(msg));
        }
        furi_string_free(msg);

        // Check if page is addressed to owner - set flag for tick handler
        if(pocsag_is_page_for_owner(app->txrx->history, hist_idx, app->txrx->own_address, app)) {
            FURI_LOG_I(TAG, "Page addressed to owner! Flag set for alarm.");
            // Note: Alarm processing moved to tick handler to avoid blocking RX callback
        }

    } else {
        FURI_LOG_D(TAG, "History did not add new data");
    }
    subghz_receiver_reset(receiver);
    furi_string_free(str_buff);
    app->txrx->rx_key_state = PCSGRxKeyStateAddKey;
}

void pocsag_pager_scene_receiver_on_enter(void* context) {
    POCSAGPagerApp* app = context;

    FuriString* str_buff;
    str_buff = furi_string_alloc();

    if(app->txrx->rx_key_state == PCSGRxKeyStateIDLE) {
        pcsg_history_reset(app->txrx->history);
        app->txrx->rx_key_state = PCSGRxKeyStateStart;
    }

    pcsg_view_receiver_set_lock(app->pcsg_receiver, app->lock);
    pcsg_view_receiver_set_ext_module_state(
        app->pcsg_receiver, radio_device_loader_is_external(app->txrx->radio_device));

    //Load history to receiver
    pcsg_view_receiver_exit(app->pcsg_receiver);
    for(uint8_t i = 0; i < pcsg_history_get_item(app->txrx->history); i++) {
        furi_string_reset(str_buff);
        pcsg_history_get_text_item_menu(app->txrx->history, str_buff, i);
        pcsg_view_receiver_add_item_to_menu(
            app->pcsg_receiver,
            furi_string_get_cstr(str_buff),
            pcsg_history_get_type_protocol(app->txrx->history, i));
        app->txrx->rx_key_state = PCSGRxKeyStateAddKey;
    }
    furi_string_free(str_buff);
    // Status bar updates automatically in draw function

    pcsg_view_receiver_set_callback(app->pcsg_receiver, pocsag_pager_scene_receiver_callback, app);
    subghz_receiver_set_rx_callback(
        app->txrx->receiver, pocsag_pager_scene_receiver_add_to_history_callback, app);

    // Get preset data for the current preset name
    const char* preset_name = furi_string_get_cstr(app->txrx->preset->name);
    uint8_t* preset_data = subghz_setting_get_preset_data_by_name(app->setting, preset_name);
    FURI_LOG_I(TAG, "Getting preset data for '%s': %p", preset_name, preset_data);

    if(app->txrx->txrx_state == PCSGTxRxStateRx) {
        pcsg_rx_end(app);
    };
    if((app->txrx->txrx_state == PCSGTxRxStateIDLE) ||
       (app->txrx->txrx_state == PCSGTxRxStateSleep)) {
        // Start RX
        pcsg_begin(app, preset_data);
        FURI_LOG_I(TAG, "Starting RX on frequency %lu", app->txrx->preset->frequency);
        pcsg_rx(app, app->txrx->preset->frequency);
    }

    pcsg_view_receiver_set_idx_menu(app->pcsg_receiver, app->txrx->idx_menu_chosen);
    view_dispatcher_switch_to_view(app->view_dispatcher, POCSAGPagerViewReceiver);
}

bool pocsag_pager_scene_receiver_on_event(void* context, SceneManagerEvent event) {
    POCSAGPagerApp* app = context;
    bool consumed = false;
    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case PCSGCustomEventViewReceiverBack:
            // Stop CC1101 Rx
            if(app->txrx->txrx_state == PCSGTxRxStateRx) {
                pcsg_rx_end(app);
                pcsg_idle(app);
            };
            app->txrx->hopper_state = PCSGHopperStateOFF;
            app->txrx->idx_menu_chosen = 0;
            subghz_receiver_set_rx_callback(app->txrx->receiver, NULL, app);

            app->txrx->rx_key_state = PCSGRxKeyStateIDLE;
            // Don't reset to hardcoded frequency - use the current settings
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, POCSAGPagerSceneStart);
            consumed = true;
            break;
        case PCSGCustomEventViewReceiverOK:
            app->txrx->idx_menu_chosen = pcsg_view_receiver_get_idx_menu(app->pcsg_receiver);
            scene_manager_next_scene(app->scene_manager, POCSAGPagerSceneReceiverInfo);
            consumed = true;
            break;
        case PCSGCustomEventViewReceiverConfig:
            app->txrx->idx_menu_chosen = pcsg_view_receiver_get_idx_menu(app->pcsg_receiver);
            scene_manager_next_scene(app->scene_manager, POCSAGPagerSceneReceiverConfig);
            consumed = true;
            break;
        case PCSGCustomEventViewReceiverOffDisplay:
            notification_message(app->notifications, &sequence_display_backlight_off);
            consumed = true;
            break;
        case PCSGCustomEventViewReceiverUnlock:
            app->lock = PCSGLockOff;
            consumed = true;
            break;
        default:
            break;
        }
    } else if(event.type == SceneManagerEventTypeTick) {
        if(app->txrx->hopper_state != PCSGHopperStateOFF) {
            pcsg_hopper_update(app);
        }
        // Get current RSSI
        float rssi = subghz_devices_get_rssi(app->txrx->radio_device);
        pcsg_receiver_rssi(app->pcsg_receiver, rssi);

        if(app->txrx->txrx_state == PCSGTxRxStateRx) {
            // Blink indicator for RX - user requested beep only, no LED blink

            // Process alarm if triggered (only once per trigger)
            if(app->txrx->alarm_triggered && !app->txrx->alarm_beep_active) {
                FURI_LOG_I(TAG, "Processing alarm in tick handler");

                // Start 1kHz tone directly
                pocsag_alarm_start_beep();
                app->txrx->alarm_start_tick = furi_get_tick();
                app->txrx->alarm_beep_active = 1;

                // Reset alarm flag
                app->txrx->alarm_triggered = false;
            }

            // Flush log buffer - write any pending pages to CSV
            // This runs when RX is active, so we can safely write to storage
            while(pocsag_log_buffer_has_entries(&app->txrx->log_buffer)) {
                uint32_t ric;
                char msg[PCSG_MAX_MESSAGE_LENGTH];
                if(pocsag_log_buffer_get_next(&app->txrx->log_buffer, &ric, msg, sizeof(msg))) {
                    pocsag_pager_log_page(ric, msg);
                }
            }

            // Stop beep if duration elapsed and beep is active
            if(app->txrx->alarm_beep_active &&
               (furi_get_tick() - app->txrx->alarm_start_tick) >= 200) {
                FURI_LOG_I(TAG, "Stopping alarm beep after 200ms");
                pocsag_alarm_stop_beep();
                app->txrx->alarm_beep_active = 0;
            }
        }
    }
    return consumed;
}

void pocsag_pager_scene_receiver_on_exit(void* context) {
    UNUSED(context);
}
