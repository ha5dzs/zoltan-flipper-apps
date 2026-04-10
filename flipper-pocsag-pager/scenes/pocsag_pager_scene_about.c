#include "../pocsag_pager_app_i.h"
#include "../helpers/pocsag_pager_types.h"

void pocsag_pager_scene_about_widget_callback(GuiButtonType result, InputType type, void* context) {
    POCSAGPagerApp* app = context;
    if(type == InputTypeShort) {
        view_dispatcher_send_custom_event(app->view_dispatcher, result);
    }
}

void pocsag_pager_scene_about_on_enter(void* context) {
    POCSAGPagerApp* app = context;

    FuriString* temp_str;
    temp_str = furi_string_alloc();
    furi_string_printf(temp_str, "\e#%s\n", "Information");

    furi_string_cat_printf(temp_str, "Version: %s\n", PCSG_VERSION_APP);
    furi_string_cat_printf(temp_str, "Developed by:\n%s\n\n", PCSG_DEVELOPED);
    furi_string_cat_printf(temp_str, "Github: %s\n\n", PCSG_GITHUB);

    furi_string_cat_printf(temp_str, "\e#%s\n", "Description");
    furi_string_cat_printf(
        temp_str,
        "Flipper POCSAG pager!\n~allows you send and receive\nPOCSAG-compliant pages.\nSTRICTLY FOR TESTING.\nCheck for files in\n/ext/pocsag/\nSome settings are in:\npocsag_settings.txt\nIncoming logs are stored in:\npocsag_received_pages.csv\nAlso, check the code!:)\n");

    furi_string_cat_printf(
        temp_str, "Supported protocols:\nPOCSAG 512, 1200, 2400\nBaudrate autodetects in RX\nTX is fixed to 512 baud, but\nthat can be very easily\nchanged in the code.\nFrequencies are hard-coded,\nbut that also can easily be\nchanged in the code.\n");

    widget_add_text_box_element(
        app->widget,
        0,
        0,
        128,
        14,
        AlignCenter,
        AlignBottom,
        "\e#\e!                                                      \e!\n",
        false);
    widget_add_text_box_element(
        app->widget,
        0,
        2,
        128,
        14,
        AlignCenter,
        AlignBottom,
        "\e#\e!        POCSAG Pager       \e!\n",
        false);
    widget_add_text_scroll_element(app->widget, 0, 16, 128, 50, furi_string_get_cstr(temp_str));
    furi_string_free(temp_str);

    view_dispatcher_switch_to_view(app->view_dispatcher, POCSAGPagerViewWidget);
}

bool pocsag_pager_scene_about_on_event(void* context, SceneManagerEvent event) {  // SCENE_UNUSED
    POCSAGPagerApp* app = context;
    bool consumed = false;
    UNUSED(app);  // Not used - this scene doesn't process events
    UNUSED(event);
    return consumed;
}

void pocsag_pager_scene_about_on_exit(void* context) {  // SCENE_UNUSED - no cleanup needed
    POCSAGPagerApp* app = context;
    UNUSED(app);  // No resources to free for widget scene
    // Clear views - widget is automatically reset by view dispatcher
    widget_reset(app->widget);
}
