// Copyright 2024 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/**
 * @brief This example demonstrates Zigbee contact switch (IAS Zone).
 */

#include <Arduino.h>
#ifndef ZIGBEE_MODE_ED
#error "Zigbee end device mode is not selected in Tools->Zigbee mode"
#endif

#include "Zigbee.h"
#include <Preferences.h>

/* Zigbee contact sensor configuration */
#define CONTACT_SWITCH_ENDPOINT_NUMBER 1

uint8_t reset_button = BOOT_PIN;
// schwarz = GND, blau = sensor pin
uint8_t sensor_pin = 18;
// Test-Tamper Button an D2: HIGH toggelt tamper
uint8_t tamper_pin = 2;

ZigbeeContactSwitch zbContactSwitch =
    ZigbeeContactSwitch(CONTACT_SWITCH_ENDPOINT_NUMBER);

/* Preferences for storing ENROLLED flag to persist across reboots */
Preferences preferences;

static uint16_t zoneStatus = 0;

bool publishZoneStatus(uint16_t status) {
  zoneStatus = status;

  esp_zb_ieee_addr_t iasCieAddr = {0};
  uint8_t zoneId = 0xff;

  esp_zb_lock_acquire(portMAX_DELAY);

  esp_err_t setRet = esp_zb_zcl_set_attribute_val(
      CONTACT_SWITCH_ENDPOINT_NUMBER, ESP_ZB_ZCL_CLUSTER_ID_IAS_ZONE,
      ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_IAS_ZONE_ZONESTATUS_ID,
      &zoneStatus, false);

  auto *cieAttr = esp_zb_zcl_get_attribute(
      CONTACT_SWITCH_ENDPOINT_NUMBER, ESP_ZB_ZCL_CLUSTER_ID_IAS_ZONE,
      ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
      ESP_ZB_ZCL_ATTR_IAS_ZONE_IAS_CIE_ADDRESS_ID);
  if (cieAttr && cieAttr->data_p) {
    memcpy(iasCieAddr, cieAttr->data_p, sizeof(esp_zb_ieee_addr_t));
  }

  auto *zoneIdAttr = esp_zb_zcl_get_attribute(
      CONTACT_SWITCH_ENDPOINT_NUMBER, ESP_ZB_ZCL_CLUSTER_ID_IAS_ZONE,
      ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_IAS_ZONE_ZONEID_ID);
  if (zoneIdAttr && zoneIdAttr->data_p) {
    zoneId = *static_cast<uint8_t *>(zoneIdAttr->data_p);
  }

  esp_zb_lock_release();

  if (setRet != ESP_OK) {
    Serial.printf("Failed to set IAS zone status: 0x%x\n", setRet);
    return false;
  }

  esp_zb_zcl_ias_zone_status_change_notif_cmd_t notifCmd;
  notifCmd.address_mode = ESP_ZB_APS_ADDR_MODE_64_ENDP_PRESENT;
  notifCmd.zcl_basic_cmd.src_endpoint = CONTACT_SWITCH_ENDPOINT_NUMBER;
  notifCmd.zcl_basic_cmd.dst_endpoint = 1; // IAS CIE endpoint (default 1)
  memcpy(notifCmd.zcl_basic_cmd.dst_addr_u.addr_long, iasCieAddr,
         sizeof(esp_zb_ieee_addr_t));
  notifCmd.zone_status = zoneStatus;
  notifCmd.extend_status = 0;
  notifCmd.zone_id = zoneId;
  notifCmd.delay = 0;

  esp_zb_lock_acquire(portMAX_DELAY);
  uint8_t notifTsn = esp_zb_zcl_ias_zone_status_change_notif_cmd_req(&notifCmd);
  esp_zb_lock_release();

  // API returns ZCL transaction sequence number (TSN), not an esp_err_t.
  Serial.printf("IAS status notification sent (TSN: 0x%02x)\n", notifTsn);

  return true;
}

void setContactState(bool isOpen) {
  uint16_t status = zoneStatus;
  if (isOpen) {
    status |= (ESP_ZB_ZCL_IAS_ZONE_ZONE_STATUS_ALARM1 |
               ESP_ZB_ZCL_IAS_ZONE_ZONE_STATUS_ALARM2);
  } else {
    status &= ~(ESP_ZB_ZCL_IAS_ZONE_ZONE_STATUS_ALARM1 |
                ESP_ZB_ZCL_IAS_ZONE_ZONE_STATUS_ALARM2);
  }
  publishZoneStatus(status);
}

void toggleTamper() {
  uint16_t status = zoneStatus;
  status ^= ESP_ZB_ZCL_IAS_ZONE_ZONE_STATUS_TAMPER;
  bool tamperOn = (status & ESP_ZB_ZCL_IAS_ZONE_ZONE_STATUS_TAMPER) != 0;
  Serial.printf("Tamper toggled: %s\n", tamperOn ? "ON" : "OFF");
  publishZoneStatus(status);
}

void setup() {
  Serial.begin(115200);

  preferences.begin("Zigbee", false);
  bool enrolled = preferences.getBool("ENROLLED");
  preferences.end();

  pinMode(reset_button, INPUT_PULLUP);
  pinMode(sensor_pin, INPUT_PULLUP);
  pinMode(tamper_pin, INPUT_PULLDOWN);

  zbContactSwitch.setManufacturerAndModel("Espressif", "ZigbeeContactSwitch");
  Zigbee.addEndpoint(&zbContactSwitch);

  Serial.println("Starting Zigbee...");
  if (!Zigbee.begin()) {
    Serial.println("Zigbee failed to start!");
    Serial.println("Rebooting...");
    ESP.restart();
  } else {
    Serial.println("Zigbee started successfully!");
  }

  Serial.println("Connecting to network");
  while (!Zigbee.connected()) {
    Serial.print(".");
    delay(100);
  }
  Serial.println();

  if (enrolled) {
    Serial.println("Device has been enrolled before - restoring IAS Zone enrollment");
    zbContactSwitch.restoreIASZoneEnroll();
  } else {
    Serial.println("Device is factory new - requesting new IAS Zone enrollment");
    zbContactSwitch.requestIASZoneEnroll();
  }

  while (!zbContactSwitch.enrolled()) {
    Serial.print(".");
    delay(100);
  }
  Serial.println();
  Serial.println("Zigbee enrolled successfully!");

  if (!enrolled) {
    preferences.begin("Zigbee", false);
    preferences.putBool("ENROLLED", true);
    preferences.end();
    Serial.println("ENROLLED flag saved to preferences");
  }

  // Nach Neustart: tamper clear. Kontaktstatus direkt aus Eingang setzen.
  zoneStatus = 0;
  setContactState(digitalRead(sensor_pin) == HIGH);
}

void loop() {
  static bool contactOpen = false;
  static bool lastTamperInputHigh = false;

  bool currentContactOpen = (digitalRead(sensor_pin) == HIGH);
  if (currentContactOpen != contactOpen) {
    setContactState(currentContactOpen);
    contactOpen = currentContactOpen;
  }

  bool tamperInputHigh = (digitalRead(tamper_pin) == HIGH);
  if (tamperInputHigh && !lastTamperInputHigh) {
    toggleTamper();
  }
  lastTamperInputHigh = tamperInputHigh;

  if (digitalRead(reset_button) == LOW) {
    delay(100);
    int startTime = millis();
    while (digitalRead(reset_button) == LOW) {
      delay(50);
      if ((millis() - startTime) > 3000) {
        Serial.println("Resetting Zigbee to factory and rebooting in 1s.");
        preferences.begin("Zigbee", false);
        preferences.putBool("ENROLLED", false);
        preferences.end();
        Serial.println("ENROLLED flag cleared from preferences");
        delay(1000);
        Zigbee.factoryReset();
      }
    }
  }

  delay(100);
}
