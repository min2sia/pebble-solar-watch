#include <pebble.h>
#include "main.h"
#include "draw.h"
#include "services_layer.h"

Layer *layer; 
GContext *ctx;
GRect bounds;
GPoint center;

void draw_battery_status() {
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

void draw_bluetooth_icon() {
    if (current_connection_available) {
        int start_x = bounds.size.w - 13;
        int start_y = bounds.size.h - 35;
        graphics_context_set_stroke_color(ctx, GColorWhite);
        graphics_draw_line(ctx, GPoint(start_x+0, start_y+3),  GPoint(start_x+6, start_y+9));
        graphics_draw_line(ctx, GPoint(start_x+6, start_y+9),  GPoint(start_x+3, start_y+12));        
        graphics_draw_line(ctx, GPoint(start_x+3, start_y+12), GPoint(start_x+3, start_y+0));
        graphics_draw_line(ctx, GPoint(start_x+3, start_y+0),  GPoint(start_x+6, start_y+3));
        graphics_draw_line(ctx, GPoint(start_x+6, start_y+3),  GPoint(start_x+0, start_y+9));
    }  
}

void draw_GPS_icon() {
    if (current_location_available) { 
        int start_x = 5;
        int start_y = bounds.size.h - 35;
        graphics_context_set_stroke_color(ctx, GColorWhite);
        graphics_draw_circle(ctx, GPoint(start_x+5, start_y+5), 2);
        graphics_draw_arc(
            ctx, 
            GRect(start_x, start_y, 11, 11), 
            GOvalScaleModeFitCircle, //GOvalScaleModeFillCircle, 
            DEG_TO_TRIGANGLE(-100),
            DEG_TO_TRIGANGLE(100));
        graphics_draw_line(ctx, GPoint(start_x-1,  start_y+5), GPoint(start_x+5, start_y+13));
        graphics_draw_line(ctx, GPoint(start_x+11, start_y+5), GPoint(start_x+5, start_y+13));
    }
}

static void draw_temperature() {
    draw_outlined_text(ctx,
        temperature_text,
        fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
        GRect(0, 0, center.x, 10),
        GTextOverflowModeWordWrap,
        GTextAlignmentLeft,
        0,
        true);
}

void services_layer_update_proc(Layer *parm_layer, GContext *parm_ctx) {
    layer  = parm_layer;
    ctx    = parm_ctx;
    bounds = layer_get_bounds(layer);
    center = grect_center_point(&bounds);  

    draw_battery_status();
    draw_bluetooth_icon();
    draw_GPS_icon();
    draw_temperature();
}