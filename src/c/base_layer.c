#include <pebble.h>
#include "base_layer.h"
#include "main.h"
#include "draw.h"
#include "util.h"

Layer *layer; 
GContext *ctx;
GRect bounds;
GPoint center;
uint16_t radius = 0;
uint16_t deg_step = 0;

static void draw_borders() {
    graphics_context_set_stroke_color(ctx, GColorBlack);
    graphics_draw_circle(ctx, center, radius-4);
    graphics_context_set_stroke_color(ctx, GColorWhite);
    graphics_draw_circle(ctx, center, radius-3);
    /*
        for (int i=radius-2;i<radius;i++) {
            graphics_draw_circle(ctx, center, i);
        }
        center.y+=1;
        for (int i=radius-2;i<radius;i++) {
            graphics_draw_circle(ctx, center, i);
        }
        center.y-=1;
        graphics_context_set_stroke_color(ctx, GColorBlack);
        for (int i=radius;i<120;i++) {
            graphics_draw_circle(ctx, center, i);
        }
        center.y+=1;
        for (int i=radius;i<120;i++) {
            graphics_draw_circle(ctx, center, i);
        }    
    */
}

static void draw_sunrays() {
    graphics_context_set_stroke_color(ctx, COLOR_FALLBACK(GColorYellow, GColorWhite));  
    
    const int16_t start_angle = TRIG_MAX_ANGLE / 4 * -1;
    const int16_t end_angle   = -start_angle;
    const uint16_t ray_length_min = 5;
    const uint16_t ray_length_max = 20;
    uint16_t ray_length;

    deg_step = TRIG_MAX_ANGLE / 120; // density

    for (int i=start_angle; i<end_angle; i+=deg_step) {
        //float current_rads = rad_factor * (deg_step * (i - 30));
        GPoint start_point = {
            .x = ( sin_lookup(i) * radius / TRIG_MAX_RATIO) + center.x,
            .y = (-cos_lookup(i) * radius / TRIG_MAX_RATIO) + center.y
        };

        //ray_length = ray_length_min + (rand() % (ray_length_max-ray_length_min));
        ray_length = ray_length_min + (sin_lookup(my_abs(i)) % (ray_length_max-ray_length_min));

        GPoint end_point = {
            .x = ( sin_lookup(i) * (radius + ray_length) / TRIG_MAX_RATIO) + center.x,
            .y = (-cos_lookup(i) * (radius + ray_length) / TRIG_MAX_RATIO) + center.y
        };

        graphics_draw_line(ctx, start_point, end_point);
    }
}

static void draw_light_periods() {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "base_layer.draw_light_periods()");
    if (solar_offset == 0) {
        return;
    }

    GRect bounds = layer_get_bounds(layer);
    const GPoint center = grect_center_point(&bounds);
    const uint16_t radius = center.x < center.y ? center.x : center.y;
    const uint16_t clock_face_radius = radius - 3;
    GRect clock_face_bounds = bounds;
    clock_face_bounds.origin.x += 3;
    clock_face_bounds.size.w -= 6;
    
    
    if (polar_day_night == 0) { // Regular day
        graphics_context_set_fill_color(ctx, daytime_color);
        graphics_fill_circle(ctx, center, clock_face_radius);
    } else if (polar_day_night == 1) { // Polar day - nothing else to draw
        graphics_context_set_fill_color(ctx, daytime_color);
        graphics_fill_circle(ctx, center, clock_face_radius);
        return;
    } else if (polar_day_night == 2) { // Polar night
    }
    
    uint32_t seconds_per_day = HOURS_PER_DAY * SECONDS_PER_HOUR;
    int32_t sunrise_angle              = TRIG_MAX_ANGLE * (float)sunrise_time_solar              / seconds_per_day;
    int32_t sunset_angle               = TRIG_MAX_ANGLE * (float)sunset_time_solar               / seconds_per_day;
    int32_t civil_dawn_angle           = TRIG_MAX_ANGLE * (float)civil_dawn_time_solar           / seconds_per_day;
    int32_t nautical_dawn_angle        = TRIG_MAX_ANGLE * (float)nautical_dawn_time_solar        / seconds_per_day;
    int32_t astronomical_dawn_angle    = TRIG_MAX_ANGLE * (float)astronomical_dawn_time_solar    / seconds_per_day;
    int32_t civil_dusk_angle           = TRIG_MAX_ANGLE * (float)civil_dusk_time_solar           / seconds_per_day;
    int32_t nautical_dusk_angle        = TRIG_MAX_ANGLE * (float)nautical_dusk_time_solar        / seconds_per_day;
    int32_t astronomical_dusk_angle    = TRIG_MAX_ANGLE * (float)astronomical_dusk_time_solar    / seconds_per_day;
//     int32_t golden_hour_morning_angle  = TRIG_MAX_ANGLE * (float)golden_hour_morning_time_solar  / seconds_per_day;
//     int32_t golden_hour_evening_angle  = TRIG_MAX_ANGLE * (float)golden_hour_evening_time_solar  / seconds_per_day;


    // Screen's 0 degrees is at the top, while watch dial mark for 0 hours is at the bottom, we neeed to rotate all angles by -180 deg
    // E.g. 80 -> -100; 280 -> 100; etc.
    sunrise_angle += TRIG_MAX_ANGLE/2;
    sunset_angle  -= TRIG_MAX_ANGLE/2;

    // Golden hour
//     golden_hour_morning_angle += TRIG_MAX_ANGLE/2;
//     golden_hour_evening_angle -= TRIG_MAX_ANGLE/2;
//     if (golden_hour_morning_angle > golden_hour_evening_angle) {
//         #if defined(PBL_COLOR)
//         graphics_context_set_fill_color(ctx, golden_hour_color);
//         graphics_fill_radial(
//             ctx, 
//             grect_inset(clock_face_bounds, GEdgeInsets(0)), 
//             GOvalScaleModeFitCircle, 
//             clock_face_radius, 
//             golden_hour_evening_angle, 
//             golden_hour_morning_angle); 
//         #elif defined(PBL_BW)
        //             draw_dithered_radial(
        //                 25,
        //                 ctx, 
        //                 center, 
        //                 clock_face_radius, 
        //                 0, 
        //                 golden_hour_evening_angle, 
        //                 golden_hour_morning_angle,
        //                 daytime_color, 
        //                 nighttime_color);
//         #endif            
//     }

// APP_LOG(APP_LOG_LEVEL_DEBUG, "  sunrise_angle:           %d, sunset_angle:            %d", (int)sunrise_angle, (int)sunset_angle);    
    // Civil twilight - first draw on whole duration of the night
    if (polar_day_night == 0 &&
        sunrise_time_solar > 0.0  && 
        sunset_time_solar  > 0.0  && 
        sunrise_angle > 0  &&
        sunset_angle  > 0  &&
        sunrise_angle > sunset_angle) {
        
//         APP_LOG(APP_LOG_LEVEL_DEBUG, "  drawing Civil twilight");
        #if defined(PBL_COLOR)
        graphics_context_set_fill_color(ctx, civil_twilight_color);
        graphics_fill_radial(
            ctx, 
            grect_inset(clock_face_bounds, GEdgeInsets(0)), 
            GOvalScaleModeFitCircle, 
            clock_face_radius, 
            sunset_angle, 
            sunrise_angle); 
        #elif defined(PBL_BW)
        draw_dithered_radial(
            25,
            ctx, 
            center, 
            clock_face_radius, 
            0, 
            sunset_angle, 
            sunrise_angle,
            daytime_color, 
            nighttime_color);
        #endif
    } else {
//         graphics_context_set_fill_color(ctx, civil_twilight_color);
//         graphics_fill_circle(ctx, center, clock_face_radius);
    }
    
    
//     sunrise_angle += (int)((float)TRIG_MAX_ANGLE/2);
//     sunset_angle  -= (int)((float)TRIG_MAX_ANGLE/2);
    civil_dawn_angle        += TRIG_MAX_ANGLE/2;
    nautical_dawn_angle     += TRIG_MAX_ANGLE/2;
    astronomical_dawn_angle += TRIG_MAX_ANGLE/2;

    civil_dusk_angle        -= TRIG_MAX_ANGLE/2;
    nautical_dusk_angle     -= TRIG_MAX_ANGLE/2;
    astronomical_dusk_angle -= TRIG_MAX_ANGLE/2;        

APP_LOG(APP_LOG_LEVEL_DEBUG, "  civil_dawn_angle:        %d, civil_dusk_angle:        %d", (int)civil_dawn_angle, (int)civil_dusk_angle);
    // Nautical twilight
    if (civil_dawn_time_solar > 0.0 && 
        civil_dusk_time_solar > 0.0 && 
        civil_dawn_angle > 0  &&
        civil_dusk_angle > 0  &&
        civil_dawn_angle > civil_dusk_angle) {
        
//         APP_LOG(APP_LOG_LEVEL_DEBUG, "  drawing Nautical twilight");
        #if defined(PBL_COLOR)
        graphics_context_set_fill_color(ctx, nautical_twilight_color);
        graphics_fill_radial(
            ctx, 
            grect_inset(clock_face_bounds, GEdgeInsets(0)), 
            GOvalScaleModeFitCircle, 
            clock_face_radius, 
            civil_dusk_angle, 
            civil_dawn_angle); 
        #elif defined(PBL_BW)
        draw_dithered_radial(
            50,
            ctx, 
            center, 
            clock_face_radius, 
            0, 
            civil_dusk_angle, 
            civil_dawn_angle,
            daytime_color, 
            nighttime_color);
        #endif
    } else {
//         graphics_context_set_fill_color(ctx, nautical_twilight_color);
//         graphics_fill_circle(ctx, center, clock_face_radius);
    }

APP_LOG(APP_LOG_LEVEL_DEBUG, "  nautical_dawn_angle:     %d, nautical_dusk_angle:     %d", (int)nautical_dawn_angle, (int)nautical_dusk_angle);
    // Astronomical twilight
    if (nautical_dawn_time_solar > 0.0 && 
        nautical_dusk_time_solar > 0.0 && 
        nautical_dawn_angle > 0  &&
        nautical_dusk_angle > 0  &&
        nautical_dawn_angle > nautical_dusk_angle) {

//         APP_LOG(APP_LOG_LEVEL_DEBUG, "  drawing Astronomical twilight");
        
        #if defined(PBL_COLOR)
        graphics_context_set_fill_color(ctx, astronomical_twilight_color);
        graphics_fill_radial(
            ctx, 
            grect_inset(clock_face_bounds, GEdgeInsets(0)), 
            GOvalScaleModeFitCircle, 
            clock_face_radius, 
            nautical_dusk_angle, 
            nautical_dawn_angle); 
        #elif defined(PBL_BW)
        draw_dithered_radial(
            25,
            ctx, 
            center, 
            clock_face_radius, 
            0, 
            nautical_dusk_angle, 
            nautical_dawn_angle,
            nighttime_color, 
            daytime_color);
        #endif
    } else {
//         graphics_context_set_fill_color(ctx, astronomical_twilight_color);
//         graphics_fill_circle(ctx, center, clock_face_radius);
    }

APP_LOG(APP_LOG_LEVEL_DEBUG, "  astronomical_dawn_angle: %d, astronomical_dusk_angle: %d", (int)astronomical_dawn_angle, (int)astronomical_dusk_angle);
    // Darkest part of night - may not occur at all on short nights
    if (astronomical_dawn_time_solar > 0.0 && 
        astronomical_dusk_time_solar > 0.0 && 
        astronomical_dawn_angle > 0 &&
        astronomical_dusk_angle > 0 &&
        astronomical_dawn_angle > astronomical_dusk_angle) {
        APP_LOG(APP_LOG_LEVEL_DEBUG, "  drawing night");
        graphics_context_set_fill_color(ctx, nighttime_color);
        graphics_fill_radial(
            ctx, 
            grect_inset(clock_face_bounds, GEdgeInsets(0)), 
            GOvalScaleModeFitCircle, 
            clock_face_radius, 
            astronomical_dusk_angle, 
            astronomical_dawn_angle);         
    } else {
//        APP_LOG(APP_LOG_LEVEL_DEBUG, "  skipping night");
    }

    // Draw solid lines separating day and night times
    //         GPoint sunrise_point = {
    //             .x = ( sin_lookup(sunrise_angle) * clock_face_radius / TRIG_MAX_RATIO) + center.x,
    //             .y = (-cos_lookup(sunrise_angle) * clock_face_radius / TRIG_MAX_RATIO) + center.y
    //         };
    //         GPoint sunset_point = {
    //             .x = ( sin_lookup(sunset_angle) * clock_face_radius / TRIG_MAX_RATIO) + center.x,
    //             .y = (-cos_lookup(sunset_angle) * clock_face_radius / TRIG_MAX_RATIO) + center.y
    //         };
    //         graphics_context_set_stroke_color(ctx, GColorBlack);  
    //         graphics_draw_line(ctx, center, sunrise_point);
    //         graphics_draw_line(ctx, center, sunset_point);    
}

static void draw_dosa_periods() {
    //
    //  Draw dotted lines splitting night and day periods into 3 parts (vata, pitta, kapha)
    //

    /*
        double dosa_len = 0; 
        GPoint dosa_point;
        double dosa_time = 0;

        dosa_len = (sunset_time - sunrise_time) / 3; // day        
        // 1. Day Kapha/Pitta line
        dosa_time = sunrise_time+dosa_len;
        dosa_point.x = center.x - (int16_t)(my_sin(dosa_time/24 * M_PI * 2) * 120);
        dosa_point.y = center.y + (int16_t)(my_cos(dosa_time/24 * M_PI * 2) * 120);
        //APP_LOG(APP_LOG_LEVEL_DEBUG, "dosa2 = %d, %d", dosa_point.x, dosa_point.y);
        draw_dotted_line_b(ctx, center, dosa_point, GColorWhite, GColorBlack, 2); 

        // 2. Day Pitta/Vata line
        dosa_time = sunrise_time + (dosa_len * 2);
        dosa_point.x = center.x - (int16_t)(my_sin(dosa_time/24 * M_PI * 2) * 120);
        dosa_point.y = center.y + (int16_t)(my_cos(dosa_time/24 * M_PI * 2) * 120);
        draw_dotted_line(ctx, center, dosa_point); 

        dosa_len = (24 - sunset_time + sunrise_time) / 3; // night
        // 3. Night Kapha/Pitta line
        dosa_time = sunset_time + dosa_len;
        dosa_point.x = center.x - (int16_t)(my_sin(dosa_time/24 * M_PI * 2) * 120);
        dosa_point.y = center.y + (int16_t)(my_cos(dosa_time/24 * M_PI * 2) * 120);
        draw_dotted_line_b(ctx, center, dosa_point, GColorWhite, GColorBlack, 2); 

        // 4. Night Pitta/Vata line
        dosa_time = sunrise_time - dosa_len;
        dosa_point.x = center.x - (int16_t)(my_sin(dosa_time/24 * M_PI * 2) * 120);
        dosa_point.y = center.y + (int16_t)(my_cos(dosa_time/24 * M_PI * 2) * 120);
        draw_dotted_line(ctx, center, dosa_point); 
        */

    // TESTING: draw twilight gradients
    //https://github.com/MathewReiss/dithering ?
    /*
        dosa_len = (24 - sunset_time + sunrise_time) / 3; // night
        // 3. Night Kapha/Pitta line
        dosa_time = sunset_time + 10;
        dosa_point.x = center.x - (int16_t)(my_sin(dosa_time/24 * M_PI * 2) * 120);
        dosa_point.y = center.y + (int16_t)(my_cos(dosa_time/24 * M_PI * 2) * 120);
        draw_dotted_line_b(ctx, center, dosa_point, GColorWhite, GColorBlack, 5); 

        dosa_time = sunset_time + 11;
        dosa_point.x = center.x - (int16_t)(my_sin(dosa_time/24 * M_PI * 2) * 120);
        dosa_point.y = center.y + (int16_t)(my_cos(dosa_time/24 * M_PI * 2) * 120);
        draw_dotted_line_b(ctx, center, dosa_point, GColorWhite, GColorBlack, 6);

        dosa_time = sunset_time + 12;
        dosa_point.x = center.x - (int16_t)(my_sin(dosa_time/24 * M_PI * 2) * 120);
        dosa_point.y = center.y + (int16_t)(my_cos(dosa_time/24 * M_PI * 2) * 120);
        draw_dotted_line_b(ctx, center, dosa_point, GColorWhite, GColorBlack, 7); 
        */
}

static void draw_hour_marks() {
    // draw semi major hour marks
    deg_step = TRIG_MAX_ANGLE / 8; // 45deg
    for (uint16_t i=0; i<8; i++) {
        draw_dot(
            ctx, 
            GPoint(
                ( sin_lookup(deg_step * i) * (radius-10) / TRIG_MAX_RATIO) + center.x,
                (-cos_lookup(deg_step * i) * (radius-10) / TRIG_MAX_RATIO) + center.y), 
            3);
    }
    // draw each hour mark
    deg_step = TRIG_MAX_ANGLE / 24; // 15deg;
    for (uint16_t i=0;i<24;i++) {
        draw_dot(
            ctx, 
            GPoint(
                ( sin_lookup(deg_step * i) * (radius-10) / TRIG_MAX_RATIO) + center.x,
                (-cos_lookup(deg_step * i) * (radius-10) / TRIG_MAX_RATIO) + center.y),
            1);
    }
    // draw major hour marks
    deg_step = TRIG_MAX_ANGLE / 4; // 90deg
    for (uint16_t i=0;i<4;i++) {
        draw_dot(
            ctx, 
            GPoint(
                ( sin_lookup(deg_step * i) * (radius-10) / TRIG_MAX_RATIO) + center.x,
                (-cos_lookup(deg_step * i) * (radius-10) / TRIG_MAX_RATIO) + center.y), 
            4);
    }
    
    // draw hour text
    char hour_text[] = "00";

    deg_step = TRIG_MAX_ANGLE / 8; // 45deg
    for (uint16_t i=0; i<8; i++) {
        GPoint current_point = {
            .x = ( sin_lookup(deg_step * i) * (radius-24) / TRIG_MAX_RATIO) + center.x,
            .y = (-cos_lookup(deg_step * i) * (radius-24) / TRIG_MAX_RATIO) + center.y
        };

        switch(i) {
            case 0 : strcpy(hour_text, "12"); break;
            case 1 : strcpy(hour_text, "");   break;
            case 2 : strcpy(hour_text, "18"); break;
            case 3 : strcpy(hour_text, "");   break;
            case 4 : strcpy(hour_text, "0");  break;
            case 5 : strcpy(hour_text, "");   break;
            case 6 : strcpy(hour_text, "6");  break;
            case 7 : strcpy(hour_text, "");   break;
        }

        draw_outlined_text(ctx,
                           hour_text,
                           fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                           GRect(current_point.x-10, current_point.y-12, 20, 20),
                           GTextOverflowModeWordWrap,
                           GTextAlignmentCenter,
                           1,
                           false);
    }    
}

static void draw_sunrise_sunset_times() {
    static char sunrise_text[] = "  ...   ";
    static char sunset_text[]  = "  ...   ";
    
    if (sunrise_time_solar && sunset_time_solar) {
        strftime(sunrise_text, sizeof(sunrise_text), time_format, &sunrise_time_tm);
        strftime(sunset_text,  sizeof(sunset_text),  time_format, &sunset_time_tm);
    }
    
    graphics_context_set_text_color(ctx, GColorWhite);

    graphics_draw_text(
        ctx,
        sunrise_text,
        fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
        GRect(3, 145, 144-3, 168-145),
        GTextOverflowModeWordWrap,
        GTextAlignmentLeft,
        NULL);
    
    graphics_draw_text(
        ctx,
        sunset_text,
        fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
        GRect(3, 145, 144-3, 168-145),
        GTextOverflowModeWordWrap,
        GTextAlignmentRight,
        NULL);
}

static void draw_date() {
    //draw current month + day    
    draw_outlined_text(ctx,
        date_text,
        fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
        GRect(center.x, 0, center.x, 10),
        GTextOverflowModeWordWrap,
        GTextAlignmentRight,
        0,
        true);    
}

void base_layer_update_proc(Layer *parm_layer, GContext *parm_ctx) {
APP_LOG(APP_LOG_LEVEL_DEBUG, "base_layer.base_layer_update_proc() !!!!!!!!!!!!!!!!!!!");
    layer  = parm_layer;
    ctx    = parm_ctx;
    bounds = layer_get_bounds(layer);
    center = grect_center_point(&bounds);  
    radius = center.x < center.y ? center.x : center.y;
    
    #if defined(PBL_RECT)
        draw_sunrays();
    #endif
    draw_light_periods();
    draw_borders();
    draw_hour_marks();
    #if defined(PBL_RECT)
        draw_sunrise_sunset_times();
        draw_date();
    #endif
}