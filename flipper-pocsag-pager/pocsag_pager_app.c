#include "pocsag_pager_app_i.h"

#define TAG "POCSAGPagerApp"

#include <furi.h>
#include <furi_hal.h>
#include <lib/flipper_format/flipper_format.h>
#include "protocols/protocol_items.h"
#include "helpers/pocsag_config.h"
#include "helpers/radio_device_loader.h"

static bool pocsag_pager_app_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    POCSAGPagerApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool pocsag_pager_app_back_event_callback(void* context) {
    furi_assert(context);
    POCSAGPagerApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static void pocsag_pager_app_tick_event_callback(void* context) {
    furi_assert(context);
    POCSAGPagerApp* app = context;
    scene_manager_handle_tick_event(app->scene_manager);
}

POCSAGPagerApp* pocsag_pager_app_alloc() {
    // GUI first - needed for many Furi operations
    Gui* gui = furi_record_open(RECORD_GUI);

    POCSAGPagerApp* app = malloc(sizeof(POCSAGPagerApp));
    memset(app, 0, sizeof(POCSAGPagerApp));
    app->gui = gui;

    // View Dispatcher
    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&pocsag_pager_scene_handlers, app);

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, pocsag_pager_app_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, pocsag_pager_app_back_event_callback);
    view_dispatcher_set_tick_event_callback(
        app->view_dispatcher, pocsag_pager_app_tick_event_callback, 100);

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    // Open Notification record
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    // Variable Item List
    app->variable_item_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        POCSAGPagerViewVariableItemList,
        variable_item_list_get_view(app->variable_item_list));

    // SubMenu
    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, POCSAGPagerViewSubmenu, submenu_get_view(app->submenu));

    // Widget
    app->widget = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, POCSAGPagerViewWidget, widget_get_view(app->widget));

    // Receiver
    app->pcsg_receiver = pcsg_view_receiver_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        POCSAGPagerViewReceiver,
        pcsg_view_receiver_get_view(app->pcsg_receiver));

    // Receiver Info
    app->pcsg_receiver_info = pcsg_view_receiver_info_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        POCSAGPagerViewReceiverInfo,
        pcsg_view_receiver_info_get_view(app->pcsg_receiver_info));

    // Number Input (for RIC entry)
    app->number_input = number_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        POCSAGPagerViewNumberInput,
        number_input_get_view(app->number_input));

    // Text Input (for message entry)
    app->text_input = text_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        POCSAGPagerViewTextInput,
        text_input_get_view(app->text_input));

    //init setting
    app->setting = subghz_setting_alloc();

    //init Worker & Protocol & History
    app->lock = PCSGLockOff;
    app->txrx = malloc(sizeof(POCSAGPagerTxRx));
    memset(app->txrx, 0, sizeof(POCSAGPagerTxRx));
    app->txrx->preset = malloc(sizeof(SubGhzRadioPreset));
    memset(app->txrx->preset, 0, sizeof(SubGhzRadioPreset));
    app->txrx->preset->name = furi_string_alloc();

    furi_hal_power_suppress_charge_enter();

    FURI_LOG_D(TAG, "Radio devices init");
    subghz_devices_init();

    FURI_LOG_D(TAG, "Radio device loader set");
    app->txrx->radio_device =
        radio_device_loader_set(NULL, SubGhzRadioDeviceTypeInternal);

    FURI_LOG_D(TAG, "Devices reset");
    subghz_devices_reset(app->txrx->radio_device);
    FURI_LOG_D(TAG, "Devices idle");
    subghz_devices_idle(app->txrx->radio_device);

    // Custom Presets load
    FlipperFormat* temp_fm_preset = flipper_format_string_alloc();

    // FM95 preset: high deviation (~42 kHz), for testing
    // MDMCFG2 = 0x83 (MOD_FORMAT = 1, 2FSK, RZ=0)
    flipper_format_write_string_cstr(
        temp_fm_preset,
        (const char*)"Custom_preset_data",
        (const char*)"02 0D 0B 06 08 32 07 04 14 00 13 02 12 04 11 83 10 67 15 24 18 18 19 16 1D 91 1C 00 1B 07 20 FB 22 10 21 56 00 00 C0 00 00 00 00 00 00 00");
    flipper_format_rewind(temp_fm_preset);
    subghz_setting_load_custom_preset(app->setting, (const char*)"FM95", temp_fm_preset);

    // FM45 preset: lower deviation (~4.5 kHz), for POCSAG
    // Same as FM95 but with DEVIATN = 0x01 for lower deviation
    flipper_format_write_string_cstr(
        temp_fm_preset,
        (const char*)"Custom_preset_data",
        (const char*)"02 0D 0B 06 08 32 07 04 14 00 13 02 12 04 11 83 10 67 15 01 18 18 19 16 1D 91 1C 00 1B 07 20 FB 22 10 21 56 00 00 C0 00 00 00 00 00 00 00");
    flipper_format_rewind(temp_fm_preset);
    subghz_setting_load_custom_preset(app->setting, (const char*)"FM45", temp_fm_preset);

    // FM45_BIT0_HIGH: 2FSK with bit 0 = higher frequency - UNUSED
    // Same as FM45 but with MDMCFG1 = 0x13 for bit0=high freq
    // This preset was created to invert bit assignments but is NOT used
    // in current implementation - kept for potential future debugging
    flipper_format_write_string_cstr(
        temp_fm_preset,
        (const char*)"Custom_preset_data",
        (const char*)"02 0D 0B 06 08 32 07 04 14 00 13 02 12 04 11 83 10 13 15 01 18 18 19 16 1D 91 1C 00 1B 07 20 FB 22 10 21 56 00 00 C0 00 00 00 00 00 00 00");
    flipper_format_rewind(temp_fm_preset);
    subghz_setting_load_custom_preset(app->setting, (const char*)"FM45_B0H", temp_fm_preset);

    flipper_format_free(temp_fm_preset);

    // Initialize with defaults first
    furi_string_set_str(app->txrx->preset->name, PCSG_DEFAULT_PRESET);
    app->txrx->preset->frequency = PCSG_DEFAULT_FREQUENCY;
    app->txrx->hopper_state = PCSGHopperStateOFF;
    app->txrx->own_address = PCSG_DEFAULT_ADDRESS;
    FURI_LOG_I(TAG, "Initialized defaults: freq=%lu, preset=%s",
               app->txrx->preset->frequency, furi_string_get_cstr(app->txrx->preset->name));

    // Load settings from config file (will use defaults if file doesn't exist or is invalid)
    pocsag_pager_app_load_settings(app);
    FURI_LOG_I(TAG, "After load: freq=%lu, preset=%s, addr=%lu",
               app->txrx->preset->frequency, furi_string_get_cstr(app->txrx->preset->name),
               app->txrx->own_address);

    // Copy preset data to app's preset structure (after settings are loaded)
    const char* preset_name = furi_string_get_cstr(app->txrx->preset->name);
    uint8_t* preset_data = subghz_setting_get_preset_data_by_name(app->setting, preset_name);
    size_t preset_data_size = 0;
    for(size_t i = 0; i < subghz_setting_get_preset_count(app->setting); i++) {
        if(strcmp(subghz_setting_get_preset_name(app->setting, i), preset_name) == 0) {
            preset_data_size = subghz_setting_get_preset_data_size(app->setting, i);
            break;
        }
    }
    FURI_LOG_I(TAG, "Preset data: ptr=%p, size=%zu", preset_data, preset_data_size);
    // Allocate and copy preset data to ensure it persists
    if(preset_data && preset_data_size > 0) {
        uint8_t* copied_data = malloc(preset_data_size);
        memcpy(copied_data, preset_data, preset_data_size);
        app->txrx->preset->data = copied_data;
        app->txrx->preset->data_size = preset_data_size;
        FURI_LOG_I(TAG, "Preset data copied successfully");
    } else {
        app->txrx->preset->data = NULL;
        app->txrx->preset->data_size = 0;
        FURI_LOG_E(TAG, "Failed to get preset data");
    }

    app->txrx->history = pcsg_history_alloc();
    app->txrx->worker = subghz_worker_alloc();
    app->txrx->environment = subghz_environment_alloc();
    subghz_environment_set_protocol_registry(
        app->txrx->environment, (void*)&pocsag_pager_protocol_registry);
    app->txrx->receiver = subghz_receiver_alloc_init(app->txrx->environment);

    // Initialize log buffer - UNUSED
    // The log buffer (PCSGLogBuffer) is allocated but NOT actively used
    // Pages are logged directly via pocsag_pager_log_buffer_add() and
    // pocsag_pager_log_page() without using this buffer structure
    pocsag_log_buffer_init(&app->txrx->log_buffer);  // UNUSED

    subghz_receiver_set_filter(app->txrx->receiver, SubGhzProtocolFlag_Decodable);
    subghz_worker_set_overrun_callback(
        app->txrx->worker, (SubGhzWorkerOverrunCallback)subghz_receiver_reset);
    subghz_worker_set_pair_callback(
        app->txrx->worker, (SubGhzWorkerPairCallback)subghz_receiver_decode);
    subghz_worker_set_context(app->txrx->worker, app->txrx->receiver);

    scene_manager_next_scene(app->scene_manager, POCSAGPagerSceneStart);

    return app;
}

void pocsag_pager_app_free(POCSAGPagerApp* app) {
    furi_assert(app);

    // Radio Devices sleep & off
    pcsg_sleep(app);
    radio_device_loader_end(app->txrx->radio_device);

    subghz_devices_deinit();

    // Submenu
    view_dispatcher_remove_view(app->view_dispatcher, POCSAGPagerViewSubmenu);
    submenu_free(app->submenu);

    // Variable Item List
    view_dispatcher_remove_view(app->view_dispatcher, POCSAGPagerViewVariableItemList);
    variable_item_list_free(app->variable_item_list);

    //  Widget
    view_dispatcher_remove_view(app->view_dispatcher, POCSAGPagerViewWidget);
    widget_free(app->widget);

    // Receiver
    view_dispatcher_remove_view(app->view_dispatcher, POCSAGPagerViewReceiver);
    pcsg_view_receiver_free(app->pcsg_receiver);

    // Receiver Info
    view_dispatcher_remove_view(app->view_dispatcher, POCSAGPagerViewReceiverInfo);
    pcsg_view_receiver_info_free(app->pcsg_receiver_info);

    // Number Input
    view_dispatcher_remove_view(app->view_dispatcher, POCSAGPagerViewNumberInput);
    number_input_free(app->number_input);

    // Text Input
    view_dispatcher_remove_view(app->view_dispatcher, POCSAGPagerViewTextInput);
    text_input_free(app->text_input);

    //setting
    subghz_setting_free(app->setting);

    //Worker & Protocol & History
    subghz_receiver_free(app->txrx->receiver);
    subghz_environment_free(app->txrx->environment);
    pcsg_history_free(app->txrx->history);
    subghz_worker_free(app->txrx->worker);

    // Free preset name and preset
    if(app->txrx->preset) {
        if(app->txrx->preset->name) {
            furi_string_free(app->txrx->preset->name);
        }
        // Free copied preset data if it exists
        if(app->txrx->preset->data) {
            free(app->txrx->preset->data);
        }
        free(app->txrx->preset);
    }
    free(app->txrx);

    // View dispatcher
    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    // Notifications
    furi_record_close(RECORD_NOTIFICATION);
    app->notifications = NULL;

    // Close records
    furi_record_close(RECORD_GUI);

    furi_hal_power_suppress_charge_exit();

    free(app);
}

int32_t pocsag_pager_app(void* p) {
    UNUSED(p);
    POCSAGPagerApp* pocsag_pager_app = pocsag_pager_app_alloc();

    view_dispatcher_run(pocsag_pager_app->view_dispatcher);

    pocsag_pager_app_free(pocsag_pager_app);

    return 0;
}
