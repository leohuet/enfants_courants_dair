#include "Arduino.h"

#include <ESPUI.h>

#include <WiFiUdp.h>
#include <EthernetUdp.h>
#include <OSCMessage.h>

#include "driver/rtc_io.h"

#include "wifi_config.h"
#include "persistentValue.h"
#include "define_functions.h"


#define WIND_SENS_PIN GPIO_NUM_6
#define NUM_PARAMS 1


OSCParam oscParams[NUM_PARAMS];

baseOSCParam baseOscParams[] = {
  {"1_wind_speed", "%", "/1/wind_speed", 0, 100, 5, 0, 0, 0, true, false}
};

uint8_t size_mean = 5;
uint16_t value_for_mean[MAX_MEAN_SIZE];

float windData = 0.0;
float windOldData = 0.0;
int oldWindNoise = 0;

bool espUiOn = true;

void setup(){
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(VBUS_SENSE_PIN, INPUT);
  pinMode(BATT_PIN, INPUT);
  pinMode(WIND_SENS_PIN, INPUT);
  pinMode(LED_R_PIN, OUTPUT);
  pinMode(LED_G_PIN, OUTPUT);
  pinMode(LED_B_PIN, OUTPUT);
  delay(2000);
  w5500PowerUp();
  delay(1000);
  onEthernetBool = begin_ethernet(LED_R_PIN);
  delay(2000);
  if(onEthernetBool) ethUdp.begin(8888);
  else begin_wifi(LED_R_PIN);

  if(espUiOn){
    // ESPUI control init
    ESPUI.begin("Les enfants des courants d'air");
    setupUI(1);
    delay(2000);
    onBatteryBool = onBattery->getBool();
  }

  digitalWrite(LED_R_PIN, HIGH);
  digitalWrite(LED_G_PIN, LOW);
  digitalWrite(LED_B_PIN, LOW);
  delay(500);
  digitalWrite(LED_R_PIN, LOW);
  digitalWrite(LED_G_PIN, HIGH);
  digitalWrite(LED_B_PIN, LOW);
  delay(500);

  if(onBatteryBool){
    xTaskCreatePinnedToCore(
      vbusWatcherTask,   // task function
      "vbusWatcher",     // name
      2048,              // stack size (bytes)
      NULL,              // parameters
      1,                 // priority (low is fine here)
      NULL,              // task handle (not needed)
      0                  // core to pin to (0 or 1)
    );
  }
  else{
    digitalWrite(LED_R_PIN, LOW);
    digitalWrite(LED_G_PIN, LOW);
    digitalWrite(LED_B_PIN, HIGH);
  }

  if(espUiOn){
    size_mean = oscParams[0].sizeMean->getInt();
  }
  
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

  now = millis();

  // test OSC if toggle on
  if((now - lastTest) > 2000){
    lastTest = millis();
    for(uint8_t a=0; a<NUM_PARAMS; a++){
      if(oscParams[a].testOn->getBool()){
        testSend(a);
      }
    }
  }

  if((now - lastReading) > readingFreq && started){
    lastReading = millis();
    float minSpeed = 0.0;
    float maxSpeed = 1.0;
    uint8_t noiseAmount = 0;
    uint8_t noiseAmplitude = 0;
    uint8_t noiseMax = 0;
    if(espUiOn){
      minSpeed = float(oscParams[0].minVal->getInt()) / 100.0;
      maxSpeed = float(oscParams[0].maxVal->getInt()) / 100.0;
      noiseAmount = oscParams[0].noiseAmount->getInt();
      noiseAmplitude = oscParams[0].noiseAmplitude->getInt();
      noiseMax = oscParams[0].noiseMax->getInt();
    }
    float wind_noise = noise_random(oldWindNoise, noiseAmount, noiseAmplitude, noiseMax);
    oldWindNoise = wind_noise;
    windData = analogRead(WIND_SENS_PIN);
    windData = moyenne_glissante(value_for_mean, size_mean, windData) / 3500.0;
    if(windData != windOldData && windData >= minSpeed){
      windOldData = windData;
      windData = (windData - minSpeed) / (maxSpeed - minSpeed);
      windData = ((int) (windData * 100 + wind_noise)) / 100.0;
      sendData(0, windData);
    }
  }
  delay(20);
}