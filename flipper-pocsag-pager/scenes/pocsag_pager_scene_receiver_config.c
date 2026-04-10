#include "../pocsag_pager_app_i.h"
#include "../helpers/pocsag_config.h"

enum PCSGSettingIndex {
    PCSGSettingIndexFrequency,
    PCSGSettingIndexHopping,
    PCSGSettingIndexModulation,
    PCSGSettingIndexAddress,
};

// POCSAG frequency options (in Hz)
static const uint32_t pocsag_frequencies[] = {
    304000000,  // 304 MHz
    312000000,  // 312 MHz
    315000000,  // 315 MHz
    433920000,  // 433.92 MHz (testing)
    434000000,  // 434 MHz
    439987500,  // 439.9875 MHz (FM95 default)
    441000000,  // 441 MHz
    445000000,  // 445 MHz
    446000000,  // 446 MHz
    868000000,  // 868 MHz
};

static const uint8_t pocsag_frequency_count = sizeof(pocsag_frequencies) / sizeof(uint32_t);

#define HOPPING_COUNT 2
const char* const hopping_text[HOPPING_COUNT] = {
    "OFF",
    "ON",
};
const uint32_t hopping_value[HOPPING_COUNT] = {
    PCSGHopperStateOFF,
    PCSGHopperStateRunnig,
};

static void pocsag_pager_scene_receiver_config_set_frequency(VariableItem* item) {
    POCSAGPagerApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    if(app->txrx->hopper_state == PCSGHopperStateOFF) {
        char text_buf[10] = {0};
        snprintf(
            text_buf,
            sizeof(text_buf),
            "%lu.%02lu",
            pocsag_frequencies[index] / 1000000,
            (pocsag_frequencies[index] % 1000000) / 10000);
        variable_item_set_current_value_text(item, text_buf);
        app->txrx->preset->frequency = pocsag_frequencies[index];
    } else {
        variable_item_set_current_value_index(item, 0);
    }
}

static uint8_t pocsag_pager_scene_receiver_config_next_frequency(const uint32_t value) {
    uint8_t index = 0;
    for(uint8_t i = 0; i < pocsag_frequency_count; i++) {
        if(value == pocsag_frequencies[i]) {
            index = i;
            break;
        }
    }
    return index;
}

static void pocsag_pager_scene_receiver_config_set_preset(VariableItem* item) {
    POCSAGPagerApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(
        item, subghz_setting_get_preset_name(app->setting, index));
    pcsg_preset_init(
        app,
        subghz_setting_get_preset_name(app->setting, index),
        app->txrx->preset->frequency,
        subghz_setting_get_preset_data(app->setting, index),
        subghz_setting_get_preset_data_size(app->setting, index));
}

static uint8_t pocsag_pager_scene_receiver_config_next_preset(POCSAGPagerApp* app, const char* preset_name) {
    uint8_t index = 0;
    for(uint8_t i = 0; i < subghz_setting_get_preset_count(app->setting); i++) {
        if(!strcmp(subghz_setting_get_preset_name(app->setting, i), preset_name)) {
            index = i;
            break;
        }
    }
    return index;
}

static void pocsag_pager_scene_receiver_config_set_hopping_running(VariableItem* item) {
    POCSAGPagerApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    variable_item_set_current_value_text(item, hopping_text[index]);
    if(hopping_value[index] == PCSGHopperStateOFF) {
        char text_buf[10] = {0};
        snprintf(
            text_buf,
            sizeof(text_buf),
            "%lu.%02lu",
            (unsigned long)app->txrx->preset->frequency / 1000000,
            ((unsigned long)app->txrx->preset->frequency % 1000000) / 10000);
        variable_item_set_current_value_text(
            (VariableItem*)scene_manager_get_scene_state(
                app->scene_manager, POCSAGPagerSceneReceiverConfig),
            text_buf);
    } else {
        variable_item_set_current_value_text(
            (VariableItem*)scene_manager_get_scene_state(
                app->scene_manager, POCSAGPagerSceneReceiverConfig),
            " -----");
    }

    app->txrx->hopper_state = hopping_value[index];
}

// Store the initial own_address when entering config scene
static uint32_t pocsag_pager_scene_config_center_address = 0;

static void pocsag_pager_scene_receiver_config_set_address(VariableItem* item) {
    POCSAGPagerApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    // Use the stored center address, not the one being edited
    uint32_t center = pocsag_pager_scene_config_center_address;
    char text_buf[16] = {0};
    uint32_t address;

    // Calculate offset from center (index 10 = center address)
    // Index 0 = center - 10, Index 10 = center, Index 20 = center + 10
    int32_t offset = (int32_t)index - 10;

    // Calculate new address with wrapping
    if(center == 0) {
        // Special case: center is 0
        // Range wraps: 9999990 to 0000010
        int32_t new_addr = (int32_t)center + offset;
        if(new_addr > 10) {
            new_addr = new_addr - 10000000;
        }
        if(new_addr < 9999990) {
            new_addr = new_addr + 10000000;
        }
        address = (uint32_t)new_addr;
    } else if(center <= 10) {
        // Near 0 boundary: range wraps
        int32_t new_addr = (int32_t)center + offset;
        if(new_addr < 0) {
            new_addr = 10000000 + new_addr;
        }
        if(new_addr > 9999999) {
            new_addr = new_addr - 10000000;
        }
        address = (uint32_t)new_addr;
    } else if(center >= 9999990) {
        // Near max boundary: range wraps
        int32_t new_addr = (int32_t)center + offset;
        if(new_addr > 9999999) {
            new_addr = new_addr - 10000000;
        }
        if(new_addr < 9999990) {
            new_addr = new_addr + 10000000;
        }
        address = (uint32_t)new_addr;
    } else {
        // Normal case: center - 10 to center + 10
        int32_t new_addr = (int32_t)center + offset;
        if(new_addr < 0) {
            new_addr = 0;
        }
        if(new_addr > 9999999) {
            new_addr = 9999999;
        }
        address = (uint32_t)new_addr;
    }

    snprintf(text_buf, sizeof(text_buf), "%07lu", address);
    variable_item_set_current_value_text(item, text_buf);
    // Update the actual own_address to the current display value
    app->txrx->own_address = address;
}

// Enter callback for variable item list - UNUSED in current implementation
// The config scene doesn't need special handling for item selection
static void
    pocsag_pager_scene_receiver_config_var_list_enter_callback(void* context, uint32_t index) {  // SCENE_UNUSED
    UNUSED(context);
    UNUSED(index);
}

void pocsag_pager_scene_receiver_config_on_enter(void* context) {
    POCSAGPagerApp* app = context;
    VariableItem* item;
    uint8_t value_index;

    // Store the initial own_address for linear stepping
    pocsag_pager_scene_config_center_address = app->txrx->own_address;

    item = variable_item_list_add(
        app->variable_item_list,
        "Frequency:",
        pocsag_frequency_count,
        pocsag_pager_scene_receiver_config_set_frequency,
        app);
    value_index = pocsag_pager_scene_receiver_config_next_frequency(app->txrx->preset->frequency);
    scene_manager_set_scene_state(
        app->scene_manager, POCSAGPagerSceneReceiverConfig, (uint32_t)item);
    variable_item_set_current_value_index(item, value_index);
    char text_buf[10] = {0};
    snprintf(
        text_buf,
        sizeof(text_buf),
        "%lu.%02lu",
        pocsag_frequencies[value_index] / 1000000,
        (pocsag_frequencies[value_index] % 1000000) / 10000);
    variable_item_set_current_value_text(item, text_buf);

    item = variable_item_list_add(
        app->variable_item_list,
        "Hopping:",
        HOPPING_COUNT,
        pocsag_pager_scene_receiver_config_set_hopping_running,
        app);
    value_index = (app->txrx->hopper_state == PCSGHopperStateOFF) ? 0 : 1;
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(item, hopping_text[value_index]);

    item = variable_item_list_add(
        app->variable_item_list,
        "Modulation:",
        subghz_setting_get_preset_count(app->setting),
        pocsag_pager_scene_receiver_config_set_preset,
        app);
    value_index = pocsag_pager_scene_receiver_config_next_preset(
        app, furi_string_get_cstr(app->txrx->preset->name));
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(
        item, subghz_setting_get_preset_name(app->setting, value_index));

    // Add Address setting with fixed range of 21 items
    // Range is +/- 10 around the own_address value
    // The set_address function handles the display logic
    item = variable_item_list_add(
        app->variable_item_list,
        "Own Address:",
        21,  // Fixed count: own_addr - 10 to own_addr + 10
        pocsag_pager_scene_receiver_config_set_address,
        app);
    snprintf(text_buf, sizeof(text_buf), "%07lu", app->txrx->own_address);
    variable_item_set_current_value_text(item, text_buf);

    // Set initial value index to show own_address (index 10 = own_addr)
    value_index = 10;
    variable_item_set_current_value_index(item, value_index);

    variable_item_list_set_enter_callback(
        app->variable_item_list, pocsag_pager_scene_receiver_config_var_list_enter_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, POCSAGPagerViewVariableItemList);
}

bool pocsag_pager_scene_receiver_config_on_event(void* context, SceneManagerEvent event) {  // SCENE_UNUSED
    UNUSED(context);  // Not used - this scene doesn't process events
    UNUSED(event);
    return false;
}

void pocsag_pager_scene_receiver_config_on_exit(void* context) {
    POCSAGPagerApp* app = context;
    variable_item_list_set_selected_item(app->variable_item_list, 0);
    variable_item_list_reset(app->variable_item_list);

    // Save settings to file on exit
    pocsag_pager_app_save_settings(app);
}
