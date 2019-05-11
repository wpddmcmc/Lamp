/*********************************************************************************************
 * WS2812b WiFi Remote LED Desk Lamp based on ESP8266
 * Author: TGeek
 * Date: 11/5/2019
 * Description: This a gift for my girlfriend, which can be control by button and Smartphone App,
 * Thanks for the idea form 知乎@英语老师摸我腿, GitHub@scottlawsonbc
 ********************************************************************************************/
 
#include <FS.h>                   // this needs to be first, or it all crashes and burns...
//#define BLYNK_DEBUG             // Comment this out to disable debug and save space
#define BLYNK_PRINT Serial        // Comment this out to disable prints and save space

#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          // https://github.com/tzapu/WiFiManager
#define FASTLED_ESP8266_RAW_PIN_ORDER

#include "FastLED.h"              // LED control head file

#include <ArduinoJson.h>          // https://github.com/bblanchon/ArduinoJson
#include <BlynkSimpleEsp8266.h>
bool shouldSaveConfig = false;    // flag for saving data

// FastLED Params
char blynk_token[34] = "";
#define LED_PIN 5                 // GPIO5 - Port D1 for data output to LED
#define NUM_LEDS 60               // Number of LED on WS2812b
#define GPIO_Pin 12               // GPIN12 - Pin D6 for Button contorl
#define LED_TYPE WS2812B          // LED type
#define COLOR_ORDER GRB           // Color order
CRGB leds[NUM_LEDS];

// RGB and brightness
int r = 255;
int g = 255;
int b = 255;
int led_bright = 128;

// Main switch and Rainbow Mode
int masterSwitch = 1;
int autoMode = 0;
int mode = 0;
uint8_t gHue = 0; 


/************************************************* 
    Function:       saveConfigCallback 
    Description:    Callback notifying us of the need to save config
    *************************************************/
void saveConfigCallback () {  //callback notifying us of the need to save config
  Serial.println("Should save config");
  shouldSaveConfig = true;
}


/************************************************* 
    Function:       setup 
    Description:    Setup system
    *************************************************/
void setup()
{
    delay(1000);            // Wait for capacitor discharging 
  Serial.begin(115200);   // Serial communication Frequency
  Serial.println();
  
  attachInterrupt(digitalPinToInterrupt(GPIO_Pin), IntCallback, RISING);  // Rising Interrupt

  // Fast LED setup
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalSMD5050);
  FastLED.setBrightness(led_bright);
  #define FRAMES_PER_SECOND 120
  
  // LED switch on
  fill_gradient(leds, NUM_LEDS, CHSV(50, 255,255), CHSV(100,255,255), LONGEST_HUES); 
  for (int i = 0; i < NUM_LEDS; i++){
      leds[i] =  CRGB::White;
      FastLED.show();
      delay(60);
    }
  for (int i = 0; i < NUM_LEDS; i++){
      leds[i] =  CRGB::Black;
      FastLED.show();
      delay(50);
    }
  FastLED.show();

  //SPIFFS.format();                    // clean FS, for testing
  Serial.println("Mounting FS...");     // read configuration from FS json

  if (SPIFFS.begin()) {
    Serial.println("Mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("Reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("Opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");

          strcpy(blynk_token, json["blynk_token"]);

        } else {
          Serial.println("Failed to load json config");
        }
      }
    }
  } else {
    Serial.println("Failed to mount FS");
  }
  // end read

  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length
  WiFiManagerParameter custom_blynk_token("blynk", "blynk token", blynk_token, 33);   // was 32 length
  
  Serial.println(blynk_token);

  // WiFiManager
  // Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

  wifiManager.setSaveConfigCallback(saveConfigCallback);   //set config save notify callback

  // set static ip
  // this is for connecting to Office router not GargoyleTest but it can be changed in AP mode at 192.168.4.1
  // wifiManager.setSTAStaticIPConfig(IPAddress(192,168,10,111), IPAddress(192,168,10,90), IPAddress(255,255,255,0));
  
  wifiManager.addParameter(&custom_blynk_token);   //add all your parameters here

  // wifiManager.resetSettings();  //reset settings - for testing

  // set minimu quality of signal so it ignores AP's under that quality
  // defaults to 8%
  // wifiManager.setMinimumSignalQuality();
  
  // sets timeout until configuration portal gets turned off
  // useful to make it all retry or go to sleep, in seconds
  wifiManager.setTimeout(6000);   // 10 minutes to enter data and then Wemos resets to try again.

  // fetches ssid and pass and tries to connect, if it does not connect it starts an access point with the specified name
  // and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect("SmartLed")) {
    Serial.println("Failed to connect and hit timeout");
    delay(3000);
    // reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  }

  strcpy(blynk_token, custom_blynk_token.getValue());    // read updated parameters

  if (shouldSaveConfig) {      // save the custom parameters to FS
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["blynk_token"] = blynk_token;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("Failed to open config file for writing");
    }
    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
    //end save
  }

  Serial.println("local ip");
  Serial.println(WiFi.localIP());
  
  Blynk.config(blynk_token, "smartled.cc", 8080);
  Blynk.connect();
  Blynk.syncAll(); 
}

/************************************************* 
    Function:       loop 
    Description:    Main function for Wifi control
    *************************************************/
void loop()
{
  FastLED.setBrightness(led_bright);
  
  Blynk.run();                        // Run Blynk

  // Switch off
  if(masterSwitch == 0) {
    for (int i = 0; i < NUM_LEDS; i++) {
      leds[i] = CRGB::Black;
      FastLED.show();
    }
  }
  
  // Light on as params in Blynk
  if(autoMode == 0 && masterSwitch == 1) {
    for (int i = 0; i < NUM_LEDS; i++){
      leds[i] = CRGB(r, g, b);
      FastLED.show();
    }
  }

  // Light on as Rainbow Mode
  if(autoMode == 1 && masterSwitch == 1) {
    fill_rainbow( leds, NUM_LEDS, gHue, 7);
    FastLED.show(); 
    EVERY_N_MILLISECONDS(20) {
      gHue++; 
    } 
  }

}

/************************************************* 
    Description:    Interrupt for button
    *************************************************/
void IntCallback()
{ 
  mode ++;    // Mode flag

  // Normal mode
  if(mode == 1)
  {
    FastLED.setBrightness(led_bright);
    for (int i = 0; i < NUM_LEDS; i++){
      leds[i] = CRGB(255, 255, 255);
      FastLED.show();
    }
  }
  
  // Dark mode
  if(mode == 2)
  {
    FastLED.setBrightness(80);
    for (int i = 0; i < NUM_LEDS; i++){
      leds[i] = CRGB(255, 255, 80);
      FastLED.show();
    }
  }

  // Rainbow mode
  if(mode == 3)
  {
    FastLED.setBrightness(led_bright);
    fill_rainbow( leds, NUM_LEDS, gHue, 7); 
    FastLED.show(); 
  }

  // Off
  if(mode == 4)
  {
    for (int i = 0; i < NUM_LEDS; i++){
      leds[i] = CRGB::Black;
      FastLED.show();
    }
    mode = 0;
  }
  
}

//---Main Button---
BLYNK_WRITE(V0) {
  masterSwitch = param.asInt();
}

//--- R Slider ---
BLYNK_WRITE(V1) {
  r = param.asInt();
}

//--- G Slider ---
BLYNK_WRITE(V2) {
  g = param.asInt();
}

//--- B Slider ---
BLYNK_WRITE(V3) {
  b = param.asInt(); 
}

//--- Rainbow Button ---
BLYNK_WRITE(V4) {
  autoMode = param.asInt();
}

//--- Light Slider ---
BLYNK_WRITE(V5) {
  led_bright = param.asInt();
}
