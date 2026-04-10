#include <furi.h>
#include <gui/gui.h>
#include <gui/view_port.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
/*
*  Comm to user menu
*/

void comm_to_user_menu_on_enter(void *context)
{
    UNUSED(context);
    return;
}

int comm_to_user_menu_on_event(void *context, SceneManagerEvent event, void *event_data)
{
    UNUSED(context);
    UNUSED(event_data);
    UNUSED(event);
    /*
    if (event == SceneManagerEventBack) {
        // Exit the app when the back button is pressed
        furi_app_exit();
        return true; // Event handled
    }
    return false; // Event not handled
    */
   return 0;
}

void comm_to_user_menu_on_exit(void *context)
{
    UNUSED(context);
    return;
}