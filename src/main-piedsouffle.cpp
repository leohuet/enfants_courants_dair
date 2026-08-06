#include "Arduino.h"

#include <ESPUI.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <OSCMessage.h>

#include "driver/rtc_io.h"

#include "wifi_config.h"
#include "persistentValue.h"
#include "define_functions.h"

#define NUM_PARAMS 2


OSCParam oscParams[NUM_PARAMS];

baseOSCParam baseOscParams[] = {
  {"1_wind_speed", "/1/wind_speed", 0, 100, true, false}
};


const uint8_t size_mean = 4;
uint16_t value_for_mean[size_mean];

float data = 0.0;
float old_data = 0.0;


void setup(){
  // open serial for USB and radar UART
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(VBUS_SENSE_PIN, INPUT);
  pinMode(BATT_PIN, INPUT);
  pinMode(LED_R_PIN, OUTPUT);
  pinMode(LED_G_PIN, OUTPUT);
  pinMode(LED_B_PIN, OUTPUT);
  pinMode(D5, INPUT);
  delay(2000);
  begin_wifi();
  delay(2000);

  if(espUiOn){
    // ESPUI control init
    ESPUI.begin("Les enfants des courants d'air");
    setupUI();
  }
  
  delay(2000);
  xTaskCreatePinnedToCore(
    vbusWatcherTask,   // task function
    "vbusWatcher",     // name
    2048,              // stack size (bytes)
    NULL,              // parameters
    1,                 // priority (low is fine here)
    NULL,              // task handle (not needed)
    0                  // core to pin to (0 or 1)
  );

  Serial.println("Starting programm..");
}

void loop(){
  int readingFreq = 100;
  int inactTimer = 1000;
  bool started = true;
  if(espUiOn){
    readingFreq = readingFrequency->getInt();
    started = isStarted->getBool();
  }
  
  if((millis() - lastReading) > readingFreq && started){
    lastReading = millis();
    data = analogRead(D5);
    data = moyenne_glissante(value_for_mean, size_mean, data) / 3500.0;
    if(data != old_data && data >= 0.10){
      data = data - 0.10;
      old_data = data;
      sendData(0, data);
    }
  }
  delay(20);
}