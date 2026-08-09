#include "Arduino.h"

#define VBUS_SENSE_PIN GPIO_NUM_9
#define BATT_PIN GPIO_NUM_7
#define ANEMOMETER_PIN GPIO_NUM_4
#define WIND_VANE_PIN GPIO_NUM_5
#define WIND_SENS_PIN GPIO_NUM_6
#define LED_R_PIN GPIO_NUM_1
#define LED_G_PIN GPIO_NUM_2
#define LED_B_PIN GPIO_NUM_3
#define COMPASS_DIRECTIONS 16
#define VSENSE_THRESHOLD 2500
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
extern OSCParam oscParams[];

struct baseOSCParam {
  String name;
  String dataType;
  String address;
  uint16_t minVal;
  uint16_t maxVal;
  bool sendToMad;
  bool testOn;
};
extern baseOSCParam baseOscParams[];

//persistentvalues for ESPUI control values 
PersistentValue* isStarted;
PersistentValue* onBattery;

PersistentValue* ipAddress;
PersistentValue* madPort;

PersistentValue* readingFrequency;

unsigned long now;
uint32_t lastReading = 0;
uint32_t lastTest = 0;
bool toWakeup = false;
extern bool espUiOn;

// UDP pour les messages OSC
WiFiUDP wifiUdp;
EthernetUDP ethUdp;
String baseIP = "192.168.68.100";
uint16_t madPortValue = 9001;
bool onEthernetBool = false;
bool onBatteryBool = false;


// ====== SETUP UI ======
void addOscControls(int startIdx, int endIdx, uint16_t tabId) {
  for (int i = startIdx; i < endIdx; i++) {
    ESPUI.addControl(ControlType::Separator, baseOscParams[i].name.c_str(), baseOscParams[i].name.c_str(), ControlColor::Turquoise, tabId);
    String label = baseOscParams[i].name;
    String type = baseOscParams[i].dataType;
    oscParams[i].address = new PersistentValue(label + "_address", ControlColor::Peterriver, baseOscParams[i].address, tabId);
    oscParams[i].minVal = new PersistentValue(label + "_min_" + type, ControlColor::Wetasphalt, baseOscParams[i].minVal, baseOscParams[i].minVal, baseOscParams[i].maxVal, tabId);
    oscParams[i].maxVal = new PersistentValue(label + "_max_" + type, ControlColor::Wetasphalt, baseOscParams[i].maxVal, baseOscParams[i].minVal, baseOscParams[i].maxVal, tabId);
    oscParams[i].sendToMad = new PersistentValue(label + "_mad", ControlColor::Alizarin, baseOscParams[i].sendToMad, tabId);
    oscParams[i].testOn = new PersistentValue(label + "_test", ControlColor::Alizarin, baseOscParams[i].testOn, tabId);
  }
}

void setupUI(uint8_t numControl) {
  uint16_t generalTab = ESPUI.addControl(ControlType::Tab, "Settings", "Settings");
  uint16_t oscTabs[] = {
    ESPUI.addControl(ControlType::Tab, "OSC tab", "OSC tab"),
  };
  
  // general tab
  ESPUI.addControl(ControlType::Separator, "Global controls", "", ControlColor::None, generalTab);
  isStarted = new PersistentValue("Start", ControlColor::Alizarin, false, generalTab);
  onBattery = new PersistentValue("On battery", ControlColor::Alizarin, false, generalTab);
  ESPUI.addControl(ControlType::Separator, "Network", "", ControlColor::None, generalTab);
  ipAddress = new PersistentValue("IP destination", ControlColor::Peterriver, baseIP, generalTab);
  madPort = new PersistentValue("mad port", ControlColor::Wetasphalt, 9001, 1000, 12000, generalTab);
  ESPUI.addControl(ControlType::Separator, "Data controls", "", ControlColor::None, generalTab);  
  readingFrequency = new PersistentValue("Reading frequency (in ms)", ControlColor::Wetasphalt, 1000, 50, 5000, generalTab);

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

void displayBatteryCapacity(){
  int battRaw = analogRead(BATT_PIN);
  float battVoltage = (battRaw / 2430.0) * 4.13;
  if (battVoltage <= 3.7){
    digitalWrite(LED_R_PIN, HIGH);
    digitalWrite(LED_G_PIN, LOW);
    digitalWrite(LED_B_PIN, LOW);
  }
  else if (battVoltage > 3.7 && battVoltage <= 4){
    digitalWrite(LED_R_PIN, LOW);
    digitalWrite(LED_G_PIN, HIGH);
    digitalWrite(LED_B_PIN, LOW);
  }
  else{
    digitalWrite(LED_R_PIN, LOW);
    digitalWrite(LED_G_PIN, LOW);
    digitalWrite(LED_B_PIN, HIGH);
  }
}


void vbusWatcherTask(void *pvParameters) {
  while(true){
    displayBatteryCapacity();
    if(analogRead(VBUS_SENSE_PIN) > VSENSE_THRESHOLD){
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
    else if(analogRead(VBUS_SENSE_PIN) < VSENSE_THRESHOLD && toWakeup){
      // USB is not plugged in, keep running
      Serial.println("USB unplugged, trying to reconnect..");
      toWakeup = false;
      rtc_gpio_deinit(VBUS_SENSE_PIN);
      if(onEthernetBool) begin_ethernet();
      else{
        WiFi.mode(WIFI_STA);
        begin_wifi();
      }
    }
    vTaskDelay(pdMS_TO_TICKS(1000)); // check once a second
  }
}


void sendData(uint8_t index, float value){
  // only if toggles to send to Ableton and/or Touch are on
  IPAddress outIP;
  outIP.fromString(baseIP);
  String address = baseOscParams[index].address;
  bool toMad = baseOscParams[index].sendToMad;
  if(espUiOn){
    outIP.fromString(ipAddress->getString());
    address = oscParams[index].address->getString();
    toMad = oscParams[index].sendToMad->getBool();
    madPortValue = madPort->getInt();
  }
  if(!toMad){ return;}
  OSCMessage msg(address.c_str());
  msg.add(value);
  if(onEthernetBool){
    ethUdp.beginPacket(outIP, madPortValue);
    msg.send(ethUdp);
    ethUdp.endPacket();
    msg.empty();
  }
  else{
    wifiUdp.beginPacket(outIP, madPortValue);
    msg.send(wifiUdp);
    wifiUdp.endPacket();
    msg.empty();
  }
  delay(5);
}

// ====== test OSC send function ======
void testSend(uint8_t index){
  IPAddress outIP;
  outIP.fromString(baseIP);
  String address = baseOscParams[index].address;
  float test = 0.5;
  if(espUiOn){
    outIP.fromString(ipAddress->getString());
    address = oscParams[index].address->getString();
    madPortValue = madPort->getInt();
  }
  OSCMessage msg(address.c_str());
  msg.add(test);
  if(onEthernetBool){
    ethUdp.beginPacket(outIP, madPortValue);
    msg.send(ethUdp);
    ethUdp.endPacket();
    msg.empty();
  }
  else{
    wifiUdp.beginPacket(outIP, madPortValue);
    msg.send(wifiUdp);
    wifiUdp.endPacket();
    msg.empty();
  }
}