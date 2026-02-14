# ESP32 MQTT Power LCD

ESP32-based MQTT client that subscribes to live power data and displays
the current value on a 16x2 I2C LCD.

Designed for home energy monitoring setups (PV, grid consumption,
battery systems, ioBroker, Home Assistant, Node-RED).

## Features

-   MQTT subscription to real-time power data\
-   Display of numeric power values on I2C LCD\
-   Unique MQTT client ID (MAC-based)\
-   Stable reconnect logic\
-   Flicker-free LCD updates (only refresh on value change)\
-   Lightweight and suitable for 24/7 operation

## Hardware

-   ESP32 DevKit (USB Type-C or Micro USB)
-   16x2 I2C I2C LCD display
-   MQTT broker (e.g. ioBroker, Mosquitto, Home Assistant)

## Wiring (I2C LCD)

  LCD   ESP32
  ----- --------
  VCC   5V
  GND   GND
  SDA   GPIO21
  SCL   GPIO22

Typical I2C address: 0x27

## MQTT Setup

Topic: energy/Power_cur

Payload format: 1234

(Plain numeric value as string, no JSON)

Example: 850

Display output:

Power: 850 W

## Project Structure

ESP32_Power_MQTT_LCD/

-   ESP32_Power_MQTT_LCD.ino
-   secrets.h (not committed)
-   secrets.example.h (template)
-   README.md

## Secrets Configuration

Create a file named: secrets.h

Content:

#pragma once

#define WIFI_SSID "YOUR_WIFI" #define WIFI_PASSWORD "YOUR_PASSWORD"

#define MQTT_HOST "192.168.x.x" #define MQTT_PORT 1884 #define MQTT_USER
"user" #define MQTT_PASS "pass"

#define MQTT_TOPIC_SUB "energy/Power_cur"

This file is excluded from Git via .gitignore.

## Build & Flash

Using Arduino IDE:

1.  Open ESP32_Power_MQTT_LCD.ino
2.  Select board: ESP32 Dev Module
3.  Select correct port
4.  Upload

## Use Cases

-   Real-time grid consumption display
-   PV surplus monitoring
-   Battery discharge/charge visualization

## Future Extensions

Possible next steps:

-   OTA firmware updates\
-   Webinterface with additional visualization

