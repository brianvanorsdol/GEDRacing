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


// you can enable debug logging to Serial at 115200
//#define REMOTEXY__DEBUGLOG    

// RemoteXY connection settings 
#define REMOTEXY_BLUETOOTH_NAME "Shift Light Controller"

// IMPORTANT: Include RemoteXY.h AFTER the configuration defines
#include <RemoteXY.h>

// RemoteXY GUI configuration  
#pragma pack(push, 1)  
uint8_t RemoteXY_CONF[] =   // 223 bytes
  { 255,8,0,0,0,216,0,19,0,0,0,83,104,105,102,116,32,76,105,103,
  104,116,115,0,25,1,106,200,1,1,11,0,129,7,6,90,7,64,17,65,
  57,48,32,83,117,112,114,97,32,83,104,105,102,116,32,76,105,103,104,116,
  32,67,111,110,116,114,111,108,0,129,8,28,34,11,64,17,80,111,119,101,
  114,58,0,2,54,25,39,18,0,2,26,31,31,79,78,0,79,70,70,0,
  4,7,76,90,14,128,2,26,129,22,59,55,11,64,17,66,114,105,103,104,
  116,110,101,115,115,58,0,7,58,109,40,10,118,0,2,26,2,129,15,112,
  39,8,64,17,83,104,105,102,116,32,80,111,105,110,116,58,0,129,6,128,
  49,7,64,17,76,69,68,32,83,116,97,114,116,32,80,111,105,110,116,58,
  0,7,58,125,40,10,118,0,2,26,2,129,8,143,48,7,64,17,76,69,
  68,32,73,110,99,114,101,109,101,110,116,58,0,7,58,140,40,10,118,0,
  2,26,2 };
  
// this structure defines all the variables and events of your control interface 
struct {
  uint8_t powerSwitch; // =1 if switch ON and =0 if OFF, INPUT
  uint8_t brightness; // from 0 to 100, INPUT
  uint16_t rpmFlashControl; // positive number, INPUT
  uint16_t rpmStartControl;
  uint16_t rpmIncrement;

    // other variable
  uint8_t connect_flag;  // =1 if wire connected, else =0

} RemoteXY;   
#pragma pack(pop)

// Pin definitions
#define CAN_TX_PIN 8
#define CAN_RX_PIN 7
#define NEOPIXEL_PIN 1

// CAN configuration
#define RPM_CAN_ID 0x0A5
#define RPM_DATA_OFFSET 5  // Offset in the data buffer for RPM value

// LED configuration
#define NUM_PIXELS 8  // Total number of LEDs
#define GREEN_LEDS 3
#define YELLOW_LEDS 3
#define RED_LEDS 2

// Configurable RPM thresholds
uint16_t RPM_START = 3000;    // RPM at which LEDs start lighting up
uint16_t RPM_INCREMENT = 500; // RPM increment for each LED
uint16_t RPM_FLASH = 6700;    // RPM at which LEDs start flashing
uint16_t BRIGHTNESS = 50;

// Color definitions
const uint32_t COLOR_GREEN = 0x00FF00;
const uint32_t COLOR_YELLOW = 0xFFFF00;
const uint32_t COLOR_RED = 0xFF0000;
const uint32_t COLOR_WHITE = 0xFFFFFF;
const uint32_t COLOR_OFF = 0x000000;

// Create NeoPixel object
Adafruit_NeoPixel pixels(NUM_PIXELS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

// Variables for RPM handling
uint16_t currentRPM = 0;
bool flashState = false;
unsigned long lastFlashTime = 0;
const int flashInterval = 100;  // Flash interval in milliseconds

nvs_handle_t rpm_nvs_handle;


void setup() {
    Serial.begin(115200);
    delay(100);  // Give serial time to initialize

    Serial.println("Starting NVS Initialization");
    initNVS();
    Serial.println("NVS Initialized Successfully");

    Serial.println("\nStarting TWAI initialization...");
    // Initialize TWAI with correct GPIO pin type casting
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)CAN_TX_PIN, (gpio_num_t)CAN_RX_PIN, TWAI_MODE_NORMAL);
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
    
    // Configure filter to only accept RPM message ID
    twai_filter_config_t f_config = {
        .acceptance_code = (RPM_CAN_ID << 21),
        .acceptance_mask = ~(RPM_CAN_ID << 21),
        .single_filter = true
    };
    
    // Install TWAI driver
    esp_err_t result = twai_driver_install(&g_config, &t_config, &f_config);
    if (result != ESP_OK) {
        Serial.printf("Failed to install TWAI driver. Error: %d\n", result);
        return;
    }
    Serial.println("TWAI driver installed successfully");
    
    // Start TWAI driver
    result = twai_start();
    if (result != ESP_OK) {
        Serial.printf("Failed to start TWAI driver. Error: %d\n", result);
        return;
    }
    Serial.println("TWAI started successfully");
    
    // Initialize RemoteXY Service
    RemoteXY_Init();
    RemoteXY.powerSwitch = 1;
    Serial.println("RemoteXY Initialized");

    RemoteXY.rpmStartControl = RPM_START;
    RemoteXY.rpmIncrement = RPM_INCREMENT;
    RemoteXY.rpmFlashControl = RPM_FLASH;
    RemoteXY.brightness = BRIGHTNESS;
    RemoteXY.powerSwitch = 1;

    // Initialize NeoPixels
    pixels.begin();
    pixels.setBrightness(BRIGHTNESS);
    pixels.show();
    
    // Initialize NeoPixel Startup Sequence
    startupSequence();

    Serial.println("Setup complete");
}

void loop() 
{
    RemoteXY_Handler();

    if(RemoteXY.powerSwitch == 1){
      pixels.setBrightness(RemoteXY.brightness);
      rpmValueUpdate();
    }else{
        pixels.setBrightness(0);
    }
    // Check for TWAI messages
    twai_message_t message;
    esp_err_t result = twai_receive(&message, pdMS_TO_TICKS(10));
    
    if (result == ESP_OK) {
        Serial.println("\nReceived CAN message:");
        Serial.printf("Message ID: 0x%03X\n", message.identifier);
        Serial.printf("Data Length: %d\n", message.data_length_code);
        Serial.printf("Flags: %02X\n", message.flags);
        
        Serial.print("Raw data: ");
        for (int i = 0; i < message.data_length_code; i++) {
            Serial.printf("%02X ", message.data[i]);
        }
        Serial.println();
        
        processRPMData(message.data, message.data_length_code);
    } else if (result == ESP_ERR_TIMEOUT) {
        // No message received within timeout
    } else {
        Serial.printf("Error receiving message: %d\n", result);
    }
    
    updateLEDs();
    
    RemoteXY_delay(60); //Small delay to prevent overwhelming the BLE connection
    
}

// Function to process RPM data from CAN message
void processRPMData(const uint8_t* buf, int dlc) {
    Serial.println("Processing RPM data:");
    Serial.printf("Data Length Code: %d\n", dlc);
    
    // Print all bytes in the message
    Serial.print("All message bytes: ");
    for(int i = 0; i < dlc; i++) {
        Serial.printf("%02X ", buf[i]);
    }
    Serial.println();
    
    // Print specific RPM bytes
    Serial.printf("RPM byte at offset %d: %02X\n", RPM_DATA_OFFSET, buf[RPM_DATA_OFFSET]);
    Serial.printf("RPM byte at offset %d: %02X\n", RPM_DATA_OFFSET + 1, buf[RPM_DATA_OFFSET + 1]);
    
    unsigned int rawRPM = (buf[RPM_DATA_OFFSET] | (buf[RPM_DATA_OFFSET + 1] << 8));
    Serial.println(rawRPM);
    currentRPM = rawRPM;
    
    Serial.printf("Calculated RPM value: %d\n", currentRPM);
    Serial.println("--------------------");
}

//Function to update the LEDs on every packet received from the CANBUS
void updateLEDs() {

  if (currentRPM >= RPM_FLASH) {
    // Flash all LEDs white when above flash threshold
    if (millis() - lastFlashTime > flashInterval) {
      flashState = !flashState;
      lastFlashTime = millis();
      
      uint32_t flashColor = flashState ? COLOR_WHITE : COLOR_OFF;
      for (int i = 0; i < NUM_PIXELS; i++) {
        pixels.setPixelColor(i, flashColor);
      }
    }
  } else {
    // Calculate how many LEDs should be lit based on RPM
    int ledsToLight = 0;
    if (currentRPM >= RPM_START) {
      ledsToLight = (currentRPM - RPM_START) / RPM_INCREMENT + 1;
      ledsToLight = constrain(ledsToLight, 0, NUM_PIXELS);
    }
    
    // Set LED colors based on position and number to light
    for (int i = 0; i < NUM_PIXELS; i++) {
      if (i < ledsToLight) {
        // Determine color based on LED position
        if (i < GREEN_LEDS) {
          pixels.setPixelColor(i, COLOR_GREEN);
        } else if (i < (GREEN_LEDS + YELLOW_LEDS)) {
          pixels.setPixelColor(i, COLOR_YELLOW);
        } else {
          pixels.setPixelColor(i, COLOR_RED);
        }
      } else {
        pixels.setPixelColor(i, COLOR_OFF);
      }
    }
  }
  
  pixels.show();
}

//Function to process changes to RPM_START, RPM_INCREMENT, and RPM_FLASH based on user input in RemoteXY as well as read/write stored values in NVS
void rpmValueUpdate(){
   esp_err_t err;
    bool needs_commit = false;

    if(RemoteXY.rpmFlashControl != RPM_FLASH) {
        RPM_FLASH = RemoteXY.rpmFlashControl;
        err = nvs_set_i32(rpm_nvs_handle, "rpm_flash", RPM_FLASH);
        if (err != ESP_OK) {
            Serial.println("Error saving RPM_FLASH!");
        }
        needs_commit = true;
    }

    if(RemoteXY.rpmStartControl != RPM_START) {
        RPM_START = RemoteXY.rpmStartControl;
        err = nvs_set_i32(rpm_nvs_handle, "rpm_start", RPM_START);
        if (err != ESP_OK) {
            Serial.println("Error saving RPM_START!");
        }
        needs_commit = true;
    }

    if(RemoteXY.rpmIncrement != RPM_INCREMENT) {
        RPM_INCREMENT = RemoteXY.rpmIncrement;
        err = nvs_set_i32(rpm_nvs_handle, "rpm_increment", RPM_INCREMENT);
        if (err != ESP_OK) {
            Serial.println("Error saving RPM_INCREMENT!");
        }
        needs_commit = true;
    }

    if(RemoteXY.brightness != BRIGHTNESS) {
        BRIGHTNESS = RemoteXY.brightness;
        err = nvs_set_i32(rpm_nvs_handle, "brightness", BRIGHTNESS);
        if (err != ESP_OK) {
            Serial.println("Error saving BRIGHTNESS!");
        }
        needs_commit = true;
    }

    // Only commit if we've made changes
    if (needs_commit) {
        err = nvs_commit(rpm_nvs_handle);
        if (err != ESP_OK) {
            Serial.println("Error committing values to NVS!");
        }
    }
}

//LED sequence that plays upon startup to test functionality
void startupSequence() {
  // Test green LEDs
  for (int i = 0; i < GREEN_LEDS; i++) {
    pixels.setPixelColor(i, COLOR_GREEN);
    pixels.show();
    delay(50);
  }
  delay(200);
  
  // Test yellow LEDs
  for (int i = GREEN_LEDS; i < GREEN_LEDS + YELLOW_LEDS; i++) {
    pixels.setPixelColor(i, COLOR_YELLOW);
    pixels.show();
    delay(50);
  }
  delay(200);
  
  // Test red LEDs
  for (int i = GREEN_LEDS + YELLOW_LEDS; i < NUM_PIXELS; i++) {
    pixels.setPixelColor(i, COLOR_RED);
    pixels.show();
    delay(50);
  }
  delay(200);
  
  // Clear all LEDs
  pixels.clear();
  pixels.show();
}

void initNVS() {
    // Initialize NVS
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    
    // Open NVS namespace
    err = nvs_open("rpm_storage", NVS_READWRITE, &rpm_nvs_handle);
    if (err != ESP_OK) {
        Serial.println("Error opening NVS handle!");
        return;
    }

    // Read stored values or use defaults if not found
    int32_t stored_value;
    
    // Read RPM_START
    err = nvs_get_i32(rpm_nvs_handle, "rpm_start", &stored_value);
    if (err == ESP_OK) {
        RPM_START = stored_value;
        RemoteXY.rpmStartControl = stored_value;
    }
    
    // Read RPM_INCREMENT
    err = nvs_get_i32(rpm_nvs_handle, "rpm_increment", &stored_value);
    if (err == ESP_OK) {
        RPM_INCREMENT = stored_value;
        RemoteXY.rpmIncrement = stored_value;
    }
    
    // Read RPM_FLASH
    err = nvs_get_i32(rpm_nvs_handle, "rpm_flash", &stored_value);
    if (err == ESP_OK) {
        RPM_FLASH = stored_value;
        RemoteXY.rpmFlashControl = stored_value;
    }

    // Read BRIGHTNESS
    err = nvs_get_i32(rpm_nvs_handle, "brightness", &stored_value);
    if (err == ESP_OK) {
        BRIGHTNESS = stored_value;
        RemoteXY.brightness = stored_value;
    }
}


// Cleanup function - call this if you need to reinstall the driver
void cleanupTWAI() {
  twai_stop();
  twai_driver_uninstall();
  nvs_close(rpm_nvs_handle);
}

