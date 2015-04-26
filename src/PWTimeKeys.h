//
//  PWTimeKeys.h
//  PebbleWorldTime
//
//  Created by Don Krause on 6/3/13.
//  Copyright (c) 2013 Don Krause. All rights reserved.
//

#ifndef PebbleWorldTime_PWTimeKeys_h
#define PebbleWorldTime_PWTimeKeys_h

// Keys and values for messages from iOS to the Pebble

// Factors, the keys are actually grouped by 16, depending on the watch to update
// Add these to the KEYs to get the actual value
#define LOCAL_WATCH_OFFSET                  0x00
#define TZ1_WATCH_OFFSET                    0x10
#define TZ2_WATCH_OFFSET                    0x20

// Keys for local time data, maximum of 16, including 0x00
#define PBCOMM_GMT_SEC_OFFSET_KEY           0x02    // number of seconds before or after GMT
#define PBCOMM_CITY_KEY                     0x03    // string with city name
#define PBCOMM_BACKGROUND_KEY               0x04    // light, dark or Sunrise/Sunset (AM/PM) background
#define PBCOMM_12_24_DISPLAY_KEY            0x05    // display in watch-configured-, 12- or 24-hour time
#define PBCOMM_WEATHER_KEY                  0x06    // weather conditions icon, temps, suns times
#define KEYS_PER_WATCH                      0x10    // maximum number of keys/watch

// Values for PBCOMMM_BACKGROUND_KEY
#define BACKGROUND_DARK                     0x00    // Dark background
#define BACKGROUND_LIGHT                    0x01    // Light background
#define BACKGROUND_SUNS                     0x02    // Light for Sunrise (or AM), dark for Sunset (or PM)

// Values for PBCOMM_12_24_DISPLAY_KEY
#define DISPLAY_WATCH_CONFIG_TIME           0x00    // Show 12- or 24-hour time as configured on watch
#define DISPLAY_12_HOUR_TIME                0x01    // Show 12-hour time
#define DISPLAY_24_HOUR_TIME                0x02    // Show 24-hour time

#define MAX_WEATHER_DAYS                    3       // Number of days of weather to show

// Offsets in PBCOMM_WEATHER_KEY data
#define WEATHER_ICONS                       0       // Three icons (today-2 days from now)
#define CURRENT_TEMP                        3       // Current temperature
#define MAX_TEMPS                           4       // Three highs (today-2 days from now)
#define MIN_TEMPS                           7       // Three lows (today-2 days from now)
#define SUNRISE_HOUR                        10      // Sunrise hour (today, used for background)
#define SUNRISE_MINUTE                      11      // Sunrise minute (today, used for background)
#define SUNSET_HOUR                         12      // Sunset hour (today, used for background)
#define SUNSET_MINUTE                       13      // Sunset minute (today, used for background)
#define WEATHER_KEY_LEN                     14      // Number of bytes in PBCOMM_WEATHER_KEY data

// Values for PBCOMM_WEATHER_KEY
// Map directly to forecast.io weather icon values
#define WEATHER_UNKNOWN                     0x00
#define WEATHER_CLEAR_DAY                   0x01
#define WEATHER_CLEAR_NIGHT                 0x02
#define WEATHER_RAIN                        0x03
#define WEATHER_SNOW                        0x04
#define WEATHER_SLEET                       0x05
#define WEATHER_WIND                        0x06
#define WEATHER_FOG                         0x07
#define WEATHER_CLOUDY                      0x08
#define WEATHER_PARTLY_CLOUDY_DAY           0x09
#define WEATHER_PARTLY_CLOUDY_NIGHT         0x0A
#define MAX_WEATHER_CONDITIONS              0x0B    // Number of weather conditions

#endif
