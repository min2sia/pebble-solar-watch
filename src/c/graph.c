#include <pebble.h>
#include "graph.h"
#include "util.h"

void draw_dotted_line_b(GContext* ctx, GPoint from, GPoint to, GColor color_1, GColor color_2, int step) {
    int x = from.x;
    int y = from.y;
    int dx = abs(to.x - from.x);
    int sx = from.x < to.x ? 1 : -1;
    int dy = abs(to.y - from.y); 
    int sy = from.y < to.y ? 1 : -1; 
    int err = (dx > dy ? dx : -dy)/2; 
    int e2;
 
    for(;;) {
        if ((dx > dy && x % step == 0) || (dy >= dx && y % step == 0)) {
            graphics_context_set_stroke_color(ctx, color_1);    
        } else {
            graphics_context_set_stroke_color(ctx, color_2);    
        }
        graphics_draw_pixel(ctx, GPoint(x, y));
        if (x == to.x && y == to.y) {
            break;
        }
        e2 = err;
        if (e2 > -dx) { 
            err -= dy; 
            x += sx; 
        }
        if (e2 < dy) { 
            err += dx; 
            y += sy; 
        }
    }
}

void draw_dotted_line(GContext* ctx, GPoint from, GPoint to) {
    const uint16_t x_pixels = my_abs(from.x - to.x) + 1;
    const uint16_t y_pixels = my_abs(from.y - to.y) + 1;
    
    int16_t x_direction = 1;
    int16_t y_direction = 1;    
    if (from.x > to.x){
        x_direction = -1;
    }
    if (from.y > to.y){
        y_direction = -1;
    }

    int16_t j = 0;
    int16_t x = 0;
    int16_t y = 0;
    if (x_pixels >= y_pixels) { 
        // there are more pixels to draw on X axis, use X as major axis for optimal pixel density 
        for (uint16_t i=0; i<x_pixels; i++) {            
            // Approximate j:
            j = (int)round((double)(y_pixels-1) * ((double)i / (double)x_pixels)); 
            
            x = from.x + (i * x_direction);
            y = from.y + (j * y_direction);
            
            if (i % 2 == 0){
                graphics_context_set_stroke_color(ctx, GColorBlack);    
            } else {
                graphics_context_set_stroke_color(ctx, GColorWhite);    
            }
            graphics_draw_pixel(ctx, GPoint(x, y));
        }
    } else { 
        // there are more pixels to draw on Y axis, use Y as major axis for optimal pixel density 
        for (uint16_t i=0; i<y_pixels; i++) {            
            // Approximate j:
            j = (int)round((double)(x_pixels-1) * ((double)i / (double)y_pixels));
            
            y = from.y + (i * y_direction);
            x = from.x + (j * x_direction);
            
            if (i % 2 == 0){
                graphics_context_set_stroke_color(ctx, GColorBlack);    
            } else {
                graphics_context_set_stroke_color(ctx, GColorWhite);    
            }
            graphics_draw_pixel(ctx, GPoint(x, y));
        }
    }   
}

void draw_outlined_text(
    GContext* ctx, 
    char* text, 
    GFont font, 
    GRect rect, 
    GTextOverflowMode mode, 
    GTextAlignment alignment, 
    int outline_pixels, 
    bool inverted) {
    
    (inverted) ? graphics_context_set_text_color(ctx, GColorBlack) :
    graphics_context_set_text_color(ctx, GColorWhite);
// results are pretty awful for `outline_pixel' values larger than 2...    
    GRect volatile_rect = rect;
    for (int i=0; i<outline_pixels; i++) {
        volatile_rect.origin.x+=1;
        graphics_draw_text(ctx,
        text,
        font,
        volatile_rect,
        mode,
        alignment,
        NULL);
    }
    volatile_rect = rect;
    for (int i=0; i<outline_pixels; i++) {
        volatile_rect.origin.y+=1;
        graphics_draw_text(ctx,
        text,
        font,
        volatile_rect,
        mode,
        alignment,
        NULL);
    }
    volatile_rect = rect;
    for (int i=0; i<outline_pixels; i++) {
        volatile_rect.origin.x-=1;
        graphics_draw_text(ctx,
        text,
        font,
        volatile_rect,
        mode,
        alignment,
        NULL);
    }
    volatile_rect = rect;
    for (int i=0; i<outline_pixels; i++) {
        volatile_rect.origin.y-=1;
        graphics_draw_text(ctx,
        text,
        font,
        volatile_rect,
        mode,
        alignment,
        NULL);
    }
    (inverted) ? graphics_context_set_text_color(ctx, GColorWhite) :
    graphics_context_set_text_color(ctx, GColorBlack);
    graphics_draw_text(ctx,
    text,
    font,
    rect,
    mode,
    alignment,
    NULL);
}

void draw_dot(GContext* ctx, GPoint center, int radius) {
    graphics_context_set_stroke_color(ctx, GColorWhite);
    graphics_context_set_fill_color(ctx,GColorBlack);
    graphics_fill_circle(ctx, center, radius);
    graphics_draw_circle(ctx, center, radius);
    graphics_context_set_stroke_color(ctx, GColorBlack);
    graphics_draw_circle(ctx, center, radius+1);
}

void draw_dithered_radial(
    uint16_t dither_percent,
    GContext *ctx, 
    GPoint center, 
    uint16_t radius, 
    uint16_t inset_radius, 
    int32_t angle_start, 
    int32_t angle_end, 
    GColor first, 
    GColor second) {
    
    int16_t     opposite = 0;
    int16_t     adjacent = 0;
    int32_t angle = 0;
    
    //APP_LOG(APP_LOG_LEVEL_DEBUG, "draw_thirty_percent_radial() angle_start = %d (%d), angle_end = %d (%d)", (int)angle_start, (int)TRIGANGLE_TO_DEG(angle_start), (int)angle_end, (int)TRIGANGLE_TO_DEG(angle_end));
    for (int16_t x = center.x-radius; x < center.x+radius; x++) {
        for (int16_t y = center.y-radius; y < center.y+radius; y++) {
            if (dither_percent == 25) {
                if ((x%4 == 0 && y%2 == 0) || ((x+2)%4 == 0 && (y+1)%2 == 0)) {
                    graphics_context_set_stroke_color(ctx, second);
                } else { 
                    graphics_context_set_stroke_color(ctx, first);
                }                
            } else if (dither_percent == 30) {
                if ( (x+y)%2 == 0 && ( (x+y)%4 != 0 || ( (x+y)%4 == 0 && x%2 != 0 && y%2 != 0) ) ) {
                    graphics_context_set_stroke_color(ctx, second);
                } else { 
                    graphics_context_set_stroke_color(ctx, first);
                }
            } else if (dither_percent == 50) {    
                if ((x+y)%2 == 0) {
                    graphics_context_set_stroke_color(ctx, second);
                } else { 
                    graphics_context_set_stroke_color(ctx, first);
                }                
            }
            
            if ((x-center.x) * (x-center.x) + (y-center.y) * (y-center.y) < radius * radius) {                 // point is within bounds of circle
                if ((x-center.x) * (x-center.x) + (y-center.y) * (y-center.y) > inset_radius * inset_radius) { // point is outside center inset
                    // Check current point's angle
                    opposite = x-center.x;
                    adjacent = center.y-y;
                    angle = atan2_lookup(opposite, adjacent);
                    

                   //TODO: negative angles, e.g. -45deg to +45deg  
                    if (angle >= angle_start && angle <= angle_end) { // point is within angle range
                        graphics_draw_pixel(ctx, GPoint(x,y));
                    }
                }
            }
        }
    }
}
