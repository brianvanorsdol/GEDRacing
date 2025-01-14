/*
   -- A90 Supra Shift Light Control --
   
   This source code of graphical user interface 
   has been generated automatically by RemoteXY editor.
   To compile this code using RemoteXY library 3.1.13 or later version 
   download by link http://remotexy.com/en/library/
   To connect using RemoteXY mobile app by link http://remotexy.com/en/download/                   
     - for ANDROID 4.15.01 or later version;
     - for iOS 1.12.1 or later version;
    
   This source code is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.    
*/
// RemoteXY select connection mode and include library 
#define REMOTEXY_MODE__ESP32CORE_BLE
#include <Arduino.h>
#include "driver/twai.h"
#include <Adafruit_NeoPixel.h>
#include <BLEDevice.h>
#include <nvs_flash.h>
#include <nvs.h>
#include <RemoteXY.h>

// RemoteXY connection settings 
#define REMOTEXY_BLUETOOTH_NAME "Shift Light Controller"

// RemoteXY configurator settings
#pragma pack(push, 1)
uint8_t RemoteXY_CONF[] = {
    255,8,0,0,0,216,0,19,0,0,0,83,104,105,102,116,32,76,105,103,
  104,116,115,0,25,1,106,200,1,1,11,0,129,7,6,90,7,64,195,65,
  57,48,32,83,117,112,114,97,32,83,104,105,102,116,32,76,105,103,104,116,
  32,67,111,110,116,114,111,108,0,129,8,28,34,11,64,195,80,111,119,101,
  114,58,0,2,54,25,39,18,0,192,26,31,31,79,78,0,79,70,70,0,
  4,1,76,104,15,128,193,26,129,22,59,55,11,64,195,66,114,105,103,104,
  116,110,101,115,115,58,0,7,58,109,40,10,118,0,195,26,192,129,15,112,
  39,8,64,195,83,104,105,102,116,32,80,111,105,110,116,58,0,129,6,128,
  49,7,64,195,76,69,68,32,83,116,97,114,116,32,80,111,105,110,116,58,
  0,7,58,125,40,10,118,0,195,26,192,129,8,143,48,7,64,195,76,69,
  68,32,73,110,99,114,101,109,101,110,116,58,0,7,58,140,40,10,118,0,
  195,26,192
};

// This structure defines all the variables and events of your control interface 
struct {
    // input variables
    uint8_t powerSwitch; // =1 if switch ON and =0 if OFF 
    uint8_t brightness; // =0..100 slider position 
    uint16_t rpmFlashControl; // =0..10000 slider position
    uint16_t rpmStartControl; // =0..10000 slider position
    uint16_t rpmIncrement; // =0..1000 slider position

    // other variable
    uint8_t connect_flag;  // =1 if wire connected, else =0

} RemoteXY;
#pragma pack(pop)

// Constants
constexpr uint16_t DEFAULT_RPM_START = 3000;
constexpr uint16_t DEFAULT_RPM_INCREMENT = 500;
constexpr uint16_t DEFAULT_RPM_FLASH = 6700;
constexpr uint8_t DEFAULT_BRIGHTNESS = 50;
constexpr uint16_t FLASH_INTERVAL_MS = 100;

// LED configuration
constexpr uint8_t NUM_PIXELS = 8;
constexpr uint8_t GREEN_LEDS = 3;
constexpr uint8_t YELLOW_LEDS = 3;
constexpr uint8_t RED_LEDS = 2;

// Pin definitions
constexpr uint8_t CAN_TX_PIN = 43;
constexpr uint8_t CAN_RX_PIN = 44;
constexpr uint8_t NEOPIXEL_PIN = 1;

// CAN configuration
constexpr uint32_t RPM_CAN_ID = 0x0A5;
constexpr uint8_t RPM_DATA_OFFSET = 5;

// Color definitions
constexpr uint32_t COLOR_GREEN = 0x00FF00;
constexpr uint32_t COLOR_YELLOW = 0xFFFF00;
constexpr uint32_t COLOR_RED = 0xFF0000;
constexpr uint32_t COLOR_WHITE = 0xFFFFFF;
constexpr uint32_t COLOR_OFF = 0x000000;

// Global variables
struct Settings {
    uint16_t rpm_start;
    uint16_t rpm_increment;
    uint16_t rpm_flash;
    uint8_t brightness;
} settings;

Adafruit_NeoPixel pixels(NUM_PIXELS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);
uint16_t currentRPM = 0;
bool flashState = false;
unsigned long lastFlashTime = 0;
nvs_handle_t rpm_nvs_handle;

// Rest of the functions remain the same as in the previous version...

// Function prototypes
void initNVS();
void initTWAI();
void startupSequence();
void updateLEDs();
void processRPMData(const uint8_t* buf, int dlc);
void updateRPMSettings();
esp_err_t loadNVSValue(const char* key, int32_t& value, int32_t defaultValue);

void setup() {
    Serial.begin(115200);
    
    initNVS();
    initTWAI();
    
    RemoteXY_Init();
    
    // Initialize RemoteXY values from settings
    RemoteXY.powerSwitch = 1;
    RemoteXY.brightness = settings.brightness;
    RemoteXY.rpmFlashControl = settings.rpm_flash;
    RemoteXY.rpmStartControl = settings.rpm_start;
    RemoteXY.rpmIncrement = settings.rpm_increment;
    
    pixels.begin();
    pixels.setBrightness(settings.brightness);
    pixels.show();
    
    startupSequence();

    // Debug print initial values
    /*
    Serial.println("Initial settings:");
    Serial.printf("RPM Start: %d\n", settings.rpm_start);
    Serial.printf("RPM Increment: %d\n", settings.rpm_increment);
    Serial.printf("RPM Flash: %d\n", settings.rpm_flash);
    Serial.printf("Brightness: %d\n", settings.brightness);
    */
}

void loop() {
    RemoteXY_Handler();
    
    // Debug print when values change
    static uint16_t last_rpm_flash = 0;
    static uint16_t last_rpm_start = 0;
    static uint16_t last_rpm_increment = 0;

    if (last_rpm_flash != RemoteXY.rpmFlashControl ||
        last_rpm_start != RemoteXY.rpmStartControl ||
        last_rpm_increment != RemoteXY.rpmIncrement) {

        /*
        Serial.println("\nControl values changed:");
        Serial.printf("RPM Flash: %d\n", RemoteXY.rpmFlashControl);
        Serial.printf("RPM Start: %d\n", RemoteXY.rpmStartControl);
        Serial.printf("RPM Increment: %d\n", RemoteXY.rpmIncrement);
        */

        last_rpm_flash = RemoteXY.rpmFlashControl;
        last_rpm_start = RemoteXY.rpmStartControl;
        last_rpm_increment = RemoteXY.rpmIncrement;
    }
    
    // Update LED brightness based on power switch
    pixels.setBrightness(RemoteXY.powerSwitch ? RemoteXY.brightness : 0);
    
    if (RemoteXY.powerSwitch) {
        updateRPMSettings();
    }
    
    // Check for TWAI messages
    twai_message_t message;
    if (twai_receive(&message, pdMS_TO_TICKS(10)) == ESP_OK) {
        processRPMData(message.data, message.data_length_code);
    }
    
    updateLEDs();
    RemoteXY_delay(60);
}


void initTWAI() {
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)CAN_TX_PIN, (gpio_num_t)CAN_RX_PIN, TWAI_MODE_NORMAL);
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
    twai_filter_config_t f_config = {
        .acceptance_code = (RPM_CAN_ID << 21),
        .acceptance_mask = ~(RPM_CAN_ID << 21),
        .single_filter = true
    };
    
    ESP_ERROR_CHECK(twai_driver_install(&g_config, &t_config, &f_config));
    ESP_ERROR_CHECK(twai_start());
}

void updateLEDs() {
    if (currentRPM >= settings.rpm_flash) {
        // Flash all LEDs white when above flash threshold
        if (millis() - lastFlashTime > FLASH_INTERVAL_MS) {
            flashState = !flashState;
            lastFlashTime = millis();
            
            const uint32_t color = flashState ? COLOR_WHITE : COLOR_OFF;
            for (uint8_t i = 0; i < NUM_PIXELS; i++) {
                pixels.setPixelColor(i, color);
            }
        }
    } else {
        // Calculate LEDs to light based on RPM
        const int ledsToLight = currentRPM >= settings.rpm_start ? 
            constrain((currentRPM - settings.rpm_start) / settings.rpm_increment + 1, 0, NUM_PIXELS) : 0;
        
        // Update LED colors
        for (uint8_t i = 0; i < NUM_PIXELS; i++) {
            uint32_t color = COLOR_OFF;
            if (i < ledsToLight) {
                if (i < GREEN_LEDS) color = COLOR_GREEN;
                else if (i < (GREEN_LEDS + YELLOW_LEDS)) color = COLOR_YELLOW;
                else color = COLOR_RED;
            }
            pixels.setPixelColor(i, color);
        }
    }
    pixels.show();
}

void processRPMData(const uint8_t* buf, int dlc) {
    if (dlc > RPM_DATA_OFFSET + 1) {
        currentRPM = buf[RPM_DATA_OFFSET] | (buf[RPM_DATA_OFFSET + 1] << 8);
    }
}

void updateRPMSettings() {
    bool needs_commit = false;
    
    // Check and update RPM flash point
    if (RemoteXY.rpmFlashControl != settings.rpm_flash) {
        settings.rpm_flash = RemoteXY.rpmFlashControl;
        ESP_ERROR_CHECK(nvs_set_i32(rpm_nvs_handle, "rpm_flash", settings.rpm_flash));
        needs_commit = true;
        //Serial.printf("Updated RPM Flash to: %d\n", settings.rpm_flash);
    }

    // Check and update RPM start point
    if (RemoteXY.rpmStartControl != settings.rpm_start) {
        settings.rpm_start = RemoteXY.rpmStartControl;
        ESP_ERROR_CHECK(nvs_set_i32(rpm_nvs_handle, "rpm_start", settings.rpm_start));
        needs_commit = true;
        //Serial.printf("Updated RPM Start to: %d\n", settings.rpm_start);
    }

    // Check and update RPM increment
    if (RemoteXY.rpmIncrement != settings.rpm_increment) {
        settings.rpm_increment = RemoteXY.rpmIncrement;
        ESP_ERROR_CHECK(nvs_set_i32(rpm_nvs_handle, "rpm_increment", settings.rpm_increment));
        needs_commit = true;
        //Serial.printf("Updated RPM Increment to: %d\n", settings.rpm_increment);
    }

    // Check and update brightness
    if (RemoteXY.brightness != settings.brightness) {
        settings.brightness = RemoteXY.brightness;
        ESP_ERROR_CHECK(nvs_set_i32(rpm_nvs_handle, "brightness", settings.brightness));
        needs_commit = true;
        //Serial.printf("Updated Brightness to: %d\n", settings.brightness);
    }
    
    if (needs_commit) {
        ESP_ERROR_CHECK(nvs_commit(rpm_nvs_handle));
    }
}

esp_err_t loadNVSValue(const char* key, int32_t& value, int32_t defaultValue) {
    int32_t stored_value;
    esp_err_t err = nvs_get_i32(rpm_nvs_handle, key, &stored_value);
    if (err == ESP_OK) {
        value = stored_value;
    } else {
        value = defaultValue;
    }
    return err;
}

void initNVS() {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    
    ESP_ERROR_CHECK(nvs_open("rpm_storage", NVS_READWRITE, &rpm_nvs_handle));
    
    // Load all settings with defaults
    loadNVSValue("rpm_start", (int32_t&)settings.rpm_start, DEFAULT_RPM_START);
    loadNVSValue("rpm_increment", (int32_t&)settings.rpm_increment, DEFAULT_RPM_INCREMENT);
    loadNVSValue("rpm_flash", (int32_t&)settings.rpm_flash, DEFAULT_RPM_FLASH);
    loadNVSValue("brightness", (int32_t&)settings.brightness, DEFAULT_BRIGHTNESS);
}

void startupSequence() {
    const uint32_t colors[] = {COLOR_GREEN, COLOR_YELLOW, COLOR_RED};
    const uint8_t sections[] = {GREEN_LEDS, YELLOW_LEDS, RED_LEDS};
    uint8_t currentLED = 0;
    
    for (uint8_t section = 0; section < 3; section++) {
        for (uint8_t i = 0; i < sections[section]; i++) {
            pixels.setPixelColor(currentLED++, colors[section]);
            pixels.show();
            delay(50);
        }
        delay(200);
    }
    
    pixels.clear();
    pixels.show();
}