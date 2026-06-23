#include "esp32-hal-gpio.h"
#ifndef ZIGBEE_MODE_ED
#error "Zigbee end device mode is not selected in Tools->Zigbee mode"
#endif
#include "SHT31.h"
#include "Zigbee.h"
#include "esp_sleep.h"
#include "esp_system.h"
#include <Arduino.h>
#include <Wire.h>
#define ZIGBEE_DEFAULT_ED_CONFIG()                                             \
  {                                                                            \
      .esp_zb_role = ESP_ZB_DEVICE_TYPE_ED,                                    \
      .install_code_policy = false,                                            \
      .nwk_cfg =                                                               \
          {                                                                    \
              .zed_cfg =                                                       \
                  {                                                            \
                      .ed_timeout = ESP_ZB_ED_AGING_TIMEOUT_64MIN,             \
                      .keep_alive = 3000,                                      \
                  },                                                           \
          },                                                                   \
  }
#define TEMP_SENSOR_ENDPOINT_NUMBER 10
#define uS_TO_S_FACTOR 1000000ULL
#define TIME_TO_SLEEP 5
#define JOIN_TIMEOUT_MS 60000
#define POST_JOIN_DELAY_MS 30000
#define ZIGBEE_START_TIMEOUT_MS 10000

const uint8_t SENSE_POWER_PIN = 16;
const uint8_t BATTERY_ADC_PIN = A0;
const float BATTERY_DIVIDER_RATIO = 2.0f;
const float MIN_BATTERY_VOLTAGE = 3.0;
const float MAX_BATTERY_VOLTAGE = 4.2;

SHT31 sht;
ZigbeeTempSensor zbTempSensor(TEMP_SENSOR_ENDPOINT_NUMBER);

void flashLED(int n) {
  for (int i = 0; i < n; i++) {
    digitalWrite(LED_BUILTIN, LOW);
    delay(100);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(200);
  }
}

uint8_t mapFloatToPercent(float x, float inMin, float inMax) {
  float value = (x - inMin) * 100.0f / (inMax - inMin);
  if (value < 0.0f) {
    value = 0.0f;
  } else if (value > 100.0f) {
    value = 100.0f;
  }
  return static_cast<uint8_t>(value);
}

float readBatteryVoltage() {
  // Some boards return 0 mV on the first ADC reads after wakeup.
  for (int i = 0; i < 4; i++) {
    (void)analogReadMilliVolts(BATTERY_ADC_PIN);
    delay(2);
  }

  uint32_t adcMilliVolts = 0;
  for (int i = 0; i < 32; i++) {
    adcMilliVolts += analogReadMilliVolts(BATTERY_ADC_PIN);
  }
  float milliVolts = adcMilliVolts / 32.0f;

  // Fallback for cases where analogReadMilliVolts is not calibrated and returns
  // near 0: use raw ADC (12-bit) with 3.3V reference approximation.
  if (milliVolts < 50.0f) {
    uint32_t adcRaw = 0;
    for (int i = 0; i < 32; i++) {
      adcRaw += analogRead(BATTERY_ADC_PIN);
    }
    float rawAvg = adcRaw / 32.0f;
    milliVolts = (rawAvg / 4095.0f) * 3300.0f;
  }

  return (BATTERY_DIVIDER_RATIO * milliVolts) / 1000.0f;
}

void measureReportAndSleep() {
  float temperature = NAN;
  float humidity = NAN;

  if (!sht.read()) {
    flashLED(3);
  } else {
    temperature = sht.getTemperature();
    humidity = sht.getHumidity();
  }

  uint8_t batteryPercent = random(40, 100);
  float batteryVoltage =
      MIN_BATTERY_VOLTAGE +
      (MAX_BATTERY_VOLTAGE - MIN_BATTERY_VOLTAGE) * (batteryPercent / 100.0f);

  zbTempSensor.setBatteryPercentage(batteryPercent);
  zbTempSensor.setBatteryVoltage(static_cast<uint8_t>(batteryVoltage * 10.0f));

  if (!isnan(temperature) && !isnan(humidity)) {
    zbTempSensor.setTemperature(temperature);
    zbTempSensor.setHumidity(humidity);
    zbTempSensor.report();
  }
  zbTempSensor.reportBatteryPercentage();

  flashLED(1);
  delay(500);

  Wire.end();
  delay(1);
  digitalWrite(SENSE_POWER_PIN, LOW);
  delay(10);

  pinMode(SDA, INPUT);
  pinMode(SCL, INPUT);
  gpio_pullup_dis((gpio_num_t)SDA);
  gpio_pullup_dis((gpio_num_t)SCL);
  gpio_pulldown_dis((gpio_num_t)SDA);
  gpio_pulldown_dis((gpio_num_t)SCL);

  esp_deep_sleep_start();
}

void setup() {
  Serial.begin(115200);
  pinMode(WIFI_ENABLE, OUTPUT);
  digitalWrite(WIFI_ENABLE, LOW);
  delay(100);
  pinMode(WIFI_ANT_CONFIG, OUTPUT);
  digitalWrite(WIFI_ANT_CONFIG, HIGH);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  flashLED(2);

  pinMode(SENSE_POWER_PIN, OUTPUT);
  digitalWrite(SENSE_POWER_PIN, HIGH);
  delay(100);

  Wire.begin();
  sht.begin();

  pinMode(BATTERY_ADC_PIN, INPUT);
  randomSeed(micros());
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);

  zbTempSensor.setManufacturerAndModel("pixeledi", "xiao-c6-sht3x-battery");
  zbTempSensor.setMinMaxValue(-20, 80);
  zbTempSensor.setTolerance(0.5);
  zbTempSensor.setPowerSource(ZB_POWER_SOURCE_BATTERY, 100, 35);
  zbTempSensor.addHumiditySensor(0, 100, 1);

  Zigbee.addEndpoint(&zbTempSensor);

  esp_zb_cfg_t zigbeeConfig = ZIGBEE_DEFAULT_ED_CONFIG();
  zigbeeConfig.nwk_cfg.zed_cfg.keep_alive =
      (TIME_TO_SLEEP + 60) * 1000;
  Zigbee.setTimeout(ZIGBEE_START_TIMEOUT_MS);

  if (!Zigbee.begin(&zigbeeConfig, false)) {
    flashLED(10);
    ESP.restart();
  }

  unsigned long joinStart = millis();
  while (!Zigbee.connected()) {
    delay(500);
    if (millis() - joinStart > JOIN_TIMEOUT_MS) {
      flashLED(5);
      ESP.restart();
    }
  }

  // Give a pairing window after fresh boot, skip delay on deep sleep wake.
  if (esp_reset_reason() != ESP_RST_DEEPSLEEP) {
    delay(POST_JOIN_DELAY_MS);
  }

  measureReportAndSleep();
}

void loop() {}
