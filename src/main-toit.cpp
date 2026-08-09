#include "Arduino.h"

#include <ESPUI.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <EthernetUdp.h>
#include <OSCMessage.h>

#include "driver/rtc_io.h"
#include <utility/w5100.h>

#include "wifi_config.h"
#include "persistentValue.h"
#include "define_functions.h"

#define NUM_PARAMS 2
#define VBUS_SENSE_PIN GPIO_NUM_9
#define BATT_PIN GPIO_NUM_7
#define ANEMOMETER_PIN GPIO_NUM_4
#define WIND_VANE_PIN GPIO_NUM_5
#define COMPASS_DIRECTIONS 16
#define VSENSE_THRESHOLD 2500
#define SLEEP_TIME 900
#define uS_TO_S_FACTOR 1000000ULL
#define SAMPLE_INTERVAL_MS 1500UL


OSCParam oscParams[NUM_PARAMS];

baseOSCParam baseOscParams[] = {
  {"Wind_speed", "km/h", "/toit/wind_speed", 0, 50, true, false},
  {"Wind_dir", "°", "/toit/wind_dir", 0, 360, true, false}
};

const uint8_t wind_vane_size_mean = 5;
const uint8_t anemometer_size_mean = 10;
uint16_t wind_vane_for_mean[wind_vane_size_mean];
float anemometer_for_mean[anemometer_size_mean];
float oldWindSpeed = 0.0;

volatile unsigned long anemometerCount = 0;
const int reading[COMPASS_DIRECTIONS] = {110, 250, 300, 385, 555, 785, 980, 1290, 1630, 2010, 2325, 2530, 2850, 3125, 3380, 3705};
const int compass[COMPASS_DIRECTIONS] = {112,  67,  90, 157, 135, 202, 180,  22,  45, 247, 225, 337,   0, 292, 315,  270};
long windDirTot = 0;
long windDirCount = 0;
long windDirNow = 0;
long windDirPrev = 0;
float oldWindDir = 0.0;

bool espUiOn = true;

void readAnemometer(){
  anemometerCount++;
}


void setup(){
  Serial.begin(115200);
  pinMode(ANEMOMETER_PIN, INPUT_PULLUP);
  pinMode(WIND_VANE_PIN, INPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(VBUS_SENSE_PIN, INPUT);
  pinMode(BATT_PIN, INPUT);
  pinMode(LED_R_PIN, OUTPUT);
  pinMode(LED_G_PIN, OUTPUT);
  pinMode(LED_B_PIN, OUTPUT);
  attachInterrupt(digitalPinToInterrupt(ANEMOMETER_PIN), readAnemometer, FALLING);
  delay(2000);
  onEthernetBool = begin_ethernet();
  delay(2000);
  if(onEthernetBool) ethUdp.begin(8888);
  else begin_wifi();

  if(espUiOn){
    // ESPUI control init
    ESPUI.begin("Les enfants des courants d'air");
    setupUI(2);
    delay(2000);
    onBatteryBool = onBattery->getBool();
  }

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
  
  Serial.println("Starting programm..");
}

void loop(){
  int readingFreq = SAMPLE_INTERVAL_MS;
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

  if ((now - lastReading) >= readingFreq && started) {
    float elapsedSeconds = (now - lastReading) / 1000.0;
    float closuresPerSecond = anemometerCount / elapsedSeconds;
    float windSpeed = moyenne_glissante(anemometer_for_mean, anemometer_size_mean, closuresPerSecond * 2.4);
    float minSpeed = 0.0;
    float maxSpeed = 50.0;
    anemometerCount = 0;
    lastReading = now;
    if(espUiOn){
      minSpeed = float(oscParams[0].minVal->getInt());
      maxSpeed = float(oscParams[0].maxVal->getInt());
    }
    if(windSpeed >= minSpeed && windSpeed != oldWindSpeed){
      oldWindSpeed = windSpeed;
      if(windSpeed > maxSpeed) windSpeed = maxSpeed;
      windSpeed = (windSpeed - minSpeed) / (maxSpeed - minSpeed);
      windSpeed = ((int) (windSpeed * 100)) / 100.0;
      // Serial.print("Wind speed: ");
      // Serial.println(windSpeed);
      sendData(0, windSpeed);
    }

    float windDir;
    if(windDirCount > 0) windDir = windDirTot / windDirCount; else windDir = 0;
    while (windDir >= 360) windDir -= 360;
    while (windDir < 0) windDir += 360;
    float minDir = 0.0;
    float maxDir = 50.0;
    if(espUiOn){
      minDir = float(oscParams[1].minVal->getInt());
      maxDir = float(oscParams[1].maxVal->getInt());
    }
    windDir = moyenne_glissante(wind_vane_for_mean, wind_vane_size_mean, windDir);
    if(windDir != oldWindDir){
      oldWindDir = windDir;
      windDir = (windDir - minDir) / (maxDir - minDir);
      windDir = ((int) (windDir * 100)) / 100.0;
      // Serial.print("Wind dir: ");
      // Serial.println(windDir);
      sendData(1, windDir);
    }
    windDirTot = 0;
    windDirCount = 0;
  }

  uint16_t windDirRaw = analogRead(WIND_VANE_PIN);
  for(uint8_t i=0; i < COMPASS_DIRECTIONS; i++){
    if(windDirRaw >= reading[i]) windDirNow = compass[i];
  }
  if(windDirNow - windDirPrev > 180) windDirNow -= 360;
  if(windDirPrev - windDirNow > 180) windDirNow += 360;
  // Update total and count of data points for calculating average
  windDirTot += windDirNow;
  windDirCount++;
  windDirPrev = windDirNow;

  delay(50);
}