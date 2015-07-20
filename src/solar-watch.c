#include <pebble.h>
#include "my_math.h"

static Window *window;
static Layer *face_layer;
static Layer *hand_layer;
static Layer *sunlight_layer;
static Layer *sunrise_sunset_text_layer;
static Layer *battery_layer;

time_t clock_time_t = 0;
time_t solar_time_t = 0;
float sunrise_time;
float sunset_time;
struct tm clock_time_tm   = { .tm_hour = 0, .tm_min = 0 };
struct tm solar_time_tm   = { .tm_hour = 0, .tm_min = 0 };
struct tm sunrise_time_tm = { .tm_hour = 0, .tm_min = 0 };
struct tm sunset_time_tm  = { .tm_hour = 0, .tm_min = 0 };
int32_t solar_offset;
int current_sunrise_sunset_day = -1;

int current_battery_charge = -1;
char *battery_level_string = "100%";

bool setting_digital_display = true;
bool setting_hour_numbers = true;
bool setting_battery_status = true;

// Since compilation fails using the standard `atof',
// the following `myatof' implementation taken from:
// 09-May-2009 Tom Van Baak (tvb) www.LeapSecond.com
// 
#define white_space(c) ((c) == ' ' || (c) == '\t')
#define valid_digit(c) ((c) >= '0' && (c) <= '9')
double myatof (const char *p)
{
    int frac;
    double sign, value, scale;
    // Skip leading white space, if any.
    while (white_space(*p) ) {
        p += 1;
    }
    // Get sign, if any.
    sign = 1.0;
    if (*p == '-') {
        sign = -1.0;
        p += 1;
        } else if (*p == '+') {
        p += 1;
    }
    // Get digits before decimal point or exponent, if any.
    for (value = 0.0; valid_digit(*p); p += 1) {
        value = value * 10.0 + (*p - '0');
    }
    // Get digits after decimal point, if any.
    if (*p == '.') {
        double pow10 = 10.0;
        p += 1;
        while (valid_digit(*p)) {
            value += (*p - '0') / pow10;
            pow10 *= 10.0;
            p += 1;
        }
    }
    // Handle exponent, if any.
    frac = 0;
    scale = 1.0;
    if ((*p == 'e') || (*p == 'E')) {
        unsigned int expon;
        // Get sign of exponent, if any.
        p += 1;
        if (*p == '-') {
            frac = 1;
            p += 1;
            
            } else if (*p == '+') {
            p += 1;
        }
        // Get digits of exponent, if any.
        for (expon = 0; valid_digit(*p); p += 1) {
            expon = expon * 10 + (*p - '0');
        }
        if (expon > 308) expon = 308;
        // Calculate scaling factor.
        while (expon >= 50) { scale *= 1E50; expon -= 50; }
        while (expon >=  8) { scale *= 1E8;  expon -=  8; }
        while (expon >   0) { scale *= 10.0; expon -=  1; }
    }
    // Return signed and scaled floating point result.
    return sign * (frac ? (value / scale) : (value * scale));
}

//
// snprintf does not support double: http://forums.getpebble.com/discussion/8743/petition-please-support-float-double-for-snprintf
// workaround from MatthewClark
//
void ftoa(char* str, double val, int precision) {
    //  start with positive/negative
    if (val < 0) {
        *(str++) = '-';
        val = -val;
    }
    //  integer value
    snprintf(str, 12, "%d", (int) val);
    str += strlen(str);
    val -= (int) val;
    //  decimals
    if ((precision > 0) && (val >= .00001)) {
        //  add period
        *(str++) = '.';
        //  loop through precision
        for (int i = 0;  i < precision;  i++)
        if (val > 0) {
            val *= 10;
            *(str++) = '0' + (int) (val + ((i == precision - 1) ? .5 : 0));
            val -= (int) val;
        } else
        break;
    }
    //  terminate
    *str = '\0';
}

static void send_request(int command) {
  Tuplet command_tuple = TupletInteger(0 /*KEY_COMMAND*/ , command);
  Tuplet index_tuple = TupletInteger(1 /*KEY_INDEX*/, 0 /*entryIndex*/);
  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);
  if (iter == NULL)
    return;
  dict_write_tuplet(iter, &command_tuple);
  dict_write_tuplet(iter, &index_tuple);
  dict_write_end(iter);
  app_message_outbox_send();
}

static void update_time() {
    
    APP_LOG(APP_LOG_LEVEL_DEBUG, "update_time()");
    
    clock_time_t = time(NULL);
    clock_time_tm = *localtime(&clock_time_t);
    
    solar_time_t  = clock_time_t + solar_offset;
    solar_time_tm = *localtime(&solar_time_t);
        
    if (solar_time_tm.tm_min >= 60) {
        solar_time_tm.tm_min -= 60;
        solar_time_tm.tm_hour += 1;
    }  
    if (solar_time_tm.tm_hour == 24) {
        solar_time_tm.tm_hour = 0;
    }
    if (solar_time_tm.tm_hour > 24) {
        solar_time_tm.tm_hour = solar_time_tm.tm_hour - 24;
    }
    if (solar_time_tm.tm_hour < 0) {
        solar_time_tm.tm_hour = solar_time_tm.tm_hour + 24;
    }
    
    sunrise_time = sunrise_time_tm.tm_hour + (sunrise_time_tm.tm_min / 60.0);
    sunset_time  = sunset_time_tm.tm_hour  + (sunset_time_tm.tm_min  / 60.0);   
    

    
    // and apply solar offset, so it displays symmetric solar night
    sunrise_time += solar_offset / 3600.0;
    sunset_time  += solar_offset / 3600.0;
    
    //char txt[5];      
    //ftoa(txt, sunrise_time, 2);    
    //APP_LOG(APP_LOG_LEVEL_DEBUG, "    sunrise time:   %s", txt);
    //ftoa(txt, sunset_time, 2);    
    //APP_LOG(APP_LOG_LEVEL_DEBUG, "    sunset time:    %s", txt);
      
    if ((current_sunrise_sunset_day != clock_time_tm.tm_mday)) {   
          
        current_sunrise_sunset_day = clock_time_tm.tm_mday;
           
        send_request(0); // request data refresh from phone JS         
    }

    layer_mark_dirty(face_layer);     
    layer_mark_dirty(hand_layer);
    layer_mark_dirty(sunlight_layer);
    layer_mark_dirty(sunrise_sunset_text_layer);
    
    APP_LOG(APP_LOG_LEVEL_DEBUG, "    clock time:   %d:%d", clock_time_tm.tm_hour,   clock_time_tm.tm_min);
    APP_LOG(APP_LOG_LEVEL_DEBUG, "    solar time:   %d:%d", solar_time_tm.tm_hour,   solar_time_tm.tm_min); 

    
    //char sunrise_str_tmp[15];
    //char sunset_str_tmp[15];
    //ftoa(sunrise_str_tmp, sunrise_time, 7);
    //ftoa(sunset_str_tmp, sunset_time, 7);
    //APP_LOG(APP_LOG_LEVEL_DEBUG, "    sunrise time: %d:%d", sunrise_time_tm.tm_hour, sunrise_time_tm.tm_min);
    //APP_LOG(APP_LOG_LEVEL_DEBUG, "    sunset time:  %d:%d", sunset_time_tm.tm_hour,  sunset_time_tm.tm_min);
}

static void handle_time_tick(struct tm *tick_time, TimeUnits units_changed) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "handle_time_tick()");
    
    update_time();
}

/******************
    APPMESSAGE STUFF
*******************/
enum {
    SOLAR_OFFSET = 12,
    SUNRISE_HOURS = 13,
    SUNRISE_MINUTES = 14,
    SUNSET_HOURS = 15,
    SUNSET_MINUTES = 16
};

void in_received_handler(DictionaryIterator *received, void *ctx) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "in_received_handler()");

    Tuple *solar_offset_tuple    = dict_find(received, SOLAR_OFFSET);
    Tuple *sunrise_hours_tuple   = dict_find(received, SUNRISE_HOURS);
    Tuple *sunrise_minutes_tuple = dict_find(received, SUNRISE_MINUTES);
    Tuple *sunset_hours_tuple    = dict_find(received, SUNSET_HOURS);
    Tuple *sunset_minutes_tuple  = dict_find(received, SUNSET_MINUTES);
   
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Received from phone:");

    if (solar_offset_tuple) {         
        solar_offset = solar_offset_tuple->value->int32;
        APP_LOG(APP_LOG_LEVEL_DEBUG, "  solar offset: %d", (int)solar_offset);
    } 
    
    if (sunrise_hours_tuple && sunrise_minutes_tuple) {        
        sunrise_time_tm.tm_hour = sunrise_hours_tuple->value->int32;
        sunrise_time_tm.tm_min  = sunrise_minutes_tuple->value->int32;
        APP_LOG(APP_LOG_LEVEL_DEBUG, "  sunrise: %d:%d", (int)sunrise_time_tm.tm_hour, (int)sunrise_time_tm.tm_min);
    }
    if (sunset_hours_tuple && sunset_minutes_tuple) {        
        sunset_time_tm.tm_hour = sunset_hours_tuple->value->int32;
        sunset_time_tm.tm_min  = sunset_minutes_tuple->value->int32;
        APP_LOG(APP_LOG_LEVEL_DEBUG, "  sunset:  %d:%d", (int)sunset_time_tm.tm_hour, (int)sunset_time_tm.tm_min);
    }
    
    update_time();
}

void in_dropped_handler(AppMessageResult reason, void *context) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Watch dropped data.");
}

void out_sent_handler(DictionaryIterator *sent, void *ctx) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Message sent to phone.");
}

void out_failed_handler(DictionaryIterator *failed, AppMessageResult reason, void *ctx) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Message FAILED to send to phone.");
}

// results are pretty awful for `outline_pixel' values larger than 2...
static void draw_outlined_text(GContext* ctx, char* text, GFont font, GRect rect, GTextOverflowMode mode, GTextAlignment alignment, int outline_pixels, bool inverted) {
    (inverted) ? graphics_context_set_text_color(ctx, GColorBlack) :
    graphics_context_set_text_color(ctx, GColorWhite);
    
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

static void draw_dot(GContext* ctx, GPoint center, int radius) {
    graphics_context_set_stroke_color(ctx, GColorWhite);
    graphics_context_set_fill_color(ctx,GColorBlack);
    graphics_fill_circle(ctx, center, radius);
    graphics_draw_circle(ctx, center, radius);
    graphics_context_set_stroke_color(ctx, GColorBlack);
    graphics_draw_circle(ctx, center, radius+1);
}

static void update_battery_percentage(BatteryChargeState c) {
    if (setting_battery_status) {
        int battery_level_int = c.charge_percent;
        snprintf(battery_level_string, 4, "%d%%", battery_level_int);
        layer_mark_dirty(battery_layer);
        APP_LOG(APP_LOG_LEVEL_DEBUG, "Battery change: battery_layer marked dirty...");
    }
}

static void face_layer_update_proc(Layer *layer, GContext *ctx) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "face_layer_update_proc()");
    
    update_time();
    
    GRect bounds = layer_get_bounds(layer);
    GPoint center = grect_center_point(&bounds);  
    
    /*******************************
        DRAW PRIMITIVES FOR THIS LAYER
    *********************************/
    graphics_context_set_stroke_color(ctx, GColorBlack);
    graphics_draw_circle(ctx, center, 68);
    graphics_context_set_stroke_color(ctx, GColorWhite);
    graphics_draw_circle(ctx, center, 69);
    // yes, I'm really doing this...
    for (int i=70;i<72;i++) {
        graphics_draw_circle(ctx, center, i);
    }
    center.y+=1;
    for (int i=70;i<72;i++) {
        graphics_draw_circle(ctx, center, i);
    }
    center.y-=1;
    graphics_context_set_stroke_color(ctx, GColorBlack);
    for (int i=72;i<120;i++) {
        graphics_draw_circle(ctx, center, i);
    }
    center.y+=1;
    for (int i=72;i<120;i++) {
        graphics_draw_circle(ctx, center, i);
    }
    
    float rad_factor;
    float deg_step;
    
    // draw sunrays
    GPoint start_point;
    GPoint end_point;
    float current_rads;
    float x;
    float y;
    int ray_length;
    graphics_context_set_stroke_color(ctx, GColorWhite);  
    rad_factor = M_PI / 180;
    deg_step = 3;
  
    for (int i=-25;i<26;i++) {
        current_rads = rad_factor * (deg_step * (i - 30));
        
        x = 73 * (my_cos(current_rads)) + 72;
        y = 73 * (my_sin(current_rads)) + 84;
        start_point.x = (int16_t)x;
        start_point.y = (int16_t)y;
        
        ray_length = rand() % (int)(26 - my_abs(i)) / 2;
        x = (80 + ray_length) * (my_cos(current_rads)) + 72;
        y = (80 + ray_length) * (my_sin(current_rads)) + 84;
        end_point.x = (int16_t)x;
        end_point.y = (int16_t)y;
        
        graphics_draw_line(ctx, start_point, end_point);
    }
    
    rad_factor = M_PI / 180;
    
    // draw semi major hour marks
    deg_step = 45;
    for (int i=0;i<8;i++) {
        float current_rads = rad_factor * (deg_step * i);
        float x = 72 + 62 * (my_cos(current_rads));
        float y = 84 + 62 * (my_sin(current_rads));
        GPoint current_point;
        current_point.x = (int16_t)x;
        current_point.y = (int16_t)y;
        draw_dot(ctx, current_point, 3);
    }
    // draw each hour mark
    deg_step = 15;
    for (int i=0;i<24;i++) {
        float current_rads = rad_factor * (deg_step * i);
        float x = 72 + 62 * (my_cos(current_rads));
        float y = 84 + 62 * (my_sin(current_rads));
        GPoint current_point;
        current_point.x = (int16_t)x;
        current_point.y = (int16_t)y;
        draw_dot(ctx, current_point, 1);
    }
    // draw major hour marks
    deg_step = 90;
    for (int i=0;i<4;i++) {
        float current_rads = rad_factor * (deg_step * i);
        float x = 72 + 62 * (my_cos(current_rads));
        float y = 84 + 62 * (my_sin(current_rads));
        GPoint current_point;
        current_point.x = (int16_t)x;
        current_point.y = (int16_t)y;
        draw_dot(ctx, current_point, 4);
    }
    
    /*************************
        DRAW TEXT FOR THIS LAYER
    **************************/
    if (setting_hour_numbers) {
        // draw hour text
        char hour_text[] = "00";
        
        deg_step = 45;
        for (int i=0;i<8;i++) {
            float current_rads = rad_factor * (deg_step * i);
            float x = 72 + 48 * (my_cos(current_rads));
            float y = 84 + 48 * (my_sin(current_rads));
            GPoint current_point;
            current_point.x = (int16_t)x;
            current_point.y = (int16_t)y;
            
            switch(i) {
                case 0 : strcpy(hour_text, "18"); break;
                case 1 : strcpy(hour_text, "" /*21"*/); break;
                case 2 : strcpy(hour_text, "0");  break;
                case 3 : strcpy(hour_text, "" /*"3"*/);  break;
                case 4 : strcpy(hour_text, "6");  break;
                case 5 : strcpy(hour_text, "" /*"9"*/);  break;
                case 6 : strcpy(hour_text, "12"); break;
                case 7 : strcpy(hour_text, "" /*"15"*/); break;
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
}

static const GPathInfo p_hour_hand_info = {
    .num_points = 5,
    .points = (GPoint []) {{3,12},{-3,12},{-2,-55},{0,-56},{2,-55}}
};

static void hand_layer_update_proc(Layer* layer, GContext* ctx) {
    GRect bounds = layer_get_bounds(layer);
    GPoint center = grect_center_point(&bounds);
    
    static GPath *p_hour_hand = NULL;
    
    // 24 hour hand
    int32_t hour_angle = (((solar_time_tm.tm_hour * 60) + solar_time_tm.tm_min) / 4) + 180;
    
    // draw the hour hand
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

GPathInfo sun_path_info = {
    5,
    (GPoint []) {
        {0, 0},
        {-73, +84}, //replaced by sunrise angle
        {-73, +84}, //bottom left
        {+73, +84}, //bottom right
        {+73, +84}, //replaced by sunset angle
    }
};

static void battery_layer_update_proc(Layer* layer, GContext* ctx) {
    if (setting_battery_status) {
        draw_outlined_text(ctx,
        battery_level_string,
        fonts_get_system_font(FONT_KEY_GOTHIC_14),
        GRect(55, 153, 40, 40),
        GTextOverflowModeWordWrap,
        GTextAlignmentCenter,
        1,
        true);
    }
}

static void sunlight_layer_update_proc(Layer* layer, GContext* ctx) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "sunlight_layer_update_proc()");
    
    if (sunrise_time && sunset_time) {   
        GRect bounds = layer_get_bounds(layer);
        GPoint center = grect_center_point(&bounds);
    
        //TODO: works somewhat ok, but could be more accurate
        //APP_LOG(APP_LOG_LEVEL_DEBUG, "sun_path_info.points[1].x = %d", sun_path_info.points[1].x);
        sun_path_info.points[1].x = -(int16_t)(my_sin(sunrise_time/24 * M_PI * 2) * 120);
        //APP_LOG(APP_LOG_LEVEL_DEBUG, "sun_path_info.points[1].x = %d", sun_path_info.points[1].x);
    
        sun_path_info.points[1].y = (int16_t)(my_cos(sunrise_time/24 * M_PI * 2) * 120);
        //APP_LOG(APP_LOG_LEVEL_DEBUG, "sun_path_info.points[1].y = %d", sun_path_info.points[1].y);

        sun_path_info.points[4].x = -(int16_t)(my_sin(sunset_time/24 * M_PI * 2) * 120);
        sun_path_info.points[4].y =  (int16_t)(my_cos(sunset_time/24 * M_PI * 2) * 120);

        struct GPath *sun_path;
        sun_path = gpath_create(&sun_path_info);
        graphics_context_set_stroke_color(ctx, GColorBlack);
        graphics_context_set_fill_color(ctx, GColorBlack);
        gpath_move_to(sun_path, center);
        gpath_draw_outline(ctx, sun_path);
        gpath_draw_filled(ctx, sun_path);

        gpath_destroy(sun_path);
    }
}

static void sunrise_sunset_text_layer_update_proc(Layer* layer, GContext* ctx) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "sunrise_sunset_text_layer_update_proc()"); 
        
    GRect bounds = layer_get_bounds(layer);
    GPoint center = grect_center_point(&bounds);     
    
    static char sunrise_text[] = "00:00";
    static char sunset_text[] = "00:00";
    static char clock_time_text[] = "00:00";
    static char solar_time_text[] = "00:00";
    static char month_text[] = "Jan";
    static char day_text[] = "00";
    char *time_format;
    
    if (clock_is_24h_style()) {
        time_format = "%H:%M";
    } else {
        time_format = "%l:%M";
    }
    char *month_format = "%b";
    char *day_format = "%e";
    char *ellipsis = ".....";
    
    // draw current time
    if (setting_digital_display) {   
        strftime(solar_time_text, sizeof(solar_time_text), time_format, &solar_time_tm);
        draw_outlined_text(ctx,
            solar_time_text,
            fonts_get_system_font(FONT_KEY_BITHAM_30_BLACK),//FONT_KEY_BITHAM_34_MEDIUM_NUMBERS),
            GRect(center.x - 50, 40, 100, 50), //GRect(42, 47, 64, 32),
            GTextOverflowModeFill,
            GTextAlignmentCenter,
            1,
            false);
        
        strftime(clock_time_text, sizeof(clock_time_text), time_format, &clock_time_tm);
        draw_outlined_text(ctx,
            clock_time_text,
            fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD),
            GRect(42, 86, 64, 32),
            GTextOverflowModeFill,
            GTextAlignmentCenter,
            1,
            true);
    }
    
    strftime(sunrise_text, sizeof(sunrise_text), time_format, &sunrise_time_tm);
    strftime(sunset_text,  sizeof(sunset_text),  time_format, &sunset_time_tm);
    graphics_context_set_text_color(ctx, GColorWhite);
    
    if (sunrise_time && sunset_time) {
        graphics_draw_text(ctx,
            sunrise_text,
            fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
            GRect(3, 145, 144-3, 168-145),
            GTextOverflowModeWordWrap,
            GTextAlignmentLeft,
            NULL);
        graphics_draw_text(ctx,
            sunset_text,
            fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
            GRect(3, 145, 144-3, 168-145),
            GTextOverflowModeWordWrap,
            GTextAlignmentRight,
            NULL);
    } else {
        graphics_draw_text(ctx,
            ellipsis,
            fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
            GRect(3, 145, 144-3, 168-145),
            GTextOverflowModeWordWrap,
            GTextAlignmentLeft,
            NULL);
        graphics_draw_text(ctx,
            ellipsis,
            fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
            GRect(3, 145, 144-3, 168-145),
            GTextOverflowModeWordWrap,
            GTextAlignmentRight,
            NULL);
    }
    
    //draw current date month
    strftime(month_text, sizeof(month_text), month_format, &clock_time_tm);
    draw_outlined_text(ctx,
        month_text,
        fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
        GRect(3, 0, 144-3, 32),
        GTextOverflowModeWordWrap,
        GTextAlignmentLeft,
        0,
        true);
    
    //draw current date day
    strftime(day_text, sizeof(day_text), day_format, &clock_time_tm);
    draw_outlined_text(ctx,
        day_text,
        fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
        GRect(3, 0, 144-13, 32),
        GTextOverflowModeWordWrap,
        GTextAlignmentRight,
        0,
        true);
}

static void window_unload(Window *window) {
}

static void window_load(Window *window) {
    Layer *window_layer = window_get_root_layer(window);
    GRect bounds = layer_get_bounds(window_layer);
    
    // sunlight_layer
    sunlight_layer = layer_create(bounds);
    layer_set_update_proc(sunlight_layer, &sunlight_layer_update_proc);
    layer_add_child(window_layer, sunlight_layer);
    
    // clockface_layer
    face_layer = layer_create(bounds);
    layer_set_update_proc(face_layer, &face_layer_update_proc);
    layer_add_child(window_layer, face_layer);
    
    // hand_layer
    hand_layer = layer_create(bounds);
    layer_set_update_proc(hand_layer, &hand_layer_update_proc);
    layer_add_child(window_layer, hand_layer);
    
    // sunrise_sunset_text_layer
    sunrise_sunset_text_layer = layer_create(bounds);
    layer_set_update_proc(sunrise_sunset_text_layer, &sunrise_sunset_text_layer_update_proc);
    layer_add_child(window_layer, sunrise_sunset_text_layer);
    
    // battery_layer
    battery_layer = layer_create(bounds);
    layer_set_update_proc(battery_layer, &battery_layer_update_proc);
    layer_add_child(window_layer, battery_layer);
}

static void init(void) {
    app_message_register_inbox_received(in_received_handler);
    app_message_register_inbox_dropped(in_dropped_handler);
    app_message_register_outbox_sent(out_sent_handler);
    app_message_register_outbox_failed(out_failed_handler);
    
    battery_state_service_subscribe(update_battery_percentage);
    
    app_message_open(app_message_inbox_size_maximum(),app_message_outbox_size_maximum());
    
    window = window_create();
    window_set_window_handlers(window, (WindowHandlers) {
        .load = window_load,
        .unload = window_unload,});
    const bool animated = true;
    window_stack_push(window, animated);
          
    tick_timer_service_subscribe(MINUTE_UNIT, handle_time_tick);
        
    // get the _actual_ battery state (global variables set it up as if it were 100%).
    update_battery_percentage(battery_state_service_peek());
}

static void deinit(void) {
    layer_remove_from_parent(hand_layer);
    layer_remove_from_parent(sunlight_layer);
    layer_remove_from_parent(sunrise_sunset_text_layer);
    layer_remove_from_parent(face_layer);
    layer_remove_from_parent(battery_layer);
    
    layer_destroy(face_layer);
    layer_destroy(hand_layer);
    layer_destroy(sunlight_layer);
    layer_destroy(sunrise_sunset_text_layer);
    layer_destroy(battery_layer);
    
    window_destroy(window);
}

int main(void) {
    init();
    
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", window);
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Max inbox size:  %d.", (int) app_message_inbox_size_maximum());
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Max outbox size: %d.", (int) app_message_outbox_size_maximum());
    
    app_event_loop();
    deinit();
}
