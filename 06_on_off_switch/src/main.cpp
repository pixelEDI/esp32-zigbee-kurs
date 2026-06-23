#include <Arduino.h>
#ifndef ZIGBEE_MODE_ED
#error "Zigbee end device mode is not selected in Tools->Zigbee mode"
#endif
#include "Zigbee.h"
#define BUTTON_ON 2
#define BUTTON_OFF 21
ZigbeeSwitch zbSwitch(1);

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_ON, INPUT_PULLUP);
  pinMode(BUTTON_OFF, INPUT_PULLUP);
  zbSwitch.setManufacturerAndModel("pixeledi", "2button-switch");
  // Binding über Z2M
  zbSwitch.setManualBinding(true);
  Zigbee.addEndpoint(&zbSwitch);
  if (!Zigbee.begin()) {
    ESP.restart();
  }
  while (!Zigbee.connected()) {
    delay(100);
  }
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
