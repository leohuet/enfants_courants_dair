#include "Arduino.h"

#include <ESPUI.h>

#include <WiFiUdp.h>
#include <EthernetUdp.h>
#include <OSCMessage.h>

#include "driver/rtc_io.h"
#include <utility/w5100.h>

#include "wifi_config.h"
#include "persistentValue.h"
#include "define_functions.h"


#define ANEMOMETER_PIN GPIO_NUM_4
#define WIND_VANE_PIN GPIO_NUM_5
#define COMPASS_DIRECTIONS 16
#define SAMPLE_INTERVAL_MS 1500UL
#define NUM_PARAMS 2

OSCParam oscParams[NUM_PARAMS];

baseOSCParam baseOscParams[] = {
  {"Wind_speed", "km/h", "/toit/wind_speed", 0, 100, 10, 0, 0, 0, true, false},
  {"Wind_dir", "°", "/toit/wind_dir", 0, 360, 5, 0, 0, 0, true, false}
};

float anemometer_for_mean[MAX_MEAN_SIZE];
uint16_t wind_vane_for_mean[MAX_MEAN_SIZE];
uint8_t anemometer_size_mean = 10;
uint8_t wind_vane_size_mean = 5;

float oldWindSpeed = 0.0;
int oldSpeedNoise = 0;

volatile unsigned long anemometerCount = 0;
const int reading[COMPASS_DIRECTIONS] = {110, 250, 300, 385, 555, 785, 980, 1290, 1630, 2010, 2325, 2530, 2850, 3125, 3380, 3705};
const int compass[COMPASS_DIRECTIONS] = {112,  67,  90, 157, 135, 202, 180,  22,  45, 247, 225, 337,   0, 292, 315,  270};
long windDirTot = 0;
long windDirCount = 0;
long windDirNow = 0;
long windDirPrev = 0;
float oldWindDir = 0.0;
int oldDirNoise = 0;

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
  randomSeed(analogRead(GPIO_NUM_1));
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
    setupUI(2);
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
    anemometer_size_mean = oscParams[0].sizeMean->getInt();
    wind_vane_size_mean = oscParams[1].sizeMean->getInt();
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
    float minSpeed = 0.0;
    float maxSpeed = 50.0;
    uint8_t noiseAmount = 0;
    uint8_t noiseAmplitude = 0;
    uint8_t noiseMax = 0;
    anemometerCount = 0;
    lastReading = now;
    if(espUiOn){
      minSpeed = float(oscParams[0].minVal->getInt());
      maxSpeed = float(oscParams[0].maxVal->getInt());
      noiseAmount = oscParams[0].noiseAmount->getInt();
      noiseAmplitude = oscParams[0].noiseAmplitude->getInt();
      noiseMax = oscParams[0].noiseMax->getInt();
    }
    float speed_noise = noise_random(oldSpeedNoise, noiseAmount, noiseAmplitude, noiseMax);
    oldSpeedNoise = speed_noise;
    float windSpeed = moyenne_glissante(anemometer_for_mean, anemometer_size_mean, closuresPerSecond * 2.4);
    if(windSpeed < minSpeed) windSpeed = minSpeed;
    if(windSpeed > maxSpeed) windSpeed = maxSpeed;
    windSpeed = (windSpeed - minSpeed) / (maxSpeed - minSpeed);
    windSpeed = ((int) (windSpeed * 100 + speed_noise)) / 100.0;
    if(windSpeed > 1) windSpeed = 1;
    if(windSpeed >= minSpeed && windSpeed != oldWindSpeed){
      oldWindSpeed = windSpeed;
      // Serial.print("Wind speed: ");
      // Serial.println(windSpeed);
      sendData(0, windSpeed);
    }

    float windDir;
    float minDir = 0.0;
    float maxDir = 360.0;
    if(espUiOn){
      minDir = float(oscParams[1].minVal->getInt());
      maxDir = float(oscParams[1].maxVal->getInt());
      noiseAmount = oscParams[1].noiseAmount->getInt();
      noiseAmplitude = oscParams[1].noiseAmplitude->getInt();
      noiseMax = oscParams[1].noiseMax->getInt();
    }
    if(windDirCount > 0) windDir = windDirTot / windDirCount; else windDir = 0;
    while (windDir >= 360) windDir -= 360;
    while (windDir < 0) windDir += 360;
    float dir_noise = noise_random(oldSpeedNoise, noiseAmount, noiseAmplitude, noiseMax);
    oldDirNoise = dir_noise;
    windDir = moyenne_glissante(wind_vane_for_mean, wind_vane_size_mean, windDir);
    windDir = (windDir - minDir) / (maxDir - minDir);
    windDir = ((int) (windDir * 100 + dir_noise)) / 100.0;
    if(windDir != oldWindDir){
      oldWindDir = windDir;
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