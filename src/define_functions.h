#include "Arduino.h"
#include "utility/w5100.h"
#include <DallasTemperature.h>
#include <Adafruit_NeoPixel.h>

#define VBUS_SENSE_PIN GPIO_NUM_6
#define BATT_PIN GPIO_NUM_5
#define LED_R_PIN GPIO_NUM_2
#define LED_G_PIN GPIO_NUM_3
#define LED_B_PIN GPIO_NUM_1
#define VSENSE_THRESHOLD 2500
#define SLEEP_TIME 300
#define COLD_SLEEP_TIME 60
#define V_TASK_WAIT 5
#define uS_TO_S_FACTOR 1000000ULL
#define MAX_MEAN_SIZE 100
#define CHARGER_CONTROL GPIO_NUM_7
#define LED_STRIP_PIN GPIO_NUM_8
#define NUMPIXELS 12
#define PHYCFGR_POWERDOWN   0xD8
#define PHYCFGR_NORMAL      0xF8

// Struct for OSC parameters corresponding to each data
struct OSCParam {
  PersistentValue* name;
  PersistentValue* address;
  PersistentValue* minVal;
  PersistentValue* maxVal;
  PersistentValue* sizeMean;
  PersistentValue* noiseAmount;
  PersistentValue* noiseAmplitude;
  PersistentValue* noiseMax;
  PersistentValue* send;
  PersistentValue* testOn;
};
extern OSCParam oscParams[];

struct baseOSCParam {
  String name;
  String dataType;
  String address;
  uint16_t minVal;
  uint16_t maxVal;
  uint8_t sizeMean;
  uint8_t noiseAmount;
  uint8_t noiseAmplitude;
  uint8_t noiseMax;
  bool send;
  bool testOn;
};
extern baseOSCParam baseOscParams[];

//persistentvalues for ESPUI control values
PersistentValue* espName;
PersistentValue* ipAddressHost;
PersistentValue* isStarted;
PersistentValue* onBattery;
PersistentValue* tempControl;
PersistentValue* debug;

PersistentValue* ipAddressSend;
PersistentValue* madPort;
PersistentValue* maxPort;

PersistentValue* readingFrequency;

PersistentValue* ledActive;
PersistentValue* ledOnDelay;
PersistentValue* ledBrightness;

unsigned long now;
uint32_t lastReading = 0;
uint32_t lastTest = 0;
bool toWakeup = false;
extern bool espUiOn;
extern DallasTemperature sensors;
extern Adafruit_NeoPixel pixels;

// UDP pour les messages OSC
WiFiUDP wifiUdp;
EthernetUDP ethUdp;
String baseName = "ESP_toit_2";
String baseSendIP = "192.168.68.100";
String baseHostIP = "192.168.68.105";
uint16_t madPortValue = 9001;
uint16_t maxPortValue = 8001;
bool onEthernetBool = false;
bool onBatteryBool = false;
bool ledOff = true;
bool chargerCutByTemp = false;
bool firstTimeCharge = false;
unsigned long ledCounter = 0;
bool toTurnOn = true;
static int lowCount = 0;
uint16_t displayBatteryCounter = 0;
uint8_t tempState = HIGH;

// ====== SETUP UI ======
void addOscControls(int startIdx, int endIdx, uint16_t tabId) {
  for (int i = startIdx; i < endIdx; i++) {
    ESPUI.addControl(ControlType::Separator, baseOscParams[i].name.c_str(), baseOscParams[i].name.c_str(), ControlColor::Turquoise, tabId);
    String label = baseOscParams[i].name;
    String short_label = baseOscParams[i].name.substring(5);
    String type = baseOscParams[i].dataType;
    oscParams[i].address = new PersistentValue(label + "_address", ControlColor::Peterriver, baseOscParams[i].address, tabId);
    oscParams[i].minVal = new PersistentValue(short_label + "_min_" + type, ControlColor::Wetasphalt, baseOscParams[i].minVal, baseOscParams[i].minVal, baseOscParams[i].maxVal, tabId);
    oscParams[i].maxVal = new PersistentValue(short_label + "_max_" + type, ControlColor::Wetasphalt, baseOscParams[i].maxVal, baseOscParams[i].minVal, baseOscParams[i].maxVal, tabId);
    oscParams[i].sizeMean = new PersistentValue(short_label + "_size_mean", ControlColor::Wetasphalt, 10, 1, 100, tabId);
    oscParams[i].noiseAmount = new PersistentValue(short_label + "_noise_%", ControlColor::Emerald, baseOscParams[i].noiseAmount, 0, 100, tabId);
    oscParams[i].noiseAmplitude = new PersistentValue(short_label + "_noise_amplitude", ControlColor::Emerald, baseOscParams[i].noiseAmplitude, 0, 100, tabId);
    oscParams[i].noiseMax = new PersistentValue(short_label + "_noise_max", ControlColor::Emerald, baseOscParams[i].noiseMax, 0, 100, tabId);
    oscParams[i].send = new PersistentValue(short_label + "_send", ControlColor::Alizarin, baseOscParams[i].send, tabId);
    oscParams[i].testOn = new PersistentValue(short_label + "_test", ControlColor::Alizarin, baseOscParams[i].testOn, tabId);
  }
}

void setupUI(uint8_t numControl) {
  uint16_t generalTab = ESPUI.addControl(ControlType::Tab, "Settings", "Settings");
  uint16_t oscTabs[] = {
    ESPUI.addControl(ControlType::Tab, "OSC tab", "OSC tab"),
  };
  
  uint16_t rebootButton = ESPUI.button("Reboot", [](Control *sender, int eventname) {
    ESP.restart();
  }, ControlColor::Alizarin);
  uint16_t toDeepSleepButton = ESPUI.button("Deep Sleep", [](Control *sender, int eventname) {
    WiFi.mode(WIFI_OFF);
    esp_sleep_enable_timer_wakeup(SLEEP_TIME * uS_TO_S_FACTOR);
    esp_deep_sleep_start();
  }, ControlColor::Alizarin);

  // general tab
  ESPUI.addControl(ControlType::Separator, "Global controls", "", ControlColor::None, generalTab);
  espName = new PersistentValue("Name", ControlColor::Peterriver, baseName, generalTab);
  isStarted = new PersistentValue("Start", ControlColor::Alizarin, false, generalTab);
  onBattery = new PersistentValue("On battery", ControlColor::Alizarin, false, generalTab);
  tempControl = new PersistentValue("Temperature charge control", ControlColor::Alizarin, true, generalTab);
  debug = new PersistentValue("Debug", ControlColor::Alizarin, false, generalTab);
  
  ESPUI.addControl(ControlType::Separator, "Network", "", ControlColor::None, generalTab);
  ipAddressHost = new PersistentValue("IP host", ControlColor::Peterriver, baseHostIP, generalTab);
  ipAddressSend = new PersistentValue("IP destination", ControlColor::Peterriver, baseSendIP, generalTab);
  madPort = new PersistentValue("mad port", ControlColor::Wetasphalt, 9001, 1000, 12000, generalTab);
  maxPort = new PersistentValue("max port", ControlColor::Wetasphalt, 8001, 1000, 12000, generalTab);
  ESPUI.addControl(ControlType::Separator, "Data controls", "", ControlColor::None, generalTab);  
  readingFrequency = new PersistentValue("Reading frequency (in ms)", ControlColor::Wetasphalt, 1000, 50, 5000, generalTab);
  ESPUI.addControl(ControlType::Separator, "LED Strip", "", ControlColor::None, generalTab);

  ledActive = new PersistentValue("LEDStrip active", ControlColor::Wetasphalt, true, generalTab);
  ledOnDelay = new PersistentValue("LEDStrip on delay (in hours)", ControlColor::Wetasphalt, 1, 0, 20, generalTab);
  ledBrightness = new PersistentValue("LEDStrip brightness", ControlColor::Wetasphalt, 100, 0, 255, generalTab);
  // osc tabs
  // for each parameter, display controls for address, min & max, toggle for Touch/Ableton send and test button
  addOscControls(0, numControl, oscTabs[0]);
}


uint16_t moyenne_glissante(uint16_t data_array[], uint8_t size, uint16_t data){
	// calcule la moyenne glissante sur x données (défini par size_mean)
	uint16_t somme = 0;
	for (int i=1; i<size; i++){
		data_array[i-1] = data_array[i];
		somme += data_array[i-1];
	}
	data_array[size-1] = data;
	somme += data_array[size-1];
	return somme/size;
}

float moyenne_glissante(float data_array[], uint8_t size, float data){
	// calcule la moyenne glissante sur x données (défini par size_mean)
	float somme = 0;
	for (int i=1; i<size; i++){
		data_array[i-1] = data_array[i];
		somme += data_array[i-1];
	}
	data_array[size-1] = data;
	somme += data_array[size-1];
	return somme/size;
}

int noise_random(uint8_t old_noise, uint8_t noise_amount, uint8_t noise_amplitude, uint8_t noise_max){
  // add random noise to the data, based on the noise amount and amplitude defined in the ESPUI controls
  if (noise_amount > 0 && noise_amplitude > 0){
    int random_value = random(-noise_amplitude, noise_amplitude);
    int random_chance = random(0, 100);
    if (old_noise + random_value > noise_max) random_value = -random_value;
    if (old_noise + random_value < 0) random_value = -random_value;
    if (random_chance < noise_amount){
      return old_noise + random_value;
    }
    return old_noise;
  }
  return 0;
}


void w5500PowerDown() {
  W5100.writePHYCFGR_W5500(PHYCFGR_POWERDOWN);
}

void w5500PowerUp() {
  W5100.writePHYCFGR_W5500(PHYCFGR_NORMAL);
}


void sendDebug(String address, float value){
  IPAddress outIP;
  outIP.fromString(baseSendIP);
  OSCMessage msg(address.c_str());
  if(espUiOn){
    outIP.fromString(ipAddressSend->getString());
  }
  msg.add(value);
  wifiUdp.beginPacket(outIP, 8888);
  msg.send(wifiUdp);
  wifiUdp.endPacket();
  msg.empty();
}

bool checkTemperature(){
  sensors.requestTemperatures();
  delay(1000);
  float tempC = sensors.getTempCByIndex(0);
  if (tempC == DEVICE_DISCONNECTED_C) tempC = 3.0;

  if(tempC < 2.0 || tempC > 45.0) tempState = LOW;
  else if(tempC > 4.0 && tempC < 40.0) tempState = HIGH;
  digitalWrite(CHARGER_CONTROL, tempState);
  if(debug->getBool()) sendDebug("/temperature", tempC);
  return !tempState;
}

void goDeepSleep(){
  WiFi.mode(WIFI_OFF);
  if(tempControl->getBool()){
    chargerCutByTemp = checkTemperature();
  }
  else{
    digitalWrite(CHARGER_CONTROL, HIGH);
    chargerCutByTemp = false;
  }
  if(chargerCutByTemp) digitalWrite(CHARGER_CONTROL, LOW);
  else digitalWrite(CHARGER_CONTROL, HIGH);
  gpio_hold_en((gpio_num_t)CHARGER_CONTROL);
  gpio_deep_sleep_hold_en();
  esp_sleep_enable_timer_wakeup(SLEEP_TIME * uS_TO_S_FACTOR);
  esp_sleep_enable_ext0_wakeup((gpio_num_t)VBUS_SENSE_PIN, 1);
  esp_deep_sleep_start();
}

void displayBatteryCapacity(){
  int battRaw = analogRead(BATT_PIN);
  float battVoltage = (battRaw / 2430.0) * 4.13;
  if (battVoltage <= 3.8){
    digitalWrite(LED_R_PIN, HIGH);
    digitalWrite(LED_G_PIN, LOW);
    digitalWrite(LED_B_PIN, LOW);
  }
  else if (battVoltage > 3.8 && battVoltage <= 4){
    digitalWrite(LED_R_PIN, LOW);
    digitalWrite(LED_G_PIN, HIGH);
    digitalWrite(LED_B_PIN, LOW);
  }
  else{
    digitalWrite(LED_R_PIN, LOW);
    digitalWrite(LED_G_PIN, LOW);
    digitalWrite(LED_B_PIN, HIGH);
  }

  if(analogRead(VBUS_SENSE_PIN) < VSENSE_THRESHOLD && battVoltage < 3.4){
    lowCount++;
    if(lowCount >= 3){
      goDeepSleep();
    }
  }
  else {
    lowCount = 0;
  }

  if(debug->getBool()) sendDebug("/battery", battVoltage);
}

void checkBatteryAtBoot(){
  int battRaw = analogRead(BATT_PIN);
  float battVoltage = (battRaw / 2430.0) * 4.13;
  if(battVoltage >= 3.5){
    return;
  }
  else{
    goDeepSleep();
  }
}


void vbusWatcherTask(void *pvParameters){
  while(true){
    if(debug->getBool()){
      displayBatteryCapacity();
      checkTemperature();
    }
    if(ledCounter*V_TASK_WAIT >= ledOnDelay->getInt() * 3600 && ledOff && ledActive->getBool()){
      ledOff = false;
      for(int i=0; i<NUMPIXELS; i++){
        pixels.setPixelColor(i, pixels.Color(255, 255, 255));
        pixels.show();
        delay(50);
      }
      pixels.show();
    }
    else{
      ledCounter++;
    }

    if((displayBatteryCounter == 0 || displayBatteryCounter*V_TASK_WAIT >= 300) && !toWakeup){
      displayBatteryCapacity();
      displayBatteryCounter = 1;
    }
    else{
      displayBatteryCounter++;
    }

    if (toWakeup && chargerCutByTemp) {
      digitalWrite(CHARGER_CONTROL, HIGH);   // enable briefly just to sense VBUS
      delay(100);
    }
    firstTimeCharge = !toWakeup;
    if(analogRead(VBUS_SENSE_PIN) > VSENSE_THRESHOLD){
      // USB just got plugged in while we were running — go to sleep
      toWakeup = true;
      if(tempControl->getBool()){
        chargerCutByTemp = checkTemperature();
      }
      else{
        digitalWrite(CHARGER_CONTROL, HIGH);
        chargerCutByTemp = false;
      }
      pixels.clear();
      pixels.show();
      digitalWrite(LED_BUILTIN, HIGH);
      Serial.flush();
      WiFi.mode(WIFI_OFF);
      if(onEthernetBool){
        ethUdp.stop();
        w5500PowerDown();
      }
      if(!chargerCutByTemp){
        if(firstTimeCharge){
          digitalWrite(LED_R_PIN, LOW);
          digitalWrite(LED_G_PIN, LOW);
          digitalWrite(LED_B_PIN, LOW);
          delay(500);
          digitalWrite(LED_R_PIN, LOW);
          digitalWrite(LED_G_PIN, LOW);
          digitalWrite(LED_B_PIN, HIGH);
          delay(500);
          digitalWrite(LED_R_PIN, LOW);
          digitalWrite(LED_G_PIN, LOW);
          digitalWrite(LED_B_PIN, LOW);
          delay(500);
          digitalWrite(LED_R_PIN, LOW);
          digitalWrite(LED_G_PIN, HIGH);
          digitalWrite(LED_B_PIN, LOW);
        }
        esp_sleep_enable_ext0_wakeup((gpio_num_t)VBUS_SENSE_PIN, 0);
        esp_sleep_enable_timer_wakeup(SLEEP_TIME * uS_TO_S_FACTOR);
      }
      else{
        digitalWrite(LED_R_PIN, HIGH);
        digitalWrite(LED_G_PIN, LOW);
        digitalWrite(LED_B_PIN, LOW);
        esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_EXT0);
        esp_sleep_enable_timer_wakeup(COLD_SLEEP_TIME * uS_TO_S_FACTOR);
      }
      esp_light_sleep_start();
    }
    else if(analogRead(VBUS_SENSE_PIN) < VSENSE_THRESHOLD && toWakeup){
      // USB just got unplugged, restart
      toWakeup = false;
      chargerCutByTemp = false;
      digitalWrite(CHARGER_CONTROL, HIGH);
      rtc_gpio_deinit(VBUS_SENSE_PIN);  
      digitalWrite(LED_R_PIN, LOW);
      digitalWrite(LED_G_PIN, LOW);
      digitalWrite(LED_B_PIN, LOW);
      delay(500);
      displayBatteryCounter = 0;
      ledCounter = 0;
      ledOff = true;
      if(onEthernetBool){
        w5500PowerUp();
        begin_ethernet(LED_R_PIN, ipAddressHost->getString());
        delay(2000);
        ethUdp.begin(8888);
      }
      else{
        WiFi.mode(WIFI_STA);
        begin_wifi(LED_R_PIN, ipAddressHost->getString());
      }
      if(ledActive->getBool()){
        for(int i=0; i<NUMPIXELS; i++){
          pixels.setPixelColor(i, pixels.Color(255, 255, 255));
          pixels.show();
          delay(100);
        }
        delay(30000);
        pixels.clear();
        pixels.show();
      }
    }
    vTaskDelay(pdMS_TO_TICKS(V_TASK_WAIT*1000));
  }
}


void sendData(uint8_t index, float value){
  IPAddress outIP;
  outIP.fromString(baseSendIP);
  String address = baseOscParams[index].address;
  bool toSend = baseOscParams[index].send;
  if(espUiOn){
    outIP.fromString(ipAddressSend->getString());
    address = oscParams[index].address->getString();
    toSend = oscParams[index].send->getBool();
    madPortValue = madPort->getInt();
    maxPortValue = maxPort->getInt();
  }
  if(!toSend){ return;}
  OSCMessage msg(address.c_str());
  msg.add(value);
  if(onEthernetBool){
    ethUdp.beginPacket(outIP, madPortValue);
    msg.send(ethUdp);
    ethUdp.endPacket();
    ethUdp.beginPacket(outIP, maxPortValue);
    msg.send(ethUdp);
    ethUdp.endPacket();
    msg.empty();
  }
  else{
    wifiUdp.beginPacket(outIP, madPortValue);
    msg.send(wifiUdp);
    wifiUdp.endPacket();
    wifiUdp.beginPacket(outIP, maxPortValue);
    msg.send(wifiUdp);
    wifiUdp.endPacket();
    msg.empty();
  }
  delay(5);
}

// ====== test OSC send function ======
void testSend(uint8_t index){
  IPAddress outIP;
  outIP.fromString(baseSendIP);
  String address = baseOscParams[index].address;
  float test = 0.5;
  if(espUiOn){
    outIP.fromString(ipAddressSend->getString());
    address = oscParams[index].address->getString();
    madPortValue = madPort->getInt();
    maxPortValue = maxPort->getInt();
  }
  OSCMessage msg(address.c_str());
  msg.add(test);
  if(onEthernetBool){
    ethUdp.beginPacket(outIP, madPortValue);
    msg.send(ethUdp);
    ethUdp.endPacket();
    ethUdp.beginPacket(outIP, maxPortValue);
    msg.send(ethUdp);
    ethUdp.endPacket();
    msg.empty();
  }
  else{
    wifiUdp.beginPacket(outIP, madPortValue);
    msg.send(wifiUdp);
    wifiUdp.endPacket();
    wifiUdp.beginPacket(outIP, maxPortValue);
    msg.send(wifiUdp);
    wifiUdp.endPacket();
    msg.empty();
  }
}