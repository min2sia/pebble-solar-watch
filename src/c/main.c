#include <pebble.h>
#include "main.h"
#include "util.h"
#include "base_layer.h"
#include "minute_layer.h"
#include "services_layer.h"

static Window *window;
Layer *base_layer;
Layer *minute_layer;
Layer *services_layer;

static bool screen_is_obstructed = false;

int16_t solar_offset = 0;
int16_t current_sunrise_sunset_day = -1;

// times in wall clock scale
struct tm wall_time_tm    = { .tm_hour = 0, .tm_min = 0 };
struct tm solar_time_tm   = { .tm_hour = 0, .tm_min = 0 };
struct tm sunrise_time_tm = { .tm_hour = 0, .tm_min = 0 };
struct tm sunset_time_tm  = { .tm_hour = 0, .tm_min = 0 };

// times in solar time scale (solar offset applied)
float sunrise_time_solar             = 0.0;    
float sunset_time_solar              = 0.0;
float civil_dawn_time_solar          = 0.0;
float nautical_dawn_time_solar       = 0.0;
float astronomical_dawn_time_solar   = 0.0;
float civil_dusk_time_solar          = 0.0;
float nautical_dusk_time_solar       = 0.0;
float astronomical_dusk_time_solar   = 0.0;
float golden_hour_morning_time_solar = 0.0;
float golden_hour_evening_time_solar = 0.0;

int16_t current_battery_charge = -1;
char *battery_level_string = "...";
char date_text[] = " ...  ";
char temperature_text[] = " ... ";
char *time_format;

bool current_connection_available = false;
bool current_location_available = false;

bool setting_digital_display = true;
bool setting_hour_numbers = true;
bool setting_battery_status = true;
bool setting_connection_status = true;
bool setting_location_status = true;

GFont font_solar_time;
GFont font_clock_time;

GColor daytime_color;
GColor nighttime_color;

void send_request(int command) {
  Tuplet command_tuple = TupletInteger(0 /*KEY_COMMAND*/ , command);
  Tuplet index_tuple = TupletInteger(1 /*KEY_INDEX*/, 0 /*entryIndex*/);
  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);
  if (iter == NULL) {
    return;
  }
  dict_write_tuplet(iter, &command_tuple);
  dict_write_tuplet(iter, &index_tuple);
  dict_write_end(iter);
  app_message_outbox_send();
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

    layer_mark_dirty(base_layer);
    layer_mark_dirty(minute_layer);
//     layer_mark_dirty(face_layer);     
//     layer_mark_dirty(hand_layer);
//     layer_mark_dirty(sunlight_layer);
//     layer_mark_dirty(sunrise_sunset_text_layer);
}

static void handle_time_tick(struct tm *tick_time, TimeUnits units_changed) {
    //APP_LOG(APP_LOG_LEVEL_DEBUG, "handle_time_tick()");    
    update_time();
}

void in_received_handler(DictionaryIterator *received, void *ctx) {
    //APP_LOG(APP_LOG_LEVEL_DEBUG, "in_received_handler()");
    Tuple *date_tuple                        = dict_find(received, MESSAGE_KEY_DATE);
    Tuple *temperature_tuple                 = dict_find(received, MESSAGE_KEY_TEMPERATURE);    
    Tuple *solar_offset_tuple                = dict_find(received, MESSAGE_KEY_SOLAR_OFFSET);
    Tuple *sunrise_hours_tuple               = dict_find(received, MESSAGE_KEY_SUNRISE_HOURS);
    Tuple *sunrise_minutes_tuple             = dict_find(received, MESSAGE_KEY_SUNRISE_MINUTES);
    Tuple *civil_dawn_hours_tuple            = dict_find(received, MESSAGE_KEY_CIVIL_DAWN_HOURS);
    Tuple *civil_dawn_minutes_tuple          = dict_find(received, MESSAGE_KEY_CIVIL_DAWN_MINUTES);
    Tuple *nautical_dawn_hours_tuple         = dict_find(received, MESSAGE_KEY_NAUTICAL_DAWN_HOURS);
    Tuple *nautical_dawn_minutes_tuple       = dict_find(received, MESSAGE_KEY_NAUTICAL_DAWN_MINUTES);
    Tuple *astronomical_dawn_hours_tuple     = dict_find(received, MESSAGE_KEY_ASTRONOMICAL_DAWN_HOURS);
    Tuple *astronomical_dawn_minutes_tuple   = dict_find(received, MESSAGE_KEY_ASTRONOMICAL_DAWN_MINUTES);
    Tuple *golden_hour_morning_hours_tuple   = dict_find(received, MESSAGE_KEY_GOLDEN_H_MORNING_HOURS);
    Tuple *golden_hour_morning_minutes_tuple = dict_find(received, MESSAGE_KEY_GOLDEN_H_MORNING_MINUTES);
    Tuple *sunset_hours_tuple                = dict_find(received, MESSAGE_KEY_SUNSET_HOURS);
    Tuple *sunset_minutes_tuple              = dict_find(received, MESSAGE_KEY_SUNSET_MINUTES);
    Tuple *civil_dusk_hours_tuple            = dict_find(received, MESSAGE_KEY_CIVIL_DUSK_HOURS);
    Tuple *civil_dusk_minutes_tuple          = dict_find(received, MESSAGE_KEY_CIVIL_DUSK_MINUTES);
    Tuple *nautical_dusk_hours_tuple         = dict_find(received, MESSAGE_KEY_NAUTICAL_DUSK_HOURS);
    Tuple *nautical_dusk_minutes_tuple       = dict_find(received, MESSAGE_KEY_NAUTICAL_DUSK_MINUTES);
    Tuple *astronomical_dusk_hours_tuple     = dict_find(received, MESSAGE_KEY_ASTRONOMICAL_DUSK_HOURS);
    Tuple *astronomical_dusk_minutes_tuple   = dict_find(received, MESSAGE_KEY_ASTRONOMICAL_DUSK_MINUTES);
    Tuple *golden_hour_evening_hours_tuple   = dict_find(received, MESSAGE_KEY_GOLDEN_H_EVENING_HOURS);
    Tuple *golden_hour_evening_minutes_tuple = dict_find(received, MESSAGE_KEY_GOLDEN_H_EVENING_MINUTES);    
   
    // Do BZZZ.
    vibes_short_pulse();
    
    solar_offset                   = 0;
    sunrise_time_tm.tm_hour        = 0;
    sunrise_time_tm.tm_min         = 0;
    sunrise_time_solar             = 0.0;
    sunset_time_tm.tm_hour         = 0;
    sunset_time_tm.tm_min          = 0;   
    sunset_time_solar              = 0.0;
    civil_dawn_time_solar          = 0.0;
    nautical_dawn_time_solar       = 0.0;
    astronomical_dawn_time_solar   = 0.0;
    civil_dusk_time_solar          = 0.0;
    nautical_dusk_time_solar       = 0.0;
    astronomical_dusk_time_solar   = 0.0;
    golden_hour_morning_time_solar = 0.0;
    golden_hour_evening_time_solar = 0.0;

    
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Received from phone:");
    
    if (date_tuple && strlen(date_tuple->value->cstring)) {
        snprintf(date_text, sizeof(date_text), "%s", date_tuple->value->cstring);
        //APP_LOG(APP_LOG_LEVEL_DEBUG, "  date: %s", date_text);          
    }
    
    if (temperature_tuple && strlen(temperature_tuple->value->cstring)) {
        snprintf(temperature_text, sizeof(temperature_text), "%s", temperature_tuple->value->cstring);
        //APP_LOG(APP_LOG_LEVEL_DEBUG, "  temperature: %s", temperature_text);   
    }
    
    if (solar_offset_tuple) {         
        solar_offset = solar_offset_tuple->value->int32;
        //APP_LOG(APP_LOG_LEVEL_DEBUG, "  solar offset: %d", (int)solar_offset);
    
        if (sunrise_hours_tuple && sunrise_minutes_tuple) {        
            sunrise_time_tm.tm_hour = sunrise_hours_tuple->value->int32;
            sunrise_time_tm.tm_min  = sunrise_minutes_tuple->value->int32;
            sunrise_time_solar = tm_to_solar_time(sunrise_time_tm, solar_offset);
            APP_LOG(APP_LOG_LEVEL_DEBUG, "  sunrise: %d:%d", (int)sunrise_time_tm.tm_hour, (int)sunrise_time_tm.tm_min);
        }
        if (sunset_hours_tuple && sunset_minutes_tuple) {        
            sunset_time_tm.tm_hour = sunset_hours_tuple->value->int32;
            sunset_time_tm.tm_min  = sunset_minutes_tuple->value->int32;
            sunset_time_solar = tm_to_solar_time(sunset_time_tm, solar_offset);
            APP_LOG(APP_LOG_LEVEL_DEBUG, "  sunset:  %d:%d", (int)sunset_time_tm.tm_hour,  (int)sunset_time_tm.tm_min);
        }
        if (civil_dawn_hours_tuple && civil_dawn_minutes_tuple) {   
            struct tm civil_dawn_time_tm  = { 
                .tm_hour = civil_dawn_hours_tuple->value->int32, 
                .tm_min = civil_dawn_minutes_tuple->value->int32
            };
            civil_dawn_time_solar = tm_to_solar_time(civil_dawn_time_tm, solar_offset);
            //APP_LOG(APP_LOG_LEVEL_DEBUG, "  civil dawn:  %d:%d", (int)civil_dawn_time_tm.tm_hour,  (int)civil_dawn_time_tm.tm_min);
        }
        if (nautical_dawn_hours_tuple && nautical_dawn_minutes_tuple) {   
            struct tm nautical_dawn_time_tm  = { 
                .tm_hour = nautical_dawn_hours_tuple->value->int32, 
                .tm_min = nautical_dawn_minutes_tuple->value->int32
            };
            nautical_dawn_time_solar = tm_to_solar_time(nautical_dawn_time_tm, solar_offset);
            //APP_LOG(APP_LOG_LEVEL_DEBUG, "  nautical dawn:  %d:%d", (int)nautical_dawn_time_tm.tm_hour,  (int)nautical_dawn_time_tm.tm_min);
        }
        if (astronomical_dawn_hours_tuple && astronomical_dawn_minutes_tuple) {   
            struct tm astronomical_dawn_time_tm  = { 
                .tm_hour = astronomical_dawn_hours_tuple->value->int32, 
                .tm_min = astronomical_dawn_minutes_tuple->value->int32
            };
            astronomical_dawn_time_solar = tm_to_solar_time(astronomical_dawn_time_tm, solar_offset);
            //APP_LOG(APP_LOG_LEVEL_DEBUG, "  astronomical dawn:  %d:%d", (int)astronomical_dawn_time_tm.tm_hour,  (int)astronomical_dawn_time_tm.tm_min);
        }
        if (civil_dusk_hours_tuple && civil_dusk_minutes_tuple) {   
            struct tm civil_dusk_time_tm  = { 
                .tm_hour = civil_dusk_hours_tuple->value->int32, 
                .tm_min = civil_dusk_minutes_tuple->value->int32
            };
            civil_dusk_time_solar = tm_to_solar_time(civil_dusk_time_tm, solar_offset);
            //APP_LOG(APP_LOG_LEVEL_DEBUG, "  civil dusk:  %d:%d", (int)civil_dusk_time_tm.tm_hour,  (int)civil_dusk_time_tm.tm_min);
        }
        if (nautical_dusk_hours_tuple && nautical_dusk_minutes_tuple) {   
            struct tm nautical_dusk_time_tm  = { 
                .tm_hour = nautical_dusk_hours_tuple->value->int32, 
                .tm_min = nautical_dusk_minutes_tuple->value->int32
            };
            nautical_dusk_time_solar = tm_to_solar_time(nautical_dusk_time_tm, solar_offset);
            //APP_LOG(APP_LOG_LEVEL_DEBUG, "  nautical dusk:  %d:%d", (int)nautical_dusk_time_tm.tm_hour,  (int)nautical_dusk_time_tm.tm_min);
        }
        if (astronomical_dusk_hours_tuple && astronomical_dusk_minutes_tuple) {   
            struct tm astronomical_dusk_time_tm  = { 
                .tm_hour = astronomical_dusk_hours_tuple->value->int32, 
                .tm_min = astronomical_dusk_minutes_tuple->value->int32
            };
            astronomical_dusk_time_solar = tm_to_solar_time(astronomical_dusk_time_tm, solar_offset);
            //APP_LOG(APP_LOG_LEVEL_DEBUG, "  astronomical dusk:  %d:%d", (int)astronomical_dusk_time_tm.tm_hour,  (int)astronomical_dusk_time_tm.tm_min);
        }
        if (golden_hour_morning_hours_tuple && golden_hour_morning_minutes_tuple) {
            struct tm golden_hour_morning_time_tm  = { 
                .tm_hour = golden_hour_morning_hours_tuple->value->int32, 
                .tm_min  = golden_hour_morning_minutes_tuple->value->int32
            };
            //APP_LOG(APP_LOG_LEVEL_DEBUG, "  golden hour morning:  %d:%d", (int)golden_hour_morning_time_tm.tm_hour,  (int)golden_hour_morning_time_tm.tm_min);
            golden_hour_morning_time_solar = tm_to_solar_time(golden_hour_morning_time_tm, solar_offset);
        }
        if (golden_hour_evening_hours_tuple && golden_hour_evening_minutes_tuple) {
            struct tm golden_hour_evening_time_tm  = { 
                .tm_hour = golden_hour_evening_hours_tuple->value->int32, 
                .tm_min  = golden_hour_evening_minutes_tuple->value->int32
            };
            //APP_LOG(APP_LOG_LEVEL_DEBUG, "  golden hour evening:  %d:%d", (int)golden_hour_evening_time_tm.tm_hour,  (int)golden_hour_evening_time_tm.tm_min);
            golden_hour_evening_time_solar = tm_to_solar_time(golden_hour_evening_time_tm, solar_offset); 
        }
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

void app_connection_handler(bool connected) {
    //APP_LOG(APP_LOG_LEVEL_INFO, "Pebble app is %sconnected", connected ? "" : "dis");
    current_connection_available = connected;
    if (!current_connection_available) {
        vibes_short_pulse();
    }
    layer_mark_dirty(minute_layer);
}

// Event fires once, before the obstruction appears or disappears
// static void unobstructed_will_change(GRect final_unobstructed_screen_area, void *context) {
  
//   APP_LOG(APP_LOG_LEVEL_DEBUG,
//     "unobstructed_will_change(), available screen area: width: %d, height: %d",
//     final_unobstructed_screen_area.size.w,
//     final_unobstructed_screen_area.size.h);
// }

// Event fires frequently, while obstruction is appearing or disappearing
// static void unobstructed_change(AnimationProgress progress, void *context) {
//     APP_LOG(APP_LOG_LEVEL_DEBUG, "unobstructed_change(), progress %d", (int)progress);
// }

// Event fires once, after obstruction appears or disappears
// static void unobstructed_did_change(void *context) {
//     APP_LOG(APP_LOG_LEVEL_DEBUG, "unobstructed_did_change()");
//     // Keep track if the screen is obstructed or not
//     screen_is_obstructed = !screen_is_obstructed;
// }

static void window_load(Window *window) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "window_load()");
    Layer *window_layer = window_get_root_layer(window);
    GRect bounds = layer_get_bounds(window_layer);
    GRect unobstructed_bounds = layer_get_unobstructed_bounds(window_layer);
    
    // Determine if the screen is obstructed when the app starts
    screen_is_obstructed = !grect_equal(&bounds, &unobstructed_bounds);
    
//     UnobstructedAreaHandlers handlers = {
//         .will_change = unobstructed_will_change,
//         .change = unobstructed_change,
//         .did_change = unobstructed_did_change
//     };
//     unobstructed_area_service_subscribe(handlers, NULL);
    
    // stuff that changes once per day
    base_layer = layer_create(bounds);
    layer_set_update_proc(base_layer, &base_layer_update_proc);
    layer_add_child(window_layer, base_layer);
    
    // stuff that (potentially) needs to redraw every minute
    minute_layer = layer_create(bounds);
    layer_set_update_proc(minute_layer, &minute_layer_update_proc);
    layer_add_child(window_layer, minute_layer);

    // display battery status
    services_layer = layer_create(bounds);
    layer_set_update_proc(services_layer, &services_layer_update_proc);
    layer_add_child(window_layer, services_layer);
}

static void window_unload(Window *window) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "window_unload()");
    unobstructed_area_service_unsubscribe();
}

static void update_battery_percentage(BatteryChargeState c) {
    if (setting_battery_status) {
        int battery_level_int = c.charge_percent;
        snprintf(battery_level_string, 5, "%d%%", battery_level_int);
        layer_mark_dirty(services_layer);
    }
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
    uint16_t result = app_message_open(2048, APP_MESSAGE_OUTBOX_SIZE_MINIMUM);
    //APP_LOG(APP_LOG_LEVEL_DEBUG, "app_message_open(): %d", result);
    
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
            
    // get the _actual_ battery state (global variables set it up as if it were 100%).
    update_battery_percentage(battery_state_service_peek());

    current_connection_available = connection_service_peek_pebble_app_connection();
    
    if (clock_is_24h_style()) {
        time_format = "%H:%M";
    } else {
        time_format = "%l:%M";
    }
}

static void deinit(void) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "deinit()");
    
    layer_remove_from_parent(services_layer); 
    layer_remove_from_parent(minute_layer);
    layer_remove_from_parent(base_layer);
    
    layer_destroy(services_layer);
    layer_destroy(minute_layer);   
    layer_destroy(base_layer);
    
    window_destroy(window);
}

int main(void) {
    init();
    
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", window);
    
    app_event_loop();
    deinit();
}