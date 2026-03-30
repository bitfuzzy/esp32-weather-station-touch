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

This fork adds several usability and display improvements for the ESP32 WiFi Color Display Kit Grande weather station:

- configurable day / evening / night brightness
- optional dynamic brightness based on sunrise / sunset
- touch refresh
- touch-based switching between predefined locations
- rotating current temperature / feels-like view
- multilingual feels-like label support (EN / DE / IT / NL)
- dynamic text sizing for long weather descriptions and location names
- improved current weather layout and spacing
- fallback handling for unsupported special glyphs (e.g. German ß -> ss)