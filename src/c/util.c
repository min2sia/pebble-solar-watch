/*
 * loosely based on 
 * - http://stackoverflow.com/questions/11261170/c-and-maths-fast-approximation-of-a-trigonometric-function
 * - http://www.codeproject.com/Articles/69941/Best-Square-Root-Method-Algorithm-Function-Precisi
 */
#include <pebble.h>
#include "util.h"

#define SQRT_MAGIC_F 0x5f3759df 
float my_sqrt(const float x)
{
  const float xhalf = 0.5f*x;
 
  union // get bits for floating value
  {
    float x;
    int i;
  } u;
  u.x = x;
  u.i = SQRT_MAGIC_F - (u.i >> 1);  // gives initial guess y0
  return x*u.x*(1.5f - xhalf*u.x*u.x);// Newton step, repeating increases accuracy 
}   

float my_floor(float x) 
{
  return ((int)x);
}

float my_fabs(float x)
{
  if (x<0) return -x;
  return x;
}


int my_abs(int in) {
    if (in < 0) {
        return in * -1;
    }
    return in;
}

double round(double number)
{
    return (number >= 0) ? (int)(number + 0.5) : (int)(number - 0.5);
}

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

// To number of seconds since midnight
uint32_t hm_to_time(Tuple *h, Tuple *m) {
    
    uint32_t ret = (h->value->int32 * SECONDS_PER_HOUR) + (m->value->int32 * SECONDS_PER_MINUTE);
    
    // overflow?
    if (ret  > (int)(HOURS_PER_DAY * SECONDS_PER_HOUR)) { 
        ret -= (HOURS_PER_DAY * SECONDS_PER_HOUR);
    }
    return ret;
}

char *translate_AppMessageResult(AppMessageResult result) {
  switch (result) {
    case APP_MSG_OK: return "APP_MSG_OK";
    case APP_MSG_SEND_TIMEOUT: return "APP_MSG_SEND_TIMEOUT";
    case APP_MSG_SEND_REJECTED: return "APP_MSG_SEND_REJECTED";
    case APP_MSG_NOT_CONNECTED: return "APP_MSG_NOT_CONNECTED";
    case APP_MSG_APP_NOT_RUNNING: return "APP_MSG_APP_NOT_RUNNING";
    case APP_MSG_INVALID_ARGS: return "APP_MSG_INVALID_ARGS";
    case APP_MSG_BUSY: return "APP_MSG_BUSY";
    case APP_MSG_BUFFER_OVERFLOW: return "APP_MSG_BUFFER_OVERFLOW";
    case APP_MSG_ALREADY_RELEASED: return "APP_MSG_ALREADY_RELEASED";
    case APP_MSG_CALLBACK_ALREADY_REGISTERED: return "APP_MSG_CALLBACK_ALREADY_REGISTERED";
    case APP_MSG_CALLBACK_NOT_REGISTERED: return "APP_MSG_CALLBACK_NOT_REGISTERED";
    case APP_MSG_OUT_OF_MEMORY: return "APP_MSG_OUT_OF_MEMORY";
    case APP_MSG_CLOSED: return "APP_MSG_CLOSED";
    case APP_MSG_INTERNAL_ERROR: return "APP_MSG_INTERNAL_ERROR";
    default: return "UNKNOWN ERROR";
  }
}