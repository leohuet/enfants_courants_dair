#include <Arduino.h>
// Network stack
#include <SPI.h>
#include <WiFi.h>
#include <Ethernet.h>
#include <ArduinoOTA.h>
#ifndef HOST
#define HOST "ESP_ROMAIN"
#endif
#include <WiFiUdp.h>
#include <OSCMessage.h>
#include "credentials.h"

#define ETH_CS_PIN GPIO_NUM_44
#define nLOG(message) udp.broadcastTo((String("\n") + String(message)).c_str(), 1234) // listen with `nc -kluvw 0 1234`
// listen with `nc -kluvw 0 1234`*/

#define FAST_BLINK (millis() % 200 < 50)
#define HEARTBEAT ((millis() + 1000 )% 2000 < 50)

#define OTA_PASS "question"


char host[22];
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xEF };
IPAddress ip(192, 168, 1, 177);
IPAddress myDns(192, 168, 1, 1);

EthernetClient client;

void ethernet_OTA(void * _){
  while(true){
    Ethernet.maintain();
    ArduinoOTA.handle();
    vTaskDelay(1);
  }
  vTaskDelete(NULL);
}

void wifi_OTA(void * _){
  while(true){
    WiFi.status();
    ArduinoOTA.handle();
    vTaskDelay(1);
  }
  vTaskDelete(NULL);
}


void begin_wifi(){
  // build and set hostname
  uint64_t mac = ESP.getEfuseMac();
  uint64_t reversed_mac = 0;
  for (int i = 0; i < 6; i++) {
    reversed_mac |= ((mac >> (8 * i)) & 0xFF) << (8 * (5 - i));
  }

  Serial.print("MAC address: ");
  Serial.println(reversed_mac, HEX);

  snprintf(host, 22, (String("ESP_") + String(HOST) + String("_%llX")).c_str(), reversed_mac);
  // strcpy(host, "ESP32_OTA_TEST");

  // Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  // WiFi.setTxPower(WIFI_POWER_8_5dBm);
  // WiFi.config(IPAddress(10, 241, 229, 250), IPAddress(10, 241, 229, 1), IPAddress(10, 241, 229, 1), IPAddress(255, 255, 255, 0));
  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
  // WiFi.setHostname(host); //define hostname
  // WiFi.disconnect(false,true);
  WiFi.begin(WIFI_NAME, PASSWORD);
  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(LED_BUILTIN, LOW);
    digitalWrite(D0, LOW);
    delay(250);
    Serial.print(".");
    digitalWrite(LED_BUILTIN, HIGH);
    digitalWrite(D0, HIGH);
    delay(250);
  }
  Serial.println("");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  digitalWrite(LED_BUILTIN, LOW);

  WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
    Serial.printf("Event %d\n", event);

    if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
        Serial.printf("Disconnect reason: %d\n",
                      info.wifi_sta_disconnected.reason);
    }
  });
  ArduinoOTA.setHostname("arduino");
  ArduinoOTA.setPassword(OTA_PASS);
  ArduinoOTA
  .onStart([]() {
  })
  .onEnd([]() {
  })
  .onProgress([](unsigned int progress, unsigned int total) {
      digitalWrite(LED_BUILTIN, FAST_BLINK);
  })
  .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();

  xTaskCreatePinnedToCore(
        wifi_OTA,     // Function that should be called
        "wifi OTA",    // Name of the task (for debugging)
        20000,           // Stack size (bytes)
        NULL,            // Parameter to pass
        1,               // Task priority
        NULL,            // Task handle
        0                // pin to core 1
  );
}


bool begin_ethernet(){
  // Ethernet
  Ethernet.init(ETH_CS_PIN);
  WiFi.mode(WIFI_OFF);
  esp_netif_init();
  esp_event_loop_create_default();
  unsigned long startEthernet = millis();
  while(true){
    digitalWrite(LED_BUILTIN, LOW);
    digitalWrite(D0, LOW);
    delay(250);
    Serial.print(".");
    digitalWrite(LED_BUILTIN, HIGH);
    digitalWrite(D0, HIGH);
    delay(250);
    if (Ethernet.linkStatus() == LinkOFF) {
      Serial.println("Ethernet cable is not connected.");
      if(millis() - startEthernet > 5000){
        return false;
        break;
      }
    }
    else if(Ethernet.linkStatus() == LinkON) break;
    else{
      Serial.println("No Ethernet module found");
      if(millis() - startEthernet > 5000){
        return false;
        break;
      }
    }
  }
  if(Ethernet.begin(mac) != 0){
    Serial.print("DHCP assigned IP: ");
    Serial.println(Ethernet.localIP());
  }
  else Ethernet.begin(mac, ip, myDns);
  ArduinoOTA.setMdnsEnabled(false);
  ArduinoOTA.setHostname(host);
  ArduinoOTA.setPassword(OTA_PASS);
  ArduinoOTA
  .onStart([]() {
  })
  .onEnd([]() {
  })
  .onProgress([](unsigned int progress, unsigned int total) {
      digitalWrite(LED_BUILTIN, FAST_BLINK);
  })
  .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();

  xTaskCreatePinnedToCore(
        ethernet_OTA,     // Function that should be called
        "ethernet OTA",    // Name of the task (for debugging)
        20000,           // Stack size (bytes)
        NULL,            // Parameter to pass
        1,               // Task priority
        NULL,            // Task handle
        0                // pin to core 1
  );
  return true;
}
