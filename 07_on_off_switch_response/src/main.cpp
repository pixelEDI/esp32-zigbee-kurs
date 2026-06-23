#include <Arduino.h>
#ifndef ZIGBEE_MODE_ED
#error "Zigbee end device mode is not selected in Tools->Zigbee mode"
#endif
#include "Zigbee.h"
#define BUTTON_ON 2
#define BUTTON_OFF 21
ZigbeeSwitch zbSwitch(1);
void onResponse(zb_cmd_type_t command, esp_zb_zcl_status_t status) {
  Serial.printf("Default response -> command: %d, status: %s\n", command,
                esp_zb_zcl_status_to_name(status));
}
void setup() {
  Serial.begin(115200);
  delay(200);
  pinMode(BUTTON_ON, INPUT_PULLUP);
  pinMode(BUTTON_OFF, INPUT_PULLUP);
  zbSwitch.setManufacturerAndModel("pixeledi", "2button-switch");
  zbSwitch.setManualBinding(true);
  // Callback für Responses
  zbSwitch.onDefaultResponse(onResponse);
  Zigbee.addEndpoint(&zbSwitch);
  if (!Zigbee.begin()) {
    Serial.println("Zigbee start failed");
    ESP.restart();
  }
  Serial.println("Connecting to Zigbee network...");
  while (!Zigbee.connected()) {
    Serial.print(".");
    delay(100);
  }
  Serial.println();
  Serial.println("Connected");
}

void loop() {
  static bool lastOn = HIGH;
  static bool lastOff = HIGH;
  bool onState = digitalRead(BUTTON_ON);
  bool offState = digitalRead(BUTTON_OFF);
  // Button ON gedrückt
  if (lastOn == HIGH && onState == LOW) {
    Serial.println("ON pressed");
    zbSwitch.lightOn();
  }
  // Button OFF gedrückt
  if (lastOff == HIGH && offState == LOW) {
    Serial.println("OFF pressed");
    zbSwitch.lightOff();
  }
  lastOn = onState;
  lastOff = offState;
  delay(20);
}
