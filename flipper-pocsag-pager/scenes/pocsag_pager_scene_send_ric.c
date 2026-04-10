#include "../pocsag_pager_app_i.h"
#include "../helpers/pocsag_config.h"

// Number input callback - UNUSED
// The send RIC scene processes results via event handling instead of direct callback
void pocsag_pager_scene_send_ric_number_input_callback(void* context, int32_t number) {  // SCENE_UNUSED
    POCSAGPagerApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, number);
}

void pocsag_pager_scene_send_ric_on_enter(void* context) {
    POCSAGPagerApp* app = context;
    NumberInput* number_input = app->number_input;

    // Set initial value from current send_ric
    number_input_set_header_text(number_input, "Enter RIC (7-digit):");

    // Initialize with default or previously entered RIC
    // Default is 0 (all zeros), previously entered RIC if valid
    uint32_t initial_ric = app->send_ric;
    if(!pcsg_validate_ric(initial_ric)) {
        initial_ric = 0;
    }

    // Set callback with initial values (allow 0-9999999)
    number_input_set_result_callback(
        number_input, pocsag_pager_scene_send_ric_number_input_callback, app, initial_ric, 0, 9999999);

    view_dispatcher_switch_to_view(app->view_dispatcher, POCSAGPagerViewNumberInput);
}

bool pocsag_pager_scene_send_ric_on_event(void* context, SceneManagerEvent event) {
    POCSAGPagerApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        // Check if OK was pressed (number was returned)
        if(event.event <= 9999999) {
            uint32_t ric = (uint32_t)event.event;

            // Validate RIC
            if(pcsg_validate_ric(ric)) {
                app->send_ric = ric;
                app->send_message_valid = false;
                scene_manager_next_scene(app->scene_manager, POCSAGPagerSceneSend);
                consumed = true;
            }
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, POCSAGPagerSceneSend);
        consumed = true;
    }

    return consumed;
}

void pocsag_pager_scene_send_ric_on_exit(void* context) {
    POCSAGPagerApp* app = context;
    number_input_set_result_callback(app->number_input, NULL, NULL, 0, 0, 0);
}
