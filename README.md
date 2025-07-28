# HackyClock
This is an ESP-32 firmware for a hackable LED-matrix desk clock, written on C (ESP-IDF framework).

Supports WS2812(B) / SK6812 LEDs.

## Key features
- **Apps support** - App manager takes care of apps' UI tasks and input events. Somewhat modular design allows to specify which apps to include into the final image
- **Drawing APIs** - [QOI](https://qoiformat.org/) images support, minimal set of drawing functions and animations.
- **Configurable** - Minimum amount of hard-coded options, configuration of core and apps is done via ESP-IDF menuconfig (the web server with the dynamic configuration is planned)
- **Networking** - thanks to ESP-IDF
- WIP!

## Available apps
- Clock (surprisingly)
- Wi-Fi status
- Weather app (data from [OpenMeteo API](https://open-meteo.com))
- Home Assistant MQTT light control
- Sine wave animation

## Installation
#TODO \^w\^
