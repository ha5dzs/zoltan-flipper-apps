#include <furi.h>
#include <gui/gui.h>

// ViewPort draw callback for title screen
void title_screen_draw_callback(Canvas* canvas, void* ctx) {
    UNUSED(ctx);
    canvas_clear(canvas);
    canvas_draw_frame(canvas, 0, 0, 128, 64);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 5, AlignCenter, AlignTop, "GUI Examples");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignTop, "Flipper GUI Catalogue");
    canvas_draw_str_aligned(canvas, 64, 50, AlignCenter, AlignTop, "Wait...");
}
