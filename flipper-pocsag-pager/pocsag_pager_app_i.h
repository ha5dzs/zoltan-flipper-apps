#pragma once

#include "helpers/pocsag_pager_types.h"
#include "helpers/radio_device_loader.h"

#include "scenes/pocsag_pager_scene.h"
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/widget.h>
#include <notification/notification_messages.h>
#include "views/pocsag_pager_receiver.h"
#include "views/pocsag_pager_receiver_info.h"
#include "pocsag_pager_history.h"

#include <lib/subghz/subghz_setting.h>
#include <lib/subghz/subghz_worker.h>
#include <lib/subghz/receiver.h>
#include <lib/subghz/transmitter.h>
#include <lib/subghz/registry.h>
#include <lib/subghz/devices/devices.h>

#include <gui/modules/number_input.h>
#include <gui/modules/text_input.h>

#define PCSG_SETTINGS_DIR "/ext/pocsag"
#define PCSG_SETTINGS_FILE "/ext/pocsag/pocsag_settings.txt"
#define PCSG_HISTORY_FILE "/ext/pocsag/pocsag_received_pages.csv"

typedef struct POCSAGPagerApp POCSAGPagerApp;

struct POCSAGPagerTxRx {
    SubGhzWorker* worker;

    SubGhzEnvironment* environment;
    SubGhzReceiver* receiver;
    SubGhzRadioPreset* preset;
    PCSGHistory* history;
    uint16_t idx_menu_chosen;
    PCSGTxRxState txrx_state;
    PCSGHopperState hopper_state;
    uint8_t hopper_timeout;
    uint8_t hopper_idx_frequency;
    PCSGRxKeyState rx_key_state;

    const SubGhzDevice* radio_device;

    // Personalization
    uint32_t own_address;

    // Alarm mechanism - UNUSED
    // This alarm functionality was implemented but never wired up to any UI
    // and is not triggered by any receiver callbacks
    uint8_t alarm_triggered;          // Set in RX callback, processed in tick handler  // UNUSED
    uint32_t alarm_start_tick;        // Tick count when alarm beep started        // UNUSED
    uint8_t alarm_beep_active;        // Flag indicating beep is currently active  // UNUSED

    // CSV log buffer - NOTE: This structure is allocated but NOT actively used
    // The log buffer (PCSGLogBuffer) exists but pages are logged directly via
    // pocsag_pager_log_buffer_add() and pocsag_pager_log_page() without
    // using the buffer's queue mechanism
    PCSGLogBuffer log_buffer;         // Structure exists but not actively used  // PARTIALLY USED
};

typedef struct POCSAGPagerTxRx POCSAGPagerTxRx;

struct POCSAGPagerApp {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    POCSAGPagerTxRx* txrx;
    SceneManager* scene_manager;
    NotificationApp* notifications;
    VariableItemList* variable_item_list;
    Submenu* submenu;
    Widget* widget;
    PCSGReceiver* pcsg_receiver;
    PCSGReceiverInfo* pcsg_receiver_info;
    PCSGLock lock;
    SubGhzSetting* setting;

    // Sender views
    NumberInput* number_input;
    TextInput* text_input;

    // Sender state
    uint32_t send_ric;
    char send_message[PCSG_MAX_MESSAGE_LENGTH];
    bool send_message_valid;
};

void pcsg_preset_init(
    void* context,
    const char* preset_name,
    uint32_t frequency,
    uint8_t* preset_data,
    size_t preset_data_size);
void pcsg_get_frequency_modulation(
    POCSAGPagerApp* app,
    FuriString* frequency,
    FuriString* modulation);
void pcsg_begin(POCSAGPagerApp* app, uint8_t* preset_data);
uint32_t pcsg_rx(POCSAGPagerApp* app, uint32_t frequency);
void pcsg_idle(POCSAGPagerApp* app);
void pcsg_rx_end(POCSAGPagerApp* app);
void pcsg_sleep(POCSAGPagerApp* app);
void pcsg_hopper_update(POCSAGPagerApp* app);

// Sender functions
void pcsg_tx_start(POCSAGPagerApp* app, uint32_t ric, const char* message);
void pcsg_tx_stop(POCSAGPagerApp* app);
bool pcsg_validate_ric(uint32_t ric);
bool pcsg_validate_message(const char* message, size_t* out_length);

// TX completion check
bool pcsg_tx_is_complete(void);

// Config file functions are declared in helpers/pocsag_config.h
// which provides the declarations and the implementations are in helpers/pocsag_config.c
