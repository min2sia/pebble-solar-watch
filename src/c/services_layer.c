#include <pebble.h>
#include "main.h"
#include "graph.h"
#include "services_layer.h"

void services_layer_update_proc(Layer *layer, GContext *ctx) {
    if (!setting_battery_status) return;

    GRect bounds = layer_get_bounds(layer);
    GPoint center = grect_center_point(&bounds); 
    
    graphics_context_set_text_color(ctx, GColorWhite);
    draw_outlined_text(
        ctx,
        battery_level_string,
        fonts_get_system_font(FONT_KEY_GOTHIC_14),
        GRect(center.x - 10, bounds.size.h - 16, 30, 14),
        GTextOverflowModeWordWrap,
        GTextAlignmentCenter,
        1,
        true);
}