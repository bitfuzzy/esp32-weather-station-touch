// SPDX-FileCopyrightText: 2023 ThingPulse Ltd., https://thingpulse.com
// SPDX-License-Identifier: MIT

#include <LittleFS.h>

#include <OpenFontRender.h>
#include <TJpg_Decoder.h>

#include "fonts/open-sans.h"
#include "GfxUi.h"

#include <JsonListener.h>
#include <OpenWeatherMapCurrent.h>
#include <OpenWeatherMapForecast.h>
#include <SunMoonCalc.h>
#include <TaskScheduler.h>

#include "connectivity.h"
#include "display.h"
#include "persistence.h"
#include "settings.h"
#include "util.h"



// ----------------------------------------------------------------------------
// Globals
// ----------------------------------------------------------------------------
OpenFontRender ofr;
FT6236 ts = FT6236(TFT_HEIGHT, TFT_WIDTH);
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite timeSprite = TFT_eSprite(&tft);
GfxUi ui = GfxUi(&tft, &ofr);

// time management variables
int lastScheduledUpdateKey = -1;
unsigned long lastTimeSyncMillis = 0;
unsigned long lastUpdateMillis = 0;

const int16_t centerWidth = tft.width() / 2;

OpenWeatherMapCurrentData primaryLocationWeather;
OpenWeatherMapCurrentData locationCurrentWeather[NUMBER_OF_LOCATIONS];
OpenWeatherMapForecastData locationForecasts[NUMBER_OF_LOCATIONS][NUMBER_OF_FORECASTS];

bool locationHasData[NUMBER_OF_LOCATIONS] = {false};
unsigned long locationLastUpdateMillis[NUMBER_OF_LOCATIONS] = {0};

Scheduler scheduler;
uint8_t currentBacklightBrightness = 255;
unsigned long lastTouchRefreshMillis = 0;

uint8_t weatherInfoMode = 0;
unsigned long lastWeatherInfoSwitchMillis = 0;

uint8_t currentLocationIndex = 0;

// ----------------------------------------------------------------------------
// Function prototypes (declarations)
// ----------------------------------------------------------------------------
void drawAstro();
void drawCurrentWeather();
void drawForecast();
void drawProgress(const char *text, int8_t percentage);
void drawTimeAndDate();
String getWeatherIconName(uint16_t id, bool today);
void initJpegDecoder();
void initOpenFontRender();
bool pushImageToTft(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap);
void syncTime();
void repaint();
void updatePrimaryLocationData();

uint8_t getTargetBrightness();
bool isPrimaryLocationSunDown();
void handleDisplayBrightnessMode();
void handleTouchWake();
bool isPrimaryLocationNightTime();
int getMinutesForTimezone(const char* timezone);
bool shouldRunScheduledUpdate();

void handleWeatherInfoRotation();
void drawWeatherInfoBlock();

void redrawScreenFromCache();
void updateDataInBackground();
void initialPaint();

void switchToNextLocation();
bool isTouchInLocationArea(uint16_t x, uint16_t y);

void refreshCurrentLocationFromTouch();

void updateLocationData(uint8_t locationIndex);

void drawStatusOverlay(const String& message);
void drawRefreshingOverlay();
void drawSwitchingLocationOverlay();

OpenWeatherMapCurrentData& activeWeather();
OpenWeatherMapForecastData* activeForecasts();

Task clockTask(1000, TASK_FOREVER, &drawTimeAndDate);


// ----------------------------------------------------------------------------
// helper functions for dynamic brightness and setup() & loop()
// ----------------------------------------------------------------------------

bool shouldRunScheduledUpdate() {
  // safety fallback for first run
  if (lastUpdateMillis == 0) {
    return true;
  }

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    log_e("Failed to obtain time for scheduled update check.");
    return false;
  }

  int updatesPerHour = UPDATES_PER_HOUR;
  if (updatesPerHour < 1) updatesPerHour = 1;
  if (updatesPerHour > 4) updatesPerHour = 4;

  int intervalMinutes = 60 / updatesPerHour;

  if ((timeinfo.tm_min % intervalMinutes) != 0) {
    return false;
  }

  // unique key for current scheduled slot
  int currentKey = ((timeinfo.tm_yday * 24 + timeinfo.tm_hour) * 100) + timeinfo.tm_min;

  if (currentKey == lastScheduledUpdateKey) {
    return false;
  }

  lastScheduledUpdateKey = currentKey;
  return true;
}

void drawWeatherInfoBlock() {
  String text = "";

  // clear only the temperature/info area before redrawing it
  // Tuned on real hardware to avoid clipping the location/description and cloud icon.
  tft.fillRect(WEATHER_INFO_CLEAR_X,
             WEATHER_INFO_CLEAR_Y,
             WEATHER_INFO_CLEAR_WIDTH,
             WEATHER_INFO_CLEAR_HEIGHT,
             TFT_BLACK);

  if (weatherInfoMode == 0) {
    // actual temperature
    ofr.setFontSize(44);
    text = String(activeWeather().temp, 1) + "°";
    ofr.cdrawString(text.c_str(), centerWidth + 10, 145);

    // humidity + pressure on one row with custom dot separator
    ofr.setFontSize(16);

    String hum = String(activeWeather().humidity) + "%";
    String pres = String(activeWeather().pressure) + "hPa";

    int infoY = 195;

    int humCenterX = centerWidth - 34;
    int presCenterX = centerWidth + 34;

    int humWidth = ofr.getTextWidth(hum.c_str());
    int presWidth = ofr.getTextWidth(pres.c_str());

    ofr.cdrawString(hum.c_str(), humCenterX, infoY);
    ofr.cdrawString(pres.c_str(), presCenterX, infoY);

    int humRightEdge = humCenterX + (humWidth / 2);
    int presLeftEdge = presCenterX - (presWidth / 2);

    int dotX = ((humRightEdge + presLeftEdge) / 2) + 3;
    int dotY = infoY + 12;
    int dotRadius = 2;

    tft.fillCircle(dotX, dotY, dotRadius, TFT_WHITE);
  } else {
    // feels like temperature
    ofr.setFontSize(44);
    String feelsLikeTemp = String(activeWeather().feelsLike, 1) + "°";
    ofr.cdrawString(feelsLikeTemp.c_str(), centerWidth + 10, 145);

    // second line label
    String feelsLikeText = FEELS_LIKE_LABEL;

    if (feelsLikeText.length() <= 12) {
      ofr.setFontSize(16);
    } else if (feelsLikeText.length() <= 16) {
      ofr.setFontSize(14);
    } else {
      ofr.setFontSize(12);
    }

    ofr.cdrawString(feelsLikeText.c_str(), centerWidth, 195);
  }
}

void drawStatusOverlay(const String& message) {
  ofr.setFontSize(16);

  int textWidth = ofr.getTextWidth(message.c_str());
  int paddingX = 18;
  int boxW = textWidth + (paddingX * 2);
  int boxH = 28;

  // keep box within screen bounds
  if (boxW > (tft.width() - 20)) {
    boxW = tft.width() - 20;
  }

  int boxX = (tft.width() - boxW) / 2;
  int boxY = 215;

  tft.fillRoundRect(boxX, boxY, boxW, boxH, 6, TFT_BLACK);
  tft.drawRoundRect(boxX, boxY, boxW, boxH, 6, TFT_WHITE);

  ofr.cdrawString(message.c_str(), centerWidth, boxY + 6);
}

void drawRefreshingOverlay() {
  drawStatusOverlay(REFRESHING_LABEL);
}

void drawSwitchingLocationOverlay() {
  drawStatusOverlay(SWITCHING_LOCATION_LABEL);
}

void handleWeatherInfoRotation() {
  if (!WEATHER_INFO_ROTATION_ENABLED) {
    return;
  }

  if ((millis() - lastWeatherInfoSwitchMillis) >= WEATHER_INFO_ROTATION_INTERVAL_MS) {
    lastWeatherInfoSwitchMillis = millis();
    weatherInfoMode = (weatherInfoMode + 1) % 2;

    if (WEATHER_INFO_ANIMATION_ENABLED) {
      // simple pseudo-fade: clear info area, short pause, redraw
      // Tuned on real hardware to avoid clipping the location/description and cloud icon.
      tft.fillRect(WEATHER_INFO_CLEAR_X,
             WEATHER_INFO_CLEAR_Y,
             WEATHER_INFO_CLEAR_WIDTH,
             WEATHER_INFO_CLEAR_HEIGHT,
             TFT_BLACK);
      delay(WEATHER_INFO_ANIMATION_DELAY_MS);
    }

    drawWeatherInfoBlock();
  }
}

int getMinutesForTimezone(const char* timezone) {
  time_t now = time(nullptr);
  if (now <= 0) {
    return -1;
  }

  // temporarily switch timezone to evaluate local time for the given location
  setenv("TZ", timezone, 1);
  tzset();

  struct tm *timeinfo = localtime(&now);
  int minutes = -1;
  if (timeinfo != nullptr) {
    minutes = timeinfo->tm_hour * 60 + timeinfo->tm_min;
  }

  // restore currently displayed location timezone
  setenv("TZ", LOCATIONS[currentLocationIndex].timezone, 1);
  tzset();

  return minutes;
}

uint8_t getTargetBrightness() {
  // manual test override
  if (FORCE_NIGHT_MODE_FOR_TEST) {
    return TFT_LED_BRIGHTNESS_NIGHT;
  }

  // fixed night brightness window based on PRIMARY location timezone
  if (DISPLAY_NIGHT_MODE_ENABLED && isPrimaryLocationNightTime()) {
    return TFT_LED_BRIGHTNESS_NIGHT;
  }

  // evening brightness based on PRIMARY location sunrise/sunset
  if (DISPLAY_DYNAMIC_BRIGHTNESS_ENABLED && isPrimaryLocationSunDown()) {
    return TFT_LED_BRIGHTNESS_EVENING;
  }

  return TFT_LED_BRIGHTNESS_DAY;
}

void handleDisplayBrightnessMode() {
  uint8_t targetBrightness = getTargetBrightness();

  if (targetBrightness != currentBacklightBrightness) {
    currentBacklightBrightness = targetBrightness;
    log_i("Changing display brightness to %d.", targetBrightness);
    setDisplayBacklight(targetBrightness);
  }
}

void handleTouchWake() {
  if (!TOUCH_REFRESH_ENABLED) {
    return;
  }

  if (!ts.touched()) {
    return;
  }

  TS_Point p = ts.getPoint();

  uint16_t touchX = p.x;
  uint16_t touchY = p.y;

  log_i("Touch coordinates: x=%d, y=%d", touchX, touchY);

  if ((millis() - lastTouchRefreshMillis) <= TOUCH_REFRESH_DEBOUNCE_MS) {
    return;
  }

  lastTouchRefreshMillis = millis();

  if (isTouchInLocationArea(touchX, touchY)) {
    log_i("Location area touched. Switching location.");
    switchToNextLocation();
    return;
  }

  log_i("Manual refresh triggered by touch.");
  refreshCurrentLocationFromTouch();
}

void refreshCurrentLocationFromTouch() {
  drawRefreshingOverlay();

  if (WiFi.status() != WL_CONNECTED) {
    startWiFi();
  }

  // Only refresh the currently displayed location
  updateLocationData(currentLocationIndex);

  // Refresh primary location weather only if the current location is the primary one
  if (currentLocationIndex == 0) {
    updatePrimaryLocationData();
  }

  lastUpdateMillis = millis();
  redrawScreenFromCache();
}

void setup(void) {
  Serial.begin(115200);
  delay(1000);

  logBanner();
  logMemoryStats();

  initJpegDecoder();
  initTouchScreen(&ts);
  initTft(&tft);
  timeSprite.createSprite(timeSpritePos.width, timeSpritePos.height);
  logDisplayDebugInfo(&tft);

  initFileSystem();
  initOpenFontRender();

  scheduler.init();
  scheduler.addTask(clockTask);
  clockTask.enable();

  initialPaint();
}

void loop(void) {
  handleTouchWake();
  handleDisplayBrightnessMode();

  if (shouldRunScheduledUpdate()) {
    repaint();
  }

  handleWeatherInfoRotation();
  scheduler.execute();
}

OpenWeatherMapCurrentData& activeWeather() {
  return locationCurrentWeather[currentLocationIndex];
}

OpenWeatherMapForecastData* activeForecasts() {
  return locationForecasts[currentLocationIndex];
}

// ----------------------------------------------------------------------------
// Functions
// ----------------------------------------------------------------------------
void drawAstro() {
  time_t tnow = time(nullptr);
  struct tm *nowUtc = gmtime(&tnow);

  SunMoonCalc smCalc = SunMoonCalc(mkgmtime(nowUtc), activeWeather().lat, activeWeather().lon);
  const SunMoonCalc::Result result = smCalc.calculateSunAndMoonData();

  ofr.setFontSize(24);
  ofr.cdrawString(SUN_MOON_LABEL[0].c_str(), 60, 365);
  ofr.cdrawString(SUN_MOON_LABEL[1].c_str(), tft.width() - 60, 365);

  ofr.setFontSize(18);
  // Sun
  strftime(timestampBuffer, 26, UI_TIME_FORMAT_NO_SECONDS, localtime(&result.sun.rise));
  ofr.cdrawString(timestampBuffer, 60, 400);
  strftime(timestampBuffer, 26, UI_TIME_FORMAT_NO_SECONDS, localtime(&result.sun.set));
  ofr.cdrawString(timestampBuffer, 60, 425);

  // Moon
  strftime(timestampBuffer, 26, UI_TIME_FORMAT_NO_SECONDS, localtime(&result.moon.rise));
  ofr.cdrawString(timestampBuffer, tft.width() - 60, 400);
  strftime(timestampBuffer, 26, UI_TIME_FORMAT_NO_SECONDS, localtime(&result.moon.set));
  ofr.cdrawString(timestampBuffer, tft.width() - 60, 425);

  // Moon icon
  int imageIndex = round(result.moon.age * NUMBER_OF_MOON_IMAGES / LUNAR_MONTH);
  if (imageIndex == NUMBER_OF_MOON_IMAGES) imageIndex = NUMBER_OF_MOON_IMAGES - 1;
  ui.drawBmp("/moon/m-phase-" + String(imageIndex) + ".bmp", centerWidth - 37, 365);

  ofr.setFontSize(14);
  ofr.cdrawString(MOON_PHASES[result.moon.phase.index].c_str(), centerWidth, 455);

  log_i("Moon phase: %s, illumination: %f, age: %f -> image index: %d",
        result.moon.phase.name.c_str(), result.moon.illumination, result.moon.age, imageIndex);
}

void drawCurrentWeather() {
  String text = "";

  // weather icon
  String weatherIcon = getWeatherIconName(activeWeather().weatherId, true);
  ui.drawBmp("/weather/" + weatherIcon + ".bmp", 5, 125);

  // location name with dynamic font size
  String locationText = LOCATIONS[currentLocationIndex].displayName;
  if (locationText.length() <= 10) {
    ofr.setFontSize(16);
  } else if (locationText.length() <= 16) {
    ofr.setFontSize(14);
  } else {
    ofr.setFontSize(12);
  }
  ofr.cdrawString(locationText.c_str(), centerWidth, 100);

  // weather description with dynamic font size
  String desc = activeWeather().description;
  desc.replace("ß", "ss");

  if (desc.length() <= 12) {
    ofr.setFontSize(24);
  } else if (desc.length() <= 18) {
    ofr.setFontSize(20);
  } else {
    ofr.setFontSize(16);
  }
  ofr.cdrawString(desc.c_str(), centerWidth, 120);

  // rotating center info block
  drawWeatherInfoBlock();

  // wind rose icon
  int windAngleIndex = round(activeWeather().windDeg * 8 / 360);
  if (windAngleIndex > 7) windAngleIndex = 0;
  ui.drawBmp("/wind/" + WIND_ICON_NAMES[windAngleIndex] + ".bmp", tft.width() - 80, 125);

  // wind speed
  ofr.setFontSize(18);
  text = String(activeWeather().windSpeed, 0);
  if (IS_METRIC) text += " m/s";
  else text += " mph";
  ofr.cdrawString(text.c_str(), tft.width() - 43, 200);
}

void drawForecast() {
  DayForecast* dayForecasts = calculateDayForecasts(activeForecasts());
  for (int i = 0; i < NUMBER_OF_DAY_FORECASTS; i++) {
    log_i("[%d] condition code: %d, hour: %d, temp: %.1f/%.1f", dayForecasts[i].day,
          dayForecasts[i].conditionCode, dayForecasts[i].conditionHour, dayForecasts[i].minTemp,
          dayForecasts[i].maxTemp);
  }

  int widthEigth = tft.width() / 8;
  for (int i = 0; i < NUMBER_OF_DAY_FORECASTS; i++) {
    int x = widthEigth * ((i * 2) + 1);
    ofr.setFontSize(24);
    ofr.cdrawString(WEEKDAYS_ABBR[dayForecasts[i].day].c_str(), x, 235);
    ofr.setFontSize(18);
    ofr.cdrawString(String(String(dayForecasts[i].minTemp, 0) + "-" + String(dayForecasts[i].maxTemp, 0) + "°").c_str(), x, 265);
    ui.drawBmp("/weather-small/" + getWeatherIconName(dayForecasts[i].conditionCode, false) + ".bmp", x - 25, 295);
  }
}

void drawProgress(const char *text, int8_t percentage) {
  ofr.setFontSize(24);
  int pbWidth = tft.width() - 100;
  int pbX = (tft.width() - pbWidth)/2;
  int pbY = 260;
  int progressTextY = 210;

  tft.fillRect(0, progressTextY, tft.width(), 40, TFT_BLACK);
  ofr.cdrawString(text, centerWidth, progressTextY);
  ui.drawProgressBar(pbX, pbY, pbWidth, 15, percentage, TFT_WHITE, TFT_TP_BLUE);
}

void drawSeparator(uint16_t y) {
  tft.drawFastHLine(10, y, tft.width() - 2 * 15, 0x4228);
}

void drawTimeAndDate() {
  timeSprite.fillSprite(TFT_BLACK);
  ofr.setDrawer(timeSprite);

  // Date
  ofr.setFontSize(16);
  ofr.cdrawString(
    String(WEEKDAYS[getCurrentWeekday()] + ", " + getCurrentTimestamp(UI_DATE_FORMAT)).c_str(),
    centerWidth,
    10
  );

  // Time
  ofr.setFontSize(48);
  // centering that string would look optically odd for 12h times -> manage pos manually
  ofr.drawString(getCurrentTimestamp(UI_TIME_FORMAT).c_str(), timePosX, 25);
  timeSprite.pushSprite(timeSpritePos.x, timeSpritePos.y);

  // set the drawer back since we temporarily changed it to the time sprite above
  ofr.setDrawer(tft);
}

String getWeatherIconName(uint16_t id, bool today) {
  // Weather condition codes: https://openweathermap.org/weather-conditions#Weather-Condition-Codes-2

  // For the 8xx group we also have night versions of the icons.
  // Switch to night icons? This could be written w/o if-else but it'd be less legible.
  if (today && id / 100 == 8) {
    if (today && (activeWeather().observationTime < activeWeather().sunrise ||
                  activeWeather().observationTime > activeWeather().sunset)) {
      id += 1000;
    } else if (!today && false) {
      // NOT-SUPPORTED-YET
      // We currently don't need the night icons for forecast.
      // Hence, we don't even track those properties in the DayForecast struct.
      id += 1000;
    }
  }

  if (id / 100 == 2) return "thunderstorm";
  if (id / 100 == 3) return "drizzle";
  if (id == 500) return "light-rain";
  if (id == 504) return "extrem-rain";
  else if (id == 511) return "sleet";
  else if (id / 100 == 5) return "rain";
  if (id >= 611 && id <= 616) return "sleet";
  else if (id / 100 == 6) return "snow";
  if (id / 100 == 7) return "fog";
  if (id == 800) return "clear-day";
  if (id >= 801 && id <= 803) return "partly-cloudy-day";
  else if (id / 100 == 8) return "cloudy";
  // night icons
  if (id == 1800) return "clear-night";
  if (id == 1801) return "partly-cloudy-night";
  else if (id / 100 == 18) return "cloudy";

  return "unknown";
}

void initJpegDecoder() {
    // The JPEG image can be scaled by a factor of 1, 2, 4, or 8 (default: 0)
  TJpgDec.setJpgScale(1);
  // The decoder must be given the exact name of the rendering function
  TJpgDec.setCallback(pushImageToTft);
}

void initOpenFontRender() {
  ofr.loadFont(opensans, sizeof(opensans));
  ofr.setDrawer(tft);
  ofr.setFontColor(TFT_WHITE);
  ofr.setBackgroundColor(TFT_BLACK);
}

// Function will be called as a callback during decoding of a JPEG file to
// render each block to the TFT.
bool pushImageToTft(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap) {
  // Stop further decoding as image is running off bottom of screen
  if (y >= tft.height()) {
    return 0;
  }

  // Automatically clips the image block rendering at the TFT boundaries.
  tft.pushImage(x, y, w, h, bitmap);

  // Return 1 to decode next block
  return 1;
}

void syncTime() {
  if (initTime()) {
    lastTimeSyncMillis = millis();
    setTimezone(LOCATIONS[currentLocationIndex].timezone);
    log_i("Current local time: %s", getCurrentTimestamp(SYSTEM_TIMESTAMP_FORMAT).c_str());
  }
}

void initialPaint() {
  tft.fillScreen(TFT_BLACK);
  ui.drawLogo();

  ofr.setFontSize(16);
  ofr.cdrawString(APP_NAME, centerWidth, tft.height() - 50);
  ofr.cdrawString(VERSION, centerWidth, tft.height() - 30);

  drawProgress("Starting WiFi...", 10);
  if (WiFi.status() != WL_CONNECTED) {
    startWiFi();
  }

  drawProgress("Synchronizing time...", 30);
  syncTime();

  drawProgress("Loading locations...", 50);
  for (uint8_t i = 0; i < NUMBER_OF_LOCATIONS; i++) {
    updateLocationData(i);
  }

  updatePrimaryLocationData();

  drawProgress("Ready", 100);
  lastUpdateMillis = millis();

  currentLocationIndex = 0;
  setTimezone(LOCATIONS[currentLocationIndex].timezone);

  tft.fillScreen(TFT_BLACK);

  drawTimeAndDate();
  drawSeparator(90);

  drawCurrentWeather();
  drawSeparator(230);

  drawForecast();
  drawSeparator(355);

  drawAstro();

  weatherInfoMode = 0;
  lastWeatherInfoSwitchMillis = millis();

  setDisplayBacklight(getTargetBrightness());
  currentBacklightBrightness = getTargetBrightness();
}

void repaint() {
  updateDataInBackground();
  redrawScreenFromCache();
}

void redrawScreenFromCache() {
  tft.fillScreen(TFT_BLACK);

  drawTimeAndDate();
  drawSeparator(90);

  drawCurrentWeather();
  drawSeparator(230);

  drawForecast();
  drawSeparator(355);

  drawAstro();

  weatherInfoMode = 0;
  lastWeatherInfoSwitchMillis = millis();

}


void updateDataInBackground() {
  if (WiFi.status() != WL_CONNECTED) {
    startWiFi();
  }

  syncTime();

  updateLocationData(currentLocationIndex);
  updatePrimaryLocationData();

  lastUpdateMillis = millis();
}

void updatePrimaryLocationData() {
  OpenWeatherMapCurrent *primaryWeatherClient = new OpenWeatherMapCurrent();
  primaryWeatherClient->setMetric(IS_METRIC);
  primaryWeatherClient->setLanguage(OPEN_WEATHER_MAP_LANGUAGE);

  primaryWeatherClient->updateCurrentById(
    &primaryLocationWeather,
    OPEN_WEATHER_MAP_API_KEY,
    LOCATIONS[0].locationId
  );

  delete primaryWeatherClient;
  primaryWeatherClient = nullptr;

  log_i("Primary location weather updated: %s, sunrise=%ld, sunset=%ld",
        LOCATIONS[0].displayName,
        primaryLocationWeather.sunrise,
        primaryLocationWeather.sunset);
}

void switchToNextLocation() {
  currentLocationIndex = (currentLocationIndex + 1) % NUMBER_OF_LOCATIONS;
  setTimezone(LOCATIONS[currentLocationIndex].timezone);

  log_i("Switched to cached location: %s (%s), timezone=%s",
        LOCATIONS[currentLocationIndex].displayName,
        LOCATIONS[currentLocationIndex].locationId,
        LOCATIONS[currentLocationIndex].timezone);

  drawSwitchingLocationOverlay();
  delay(150);

  redrawScreenFromCache();
}

bool isTouchInLocationArea(uint16_t x, uint16_t y) {
  return (x >= 40 && x <= 280 && y >= 80 && y <= 130);
}

bool isPrimaryLocationSunDown() {
  time_t now = time(nullptr);
  if (now <= 0) {
    return false;
  }

  if (primaryLocationWeather.sunrise <= 0 || primaryLocationWeather.sunset <= 0) {
    return false;
  }

  return (now < primaryLocationWeather.sunrise || now >= primaryLocationWeather.sunset);
}

bool isPrimaryLocationNightTime() {
  int nowMinutes = getMinutesForTimezone(LOCATIONS[0].timezone);
  if (nowMinutes < 0) {
    return false;
  }

  int fromMinutes = DISPLAY_NIGHT_FROM_HOUR * 60 + DISPLAY_NIGHT_FROM_MINUTE;
  int toMinutes   = DISPLAY_NIGHT_TO_HOUR * 60 + DISPLAY_NIGHT_TO_MINUTE;

  if (fromMinutes == toMinutes) {
    return false;
  }

  // same-day range
  if (fromMinutes < toMinutes) {
    return nowMinutes >= fromMinutes && nowMinutes < toMinutes;
  }

  // overnight range
  return nowMinutes >= fromMinutes || nowMinutes < toMinutes;
}

void updateLocationData(uint8_t locationIndex) {
  OpenWeatherMapCurrent *currentWeatherClient = new OpenWeatherMapCurrent();
  currentWeatherClient->setMetric(IS_METRIC);
  currentWeatherClient->setLanguage(OPEN_WEATHER_MAP_LANGUAGE);

  currentWeatherClient->updateCurrentById(
    &locationCurrentWeather[locationIndex],
    OPEN_WEATHER_MAP_API_KEY,
    LOCATIONS[locationIndex].locationId
  );

  delete currentWeatherClient;
  currentWeatherClient = nullptr;

  OpenWeatherMapForecast *forecastClient = new OpenWeatherMapForecast();
  forecastClient->setMetric(IS_METRIC);
  forecastClient->setLanguage(OPEN_WEATHER_MAP_LANGUAGE);
  forecastClient->setAllowedHours(forecastHoursUtc, sizeof(forecastHoursUtc));

  forecastClient->updateForecastsById(
    locationForecasts[locationIndex],
    OPEN_WEATHER_MAP_API_KEY,
    LOCATIONS[locationIndex].locationId,
    NUMBER_OF_FORECASTS
  );

  delete forecastClient;
  forecastClient = nullptr;

  locationHasData[locationIndex] = true;
  locationLastUpdateMillis[locationIndex] = millis();

  log_i("Updated location %s (%s)",
        LOCATIONS[locationIndex].displayName,
        LOCATIONS[locationIndex].locationId);
}