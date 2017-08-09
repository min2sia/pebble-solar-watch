#include <pebble.h>

extern Layer *day_layer;
extern Layer *minute_layer;

extern bool js_ready;

extern int16_t solar_offset;

extern struct tm wall_time_tm;
extern struct tm solar_time_tm;
extern struct tm sunrise_time_tm;
extern struct tm sunset_time_tm;

extern uint32_t sunrise_time_solar;    
extern uint32_t sunset_time_solar;
extern uint32_t civil_dawn_time_solar;
extern uint32_t nautical_dawn_time_solar;
extern uint32_t astronomical_dawn_time_solar;
extern uint32_t civil_dusk_time_solar;
extern uint32_t nautical_dusk_time_solar;
extern uint32_t astronomical_dusk_time_solar;
extern uint32_t golden_hour_morning_time_solar;
extern uint32_t golden_hour_evening_time_solar;

extern int16_t polar_day_night;

extern int16_t current_battery_charge;
extern char *battery_level_string;
extern char date_text[];
extern char temperature_text[];

extern bool current_connection_available;
extern bool current_location_available;

extern GFont font_solar_time;
extern GFont font_clock_time;

extern GColor daytime_color;
extern GColor nighttime_color;
extern GColor civil_twilight_color;
extern GColor nautical_twilight_color;
extern GColor astronomical_twilight_color;
extern GColor golden_hour_color;

extern char *time_format;
