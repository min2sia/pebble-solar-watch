#pragma once

void draw_dotted_line_b(
    GContext* ctx, 
    GPoint from, 
    GPoint to, 
    GColor color_1, 
    GColor color_2, 
    int step);

void draw_dotted_line(GContext* ctx, GPoint from, GPoint to);

void draw_outlined_text(
    GContext* ctx, 
    char* text, 
    GFont font, 
    GRect rect, 
    GTextOverflowMode mode, 
    GTextAlignment alignment, 
    int outline_pixels, 
    bool inverted);

void draw_dot(GContext* ctx, GPoint center, int radius);

void draw_dithered_radial(
    uint16_t dither_percent,
    GContext *ctx, 
    GPoint center, 
    uint16_t radius, 
    uint16_t inset_radius, 
    int32_t angle_start, 
    int32_t angle_end, 
    GColor first, 
    GColor second);
