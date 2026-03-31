// SPDX-FileCopyrightText: 2023 ThingPulse Ltd., https://thingpulse.com
// SPDX-License-Identifier: MIT

#pragma once

// ****************************************************************************
// User settings
// ****************************************************************************
// language settings are in translations/texts_*.h
//#include "translations/texts_en.h"
#include "translations/texts_de.h"
//#include "translations/texts_it.h"
//#include "translations/texts_nl.h"

// WiFi Settings
const char *SSID = "your-wifi-SSID-here";
const char *WIFI_PWD = "your-wifi-password-here";

// timezone Europe/Berlin as per https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv
#define TIMEZONE "CET-1CEST,M3.5.0,M10.5.0/3"

// how often to retreive new weather information
#define UPDATE_INTERVAL_MINUTES 30

// uncomment to get "08/23/2022 02:55:02 pm" instead of "23.08.2022 14:55:02"
// #define DATE_TIME_FORMAT_US

// values in metric or imperial system?
bool IS_METRIC = true;

// OpenWeatherMap Settings
// Sign up here to get an API key: https://docs.thingpulse.com/how-tos/openweathermap-key/
const String OPEN_WEATHER_MAP_API_KEY = "your-openweather-api-key-here";

/*
Go to https://openweathermap.org/find?q= and search for a location. Go through the
result set and select the entry closest to the actual location you want to display
data for. It'll be a URL like https://openweathermap.org/city/2657896. The number
at the end is what you assign to the constant below.
 */
//Define a location struct and list
typedef struct {
  String displayName;
  String locationId;
  const char* timezone;
} LocationDef;

// Define a location list (Display Name, Openweather locationId and ESP32-friendly POSIX Timezone String )
const LocationDef LOCATIONS[] = {
  {"Monheim",   "2866930", "CET-1CEST,M3.5.0,M10.5.0/3"},
  {"Rome",      "3169070", "CET-1CEST,M3.5.0,M10.5.0/3"},
  {"Sydney",    "2147714", "AEST-10AEDT,M10.1.0,M4.1.0/3"},
  {"Anchorage", "5879400", "AKST9AKDT,M3.2.0,M11.1.0"}
};

const uint8_t NUMBER_OF_LOCATIONS = sizeof(LOCATIONS) / sizeof(LOCATIONS[0]);

// screen brightness settings
#define DISPLAY_DYNAMIC_BRIGHTNESS_ENABLED true
#define DISPLAY_NIGHT_MODE_ENABLED         true

// day / evening screen brighness constants
#define TFT_LED_BRIGHTNESS_DAY     200
#define TFT_LED_BRIGHTNESS_EVENING 80
#define TFT_LED_BRIGHTNESS_NIGHT   10

#define DISPLAY_EVENING_FROM_HOUR   18
#define DISPLAY_EVENING_FROM_MINUTE 0

// startup brightness
#define TFT_LED_BRIGHTNESS TFT_LED_BRIGHTNESS_DAY

// fixed night window
#define DISPLAY_NIGHT_FROM_HOUR    0
#define DISPLAY_NIGHT_FROM_MINUTE  0
#define DISPLAY_NIGHT_TO_HOUR      5
#define DISPLAY_NIGHT_TO_MINUTE    0

// test override: force night brightness at any time
#define FORCE_NIGHT_MODE_FOR_TEST false

// touch to retreive settings
#define TOUCH_REFRESH_ENABLED true
#define TOUCH_REFRESH_DEBOUNCE_MS 1000

// settings for feels like temp
#define WEATHER_INFO_ROTATION_ENABLED true
#define WEATHER_INFO_ROTATION_INTERVAL_MS 10000
#define WEATHER_INFO_ANIMATION_ENABLED true
#define WEATHER_INFO_ANIMATION_DELAY_MS 120

// ----------------------------------------------------------------------------
// Weather info clear area (tuned for display layout) do not modify unless you understand what you are doing.
// ----------------------------------------------------------------------------
#define WEATHER_INFO_CLEAR_X      105
#define WEATHER_INFO_CLEAR_Y      155
#define WEATHER_INFO_CLEAR_WIDTH  125
#define WEATHER_INFO_CLEAR_HEIGHT 62


// ****************************************************************************
// System settings - do not modify unless you understand what you are doing!
// ****************************************************************************
typedef struct RectangleDef {
  uint16_t x;
  uint16_t y;
  uint16_t width;
  uint16_t height;
} RectangleDef;

typedef struct DayForecast {
  float minTemp;
  float maxTemp;
  int conditionCode;
  int conditionHour;
  int day;
} DayForecast;

RectangleDef timeSpritePos = {0, 0, 320, 88};

const String WIND_ICON_NAMES[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};

// average approximation for the actual length of the synodic month
const double LUNAR_MONTH = 29.530588853;
const uint8_t NUMBER_OF_MOON_IMAGES = 32;

// 2: portrait, on/off switch right side -> 0/0 top left
// 3: landscape, on/off switch at the top -> 0/0 top left
#define TFT_ROTATION 2
// all other TFT_xyz flags are defined in platformio.ini as PIO build flags

// 0: portrait, on/off switch right side -> 0/0 top left
// 1: landscape, on/off switch at the top -> 0/0 top left
#define TOUCH_ROTATION 0
#define TOUCH_SENSITIVITY 40
#define TOUCH_SDA 23
#define TOUCH_SCL 22
// Initial LCD Backlight brightness
//#define TFT_LED_BRIGHTNESS 200

// the medium blue in the TP logo is 0x0067B0 which converts to 0x0336 in 16bit RGB565
#define TFT_TP_BLUE 0x0336

// format specifiers: https://cplusplus.com/reference/ctime/strftime/
#ifdef DATE_TIME_FORMAT_US
  int timePosX = 29;
  #define UI_DATE_FORMAT "%m/%d/%Y"
  #define UI_TIME_FORMAT "%I:%M:%S %P"
  #define UI_TIME_FORMAT_NO_SECONDS "%I:%M %P"
  #define UI_TIMESTAMP_FORMAT (UI_DATE_FORMAT + " " + UI_TIME_FORMAT)
#else
  int timePosX = 68;
  #define UI_DATE_FORMAT "%d.%m.%Y"
  #define UI_TIME_FORMAT "%H:%M:%S"
  #define UI_TIME_FORMAT_NO_SECONDS "%H:%M"
  #define UI_TIMESTAMP_FORMAT (UI_DATE_FORMAT + " " + UI_TIME_FORMAT)
#endif

#define SYSTEM_TIMESTAMP_FORMAT "%Y-%m-%d %H:%M:%S"

// every 3h (UTC), we need all to be able to calculate daily min/max temperatures
const uint8_t forecastHoursUtc[] = {0, 3, 6, 9, 12, 15, 18, 21};
// 5 day / 3 hour forecast data => 8 forecasts/day => 40 total
#define NUMBER_OF_FORECASTS 40
#define NUMBER_OF_DAY_FORECASTS 4

#define APP_NAME "ESP32 Weather Station Touch"
#define VERSION "1.0.0"
