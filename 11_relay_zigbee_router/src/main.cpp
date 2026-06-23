#include <Arduino.h>
#ifndef ZIGBEE_MODE_ZCZR
#error "Zigbee router mode is not selected in Tools->Zigbee mode"
#endif
#include "Zigbee.h"
/* Zigbee power outlet configuration */
#define ZIGBEE_OUTLET_ENDPOINT 1
static const uint8_t relayPin = 1;
static const bool RELAY_ACTIVE_LOW = false;
static const uint8_t ledPin = LED_BUILTIN;
uint8_t button = BOOT_PIN;
ZigbeePowerOutlet zbOutlet = ZigbeePowerOutlet(ZIGBEE_OUTLET_ENDPOINT);
/********************* Relay functions **************************/
void setRelay(bool value) {
  bool pinLevel = RELAY_ACTIVE_LOW ? !value : value;
  digitalWrite(relayPin, pinLevel);
  digitalWrite(ledPin, value ? LOW : HIGH);
}
void setup() {
  Serial.begin(115200);
  // Init relay pin and turn relay OFF
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, RELAY_ACTIVE_LOW ? HIGH : LOW);
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);
  pinMode(button, INPUT_PULLUP);
  zbOutlet.setManufacturerAndModel("Espressif", "ZBPowerOutlet");
  zbOutlet.onPowerOutletChange(setRelay);
  Serial.println("Adding ZigbeePowerOutlet endpoint to Zigbee Core");
  Zigbee.addEndpoint(&zbOutlet);
  // When all EPs are registered, start Zigbee. By default acts as
  // ZIGBEE_END_DEVICE
  if (!Zigbee.begin(ZIGBEE_ROUTER)) {
    Serial.println("Zigbee failed to start!");
    Serial.println("Rebooting...");
    ESP.restart();
  }
  Serial.println("Connecting to network");
  while (!Zigbee.connected()) {
    Serial.print(".");
    delay(100);
  }
  Serial.println();
}

void loop() {
  // Checking button for factory reset
  if (digitalRead(button) == LOW) { // Push button pressed
    // Key debounce handling
    delay(100);
    int startTime = millis();
    while (digitalRead(button) == LOW) {
      delay(50);
      if ((millis() - startTime) > 3000) {
        // If key pressed for more than 3secs, factory reset Zigbee and reboot
        Serial.println("Resetting Zigbee to factory and rebooting in 1s.");
        delay(1000);
        Zigbee.factoryReset();
      }
    }
    // Toggle state by pressing the button
    zbOutlet.setState(!zbOutlet.getPowerOutletState());
  }
  delay(100);
}
