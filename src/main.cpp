#include "Arduino.h"

#include "wifi_config.h"
#include "persistentValue.h"
#include <ESPUI.h>

#include <WiFi.h>
#include <WiFiUdp.h>
#include <OSCMessage.h>

#define NUM_PARAMS 20
#define META_PARAMS 6


// Struct for OSC parameters corresponding to each data
struct OSCParam {
  PersistentValue* name;
  PersistentValue* address;
  PersistentValue* minVal;
  PersistentValue* maxVal;
  PersistentValue* sendToTD;
  PersistentValue* sendToAbleton;
  PersistentValue* testOn;
};
OSCParam oscParams[NUM_PARAMS];

struct baseOSCParam {
  String name;
  String address;
  uint8_t minVal;
  uint8_t maxVal;
  bool sendToTD;
  bool sendToAbleton;
  bool testOn;
};
baseOSCParam baseOscParams[] = {
  {"presence", "/presence", 0, 100, false, false, false},
  {"mean_x", "/mean/x", 0, 100, false, false, false},
  {"mean_y", "/mean/y", 0, 100, false, false, false},
  {"mean_dist", "/mean/dist", 0, 100, false, false, false},
  {"mean_speed", "/mean/speed", 0, 100, false, false, false},
  {"mean_angle", "/mean/angle", 0, 100, false, false, false},
  {"1_x", "/1/x", 0, 100, true, false, false},
  {"1_y",  "/1/y",  0, 100, false, false, false},
  {"1_dist",  "/1/dist",  0, 100, false, false, false},
  {"1_speed", "/1/speed", 0, 100, false, false,  false},
  {"1_angle",  "/1/angle",  0, 100, false, false, false},
  {"2_x", "/2/x", 0, 100, false, false, false},
  {"2_y",  "/2/y",  0, 100, false, false, false},
  {"2_dist",  "/2/dist",  0, 100, false, false, false},
  {"2_speed", "/2/speed", 0, 100, false, false, false},
  {"2_angle",  "/2/angle",  0, 100, false, false, false},
  {"3_x", "/3/x", 0, 100, false, false, false},
  {"3_y",  "/3/y",  0, 100, false, false, false},
  {"3_dist",  "/3/dist",  0, 100, false, false, false},
  {"3_speed", "/3/speed", 0, 100, false, false, false},
  {"3_angle",  "/3/angle",  0, 100, false, false, false}
};


//persistentvalues for ESPUI control values 
PersistentValue* isStarted;

PersistentValue* ipAddress;
PersistentValue* abletonPort;
PersistentValue* tdPort;

PersistentValue* minDist;
PersistentValue* maxDist;
PersistentValue* inactivityTimer;
PersistentValue* readingFrequency;

bool espUiOn = false;
bool onWifi = false;
int startBound = 7000;
int startMin = 2000;

// UDP pour les messages OSC
WiFiUDP Udp;


// ====== SETUP UI ======
void addOscControls(int startIdx, int endIdx, uint16_t tabId) {
  for (int i = startIdx; i < endIdx; i++) {
    ESPUI.addControl(ControlType::Separator, baseOscParams[i].name.c_str(), baseOscParams[i].name.c_str(), ControlColor::Turquoise, tabId);
    String label = baseOscParams[i].name;
    oscParams[i].address = new PersistentValue(label + "_address", ControlColor::Peterriver, baseOscParams[i].address, tabId);
    oscParams[i].minVal = new PersistentValue(label + "_min (%)", ControlColor::Wetasphalt, 0, 0, 100, tabId);
    oscParams[i].maxVal = new PersistentValue(label + "_max (%)", ControlColor::Wetasphalt, 100, 0, 100, tabId);
    oscParams[i].sendToTD = new PersistentValue(label + "_TD", ControlColor::Alizarin, baseOscParams[i].sendToTD, tabId);
    oscParams[i].sendToAbleton = new PersistentValue(label + "_AB", ControlColor::Alizarin, baseOscParams[i].sendToAbleton, tabId);
    oscParams[i].testOn = new PersistentValue(label + "_test", ControlColor::Alizarin, baseOscParams[i].testOn, tabId);
  }
}

void setupUI() {
  uint16_t generalTab = ESPUI.addControl(ControlType::Tab, "Network", "Network");
  uint16_t oscTabs[] = {
    ESPUI.addControl(ControlType::Tab, "OSC meta data", "OSC meta data"),
    ESPUI.addControl(ControlType::Tab, "OSC 1 radar data", "OSC 1 radar data"),
    ESPUI.addControl(ControlType::Tab, "OSC 2 radar data", "OSC 2 radar data"),
    ESPUI.addControl(ControlType::Tab, "OSC 3 radar data", "OSC 3 radar data")
  };
  uint16_t radarTab = ESPUI.addControl(ControlType::Tab, "Radar control", "Radar control");
  
  // general tab
  isStarted = new PersistentValue("Start", ControlColor::Alizarin, false, generalTab);
  String baseIP = "192.168.1.108";
  ipAddress = new PersistentValue("IP destination", ControlColor::Peterriver, baseIP, generalTab);
  abletonPort = new PersistentValue("ableton port", ControlColor::Wetasphalt, 8001, 1000, 12000, generalTab);
  tdPort = new PersistentValue("td port", ControlColor::Wetasphalt, 9001, 1000, 12000, generalTab);

  // osc tabs
  // for each parameter, display controls for address, min & max, toggle for Touch/Ableton send and test button
  addOscControls(0, META_PARAMS, oscTabs[0]);
  addOscControls(META_PARAMS, META_PARAMS + 5, oscTabs[1]);
  addOscControls(META_PARAMS + 5, META_PARAMS + 10, oscTabs[2]);
  addOscControls(META_PARAMS + 10, NUM_PARAMS, oscTabs[3]);

  // radar tab
  readingFrequency = new PersistentValue("Reading frequency (in ms)", ControlColor::Wetasphalt, 100, 50, 2000, radarTab);
  minDist = new PersistentValue("min distance (in cm)", ControlColor::Wetasphalt, 200, 0, 700, radarTab);
  maxDist = new PersistentValue("max distance (in cm)", ControlColor::Wetasphalt, 700, 0, 700, radarTab);
  inactivityTimer = new PersistentValue("inactivity timer (in ms)", ControlColor::Wetasphalt, 2000, 500, 10000, radarTab);
}


void setup(){
  // open serial for USB and radar UART
  Serial.begin(115200);
  delay(500);

  if(onWifi){
    begin_wifi();
  }
  delay(2000);

  if(espUiOn){
    // ESPUI control init
    ESPUI.begin("The Lights Which Can Be Heard");
    setupUI();
  }
  
  delay(2000);
  Serial.println("Starting programm..");
}

void loop(){
  int readingFreq = 100;
  int inactTimer = 1000;
  bool started = false;
  if(espUiOn){
    readingFreq = readingFrequency->getInt();
    started = isStarted->getBool();
  }
  delay(20);
}