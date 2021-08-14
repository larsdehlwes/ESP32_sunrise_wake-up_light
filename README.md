# ESP32_sunrise_wake-up_light
This Arduino IDE project allows to use an ESP32 in order to control a smart RGBCCT light bulb running Tasmota. The ESP32 itself acts as a server in the local WiFi through which a wake-up time can be set up. In the 40 minutes preceeding the wake-up time, the ESP32 will send http requests to the smart light bulb, mimicking the color and light intensity curves of a sunrise.

## Dependencies
* Arduino IDE with the Espressif board manager installed
* [ESPAsyncWebserver](https://github.com/me-no-dev/ESPAsyncWebServer) library

## Demo Screenshots
<p float="left">
<img src="https://user-images.githubusercontent.com/9755880/129462494-36ceb8ab-a022-463c-811f-c451ac36ab1a.jpeg" width="250">
<img src="https://user-images.githubusercontent.com/9755880/129462495-4b90eeb1-4fbe-45c0-b053-43a9a9513a6f.jpeg" width="250">
</p>
