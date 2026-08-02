#include "Arduino.h"

#include "wifi_config.h"
#include "persistentValue.h"
#include <ESPUI.h>

#include <WiFi.h>
#include <WiFiUdp.h>
#include <OSCMessage.h>
#include "driver/rtc_io.h"

#define NUM_PARAMS 7
#define META_PARAMS 6
#define VBUS_SENSE_PIN GPIO_NUM_9
#define BATT_PIN GPIO_NUM_7
#define THRESHOLD 2500
#define SLEEP_TIME 900
#define uS_TO_S_FACTOR 1000000ULL

// Struct for OSC parameters corresponding to each data
struct OSCParam {
  PersistentValue* name;
  PersistentValue* address;
  PersistentValue* minVal;
  PersistentValue* maxVal;
  PersistentValue* sendToMad;
  PersistentValue* testOn;
};
OSCParam oscParams[NUM_PARAMS];

struct baseOSCParam {
  String name;
  String address;
  uint8_t minVal;
  uint8_t maxVal;
  bool sendToMad;
  bool testOn;
};
baseOSCParam baseOscParams[] = {
  {"presence", "/presence", 0, 100, false, false},
  {"mean_x", "/mean/x", 0, 100, false, false},
  {"mean_y", "/mean/y", 0, 100, false, false},
  {"mean_dist", "/mean/dist", 0, 100, false, false},
  {"mean_speed", "/mean/speed", 0, 100, false, false},
  {"mean_angle", "/mean/angle", 0, 100, false, false},
  {"1_wind_speed", "/1/wind_speed", 0, 100, true, false}
};


//persistentvalues for ESPUI control values 
PersistentValue* isStarted;

PersistentValue* ipAddress;
PersistentValue* madPort;

PersistentValue* inactivityTimer;
PersistentValue* readingFrequency;

bool espUiOn = false;
bool onWifi = true;
int startBound = 7000;
int startMin = 2000;
uint32_t lastReading = 0;
uint32_t lastTest = 0;
bool toWakeup = false;

const uint8_t size_mean = 4;
uint16_t value_for_mean[size_mean];

float data = 0.0;
float old_data = 0.0;

// UDP pour les messages OSC
WiFiUDP Udp;


void displayBatteryCapacity(){
  int battRaw = analogRead(BATT_PIN);
  float battVoltage = (battRaw / 2430.0) * 4.13;
  if (battVoltage <= 3.7){
    digitalWrite(D0, HIGH);
    digitalWrite(D1, LOW);
    digitalWrite(D2, LOW);
  }
  else if (battVoltage > 3.7 && battVoltage <= 4){
    digitalWrite(D0, LOW);
    digitalWrite(D1, HIGH);
    digitalWrite(D2, LOW);
  }
  else{
    digitalWrite(D0, LOW);
    digitalWrite(D1, LOW);
    digitalWrite(D2, HIGH);
  }
}

void vbusWatcherTask(void *pvParameters) {
  while(true){
    displayBatteryCapacity();
    if(analogRead(VBUS_SENSE_PIN) > THRESHOLD) {
      // USB just got plugged in while we were running — go to sleep
      Serial.println("USB plugged in, going to sleep");
      toWakeup = true;
      digitalWrite(LED_BUILTIN, HIGH);
      Serial.flush();
      WiFi.mode(WIFI_OFF);
      esp_sleep_enable_ext0_wakeup((gpio_num_t)VBUS_SENSE_PIN, 0);
      esp_sleep_enable_timer_wakeup(SLEEP_TIME * uS_TO_S_FACTOR);
      esp_light_sleep_start();
    }
    else if(analogRead(VBUS_SENSE_PIN) < THRESHOLD && toWakeup) {
      // USB is not plugged in, keep running
      Serial.println("USB unplugged, trying to reconnect to Wi-Fi");
      toWakeup = false;
      rtc_gpio_deinit(VBUS_SENSE_PIN);
      WiFi.mode(WIFI_STA);
      begin_wifi();
    }
    vTaskDelay(pdMS_TO_TICKS(1000)); // check once a second
  }
}

uint16_t moyenne_glissante(uint16_t data_array[size_mean], uint16_t data){
	// calcule la moyenne glissante sur x données (défini par size_mean)
	uint16_t somme = 0;
	for (int i=1; i<size_mean; i++){
		data_array[i-1] = data_array[i];
		somme += data_array[i-1];
	}
	data_array[size_mean-1] = data;
	somme += data_array[size_mean-1];
	return somme/size_mean;
}

// ====== SETUP UI ======
void addOscControls(int startIdx, int endIdx, uint16_t tabId) {
  for (int i = startIdx; i < endIdx; i++) {
    ESPUI.addControl(ControlType::Separator, baseOscParams[i].name.c_str(), baseOscParams[i].name.c_str(), ControlColor::Turquoise, tabId);
    String label = baseOscParams[i].name;
    oscParams[i].address = new PersistentValue(label + "_address", ControlColor::Peterriver, baseOscParams[i].address, tabId);
    oscParams[i].minVal = new PersistentValue(label + "_min (%)", ControlColor::Wetasphalt, 0, 0, 100, tabId);
    oscParams[i].maxVal = new PersistentValue(label + "_max (%)", ControlColor::Wetasphalt, 100, 0, 100, tabId);
    oscParams[i].sendToMad = new PersistentValue(label + "_mad", ControlColor::Alizarin, baseOscParams[i].sendToMad, tabId);
    oscParams[i].testOn = new PersistentValue(label + "_test", ControlColor::Alizarin, baseOscParams[i].testOn, tabId);
  }
}

void setupUI() {
  uint16_t generalTab = ESPUI.addControl(ControlType::Tab, "Network", "Network");
  uint16_t oscTabs[] = {
    ESPUI.addControl(ControlType::Tab, "OSC meta data", "OSC meta data"),
    ESPUI.addControl(ControlType::Tab, "OSC 1 data", "OSC 1 data")
  };
  uint16_t radarTab = ESPUI.addControl(ControlType::Tab, "Device control", "Device control");
  
  // general tab
  isStarted = new PersistentValue("Start", ControlColor::Alizarin, false, generalTab);
  String baseIP = "192.168.1.150";
  ipAddress = new PersistentValue("IP destination", ControlColor::Peterriver, baseIP, generalTab);
  madPort = new PersistentValue("mad port", ControlColor::Wetasphalt, 9001, 1000, 12000, generalTab);

  // osc tabs
  // for each parameter, display controls for address, min & max, toggle for Touch/Ableton send and test button
  addOscControls(0, META_PARAMS, oscTabs[0]);
  addOscControls(META_PARAMS, META_PARAMS + 1, oscTabs[1]);

  // radar tab
  readingFrequency = new PersistentValue("Reading frequency (in ms)", ControlColor::Wetasphalt, 100, 50, 2000, radarTab);
  inactivityTimer = new PersistentValue("inactivity timer (in ms)", ControlColor::Wetasphalt, 2000, 500, 10000, radarTab);
}


void sendData(float value){
  // only if toggles to send to Ableton and/or Touch are on
  IPAddress outIP;
  // outIP.fromString(ipAddress->getString());
  // OSCMessage msg(oscParams[6].address->getString().c_str());
  outIP.fromString("192.168.68.50");
  OSCMessage msg("/1/wind_speed");
  bool toMad = baseOscParams[6].sendToMad;
  int madPortValue = 9001;
  if(espUiOn){
    toMad = oscParams[6].sendToMad->getBool();
    madPortValue = madPort->getInt();
  }
  if(!toMad){ return;}
  if(toMad){
    msg.add(value);
    Udp.beginPacket(outIP, madPortValue);
    msg.send(Udp);
    Udp.endPacket();
    msg.empty();
    if(espUiOn){
      Serial.print(oscParams[6].address->getString().c_str());
    }
    else{
      Serial.print(baseOscParams[6].address);
    }
    Serial.print(" ");
    Serial.println(value);
  }
  delay(5);
}

void checkForWakeUp(){
  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  if (cause == ESP_SLEEP_WAKEUP_EXT0) {
    rtc_gpio_deinit(VBUS_SENSE_PIN);
    begin_wifi();
  }
  else{
    Serial.println("Going to sleep for 30 minutes");
    // esp_sleep_enable_timer_wakeup(SLEEP_TIME * uS_TO_S_FACTOR);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(200);
    digitalWrite(LED_BUILTIN, LOW);
    delay(200);
    WiFi.mode(WIFI_OFF);
    Serial.flush();
    esp_sleep_enable_ext0_wakeup((gpio_num_t)VBUS_SENSE_PIN, 0);
    esp_light_sleep_start();
  }
}

void setup(){
  // open serial for USB and radar UART
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(VBUS_SENSE_PIN, INPUT);
  pinMode(BATT_PIN, INPUT);
  pinMode(D0, OUTPUT);
  pinMode(D1, OUTPUT);
  pinMode(D2, OUTPUT);
  pinMode(D5, INPUT);
  digitalWrite(D0, LOW);
  digitalWrite(D1, LOW);
  digitalWrite(D2, LOW);
  // checkForWakeUp();
  delay(1000);
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
    data = moyenne_glissante(value_for_mean, data) / 3500.0;
    if(data != old_data && data >= 0.10){
      // Serial.println(data);
      old_data = data;
      sendData(data);
    }
  }
  delay(20);
}