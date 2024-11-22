#include <SPI.h>
#include <mcp_can.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_NeoPixel.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C

#define LED_PIN     3 // The pin connected to the NeoPixel data input
#define NUM_LEDS    8 // Number of NeoPixels in the strip
#define BRIGHTNESS_PIN A0 // Analog pin for potentiometer

// Configurable RPM thresholds
#define RPM_START 3000 // RPM at which LEDs start lighting up
#define RPM_INCREMENT 500 // RPM increment for each LED
#define RPM_FLASH 6700 // RPM at which LEDs start flashing

// LED color change points (number of LEDs for each color)
#define GREEN_LEDS 3
#define YELLOW_LEDS 3
#define RED_LEDS 2

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

#define CAN0_INT 2
MCP_CAN CAN0(10);

long unsigned int rxId;
unsigned char len = 0;
unsigned char rxBuf[8];

const unsigned long RPM_CAN_ID = 0x0A5;
const int RPM_DATA_OFFSET = 5;
const int RPM_DATA_LENGTH = 2;
const float RPM_SCALE = 0.25;
const float RPM_DIVISOR = 1.0;

int currentRPM = 0;
unsigned long lastFlashTime = 0;
bool flashState = false;

void setup() {
  Serial.begin(115200);
  
  // Initialize OLED display
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.println(F("RPM Reader"));
  display.display();
  delay(2000);

  // Initialize NeoPixel strip
  strip.begin();
  strip.show(); // Initialize all pixels to 'off'

  // Initialize CAN Bus in read-only mode
  if(CAN0.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ) == CAN_OK)
  {
    CAN0.setMode(MCP_NORMAL);
    Serial.println("MCP2515 Initialized");
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0,0);
    display.println("CAN Init OK");
    display.display();
    delay(3000);
  }
  else
  {
    Serial.println("Error Initializing MCP2515...");
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0,0);
    display.println("CAN Init");
    display.println("FAILED");
    display.display();
    for(;;);
  }

  pinMode(CAN0_INT, INPUT);
  pinMode(BRIGHTNESS_PIN, INPUT);

  CAN0.init_Mask(0, 0, 0x7FF);
  CAN0.init_Filt(0, 0, RPM_CAN_ID);
  Serial.println("CAN filter set up for ID 0x0A5");
}

void loop() {
  if(!digitalRead(CAN0_INT))
  {
    CAN0.readMsgBuf(&rxId, &len, rxBuf);
    
    if(rxId == RPM_CAN_ID)
    {
      processRPMData(rxBuf);
      updateDisplay();
      updateLEDs();
      printDebugInfo();
      
    }
  }
  
  // Update brightness based on potentiometer
  int brightness = map(analogRead(BRIGHTNESS_PIN), 0, 1023, 0, 255);
  strip.setBrightness(brightness);
  strip.show();
}

void processRPMData(const uint8_t* buf) {
  const int rawRPM = (buf[RPM_DATA_OFFSET] | (buf[RPM_DATA_OFFSET + 1] << 8));
  currentRPM = (rawRPM * RPM_SCALE) / RPM_DIVISOR;
  
}

void updateDisplay() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.println(F("Engine RPM:"));
  display.setTextSize(5);
  display.setCursor(0,20);
  display.println(currentRPM);
  display.display();
}

void updateLEDs() {
  if(currentRPM >= RPM_FLASH) {
    // Flash all LEDs white when RPM is at or above flash threshold
    unsigned long currentTime = millis();
    if (currentTime - lastFlashTime >= 100) { // Flash every 100ms
      lastFlashTime = currentTime;
      flashState = !flashState;
    }
    
    uint32_t color = flashState ? strip.Color(255, 255, 255) : strip.Color(0, 0, 0);
    for(int i = 0; i < NUM_LEDS; i++) {
      strip.setPixelColor(i, color);
    }
  } else {
    // Normal operation for RPM below flash threshold
    for(int i = 0; i < NUM_LEDS; i++) {
      int ledRPM = RPM_START + (i * RPM_INCREMENT);
      if(currentRPM >= ledRPM) {
        uint32_t color;
        if(i < GREEN_LEDS) {
          color = strip.Color(0, 255, 0);  // Green
        } else if(i < GREEN_LEDS + YELLOW_LEDS) {
          color = strip.Color(255, 255, 0);  // Yellow
        } else {
          color = strip.Color(255, 0, 0);  // Red
        }
        strip.setPixelColor(i, color);
      } else {
        strip.setPixelColor(i, strip.Color(0, 0, 0));  // Off
      }
    }
  }
  
  strip.show();
}

void printDebugInfo() {
  Serial.print("ID: 0x");
  Serial.print(rxId, HEX);
  Serial.print("  Data: ");
  for(int i = 0; i<len; i++) {
    if(rxBuf[i] < 0x10) Serial.print("0");
    Serial.print(rxBuf[i], HEX);
    Serial.print(" ");
  }
  Serial.print("CURRENT RPM: ");
  Serial.print(currentRPM);
  Serial.println();
}