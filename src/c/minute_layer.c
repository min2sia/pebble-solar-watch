#include <pebble.h>
#include "main.h"
#include "minute_layer.h"

Layer *layer; 
GContext *ctx;
GRect bounds;
GPoint center;
uint16_t radius;

static void draw_hour_hand() {
    const uint16_t hand_length = radius - 19;
    GPathInfo p_hour_hand_info = {
        .num_points = 5,
        .points = (GPoint []) {{3,12},{-3,12},{-2,-hand_length},{0,-(hand_length+1)},{2,-hand_length}}
    };
    
    static GPath *p_hour_hand = NULL;
    
    const int32_t hour_angle = (((solar_time_tm.tm_hour * 60) + solar_time_tm.tm_min) / 4) + 180;
    
    graphics_context_set_stroke_color(ctx, GColorWhite);
    p_hour_hand = gpath_create(&p_hour_hand_info);
    graphics_context_set_fill_color(ctx, GColorBlack);
    gpath_move_to(p_hour_hand, center);
    gpath_rotate_to(p_hour_hand, TRIG_MAX_ANGLE / 360 * hour_angle);
    gpath_draw_filled(ctx, p_hour_hand);
    gpath_draw_outline(ctx, p_hour_hand);
    gpath_destroy(p_hour_hand);
    
    // Draw hand "pin"
    //graphics_context_set_stroke_color(ctx, GColorBlack);
    //graphics_draw_circle(ctx, center, 5);
    draw_dot(ctx, center, 5);
}

static void draw_solar_wall_times() {
    static char wall_time_text[]  = "  ....  ";
    static char solar_time_text[] = "  ....  ";

    if (setting_digital_display) {   
        int16_t solar_time_x;
        int16_t solar_time_w = 140;
        int16_t clock_time_x;
        int16_t clock_time_w = 140;
        
        #if defined(PBL_RECT)
            int16_t solar_time_h = 30; // font height
            int16_t clock_time_h = 30; // font height
            int16_t solar_time_y = center.y/2 - 3;
            int16_t clock_time_y = center.y + 3;
        #elif defined(PBL_ROUND)       
            int16_t solar_time_h = 38; // font height
            int16_t clock_time_h = 38; // font height
            int16_t solar_time_y = center.y/2 - 12;
            int16_t clock_time_y = center.y + 7;
        #endif

        solar_time_x = center.x - (int)(solar_time_w / 2);
        clock_time_x = center.x - (int)(clock_time_w / 2);
        
        strftime(solar_time_text, sizeof(solar_time_text), time_format, &solar_time_tm);
        draw_outlined_text(ctx,
            solar_time_text,
            font_solar_time, 
            GRect(solar_time_x, solar_time_y, solar_time_w, solar_time_h),
            GTextOverflowModeFill,
            GTextAlignmentCenter,
            1,
            false);
        
        strftime(wall_time_text, sizeof(wall_time_text), time_format, &wall_time_tm);
        draw_outlined_text(ctx,
            wall_time_text,
            font_solar_time,
            GRect(clock_time_x, clock_time_y, clock_time_w, clock_time_h),
            GTextOverflowModeFill,
            GTextAlignmentCenter,
            1,
            true);
    }    
}

void minute_layer_update_proc(Layer *parm_layer, GContext *parm_ctx) {
    layer  = parm_layer;
    ctx    = parm_ctx;
    bounds = layer_get_bounds(layer);
    center = grect_center_point(&bounds);  
    radius = center.x < center.y ? center.x : center.y;
    
    draw_hour_hand();
    draw_solar_wall_times();
}


static void services_layer_update_proc(Layer* layer, GContext* ctx) {
    GRect bounds = layer_get_bounds(layer);
    GPoint center = grect_center_point(&bounds); 
    
    
    if (setting_connection_status && current_connection_available) {
        // Draw bluetooth icon
        int start_x = bounds.size.w - 13;
        int start_y = bounds.size.h - 35;
        graphics_context_set_stroke_color(ctx, GColorWhite);
        graphics_draw_line(ctx, GPoint(start_x+0, start_y+3),  GPoint(start_x+6, start_y+9));
        graphics_draw_line(ctx, GPoint(start_x+6, start_y+9),  GPoint(start_x+3, start_y+12));        
        graphics_draw_line(ctx, GPoint(start_x+3, start_y+12), GPoint(start_x+3, start_y+0));
        graphics_draw_line(ctx, GPoint(start_x+3, start_y+0),  GPoint(start_x+6, start_y+3));
        graphics_draw_line(ctx, GPoint(start_x+6, start_y+3),  GPoint(start_x+0, start_y+9));
    }  
    
    if (true) { //TODO: current_location_available
        // Draw GPS icon
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
