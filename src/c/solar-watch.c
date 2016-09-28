//TODO: replace my_cos with cos_lookup

#include <pebble.h>
#include "my_math.h"

static Window *window;
static Layer *face_layer;
static Layer *hand_layer;
static Layer *sunlight_layer;
static Layer *sunrise_sunset_text_layer;
#if defined(PBL_RECT)
    static Layer *services_layer; // battery percentage, phone connection and location availability
#endif
static bool screen_is_obstructed = false;

static const float rad_factor = M_PI / 180;

static int16_t solar_offset = 0;
static int16_t current_sunrise_sunset_day = -1;

// times in wall clock scale
static struct tm wall_time_tm    = { .tm_hour = 0, .tm_min = 0 };
static struct tm solar_time_tm   = { .tm_hour = 0, .tm_min = 0 };
static struct tm sunrise_time_tm = { .tm_hour = 0, .tm_min = 0 };
static struct tm sunset_time_tm  = { .tm_hour = 0, .tm_min = 0 };

// times in solar time scale (solar offset applied)
static float sunrise_time_solar           = 0.0;    
static float sunset_time_solar            = 0.0;
static float civil_dawn_time_solar        = 0.0;
static float nautical_dawn_time_solar     = 0.0;
static float astronomical_dawn_time_solar = 0.0;
static float civil_dusk_time_solar        = 0.0;
static float nautical_dusk_time_solar     = 0.0;
static float astronomical_dusk_time_solar = 0.0;

#if defined(PBL_RECT)
static int16_t current_battery_charge = -1;
static char *battery_level_string = "100%";
#endif
bool current_connection_available = false;
bool current_location_available = false;

bool setting_digital_display = true;
bool setting_hour_numbers = true;
#if defined(PBL_RECT)
bool setting_battery_status = true;
#endif
bool setting_connection_status = true;
bool setting_location_status = true;

static GFont font_solar_time;
static GFont font_clock_time;

GColor daytime_color;
GColor nighttime_color;

#define white_space(c) ((c) == ' ' || (c) == '\t')
#define valid_digit(c) ((c) >= '0' && (c) <= '9')
double myatof (const char *p)
{
// Since compilation fails using the standard `atof',
// the following `myatof' implementation taken from:
// 09-May-2009 Tom Van Baak (tvb) www.LeapSecond.com
    
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

void ftoa(char* str, double val, int precision) {
//
// snprintf does not support double: http://forums.getpebble.com/discussion/8743/petition-please-support-float-double-for-snprintf
// workaround from MatthewClark
//    
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

static void draw_dotted_line_b(GContext* ctx, GPoint from, GPoint to, GColor color_1, GColor color_2, int step) {
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

static void draw_dotted_line(GContext* ctx, GPoint from, GPoint to) {
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

static float tm_to_solar_time(struct tm in_tm, int16_t in_solar_offset) {
    float ret = in_tm.tm_hour + ((float)in_tm.tm_min / MINUTES_PER_HOUR);
    ret += (float)in_solar_offset / SECONDS_PER_HOUR;
    if (ret > HOURS_PER_DAY) {
        ret -= (float)HOURS_PER_DAY;
    }
    return ret;
}

static void update_time() {
    //APP_LOG(APP_LOG_LEVEL_DEBUG, "update_time()");
    
    time_t wall_time_t  = time(NULL);
    time_t solar_time_t = wall_time_t + solar_offset;
    
    wall_time_tm  = *localtime(&wall_time_t);  
    solar_time_tm = *localtime(&solar_time_t);
    
    //char solar_time_txt[5]; 
    //ftoa(solar_time_txt, solar_time_t, 2);
    //APP_LOG(APP_LOG_LEVEL_DEBUG, "sunrise_time: %s,  sunset_time: %s (solar time)", solar_time_txt, sunset_time_txt); 
          
    // When day filips over
    if ((current_sunrise_sunset_day != wall_time_tm.tm_mday)) {            
        current_sunrise_sunset_day = wall_time_tm.tm_mday;          
        send_request(0); // blank request to trigger data refresh from phone JS         
    }

    layer_mark_dirty(face_layer);     
    layer_mark_dirty(hand_layer);
    layer_mark_dirty(sunlight_layer);
    layer_mark_dirty(sunrise_sunset_text_layer);
}

static void handle_time_tick(struct tm *tick_time, TimeUnits units_changed) {
    //APP_LOG(APP_LOG_LEVEL_DEBUG, "handle_time_tick()");    
    update_time();
}

void in_received_handler(DictionaryIterator *received, void *ctx) {
    //APP_LOG(APP_LOG_LEVEL_DEBUG, "in_received_handler()");

    Tuple *solar_offset_tuple               = dict_find(received, MESSAGE_KEY_SOLAR_OFFSET);
    Tuple *sunrise_hours_tuple              = dict_find(received, MESSAGE_KEY_SUNRISE_HOURS);
    Tuple *sunrise_minutes_tuple            = dict_find(received, MESSAGE_KEY_SUNRISE_MINUTES);
    Tuple *sunset_hours_tuple               = dict_find(received, MESSAGE_KEY_SUNSET_HOURS);
    Tuple *sunset_minutes_tuple             = dict_find(received, MESSAGE_KEY_SUNSET_MINUTES);
    Tuple *civil_dawn_hours_tuple           = dict_find(received, MESSAGE_KEY_CIVIL_DAWN_HOURS);
    Tuple *civil_dawn_minutes_tuple         = dict_find(received, MESSAGE_KEY_CIVIL_DAWN_MINUTES);
    Tuple *nautical_dawn_hours_tuple        = dict_find(received, MESSAGE_KEY_NAUTICAL_DAWN_HOURS);
    Tuple *nautical_dawn_minutes_tuple      = dict_find(received, MESSAGE_KEY_NAUTICAL_DAWN_MINUTES);
    Tuple *astronomical_dawn_hours_tuple    = dict_find(received, MESSAGE_KEY_ASTRONOMICAL_DAWN_HOURS);
    Tuple *astronomical_dawn_minutes_tuple  = dict_find(received, MESSAGE_KEY_ASTRONOMICAL_DAWN_MINUTES);
    Tuple *civil_dusk_hours_tuple           = dict_find(received, MESSAGE_KEY_CIVIL_DUSK_HOURS);
    Tuple *civil_dusk_minutes_tuple         = dict_find(received, MESSAGE_KEY_CIVIL_DUSK_MINUTES);
    Tuple *nautical_dusk_hours_tuple        = dict_find(received, MESSAGE_KEY_NAUTICAL_DUSK_HOURS);
    Tuple *nautical_dusk_minutes_tuple      = dict_find(received, MESSAGE_KEY_NAUTICAL_DUSK_MINUTES);
    Tuple *astronomical_dusk_hours_tuple    = dict_find(received, MESSAGE_KEY_ASTRONOMICAL_DUSK_HOURS);
    Tuple *astronomical_dusk_minutes_tuple  = dict_find(received, MESSAGE_KEY_ASTRONOMICAL_DUSK_MINUTES);
   
    // Does BZZZ.
    vibes_short_pulse();
    
    solar_offset                 = 0;
    sunrise_time_tm.tm_hour      = 0;
    sunrise_time_tm.tm_min       = 0;
    sunrise_time_solar           = 0.0;
    sunset_time_tm.tm_hour       = 0;
    sunset_time_tm.tm_min        = 0;   
    sunset_time_solar            = 0.0;
    civil_dawn_time_solar        = 0.0;
    nautical_dawn_time_solar     = 0.0;
    astronomical_dawn_time_solar = 0.0;
    civil_dusk_time_solar        = 0.0;
    nautical_dusk_time_solar     = 0.0;
    astronomical_dusk_time_solar = 0.0;
    
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Received from phone:");

    if (solar_offset_tuple) {         
        solar_offset = solar_offset_tuple->value->int32;
    
        if (sunrise_hours_tuple && sunrise_minutes_tuple) {        
            sunrise_time_tm.tm_hour = sunrise_hours_tuple->value->int32;
            sunrise_time_tm.tm_min  = sunrise_minutes_tuple->value->int32;
            sunrise_time_solar = tm_to_solar_time(sunrise_time_tm, solar_offset);
        }
        if (sunset_hours_tuple && sunset_minutes_tuple) {        
            sunset_time_tm.tm_hour = sunset_hours_tuple->value->int32;
            sunset_time_tm.tm_min  = sunset_minutes_tuple->value->int32;
            sunset_time_solar = tm_to_solar_time(sunset_time_tm, solar_offset);
        }
        if (civil_dawn_hours_tuple && civil_dawn_minutes_tuple) {   
            struct tm civil_dawn_time_tm  = { 
                .tm_hour = civil_dawn_hours_tuple->value->int32, 
                .tm_min = civil_dawn_minutes_tuple->value->int32
            };
            civil_dawn_time_solar = tm_to_solar_time(civil_dawn_time_tm, solar_offset);
        }
        if (nautical_dawn_hours_tuple && nautical_dawn_minutes_tuple) {   
            struct tm nautical_dawn_time_tm  = { 
                .tm_hour = nautical_dawn_hours_tuple->value->int32, 
                .tm_min = nautical_dawn_minutes_tuple->value->int32
            };
            nautical_dawn_time_solar = tm_to_solar_time(nautical_dawn_time_tm, solar_offset);
        }
        if (astronomical_dawn_hours_tuple && astronomical_dawn_minutes_tuple) {   
            struct tm astronomical_dawn_time_tm  = { 
                .tm_hour = astronomical_dawn_hours_tuple->value->int32, 
                .tm_min = astronomical_dawn_minutes_tuple->value->int32
            };
            astronomical_dawn_time_solar = tm_to_solar_time(astronomical_dawn_time_tm, solar_offset);
        }
        if (civil_dusk_hours_tuple && civil_dusk_minutes_tuple) {   
            struct tm civil_dusk_time_tm  = { 
                .tm_hour = civil_dusk_hours_tuple->value->int32, 
                .tm_min = civil_dusk_minutes_tuple->value->int32
            };
            civil_dusk_time_solar = tm_to_solar_time(civil_dusk_time_tm, solar_offset);
        }
        if (nautical_dusk_hours_tuple && nautical_dusk_minutes_tuple) {   
            struct tm nautical_dusk_time_tm  = { 
                .tm_hour = nautical_dusk_hours_tuple->value->int32, 
                .tm_min = nautical_dusk_minutes_tuple->value->int32
            };
            nautical_dusk_time_solar = tm_to_solar_time(nautical_dusk_time_tm, solar_offset);
        }
        if (astronomical_dusk_hours_tuple && astronomical_dusk_minutes_tuple) {   
            struct tm astronomical_dusk_time_tm  = { 
                .tm_hour = astronomical_dusk_hours_tuple->value->int32, 
                .tm_min = astronomical_dusk_minutes_tuple->value->int32
            };
            astronomical_dusk_time_solar = tm_to_solar_time(astronomical_dusk_time_tm, solar_offset);
        }
    }
    
    APP_LOG(APP_LOG_LEVEL_DEBUG, "  solar offset: %d", (int)solar_offset);
    APP_LOG(APP_LOG_LEVEL_DEBUG, "  sunrise: %d:%d", (int)sunrise_time_tm.tm_hour, (int)sunrise_time_tm.tm_min);
    APP_LOG(APP_LOG_LEVEL_DEBUG, "  sunset:  %d:%d", (int)sunset_time_tm.tm_hour,  (int)sunset_time_tm.tm_min);
    
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

#if defined(PBL_RECT)
static void update_battery_percentage(BatteryChargeState c) {
    if (setting_battery_status) {
        int battery_level_int = c.charge_percent;
        snprintf(battery_level_string, 5, "%d%%", battery_level_int);
        layer_mark_dirty(services_layer);
    }
}

static void app_connection_handler(bool connected) {
    //APP_LOG(APP_LOG_LEVEL_INFO, "Pebble app is %sconnected", connected ? "" : "dis");
    current_connection_available = connected;
    if (!current_connection_available) {
        vibes_short_pulse();
    }
    layer_mark_dirty(services_layer);
}  
#endif

static void face_layer_update_proc(Layer *layer, GContext *ctx) {
    //APP_LOG(APP_LOG_LEVEL_DEBUG, "face_layer_update_proc()");
    
    //update_time();
    
    GRect bounds = layer_get_bounds(layer);
    GPoint center = grect_center_point(&bounds);  
    const uint16_t radius = center.x < center.y ? center.x : center.y;
    uint16_t deg_step = 0;
    /*******************************
        DRAW PRIMITIVES FOR THIS LAYER
    *********************************/
    
    #if defined(PBL_RECT)
    
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
        // Sunrays
        graphics_context_set_stroke_color(ctx, COLOR_FALLBACK(GColorYellow, GColorWhite));  
    
        const int16_t start_angle = TRIG_MAX_ANGLE / 4;
        const uint16_t ray_length_min = 5;
        const uint16_t ray_length_max = 20;
        uint16_t ray_length;
    
        deg_step = TRIG_MAX_ANGLE / 120;
           
        for (int i=-start_angle; i<start_angle; i+=deg_step) {
            //float current_rads = rad_factor * (deg_step * (i - 30));
            GPoint start_point = {
                .x = ( sin_lookup(i) * radius / TRIG_MAX_RATIO) + center.x,
                .y = (-cos_lookup(i) * radius / TRIG_MAX_RATIO) + center.y
            };
            
            ray_length = ray_length_min + (rand() % (ray_length_max-ray_length_min));
            
            GPoint end_point = {
                .x = ( sin_lookup(i) * (radius + ray_length) / TRIG_MAX_RATIO) + center.x,
                .y = (-cos_lookup(i) * (radius + ray_length) / TRIG_MAX_RATIO) + center.y
            };
            
            graphics_draw_line(ctx, start_point, end_point);
        }
    #endif
       
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
    
    /*************************
        DRAW TEXT FOR THIS LAYER
    **************************/
    if (setting_hour_numbers) {
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
}

static void hand_layer_update_proc(Layer* layer, GContext* ctx) {
    const GRect bounds = layer_get_bounds(layer);
    const GPoint center = grect_center_point(&bounds);
    const uint16_t radius = center.x < center.y ? center.x : center.y;
    const uint16_t hand_length = radius - 19;
    GPathInfo p_hour_hand_info = {
        .num_points = 5,
        .points = (GPoint []) {{3,12},{-3,12},{-2,-hand_length},{0,-(hand_length+1)},{2,-hand_length}}
    };
    
    static GPath *p_hour_hand = NULL;
    
    // 24 hour hand
    const int32_t hour_angle = (((solar_time_tm.tm_hour * 60) + solar_time_tm.tm_min) / 4) + 180;
    
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

#if defined(PBL_RECT)
static void services_layer_update_proc(Layer* layer, GContext* ctx) {
    GRect bounds = layer_get_bounds(layer);
    GPoint center = grect_center_point(&bounds); 
    
    if (setting_battery_status) {
        graphics_context_set_text_color(ctx, GColorWhite);
        draw_outlined_text(ctx,
            battery_level_string,
            fonts_get_system_font(FONT_KEY_GOTHIC_14),
            GRect(center.x - 10, bounds.size.h - 16, 30, 14),
            GTextOverflowModeWordWrap,
            GTextAlignmentCenter,
            1,
            true);
    }
    
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
#endif

#if defined(PBL_BW)
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
    
int log_point (int px, int py) {
    return 0; // disabled
    
    if ((px==center.x-10 && py==center.y-10) || 
        //(px==center.x && py==center.y-10) || 
        (px==center.x+10 && py==center.y-10) ||
        (px==center.x+10 && py==center.y+10) || 
        (px==center.x-10 && py==center.y+10)) 
    {
        return 1;
    } else {
        return 0;
    }
}
    
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
if(log_point(x,y)) {
    //char debug_txt[9];
    //ftoa(debug_txt, angle, 4); 
    APP_LOG(APP_LOG_LEVEL_DEBUG, "x = %d, y = %d, angle = %d (%d)", x, y, (int)angle, (int)TRIGANGLE_TO_DEG(angle)); 
}   
                    

if(log_point(x,y)) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Adjusted angle = %d (%d)", (int)angle, (int)TRIGANGLE_TO_DEG(angle)); 
}                   //TODO: negative angles, e.g. -45deg to +45deg  
                    if (angle >= angle_start && angle <= angle_end) { // point is within angle range
                        graphics_draw_pixel(ctx, GPoint(x,y));
                    }
                }
            }
        }
    }
}
#endif

static void sunlight_layer_update_proc(Layer* layer, GContext* ctx) {
    //APP_LOG(APP_LOG_LEVEL_DEBUG, "sunlight_layer_update_proc()");
    //APP_LOG(APP_LOG_LEVEL_DEBUG, "sunrise_time: %d, sunset_time: %d", (int)sunrise_time, (int)sunset_time);
    
    if (sunrise_time_solar && sunset_time_solar) {   
        GRect bounds = layer_get_bounds(layer);
        const GPoint center = grect_center_point(&bounds);
        const uint16_t radius = center.x < center.y ? center.x : center.y;
        const uint16_t clock_face_radius = radius - 3;
        GRect clock_face_bounds = bounds;
        clock_face_bounds.origin.x += 3;
        clock_face_bounds.size.w -= 6;
     
        int32_t sunrise_angle            = (TRIG_MAX_ANGLE * sunrise_time_solar)            / HOURS_PER_DAY;
        int32_t civil_dawn_angle         = (TRIG_MAX_ANGLE * civil_dawn_time_solar)         / HOURS_PER_DAY;
        int32_t nautical_dawn_angle      = (TRIG_MAX_ANGLE * nautical_dawn_time_solar)      / HOURS_PER_DAY;
        int32_t astronomical_dawn_angle  = (TRIG_MAX_ANGLE * astronomical_dawn_time_solar)  / HOURS_PER_DAY;
        int32_t sunset_angle             = (TRIG_MAX_ANGLE * sunset_time_solar)             / HOURS_PER_DAY;
        int32_t civil_dusk_angle         = (TRIG_MAX_ANGLE * civil_dusk_time_solar)         / HOURS_PER_DAY;
        int32_t nautical_dusk_angle      = (TRIG_MAX_ANGLE * nautical_dusk_time_solar)      / HOURS_PER_DAY;
        int32_t astronomical_dusk_angle  = (TRIG_MAX_ANGLE * astronomical_dusk_time_solar)  / HOURS_PER_DAY;
        
                
        // Screen's 0 degrees is at the top, while watch dial mark for 0 hours is at the bottom, we neeed to rotate all angles by -180 deg
        // E.g. 80 to -100, 280 to 100, etc.
        sunrise_angle -= TRIG_MAX_ANGLE/2; // it gets negative at this point
        sunset_angle  -= TRIG_MAX_ANGLE/2;
                
        // Day
        graphics_context_set_fill_color(ctx, daytime_color);
//         graphics_fill_radial(
//             ctx, 
//             grect_inset(clock_face_bounds, GEdgeInsets(0)), 
//             GOvalScaleModeFitCircle, 
//             clock_face_radius, 
//             sunrise_angle, // it is negative at this point
//             sunset_angle);
        graphics_fill_circle(ctx, center, clock_face_radius);
        
        sunrise_angle += TRIG_MAX_ANGLE; // switch from negative to positive angle        
                
        // Civil twilight - first draw on whole duration of the night
        if (sunrise_angle > sunset_angle) {
            #if defined(PBL_COLOR)
            graphics_context_set_fill_color(ctx, GColorBabyBlueEyes);
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
        }
        
        civil_dawn_angle        += TRIG_MAX_ANGLE/2;
        nautical_dawn_angle     += TRIG_MAX_ANGLE/2;
        astronomical_dawn_angle += TRIG_MAX_ANGLE/2;
        
        civil_dusk_angle        -= TRIG_MAX_ANGLE/2;
        nautical_dusk_angle     -= TRIG_MAX_ANGLE/2;
        astronomical_dusk_angle -= TRIG_MAX_ANGLE/2;        
        
// APP_LOG(APP_LOG_LEVEL_DEBUG, "sunrise=%d, sunset=%d, civil_dawn=%d, civil_dusk=%d", 
//         (int)sunrise_angle, 
//         (int)sunset_angle, 
//         (int)astronomical_dawn_angle,
//         (int)astronomical_dusk_angle);
        
        // Nautical twilight
        if (civil_dawn_angle > civil_dusk_angle) {
            #if defined(PBL_COLOR)
            graphics_context_set_fill_color(ctx, GColorElectricUltramarine);
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
        }

        // Astronomical twilight
        if (nautical_dawn_angle > nautical_dusk_angle) {
            #if defined(PBL_COLOR)
                graphics_context_set_fill_color(ctx, GColorDukeBlue);
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
        }  
            
        // Darkest part of night - may not occur at all on short nights
        if (astronomical_dawn_angle > astronomical_dusk_angle) {
            graphics_context_set_fill_color(ctx, nighttime_color);
            graphics_fill_radial(
                ctx, 
                grect_inset(clock_face_bounds, GEdgeInsets(0)), 
                GOvalScaleModeFitCircle, 
                clock_face_radius, 
                astronomical_dusk_angle, 
                astronomical_dawn_angle);         
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
}

static void sunrise_sunset_text_layer_update_proc(Layer* layer, GContext* ctx) {
    //APP_LOG(APP_LOG_LEVEL_DEBUG, "sunrise_sunset_text_layer_update_proc()"); 
        
    GRect bounds = layer_get_bounds(layer);
    GPoint center = grect_center_point(&bounds);     
    
    static char sunrise_text[] = "00:00   ";
    static char sunset_text[] = "00:00   ";
    static char wall_time_text[] = "00:00   ";
    static char solar_time_text[] = "00:00   ";
    #if defined(PBL_RECT)
    static char month_text[] = "Jan";
    static char day_text[] = "00";
    #endif
    char *time_format;
    
    if (clock_is_24h_style()) {
        time_format = "%H:%M";
    } else {
        time_format = "%l:%M";
    }
    char *month_format = "%b";
    char *day_format = "%e";
    char *ellipsis = ".....";
    
    
    // draw current solar and clock time
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
    
    #if defined(PBL_RECT)
    strftime(sunrise_text, sizeof(sunrise_text), time_format, &sunrise_time_tm);
    strftime(sunset_text,  sizeof(sunset_text),  time_format, &sunset_time_tm);
    graphics_context_set_text_color(ctx, GColorWhite);
    
    if (sunrise_time_solar && sunset_time_solar) {
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
    strftime(month_text, sizeof(month_text), month_format, &wall_time_tm);
    draw_outlined_text(ctx,
        month_text,
        fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
        GRect(3, 0, 144-3, 32),
        GTextOverflowModeWordWrap,
        GTextAlignmentLeft,
        0,
        true);
    
    //draw current date day
    strftime(day_text, sizeof(day_text), day_format, &wall_time_tm);
    draw_outlined_text(ctx,
        day_text,
        fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
        GRect(3, 0, 144-13, 32),
        GTextOverflowModeWordWrap,
        GTextAlignmentRight,
        0,
        true);
    #endif
}

// Event fires once, before the obstruction appears or disappears
static void unobstructed_will_change(GRect final_unobstructed_screen_area, void *context) {
  
  APP_LOG(APP_LOG_LEVEL_DEBUG,
    "unobstructed_will_change(), available screen area: width: %d, height: %d",
    final_unobstructed_screen_area.size.w,
    final_unobstructed_screen_area.size.h);
}

// Event fires frequently, while obstruction is appearing or disappearing
static void unobstructed_change(AnimationProgress progress, void *context) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "unobstructed_change(), progress %d", (int)progress);
}

// Event fires once, after obstruction appears or disappears
static void unobstructed_did_change(void *context) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "unobstructed_did_change()");
    // Keep track if the screen is obstructed or not
    screen_is_obstructed = !screen_is_obstructed;
}

static void window_load(Window *window) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "window_load()");
    Layer *window_layer = window_get_root_layer(window);
    GRect bounds = layer_get_bounds(window_layer);
    GRect unobstructed_bounds = layer_get_unobstructed_bounds(window_layer);
    
    // Determine if the screen is obstructed when the app starts
    screen_is_obstructed = !grect_equal(&bounds, &unobstructed_bounds);
    
    UnobstructedAreaHandlers handlers = {
        .will_change = unobstructed_will_change,
        .change = unobstructed_change,
        .did_change = unobstructed_did_change
    };
    unobstructed_area_service_subscribe(handlers, NULL);
    
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
    
    services_layer = layer_create(bounds);
    layer_set_update_proc(services_layer, &services_layer_update_proc);
    layer_add_child(window_layer, services_layer);

    // sunrise_sunset_text_layer
    sunrise_sunset_text_layer = layer_create(bounds);
    layer_set_update_proc(sunrise_sunset_text_layer, &sunrise_sunset_text_layer_update_proc);
    layer_add_child(window_layer, sunrise_sunset_text_layer);

}

static void window_unload(Window *window) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "window_unload()");
    unobstructed_area_service_unsubscribe();
}

static void init(void) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "init()");
    app_message_register_inbox_received(in_received_handler);
    app_message_register_inbox_dropped(in_dropped_handler);
    app_message_register_outbox_sent(out_sent_handler);
    app_message_register_outbox_failed(out_failed_handler);
   
    daytime_color   = GColorWhite;
    nighttime_color = GColorBlack;

    #if defined(PBL_RECT)
        //font_clock_time = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_SOURCE_SANS_PRO_BLACK_26));
        font_solar_time = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_SOURCE_SANS_PRO_BLACK_30));
    
        battery_state_service_subscribe(update_battery_percentage);
    
        connection_service_subscribe((ConnectionHandlers) {
            .pebble_app_connection_handler = app_connection_handler
        }); 
    #elif defined(PBL_ROUND)
        //font_clock_time = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_SOURCE_SANS_PRO_BLACK_32));
        font_solar_time = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_SOURCE_SANS_PRO_BLACK_38));
    #endif
    
    //app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());
    app_message_open(256, 256);
    
    window = window_create();
    window_set_window_handlers(window, (WindowHandlers) {
        .load = window_load,
        .unload = window_unload,});
    const bool animated = true;
    
    #if defined(PBL_RECT) // background is not visible on Round anyway
        window_set_background_color(window, GColorOxfordBlue);
    #endif
    
    window_stack_push(window, animated);
          
    tick_timer_service_subscribe(MINUTE_UNIT, handle_time_tick);
            
    #if defined(PBL_RECT)
        // get the _actual_ battery state (global variables set it up as if it were 100%).
        update_battery_percentage(battery_state_service_peek());
    
        current_connection_available = connection_service_peek_pebble_app_connection();
    #elif defined(PBL_ROUND)
    #endif
}

static void deinit(void) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "deinit()");
    layer_remove_from_parent(hand_layer);
    layer_remove_from_parent(sunlight_layer);
    layer_remove_from_parent(face_layer);
    layer_remove_from_parent(sunrise_sunset_text_layer);
    #if defined(PBL_RECT)
        layer_remove_from_parent(services_layer);
    #elif defined(PBL_ROUND)
    #endif
    
    layer_destroy(face_layer);
    layer_destroy(hand_layer);
    layer_destroy(sunlight_layer);
    layer_destroy(sunrise_sunset_text_layer);
    #if defined(PBL_RECT)
        layer_destroy(services_layer);
    #elif defined(PBL_ROUND)
    #endif
    
    window_destroy(window);
}

int main(void) {
    init();
    
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", window);
    
    app_event_loop();
    deinit();
}
