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

bool js_ready;

static bool screen_is_obstructed = false;
time_t wall_time_t;
int16_t solar_offset = 0;
int16_t current_sunrise_sunset_day = -1;

// times in wall clock scale
struct tm wall_time_tm    = { .tm_hour = 0, .tm_min = 0 };
struct tm solar_time_tm   = { .tm_hour = 0, .tm_min = 0 };
struct tm sunrise_time_tm = { .tm_hour = 0, .tm_min = 0 };
struct tm sunset_time_tm  = { .tm_hour = 0, .tm_min = 0 };

// times in solar time scale (solar offset applied)
// number of seconds since midnight
uint32_t sunrise_time_solar             = 0;
uint32_t sunset_time_solar              = 0;
uint32_t civil_dawn_time_solar          = 0;
uint32_t nautical_dawn_time_solar       = 0;
uint32_t astronomical_dawn_time_solar   = 0;
uint32_t civil_dusk_time_solar          = 0;
uint32_t nautical_dusk_time_solar       = 0;
uint32_t astronomical_dusk_time_solar   = 0;
uint32_t golden_hour_morning_time_solar = 0;
uint32_t golden_hour_evening_time_solar = 0;

int16_t polar_day_night = 0;

int16_t current_battery_charge = -1;
char *battery_level_string = "...";
char date_text[] = " ...  "; //
char temperature_text[] = " ... ";
char *time_format;

bool current_connection_available = false;
bool current_location_available = false;

GFont font_solar_time;
GFont font_clock_time;

GColor daytime_color;
GColor nighttime_color;
GColor civil_twilight_color;
GColor nautical_twilight_color;
GColor astronomical_twilight_color;
GColor golden_hour_color;

// Vibe pattern: ON for X ms, OFF for Y ms, ON for W ms, etc.:
static const uint32_t segments_short[] = {50};
VibePattern vibePattern_short = {
    .durations = segments_short,
    .num_segments = ARRAY_LENGTH(segments_short),
};
static const uint32_t segments_short_x2[] = {50, 100, 50};
VibePattern vibePattern_short_x2 = {
    .durations = segments_short_x2,
    .num_segments = ARRAY_LENGTH(segments_short_x2),
};
static const uint32_t segments_haribol[] = {100, 400, 100, 400, 400, 600,   100, 400, 100, 400, 400, 600,   100, 400, 100, 400, 400, 600,   100, 400, 100, 400, 400, 600};
VibePattern vibePattern_haribol = {
    .durations = segments_haribol,
    .num_segments = ARRAY_LENGTH(segments_haribol),
};
static const uint32_t segments_sunrise[] = {50, 500, 50, 400, 50, 300, 50, 200, 50, 100, 50};
VibePattern vibePattern_sunrise = {
    .durations = segments_sunrise,
    .num_segments = ARRAY_LENGTH(segments_sunrise),
};
static const uint32_t segments_sunset[] = {50, 100, 50, 200, 50, 300, 50, 400, 50, 500, 50};
VibePattern vibePattern_sunset = {
    .durations = segments_sunset,
    .num_segments = ARRAY_LENGTH(segments_sunset),
};
static const uint32_t segments_noon[] = {50, 400, 50, 100, 50, 100, 50};
VibePattern vibePattern_noon = {
    .durations = segments_noon,
    .num_segments = ARRAY_LENGTH(segments_noon),
};


void send_request(int command) {
  Tuplet command_tuple = TupletInteger(0 /*KEY_COMMAND*/ , command);
  Tuplet index_tuple = TupletInteger(1 /*KEY_INDEX*/, 0 /*entryIndex*/);
  DictionaryIterator *iter;

    if (js_ready) {
        AppMessageResult result = app_message_outbox_begin(&iter);
        if (result == APP_MSG_OK) {
            dict_write_tuplet(iter, &command_tuple);
            dict_write_tuplet(iter, &index_tuple);
            dict_write_end(iter);
            result = app_message_outbox_send();
            if (result != APP_MSG_OK) {
//                 APP_LOG(APP_LOG_LEVEL_ERROR, "Error sending the outbox: %d - s", (int)result, translate_AppMessageResult(result));
            }
        } else {
//             APP_LOG(APP_LOG_LEVEL_ERROR, "Error preparing the outbox: %d - %s", (int)result, translate_AppMessageResult(result));
        }
    } else {
//         APP_LOG(APP_LOG_LEVEL_DEBUG, "Not sending message - JS not ready");
    }
}

static void set_solar_time() {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "set_solar_time()");

    wall_time_t         = time(NULL);
    time_t solar_time_t = wall_time_t + solar_offset;
    solar_time_tm       = *localtime(&solar_time_t);

    //char solar_time_txt[5];
    //ftoa(solar_time_txt, solar_time_t, 2);
    //APP_LOG(APP_LOG_LEVEL_DEBUG, "sunrise_time: %s,  sunset_time: %s (solar time)", solar_time_txt, sunset_time_txt);
}

static void handle_time_tick(struct tm *tick_time, TimeUnits units_changed) {
    wall_time_t  = time(NULL);
    wall_time_tm = *localtime(&wall_time_t);
    set_solar_time();

    // Play Brahma muhurta tune
//     if (solar_time_tm.tm_hour == 4 && solar_time_tm.tm_min == 24) {
//         vibes_enqueue_custom_pattern(vibePattern_haribol);
//     }

    // Play sunrise tune
    if (sunrise_time_tm.tm_hour > 0 && sunrise_time_tm.tm_min > 0 &&
        wall_time_tm.tm_hour == sunrise_time_tm.tm_hour &&
        wall_time_tm.tm_min  == sunrise_time_tm.tm_min) {

        vibes_enqueue_custom_pattern(vibePattern_sunrise);
    }

    // Play sunset tune
    if (sunrise_time_tm.tm_hour > 0 && sunrise_time_tm.tm_min > 0 &&
        wall_time_tm.tm_hour == sunrise_time_tm.tm_hour &&
        wall_time_tm.tm_min  == sunrise_time_tm.tm_min) {

        vibes_enqueue_custom_pattern(vibePattern_sunset);
    }

    // Play noon tune
    if (solar_time_tm.tm_hour == 12 && solar_time_tm.tm_min == 0 ) {
        vibes_enqueue_custom_pattern(vibePattern_noon);
    }

    APP_LOG(APP_LOG_LEVEL_DEBUG, "handle_time_tick() %d:%d", (int)wall_time_tm.tm_hour, (int)wall_time_tm.tm_min);

    if (/*wall_time_tm.tm_sec == 0 && */
        (wall_time_tm.tm_min == 0 ||
         //wall_time_tm.tm_min == 10 ||
         wall_time_tm.tm_min == 20 ||
         //wall_time_tm.tm_min == 30 ||
         wall_time_tm.tm_min == 40
         // || wall_time_tm.tm_min == 50
        )) {
        //APP_LOG(APP_LOG_LEVEL_DEBUG, "  ping");
        send_request(0); // send blank request to trigger data refresh from phone JS
    }

    layer_mark_dirty(minute_layer);
}

void in_received_handler(DictionaryIterator *received, void *ctx) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "----------------------- in_received_handler() ---------------------------------------------");
    Tuple *js_ready_tuple                    = dict_find(received, MESSAGE_KEY_JSREADY);
    Tuple *date_tuple                        = dict_find(received, MESSAGE_KEY_DATE);
    Tuple *location_available_tuple          = dict_find(received, MESSAGE_KEY_LOCATION_AVAILABLE);
    Tuple *temperature_tuple                 = dict_find(received, MESSAGE_KEY_TEMPERATURE);
    Tuple *solar_offset_tuple                = dict_find(received, MESSAGE_KEY_SOLAR_OFFSET);
    Tuple *w_sunrise_hours_tuple             = dict_find(received, MESSAGE_KEY_W_SUNRISE_HOURS);
    Tuple *w_sunrise_minutes_tuple           = dict_find(received, MESSAGE_KEY_W_SUNRISE_MINUTES);
    Tuple *sunrise_hours_tuple               = dict_find(received, MESSAGE_KEY_SUNRISE_HOURS);
    Tuple *sunrise_minutes_tuple             = dict_find(received, MESSAGE_KEY_SUNRISE_MINUTES);
    Tuple *civil_dawn_hours_tuple            = dict_find(received, MESSAGE_KEY_CIVIL_DAWN_HOURS);
    Tuple *civil_dawn_minutes_tuple          = dict_find(received, MESSAGE_KEY_CIVIL_DAWN_MINUTES);
    Tuple *nautical_dawn_hours_tuple         = dict_find(received, MESSAGE_KEY_NAUTICAL_DAWN_HOURS);
    Tuple *nautical_dawn_minutes_tuple       = dict_find(received, MESSAGE_KEY_NAUTICAL_DAWN_MINUTES);
    Tuple *astronomical_dawn_hours_tuple     = dict_find(received, MESSAGE_KEY_ASTRONOMICAL_DAWN_HOURS);
    Tuple *astronomical_dawn_minutes_tuple   = dict_find(received, MESSAGE_KEY_ASTRONOMICAL_DAWN_MINUTES);
//     Tuple *golden_hour_morning_hours_tuple   = dict_find(received, MESSAGE_KEY_GOLDEN_H_MORNING_HOURS);
//     Tuple *golden_hour_morning_minutes_tuple = dict_find(received, MESSAGE_KEY_GOLDEN_H_MORNING_MINUTES);
    Tuple *w_sunset_hours_tuple              = dict_find(received, MESSAGE_KEY_W_SUNSET_HOURS);
    Tuple *w_sunset_minutes_tuple            = dict_find(received, MESSAGE_KEY_W_SUNSET_MINUTES);
    Tuple *sunset_hours_tuple                = dict_find(received, MESSAGE_KEY_SUNSET_HOURS);
    Tuple *sunset_minutes_tuple              = dict_find(received, MESSAGE_KEY_SUNSET_MINUTES);
    Tuple *civil_dusk_hours_tuple            = dict_find(received, MESSAGE_KEY_CIVIL_DUSK_HOURS);
    Tuple *civil_dusk_minutes_tuple          = dict_find(received, MESSAGE_KEY_CIVIL_DUSK_MINUTES);
    Tuple *nautical_dusk_hours_tuple         = dict_find(received, MESSAGE_KEY_NAUTICAL_DUSK_HOURS);
    Tuple *nautical_dusk_minutes_tuple       = dict_find(received, MESSAGE_KEY_NAUTICAL_DUSK_MINUTES);
    Tuple *astronomical_dusk_hours_tuple     = dict_find(received, MESSAGE_KEY_ASTRONOMICAL_DUSK_HOURS);
    Tuple *astronomical_dusk_minutes_tuple   = dict_find(received, MESSAGE_KEY_ASTRONOMICAL_DUSK_MINUTES);
//     Tuple *golden_hour_evening_hours_tuple   = dict_find(received, MESSAGE_KEY_GOLDEN_H_EVENING_HOURS);
//     Tuple *golden_hour_evening_minutes_tuple = dict_find(received, MESSAGE_KEY_GOLDEN_H_EVENING_MINUTES);
    Tuple *polar_day_night_tuple             = dict_find(received, MESSAGE_KEY_POLAR_DAY_NIGHT);


//     APP_LOG(APP_LOG_LEVEL_DEBUG, "Received from phone:");

    if (js_ready_tuple) {
        js_ready = true;
        APP_LOG(APP_LOG_LEVEL_DEBUG, "  PebbleKit JS is ready");
    }

    if (date_tuple && strlen(date_tuple->value->cstring)) {
        if (strcmp(date_text, date_tuple->value->cstring) != 0) {
            snprintf(date_text, sizeof(date_text), "%s", date_tuple->value->cstring);
            APP_LOG(APP_LOG_LEVEL_DEBUG, "  new date: %s", date_text);
            if (sunrise_time_solar > 0.0 && sunset_time_solar > 0.0) {
                layer_mark_dirty(base_layer);
            }
        }
    }

    if (location_available_tuple) {
        APP_LOG(APP_LOG_LEVEL_DEBUG, "  location_available: %d", (int)location_available_tuple->value->uint8);
        if (current_location_available != location_available_tuple->value->uint8) {
            current_location_available  = location_available_tuple->value->uint8;
            layer_mark_dirty(services_layer);
        }
    }
    if (temperature_tuple && strlen(temperature_tuple->value->cstring)) {
        if (strcmp(temperature_text, temperature_tuple->value->cstring) != 0) {
            snprintf(temperature_text, sizeof(temperature_text), "%s", temperature_tuple->value->cstring);
            APP_LOG(APP_LOG_LEVEL_DEBUG, "  temperature: %s", temperature_text);
            layer_mark_dirty(services_layer);
        }
    }

    if (solar_offset_tuple) {
        if (solar_offset != solar_offset_tuple->value->int32) {
            solar_offset  = solar_offset_tuple->value->int32;
            APP_LOG(APP_LOG_LEVEL_DEBUG, "  solar offset: %d", (int)solar_offset);
            //vibes_enqueue_custom_pattern(vibePattern2);
        }
        set_solar_time();
    }

    //sunrise_time_tm.tm_hour = 0;
    //sunrise_time_tm.tm_min = 0;
    //sunset_time_tm.tm_hour = 0;
    //sunset_time_tm.tm_min = 0;
    if (w_sunrise_hours_tuple && w_sunrise_minutes_tuple && w_sunset_hours_tuple && w_sunset_minutes_tuple) {
        sunrise_time_tm.tm_hour = w_sunrise_hours_tuple->value->int32;
        sunrise_time_tm.tm_min  = w_sunrise_minutes_tuple->value->int32;
        sunset_time_tm.tm_hour  = w_sunset_hours_tuple->value->int32;
        sunset_time_tm.tm_min   = w_sunset_minutes_tuple->value->int32;
        APP_LOG(APP_LOG_LEVEL_DEBUG, "  wall sunrise:  %d:%d, sunset: %d:%d", (int)sunrise_time_tm.tm_hour, (int)sunrise_time_tm.tm_min, (int)sunset_time_tm.tm_hour, (int)sunset_time_tm.tm_min);
    }

    if (sunrise_hours_tuple && sunrise_minutes_tuple && sunset_hours_tuple && sunset_minutes_tuple) {
        int ssrh = (int)sunrise_hours_tuple->value->int32;
        int ssrm = (int)sunrise_minutes_tuple->value->int32;
        int sssh = (int)sunset_hours_tuple->value->int32;
        int sssm = (int)sunset_minutes_tuple->value->int32;
        APP_LOG(APP_LOG_LEVEL_DEBUG, "  solar sunrise:  %d:%d, sunset: %d:%d", ssrh, ssrm, sssh, sssm);

        uint32_t old_sunrise_time_solar = sunrise_time_solar;
        uint32_t old_sunset_time_solar  = sunset_time_solar;

        sunrise_time_solar = hm_to_time(sunrise_hours_tuple, sunrise_minutes_tuple);
        sunset_time_solar  = hm_to_time(sunset_hours_tuple,  sunset_minutes_tuple);
        if (sunrise_time_solar != old_sunrise_time_solar || sunset_time_solar != old_sunset_time_solar) {
            layer_mark_dirty(base_layer);
        }
    }

    //civil_dawn_time_solar = 0;
    //civil_dusk_time_solar = 0;
    if (civil_dawn_hours_tuple && civil_dawn_minutes_tuple && civil_dusk_hours_tuple && civil_dusk_minutes_tuple) {
        int cdah = (int)civil_dawn_hours_tuple->value->int32;
        int cdam = (int)civil_dawn_minutes_tuple->value->int32;
        int cduh = (int)civil_dusk_hours_tuple->value->int32;
        int cdum = (int)civil_dusk_minutes_tuple->value->int32;
        APP_LOG(APP_LOG_LEVEL_DEBUG, "  civil dawn:  %d:%d, dusk: %d:%d", cdah, cdam, cduh, cdum);
        //APP_LOG(APP_LOG_LEVEL_DEBUG, "  civil dawn:  %d:%d, dusk: %d:%d", (int)civil_dawn_hours_tuple->value->int32,  (int)civil_dawn_hours_tuple->value->int32, (int)civil_dusk_time_solar->value->int32,  (int)civil_dusk_time_solar->value->int32);
        civil_dawn_time_solar = hm_to_time(civil_dawn_hours_tuple, civil_dawn_minutes_tuple);
        civil_dusk_time_solar = hm_to_time(civil_dusk_hours_tuple, civil_dusk_minutes_tuple);
    }

    //nautical_dawn_time_solar = 0;
    //nautical_dusk_time_solar = 0;
    if (nautical_dawn_hours_tuple && nautical_dawn_minutes_tuple && nautical_dusk_hours_tuple && nautical_dusk_minutes_tuple) {
        int ndah = (int)nautical_dawn_hours_tuple->value->int32;
        int ndam = (int)nautical_dawn_minutes_tuple->value->int32;
        int nduh = (int)nautical_dusk_hours_tuple->value->int32;
        int ndum = (int)nautical_dusk_minutes_tuple->value->int32;
        APP_LOG(APP_LOG_LEVEL_DEBUG, "  nautical dawn:  %d:%d, dusk: %d:%d", ndah, ndam, nduh, ndum);
        //APP_LOG(APP_LOG_LEVEL_DEBUG, "  nautical dawn:  %d:%d, dusk: %d:%d", (int)nautical_dawn_hours_tuple->value->int32,  (int)nautical_dawn_hours_tuple->value->int32, (int)nautical_dusk_time_solar->value->int32,  (int)nautical_dusk_time_solar->value->int32);
        nautical_dawn_time_solar = hm_to_time(nautical_dawn_hours_tuple, nautical_dawn_minutes_tuple);
        nautical_dusk_time_solar = hm_to_time(nautical_dusk_hours_tuple, nautical_dusk_minutes_tuple);
    }

    //astronomical_dawn_time_solar = 0;
    //astronomical_dusk_time_solar= 0;
    if (astronomical_dawn_hours_tuple && astronomical_dawn_minutes_tuple && astronomical_dusk_hours_tuple && astronomical_dusk_minutes_tuple) {
        int adah = (int)astronomical_dawn_hours_tuple->value->int32;
        int adam = (int)astronomical_dawn_minutes_tuple->value->int32;
        int aduh = (int)astronomical_dusk_hours_tuple->value->int32;
        int adum = (int)astronomical_dusk_minutes_tuple->value->int32;
        APP_LOG(APP_LOG_LEVEL_DEBUG, "  astronomical dawn:  %d:%d, dusk: %d:%d", adah, adam, aduh, adum);

        astronomical_dawn_time_solar = hm_to_time(astronomical_dawn_hours_tuple, astronomical_dawn_minutes_tuple);
        astronomical_dusk_time_solar = hm_to_time(astronomical_dusk_hours_tuple, astronomical_dusk_minutes_tuple);
    }

//         if (golden_hour_morning_hours_tuple && golden_hour_morning_minutes_tuple) {
//             struct tm golden_hour_morning_time_tm  = {
//                 .tm_hour = golden_hour_morning_hours_tuple->value->int32,
//                 .tm_min  = golden_hour_morning_minutes_tuple->value->int32
//             };
//             //APP_LOG(APP_LOG_LEVEL_DEBUG, "  golden hour morning:  %d:%d", (int)golden_hour_morning_time_tm.tm_hour,  (int)golden_hour_morning_time_tm.tm_min);
//             golden_hour_morning_time_solar = tm_to_solar_time(golden_hour_morning_time_tm, solar_offset);
//         }
//         if (golden_hour_evening_hours_tuple && golden_hour_evening_minutes_tuple) {
//             struct tm golden_hour_evening_time_tm  = {
//                 .tm_hour = golden_hour_evening_hours_tuple->value->int32,
//                 .tm_min  = golden_hour_evening_minutes_tuple->value->int32
//             };
//             //APP_LOG(APP_LOG_LEVEL_DEBUG, "  golden hour evening:  %d:%d", (int)golden_hour_evening_time_tm.tm_hour,  (int)golden_hour_evening_time_tm.tm_min);
//             golden_hour_evening_time_solar = tm_to_solar_time(golden_hour_evening_time_tm, solar_offset);
//         }

      if (polar_day_night_tuple) {
          polar_day_night = polar_day_night_tuple->value->int16;
      }
}

void in_dropped_handler(AppMessageResult reason, void *context) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Watch dropped data.");
}

void out_sent_handler(DictionaryIterator *sent, void *ctx) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Message sent to phone.");
}

void out_failed_handler(DictionaryIterator *failed, AppMessageResult reason, void *ctx) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Message FAILED to send to phone. Reason: %i - %s", (int)reason, translate_AppMessageResult(reason));
}

void app_connection_handler(bool connected) {
    APP_LOG(APP_LOG_LEVEL_INFO, "Pebble app is %sconnected", connected ? "" : "dis");
    current_connection_available = connected;
    if (!connected) {
        current_location_available = false;
    }
    vibes_enqueue_custom_pattern(vibePattern_short);
    if (current_connection_available) {
        send_request(0); // blank request to trigger data refresh from phone JS
    }
    layer_mark_dirty(services_layer);
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

    // display battery status, temperature, location service status
    services_layer = layer_create(bounds);
    layer_set_update_proc(services_layer, &services_layer_update_proc);
    layer_add_child(window_layer, services_layer);
}

static void window_unload(Window *window) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "window_unload()");
    unobstructed_area_service_unsubscribe();
}

static void update_battery_percentage(BatteryChargeState c) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "update_battery_percentage()");
    int battery_level_int = c.charge_percent;
    snprintf(battery_level_string, 5, "%d%%", battery_level_int);
    layer_mark_dirty(services_layer);
}

static void init(void) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "init()");

    wall_time_t  = time(NULL);
    wall_time_tm = *localtime(&wall_time_t);

    app_message_register_inbox_received(in_received_handler);
    app_message_register_inbox_dropped(in_dropped_handler);
    app_message_register_outbox_sent(out_sent_handler);
    app_message_register_outbox_failed(out_failed_handler);

    daytime_color               = GColorWhite;
    nighttime_color             = GColorBlack;
    civil_twilight_color        = GColorBabyBlueEyes;
    nautical_twilight_color     = GColorElectricUltramarine;
    astronomical_twilight_color = GColorDukeBlue;
    golden_hour_color           = GColorRajah;
//     if (persist_exists(0)) {
//         version = persist_read_int(0);
//     }
//     if (persist_exists(1)) {
//         timestamp = persist_read_int(1);
//     }
    if (persist_exists(2)) {
        sunrise_time_solar = persist_read_int(2);
    }
    if (persist_exists(3)) {
        sunset_time_solar = persist_read_int(3);
    }
    if (persist_exists(4)) {
        civil_dawn_time_solar = persist_read_int(4);
    }
    if (persist_exists(5)) {
        nautical_dawn_time_solar = persist_read_int(5);
    }
    if (persist_exists(6)) {
        astronomical_dawn_time_solar = persist_read_int(6);
    }
    if (persist_exists(7)) {
        civil_dusk_time_solar = persist_read_int(7);
    }
    if (persist_exists(8)) {
        nautical_dusk_time_solar = persist_read_int(8);
    }
    if (persist_exists(9)) {
        astronomical_dusk_time_solar = persist_read_int(9);
    }
    if (persist_exists(10)) {
        golden_hour_morning_time_solar = persist_read_int(10);
    }
    if (persist_exists(11)) {
        golden_hour_evening_time_solar = persist_read_int(11);
    }
    if (persist_exists(12)) {
        solar_offset = persist_read_int(12);
    }
    if (persist_exists(13)) {
        polar_day_night = persist_read_int(13);
    }

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
    uint16_t result = app_message_open(1024,  APP_MESSAGE_OUTBOX_SIZE_MINIMUM);
//     APP_LOG(APP_LOG_LEVEL_DEBUG, "app_message_open(): %d", result);

    window = window_create();
    window_set_window_handlers(window, (WindowHandlers) {
        .load = window_load,
        .unload = window_unload,});
    const bool animated = true;

    window_set_background_color(window, GColorBlack);

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
//     APP_LOG(APP_LOG_LEVEL_DEBUG, "deinit()");

    layer_remove_from_parent(services_layer);
    layer_remove_from_parent(minute_layer);
    layer_remove_from_parent(base_layer);

    layer_destroy(services_layer);
    layer_destroy(minute_layer);
    layer_destroy(base_layer);

    window_destroy(window);
    
    persist_write_int(0,  0);
    persist_write_int(1,  time(NULL));
    persist_write_int(2,  sunrise_time_solar);    
    persist_write_int(3,  sunset_time_solar);
    persist_write_int(4,  civil_dawn_time_solar);
    persist_write_int(5,  nautical_dawn_time_solar);
    persist_write_int(6,  astronomical_dawn_time_solar);
    persist_write_int(7,  civil_dusk_time_solar);
    persist_write_int(8,  nautical_dusk_time_solar);
    persist_write_int(9,  astronomical_dusk_time_solar);
    persist_write_int(10, golden_hour_morning_time_solar);
    persist_write_int(11, golden_hour_evening_time_solar);
    persist_write_int(12, solar_offset);
    persist_write_int(13, polar_day_night);       
}

int main(void) {
    init();

    APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", window);

    app_event_loop();
    deinit();
}