#include <pebble.h>

extern Layer *day_layer;
extern Layer *minute_layer;

extern struct tm wall_time_tm;
extern struct tm solar_time_tm;
extern struct tm sunrise_time_tm;
extern struct tm sunset_time_tm;

extern float sunrise_time_solar;    
extern float sunset_time_solar;
extern float civil_dawn_time_solar;
extern float nautical_dawn_time_solar;
extern float astronomical_dawn_time_solar;
extern float civil_dusk_time_solar;
extern float nautical_dusk_time_solar;
extern float astronomical_dusk_time_solar;
extern float golden_hour_morning_time_solar;
extern float golden_hour_evening_time_solar;

extern int16_t current_battery_charge;
extern char *battery_level_string;
extern char date_text[];
extern char temperature_text[];

extern bool current_connection_available;
extern bool current_location_available;

extern bool setting_digital_display;
extern bool setting_hour_numbers;
extern bool setting_battery_status;
extern bool setting_connection_status;
extern bool setting_location_status;

extern GFont font_solar_time;
extern GFont font_clock_time;

extern GColor daytime_color;
extern GColor nighttime_color;

extern char *time_format;
