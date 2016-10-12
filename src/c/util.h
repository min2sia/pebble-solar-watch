#define M_PI 3.141592653589793
#define white_space(c) ((c) == ' ' || (c) == '\t')
#define valid_digit(c) ((c) >= '0' && (c) <= '9')

float my_sqrt(const float x);
float my_floor(float x); 
int my_abs(int in);
double round(double number);
double myatof (const char *p);
void ftoa(char* str, double val, int precision);
float tm_to_solar_time(struct tm in_tm, int16_t in_solar_offset);