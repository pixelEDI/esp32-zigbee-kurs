// unter reporting in zigbee2mqtt battery werte anpassen damit aktualisierung
// angezeigt wird
#ifndef ZIGBEE_MODE_ED
#error "Zigbee end device mode is not selected in Tools->Zigbee mode"
#endif
#include "SHT31.h"
#include "Zigbee.h"
#include <Arduino.h>
#include <Wire.h>
SHT31 sht;
#define TEMP_SENSOR_ENDPOINT_NUMBER 10
ZigbeeTempSensor zbTempSensor(TEMP_SENSOR_ENDPOINT_NUMBER);

static void temp_sensor_value_update(void *arg) {
  for (;;) {
    if (sht.read()) {
      float temperature = sht.getTemperature();
      float humidity = sht.getHumidity();
      zbTempSensor.setTemperature(temperature);
      zbTempSensor.setHumidity(humidity);
      zbTempSensor.report();
    } else {
      Serial.println("SHT31 read failed");
    }
    vTaskDelay(pdMS_TO_TICKS(5000));
  }
}
void setup() {
  Serial.begin(115200);
  pinMode(WIFI_ENABLE, OUTPUT);
  digitalWrite(WIFI_ENABLE, LOW);
  delay(100);
  pinMode(WIFI_ANT_CONFIG, OUTPUT);
  digitalWrite(WIFI_ANT_CONFIG, HIGH);
  Wire.begin();
  sht.begin();
  zbTempSensor.setManufacturerAndModel("pixeledi", "pixel-thermometer");
  zbTempSensor.addHumiditySensor(0, 100, 1, 0.0);
  Zigbee.addEndpoint(&zbTempSensor);
  if (!Zigbee.begin()) {
    ESP.restart();
  }
  while (!Zigbee.connected()) {
    delay(100);
  }
  zbTempSensor.setReporting(5, 10, 0.3);
  zbTempSensor.setHumidityReporting(1, 5, 0.1);
  xTaskCreate(temp_sensor_value_update, "temp_sensor_update", 4096, NULL, 5,
              NULL);
}
void loop() { delay(1000); }
