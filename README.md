# Color Kit Grande Weather Station

[![Build Status](https://github.com/ThingPulse/esp32-weather-station-touch/actions/workflows/main.yml/badge.svg)](https://github.com/ThingPulse/esp32-weather-station-touch/actions)

Weather station application for the [ThingPulse Color Kit Grande](https://thingpulse.com/product/esp32-wifi-color-display-kit-grande/).

[![Color Kit Grande with sample application: weather station](https://thingpulse.com/wp-content/uploads/2022/10/ThingPulse-Color-Kit-Grand-with-sample-application.jpg)](https://thingpulse.com/product/esp32-wifi-color-display-kit-grande/)

## How to use it

See the documentation at https://docs.thingpulse.com/guides/esp32-color-kit-grande/.

## Service level promise

<table><tr><td><img src="https://thingpulse.com/assets/ThingPulse-open-source-prime.png" width="150">
</td><td>This is a ThingPulse <em>prime</em> project. See our <a href="https://thingpulse.com/about/open-source-commitment/">open-source commitment declaration</a> for what this means.</td></tr></table>

## Enhancements in this fork

This fork adds several usability and display improvements for the ESP32 WiFi Color Display Kit Grande weather station.

### Display and brightness
- configurable day / evening / night brightness
- optional dynamic evening dimming based on the sunset of a primary location
- fixed night brightness window
- test override to force night brightness for tuning

### Touch interaction
- touch refresh
- touch-based switching between predefined locations
- multilingual status popups for:
  - refreshing weather
  - switching location

### Multi-location support
- predefined list of selectable locations
- `LOCATIONS[0]` acts as the primary/home location
- primary location controls dynamic evening dimming
- currently selected location controls:
  - displayed weather
  - displayed local time
  - displayed location name
- cached location switching for faster navigation between cities

### Weather UI improvements
- rotating current temperature / feels-like view
- multilingual feels-like label support:
  - English
  - German
  - Italian
  - Dutch
- dynamic text sizing for:
  - location name
  - weather description
  - feels-like label
- improved current weather layout:
  - humidity and pressure on a single line
  - visual separator between values
- fallback handling for unsupported special glyphs (for example German `ß -> ss`)

### Refresh behavior
- lighter touch refresh path for improved responsiveness
- scheduled updates configurable as 1–4 times per hour