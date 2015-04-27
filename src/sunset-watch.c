#include <pebble.h>
#include "my_math.h"
#include "suncalc.h"

static Window *window;
static Layer *face_layer;
static Layer *hand_layer;
static Layer *sunlight_layer;
static Layer *sunrise_sunset_text_layer;
static Layer *moon_layer;
static Layer *battery_layer;
int current_sunrise_sunset_day = -1;
int current_moon_day = -1;

time_t clock_time_t = 0;
time_t utc_time_t   = 0;
time_t solar_time_t = 0;
//struct tm *clock_time_tm = &(struct tm) { .tm_hour = 0, .tm_min = 0 };
//struct tm *utc_time_tm   = &(struct tm) { .tm_hour = 0, .tm_min = 0 };
//struct tm *solar_time_tm = &(struct tm) { .tm_hour = 0, .tm_min = 0 };
struct tm clock_time_tm = { .tm_hour = 0, .tm_min = 0 };
struct tm utc_time_tm   = { .tm_hour = 0, .tm_min = 0 };
struct tm solar_time_tm = { .tm_hour = 0, .tm_min = 0 };
int32_t utc_offset; // phone clock time - UTC in seconds
int32_t seconds_per_degree = 4 * 60; // The Sun travels 1 degree of longtitude in 4 minutes

float sunriseTime;
float sunsetTime;

int phase;
double lat;
double lon;
double tz;
bool position = false;
int current_battery_charge = -1;
char *battery_level_string = "100%";

bool setting_second_hand = false;
bool setting_digital_display = true;
bool setting_hour_numbers = true;
bool setting_moon_phase = true;
bool setting_battery_status = true;
bool setting_daylight_savings = false;
bool setting_manual_timezone = false;
int  setting_manual_offset = -7;
/* bool setting_manual_location = false; */
/* double setting_manual_latitude = -111.0; */
/* double setting_manual_longitude = 38.0; */

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

static void handle_time_tick(struct tm *tick_time, TimeUnits units_changed) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "handle_time_tick()");
    
    clock_time_t = time(NULL);
    clock_time_tm = *localtime(&clock_time_t);

    utc_time_t = clock_time_t + utc_offset;
    utc_time_tm = *localtime(&utc_time_t);
    
    APP_LOG(APP_LOG_LEVEL_DEBUG, "    clock time: %d:%d", clock_time_tm.tm_hour, clock_time_tm.tm_min);
    APP_LOG(APP_LOG_LEVEL_DEBUG, "    UTC time: %d:%d", utc_time_tm.tm_hour, utc_time_tm.tm_min);  
    
    layer_mark_dirty(face_layer);    
}

/******************
    APPMESSAGE STUFF
*******************/
enum {
    LAT = 0x1,
    LON = 0X2,
    SH = 0x3,
    DD = 0x4,
    HN = 0x5,
    MP = 0x6,
    BS = 0x7, // ha ha, `BS'...
    DS = 0x8,
    MT = 0x9,
    MO = 0xA,
    TZO = 11
    /* ML = 0xB, */
    /* MLAT = 0xC, */
    /* MLON = 0xD */
};
// Define outside enum so it does not break web config:
#define LLAT 0xE   // last known latitude
#define LLON 0xF   // last know longtitude
#define LTZ  0x01  // last know TZ offset

void in_received_handler(DictionaryIterator *received, void *ctx) {
    //APP_LOG(APP_LOG_LEVEL_DEBUG, "in_received_handler()");
    Tuple *latitude = dict_find(received, LAT);
    Tuple *longitude = dict_find(received, LON);
    Tuple *second_hand = dict_find(received, SH);
    Tuple *digital_display = dict_find(received, DD);
    Tuple *hour_numbers = dict_find(received, HN);
    Tuple *moon_phase = dict_find(received, MP);
    Tuple *battery_status = dict_find(received, BS);
    Tuple *daylight_savings = dict_find(received, DS);
    Tuple *timezone_offset_tuple = dict_find(received, TZO);
    
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Received from phone:");
    if (latitude && longitude) {
        APP_LOG(APP_LOG_LEVEL_DEBUG, "    lat=%s, lon=%s", latitude->value->cstring, longitude->value->cstring);    
        position = true;    
        double lat_new = myatof(latitude->value->cstring);
        double lon_new = myatof(longitude->value->cstring);
        if (lat != lat_new || lon != lon_new) {
            APP_LOG(APP_LOG_LEVEL_DEBUG, "Location changed, redrawing...");
            lat = lat_new;
            lon = lon_new;
            
            // without marking one of the layers dirty,
            // we have to wait until the next tick_event
            // to see the new masks.
            layer_mark_dirty(face_layer);
            layer_mark_dirty(sunrise_sunset_text_layer);
        }
    }
    if (timezone_offset_tuple) {        
        utc_offset = timezone_offset_tuple->value->int32;
    }    
    
    if (second_hand && digital_display &&
    hour_numbers && moon_phase && battery_status && daylight_savings) {
        setting_second_hand = (second_hand->value->uint32 == 1) ? true : false;
        setting_digital_display = (digital_display->value->uint32 == 1) ? true : false;
        setting_hour_numbers = (hour_numbers->value->uint32  == 1) ? true : false;
        setting_moon_phase = (moon_phase->value->uint32 == 1) ? true : false;
        setting_battery_status = (battery_status->value->uint32  == 1) ? true : false;
        setting_daylight_savings = (daylight_savings->value->uint32  == 1) ? true : false;
    }
    
    // next line forces recalculation of sunrise/sunset times (in case DS/TZ option is changed).
    current_sunrise_sunset_day = -1;
    
    // if the second hand is enabled, we need to make sure the face updates on the appropriate tick event.
    if (setting_second_hand) {
        tick_timer_service_unsubscribe();
        tick_timer_service_subscribe(SECOND_UNIT, handle_time_tick);
        } else {
        tick_timer_service_unsubscribe();
        tick_timer_service_subscribe(MINUTE_UNIT, handle_time_tick);
    }
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
        snprintf(battery_level_string, sizeof(battery_level_string), "%d%%", battery_level_int);
        layer_mark_dirty(battery_layer);
        APP_LOG(APP_LOG_LEVEL_DEBUG, "Battery change: battery_layer marked dirty...");
    }
}

static void face_layer_update_proc(Layer *layer, GContext *ctx) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "face_layer_update_proc()");
    

    
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

static const GPathInfo p_second_hand_info = {
    .num_points = 6,
    .points = (GPoint []) {{4,0},{0,8},{-4,0},{-3,-60},{0,-65},{3,-60}}
};

static void hand_layer_update_proc(Layer* layer, GContext* ctx) {
    GRect bounds = layer_get_bounds(layer);
    GPoint center = grect_center_point(&bounds);
    
    static GPath *p_hour_hand = NULL;
    static GPath *p_second_hand = NULL;
    
    int32_t second_angle = solar_time_tm.tm_sec * 6;
    
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
    
    // draw the second hand
    if (setting_second_hand) {
        graphics_context_set_stroke_color(ctx, GColorBlack);
        p_second_hand = gpath_create(&p_second_hand_info);
        graphics_context_set_fill_color(ctx, GColorWhite);
        gpath_move_to(p_second_hand, center);
        gpath_rotate_to(p_second_hand, TRIG_MAX_ANGLE / 360 * second_angle);
        gpath_draw_filled(ctx, p_second_hand);
        gpath_draw_outline(ctx, p_second_hand);
        gpath_destroy(p_second_hand);
    }
    
    // Draw hand "pin"
    //graphics_context_set_stroke_color(ctx, GColorBlack);
    //graphics_draw_circle(ctx, center, 5);
    draw_dot(ctx, center, 5);
}

GPathInfo sun_path_moon_mask_info = {
    5,
    (GPoint []) {
        {0, 0},
        {-73, +84}, //replaced by sunrise angle
        {-73, -84}, //top left
        {+73, -84}, //top right
        {+73, +84}, //replaced by sunset angle
    }
};

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

// (Re)calculate sunrise, sunset, noon, etc. times
//
void calc_sun_times() {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "calc_sun_times()");
    
    //sunriseTime = calcSunRise(2014, 2, 9, 55.26, 23.65, 91.0f);
    //sunsetTime  = calcSunSet (2014, 2, 9, 55.26, 23.65, 91.0f);
    sunriseTime = calcSunRise(clock_time_tm.tm_year, clock_time_tm.tm_mon+1, clock_time_tm.tm_mday, lat, lon, 91.0f);
    sunsetTime  = calcSunSet (clock_time_tm.tm_year, clock_time_tm.tm_mon+1, clock_time_tm.tm_mday, lat, lon, 91.0f);
    /* struct tm *sunrise_time = localtime(&now_epoch); */
    /* struct tm *sunset_time = localtime(&now_epoch); */
    
    //sunriseTime = calcSunRise(time->tm_year, time->tm_mon+1, time->tm_mday, lat, lon, 91.0f);
    
   
    //if (sunsetTime > sunriseTime) {
    //  solarNoonTime = sunriseTime + (sunsetTime - sunriseTime) / 2;
    //  solarTimeOffset = solarNoonTime - 12.0;
//}

//APP_LOG(APP_LOG_LEVEL_DEBUG, "solarNoonTime = %f", solarNoonTime);
}

static void sunlight_layer_update_proc(Layer* layer, GContext* ctx) {
    //APP_LOG(APP_LOG_LEVEL_DEBUG, "sunlight_layer_update_proc()");
    char lat_str_tmp[15];
    char lon_str_tmp[15];
    char tz_str_tmp[15];
    ftoa(lat_str_tmp, lat, 7);
    ftoa(lon_str_tmp, lon, 7);
    ftoa(tz_str_tmp, tz, 7);
    //APP_LOG(APP_LOG_LEVEL_DEBUG, "  current location: %s %s, tz: %s", lat_str_tmp, lon_str_tmp, tz_str_tmp);
    
    GRect bounds = layer_get_bounds(layer);
    GPoint center = grect_center_point(&bounds);
    
    /* struct tm *sunrise_time = localtime(&now_epoch); */
    /* struct tm *sunset_time = localtime(&now_epoch); */
    
    sunriseTime = calcSunRise(clock_time_tm.tm_year, clock_time_tm.tm_mon+1, clock_time_tm.tm_mday, lat, lon, 91.0f);
    sunsetTime  = calcSunSet (clock_time_tm.tm_year, clock_time_tm.tm_mon+1, clock_time_tm.tm_mday, lat, lon, 91.0f);
    
    //if (sunsetTime > sunriseTime) {
    //  solarNoonTime = sunriseTime + (sunsetTime - sunriseTime) / 2;
    //  APP_LOG(APP_LOG_LEVEL_DEBUG, "solarNoonTime = %f", solarNoonTime);
    //  solarTimeOffset = solarNoonTime - 12.0;
    //  APP_LOG(APP_LOG_LEVEL_DEBUG, "solarTimeOffset = %f", solarTimeOffset);
    //}

    //calc_sun_times();

    sun_path_info.points[1].x = (int16_t)(my_sin(sunriseTime/24 * M_PI * 2) * 120);
    sun_path_info.points[1].y = -(int16_t)(my_cos(sunriseTime/24 * M_PI * 2) * 120);

    sun_path_info.points[4].x = (int16_t)(my_sin(sunsetTime/24 * M_PI * 2) * 120);
    sun_path_info.points[4].y = -(int16_t)(my_cos(sunsetTime/24 * M_PI * 2) * 120);

    sun_path_moon_mask_info.points[1].x = (int16_t)(my_sin(sunriseTime/24 * M_PI * 2) * 120);
    sun_path_moon_mask_info.points[1].y = -(int16_t)(my_cos(sunriseTime/24 * M_PI * 2) * 120);

    sun_path_moon_mask_info.points[4].x = (int16_t)(my_sin(sunsetTime/24 * M_PI * 2) * 120);
    sun_path_moon_mask_info.points[4].y = -(int16_t)(my_cos(sunsetTime/24 * M_PI * 2) * 120);

    struct GPath *sun_path;
    sun_path = gpath_create(&sun_path_info);
    graphics_context_set_stroke_color(ctx, GColorBlack);
    graphics_context_set_fill_color(ctx, GColorBlack);
    gpath_move_to(sun_path, center);
    if (position) {
        gpath_draw_outline(ctx, sun_path);
        gpath_draw_filled(ctx, sun_path);
    }
    gpath_destroy(sun_path);
}

static void sunrise_sunset_text_layer_update_proc(Layer* layer, GContext* ctx) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "sunrise_sunset_text_layer_update_proc()"); 
    
    GRect bounds = layer_get_bounds(layer);
    GPoint center = grect_center_point(&bounds);     
    
    struct tm tmSunriseTime = *localtime(&clock_time_t);
    struct tm tmSunsetTime  = *localtime(&clock_time_t);
    
    //struct tm tmSolarTime   = *localtime(&clock_time_t);
    
    static char sunrise_text[] = "00:00";
    static char sunset_text[] = "00:00";
    static char wall_time_text[] = "00:00";
    static char solar_time_text[] = "00:00";
    static char month_text[] = "Jan";
    static char day_text[] = "00";
    char *time_format;
    
    if (clock_is_24h_style()) {
        time_format = "%H:%M";
    }
    else {
        time_format = "%l:%M";
    }
    char *month_format = "%b";
    char *day_format = "%e";
    char *ellipsis = ".....";
    
    //TODO: uncomment
    if ((current_sunrise_sunset_day != clock_time_tm.tm_mday)) {        
        sunriseTime = calcSunRise(clock_time_tm.tm_year, clock_time_tm.tm_mon+1, clock_time_tm.tm_mday, lat, lon, 91.0f);
        sunsetTime  = calcSunSet (clock_time_tm.tm_year, clock_time_tm.tm_mon+1, clock_time_tm.tm_mday, lat, lon, 91.0f);
    }
    
    // don't calculate them again until tomorrow (unless we still don't have position)
    if (position) {
        current_sunrise_sunset_day = clock_time_tm.tm_mday;
    }
    
    // draw current time
    if (setting_digital_display) {   
        //tmSolarTime.tm_min  += (int)(60*(solarTimeOffset-((int)(solarTimeOffset))));        
        //tmSolarTime.tm_hour += (int)solarTimeOffset - 12;
        
        // TODO: 
        solar_time_t = utc_time_t + (int32_t)(seconds_per_degree * lon);
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
        
        strftime(solar_time_text, sizeof(solar_time_text), time_format, &solar_time_tm);
        draw_outlined_text(ctx,
        solar_time_text,
        fonts_get_system_font(FONT_KEY_BITHAM_34_MEDIUM_NUMBERS),
        GRect(center.x - 50, 40, 100, 50), //GRect(42, 47, 64, 32),
        GTextOverflowModeFill,
        GTextAlignmentCenter,
        1,
        false);
        
        strftime(wall_time_text, sizeof(wall_time_text), time_format, &clock_time_tm);
        draw_outlined_text(ctx,
        wall_time_text,
        fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD),
        GRect(42, 86, 64, 32),
        GTextOverflowModeFill,
        GTextAlignmentCenter,
        1,
        true);
    }
    // print sunrise/sunset times (if we can calculate our position)
    tmSunriseTime.tm_min = (int)(60*(sunriseTime-((int)(sunriseTime))));
    tmSunriseTime.tm_hour = (int)sunriseTime - 12;
    strftime(sunrise_text, sizeof(sunrise_text), time_format, &tmSunriseTime);
    tmSunsetTime.tm_min = (int)(60*(sunsetTime-((int)(sunsetTime))));
    tmSunsetTime.tm_hour = (int)sunsetTime + 12;
    strftime(sunset_text, sizeof(sunset_text), time_format, &tmSunsetTime);
    graphics_context_set_text_color(ctx, GColorWhite);
    
    if (position) {
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

int moon_phase(struct tm *time) {
    int y,m;
    double jd;
    int jdn;
    y = time->tm_year + 1900;
    m = time->tm_mon + 1;
    jdn = time->tm_mday-32075+1461*(y+4800+(m-14)/12)/4+367*(m-2-(m-14)/12*12)/12-3*((y+4900+(m-14)/12)/100)/4;
    jd = jdn-2451550.1;
    jd /= 29.530588853;
    jd -= (int)jd;
    return (int)(jd*27 + 0.5); /* scale fraction from 0-27 and round by adding 0.5 */
}

static void moon_layer_update_proc(Layer* layer, GContext* ctx) {
    if (setting_moon_phase) {        
        // don't do any calculations if they've already been done for today.
        if (current_moon_day != clock_time_tm.tm_mday) {
            phase = moon_phase(&clock_time_tm);
            // we don't have to calculate this again until tomorrow...
            current_moon_day = clock_time_tm.tm_mday;
        }
        
        int moon_y = 108;  // y-axis position of the moon's center
        int moon_r = 15;   // radius of the moon
        
        // draw the moon...
        if (position) {
            if (phase != 27) {
                graphics_context_set_fill_color(ctx,GColorWhite);
                graphics_fill_circle(ctx, GPoint(72,moon_y), moon_r);
            }
            if (phase == 27 || phase == 0) {
                graphics_context_set_stroke_color(ctx,GColorWhite);
                graphics_draw_circle(ctx, GPoint(72,moon_y), moon_r);
            }
            
            if (phase != 15 && phase != 27 ) { 
                if (phase < 15) {
                    // draw the waxing occlusion...
                    graphics_context_set_fill_color(ctx,GColorBlack);
                    graphics_fill_circle(ctx, GPoint(72 - (phase * 6), moon_y), moon_r + (phase * 4));
                }
                
                if (phase > 15) {
                    // draw the waning occlusion...
                    int phase_factor = abs(phase-30);
                    graphics_context_set_fill_color(ctx,GColorBlack);
                    graphics_fill_circle(ctx, GPoint(((72-3) + (phase_factor * 6)), moon_y), moon_r + (phase_factor * 4));
                }
            }
        }
        
        // mask off the "daylight" portion of the watchface, otherwise, we
        // see the occlusion circles where the "night" portion does not cover.
        // This is probably the messiest bit of the watch app, since it assumes
        // that a lot of things are happening in the right order to work...
        GRect bounds = layer_get_bounds(layer);
        GPoint center = grect_center_point(&bounds);
        struct GPath *sun_path_moon_mask;
        sun_path_moon_mask = gpath_create(&sun_path_moon_mask_info);
        graphics_context_set_stroke_color(ctx, GColorBlack);
        graphics_context_set_fill_color(ctx, GColorWhite);
        gpath_move_to(sun_path_moon_mask, center);
        if (position) {
            gpath_draw_outline(ctx, sun_path_moon_mask);
            gpath_draw_filled(ctx, sun_path_moon_mask);
        }
        gpath_destroy(sun_path_moon_mask);
    }
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
    
    // moon_layer
    moon_layer = layer_create(bounds);
    layer_set_update_proc(moon_layer, &moon_layer_update_proc);
    layer_add_child(window_layer, moon_layer);
    
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
    char lat_str[15];
    char lon_str[15];
    char tz_str[5];
    
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
    
    /* if (persist_exists(ML) && */
    /*     persist_exists(MLAT) && */
    /*     persist_exists(MLON)) { */
    /*   setting_manual_location = persist_read_bool(ML); */
    /*   // FIXME */
    /*   //    setting_manual_latitude = atof(persist_read_string(MLAT)); */
    /*   //    setting_manual_timezone = atof(persist_read_string(MLON)); */
    /* } */
    
    if (persist_exists(MT) && persist_exists(MO)) {
        setting_manual_timezone = persist_read_bool(MT);
        setting_manual_offset = persist_read_int(MO);
    }
    
    if (persist_exists(SH) &&
        persist_exists(DD) &&
        persist_exists(HN) &&
        persist_exists(MP) &&
        persist_exists(BS) &&
        persist_exists(DS)) {
        setting_second_hand = persist_read_bool(SH);
        setting_digital_display = persist_read_bool(DD);
        setting_hour_numbers = persist_read_bool(HN);
        setting_moon_phase = persist_read_bool(MP);
        setting_battery_status = persist_read_bool(BS);
        setting_daylight_savings = persist_read_bool(DS);
        
        APP_LOG(APP_LOG_LEVEL_DEBUG, (setting_manual_timezone) ? "true" : "false");
        APP_LOG(APP_LOG_LEVEL_DEBUG, "MO: %d", setting_manual_offset);
        
        if (setting_second_hand) {
            tick_timer_service_subscribe(SECOND_UNIT, handle_time_tick);
        } else {
            tick_timer_service_subscribe(MINUTE_UNIT, handle_time_tick);
        }
    }
    
    if (persist_exists(LLAT) && persist_exists(LLON) && persist_exists(LTZ)) {
        persist_read_string(LLAT, lat_str, sizeof(lat_str));
        persist_read_string(LLON, lon_str, sizeof(lon_str));
        persist_read_string(LTZ,  tz_str,  sizeof(tz_str));
        APP_LOG(APP_LOG_LEVEL_DEBUG, "Retrieved from storage: lat=%s lon=%s, tz=%s.", lat_str, lon_str, tz_str);
        lat = myatof(lat_str); 
        lon = myatof(lon_str); 
        tz  = myatof(tz_str);
        if (lat != 0 || lon != 0) {
            position = true;
        }      
    }
    // get the _actual_ battery state (global variables set it up as if it were 100%).
    update_battery_percentage(battery_state_service_peek());
}

static void deinit(void) {
    char lat_str[15]; 
    char lon_str[15];
    char tz_str[5];
    
    persist_write_bool(SH, setting_second_hand);
    persist_write_bool(DD, setting_digital_display);
    persist_write_bool(HN, setting_hour_numbers);
    persist_write_bool(MP, setting_moon_phase);
    persist_write_bool(BS, setting_battery_status);
    persist_write_bool(DS, setting_daylight_savings);
    persist_write_bool(MT, setting_manual_timezone);
    persist_write_int(MO, setting_manual_offset);
    /* persist_write_bool(ML, setting_manual_location); */
    /* persist_write_string(MLAT, snprintf(setting_manual_latitude)); */
    /* persist_write_string(MLON, setting_manual_longitude); */
    
    //snprintf(lat_str, 15, "%f", lat);
    //snprintf(lon_str, 15, "%f", lon);
    // not supported, see http://forums.getpebble.com/discussion/8743/petition-please-support-float-double-for-snprintf
    ftoa(lat_str, lat, 7);
    ftoa(lon_str, lon, 7);
    ftoa(tz_str, tz, 2);
    
    persist_write_string(LLAT, lat_str);
    persist_write_string(LLON, lon_str);
    persist_write_string(LTZ,  tz_str);
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Saving to storage: lat=%s lon=%s, tz=%s.", lat_str, lon_str, tz_str);
    
    layer_remove_from_parent(moon_layer);
    layer_remove_from_parent(hand_layer);
    layer_remove_from_parent(sunlight_layer);
    layer_remove_from_parent(sunrise_sunset_text_layer);
    layer_remove_from_parent(face_layer);
    layer_remove_from_parent(battery_layer);
    
    layer_destroy(face_layer);
    layer_destroy(hand_layer);
    layer_destroy(sunlight_layer);
    layer_destroy(sunrise_sunset_text_layer);
    layer_destroy(moon_layer);
    layer_destroy(battery_layer);
    
    window_destroy(window);
}

int main(void) {
    init();
    
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", window);
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Max inbox size: %d.", (int) app_message_inbox_size_maximum());
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Max outbox size: %d.", (int) app_message_outbox_size_maximum());
    
    app_event_loop();
    deinit();
}
