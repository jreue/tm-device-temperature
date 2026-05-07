#include <Arduino.h>
#include <DallasTemperature.h>
#include <FastLED.h>
#include <OneWire.h>

#include "hardware_config.h"

OneWire oneWire(ONE_WIRE_PIN);
DallasTemperature sensors(&oneWire);

struct Probe {
    const char* name;
    DeviceAddress address;
};

Probe probes[] = {
    {"PROBE1", {0x28, 0xFF, 0xE2, 0x12, 0x91, 0x16, 0x04, 0x8A}},  // cool probe
    {"PROBE2", {0x28, 0x66, 0xD4, 0x07, 0x00, 0x00, 0x80, 0x24}},  // hot probe
};

bool coolComplete = false;
bool hotComplete = false;

CRGB leds[NUM_PIXELS];

static const int COOL_START = 0;
static const int COOL_END = (NUM_PIXELS / 2) - 1;
static const int HOT_START = NUM_PIXELS / 2;
static const int HOT_END = NUM_PIXELS - 1;

void detectSensorAddresses();

void checkTemperatures();
void handleCoolTargetReached();
void handleHotTargetReached();
void playIdleCoolEffect();
void playIdleHotEffect();
void playCompleteCoolEffect();
void playCompleteHotEffect();
void playFinalEffect();
bool isCalibrated();

void setup() {
  Serial.begin(115200);

  sensors.begin();
  // Don't block for ~750ms after requestTemperatures(); read results in a separate loop tick
  // instead
  sensors.setWaitForConversion(false);

  FastLED.addLeds<WS2812B, LED_DATA_PIN, GRB>(leds, NUM_PIXELS);
  FastLED.setBrightness(180);

#ifdef DETECTION_MODE
  detectSensorAddresses();
#endif
}

void loop() {
#ifdef DETECTION_MODE
  return;
#else
  static unsigned long lastLed = 0;
  static unsigned long lastTemp = 0;
  static bool conversionPending = false;
  unsigned long now = millis();

  // Update LEDs at ~30 fps (every 33 ms)
  if (now - lastLed >= 33) {
    lastLed = now;
    if (isCalibrated()) {
      playFinalEffect();
    } else {
      coolComplete ? playCompleteCoolEffect() : playIdleCoolEffect();
      hotComplete ? playCompleteHotEffect() : playIdleHotEffect();
    }
    FastLED.show();
  }

  // Check temperatures every ~750 ms, but only after a conversion request has been made and
  // at least 750 ms has passed since then (DS18B20 max conversion time is 750 ms)
  if (!isCalibrated()) {
    if (!conversionPending && now - lastTemp >= 2000) {
      lastTemp = now;
      sensors.requestTemperatures();
      conversionPending = true;
    } else if (conversionPending && now - lastTemp >= 750) {
      conversionPending = false;
      checkTemperatures();
    }
  }
#endif
}

void detectSensorAddresses() {
  Serial.println("Scanning for DS18B20 devices...");
  DeviceAddress addr;
  int found = 0;

  oneWire.reset_search();
  while (oneWire.search(addr)) {
    if (addr[0] != 0x28)
      continue;
    found++;
    Serial.printf("Device %d: { ", found);
    for (int i = 0; i < 8; i++) {
      Serial.printf("0x%02X", addr[i]);
      if (i < 7)
        Serial.print(", ");
    }
    Serial.println(" }");
  }

  if (found == 0)
    Serial.println("No devices found.");
}

bool isCalibrated() {
  return coolComplete && hotComplete;
}

void checkTemperatures() {
  if (!coolComplete) {
    float t = sensors.getTempF(probes[0].address);
    Serial.printf("PROBE1 = %.1f  ", t);
    if (t <= COLD_TARGET_TEMP)
      handleCoolTargetReached();
  }

  if (!hotComplete) {
    float t = sensors.getTempF(probes[1].address);
    Serial.printf("PROBE2 = %.1f", t);
    if (t >= HOT_TARGET_TEMP)
      handleHotTargetReached();
  }

  Serial.println();
}

void handleCoolTargetReached() {
  float t = sensors.getTempF(probes[0].address);
  Serial.printf("Cool target reached. PROBE1 = %.1f\n", t);
  coolComplete = true;
}

void handleHotTargetReached() {
  float t = sensors.getTempF(probes[1].address);
  Serial.printf("Hot target reached. PROBE2 = %.1f\n", t);
  hotComplete = true;
}

void playIdleCoolEffect() {
  // Gentle snow: random cool-segment pixels sparkle blue/white and fade
  for (int i = COOL_START; i <= COOL_END; i++) {
    leds[i].fadeToBlackBy(10);
  }
  if (random8() < 30) {
    int idx = COOL_START + random8(COOL_END - COOL_START + 1);
    leds[idx] = (random8() < 180) ? CRGB(100, 100, 255) : CRGB::White;
  }
}

void playIdleHotEffect() {
  // Gentle embers: random hot-segment pixels flicker red/orange/yellow and fade
  for (int i = HOT_START; i <= HOT_END; i++) {
    leds[i].fadeToBlackBy(12);
  }
  if (random8() < 35) {
    int idx = HOT_START + random8(HOT_END - HOT_START + 1);
    uint8_t r = random8(3);
    if (r == 0)
      leds[idx] = CRGB(255, 20, 0);  // red
    else if (r == 1)
      leds[idx] = CRGB(255, 100, 0);  // orange
    else
      leds[idx] = CRGB(255, 200, 0);  // yellow
  }
}

void playCompleteCoolEffect() {
  // Cold-energy comet sweeping from pixel 0 toward centre (COOL_END)
  static uint8_t pos = 0;
  static unsigned long lastStep = 0;

  for (int i = COOL_START; i <= COOL_END; i++) {
    leds[i].fadeToBlackBy(40);
  }

  unsigned long now = millis();
  if (now - lastStep >= 25) {
    lastStep = now;
    pos = (pos + 1) % (COOL_END - COOL_START + 1);
  }

  int head = COOL_START + pos;
  leds[head] = CRGB::White;
  if (head - 1 >= COOL_START)
    leds[head - 1] = CRGB(150, 150, 255);
  if (head - 2 >= COOL_START)
    leds[head - 2] = CRGB(60, 60, 200);
  if (head - 3 >= COOL_START)
    leds[head - 3] = CRGB(20, 20, 120);
}

void playCompleteHotEffect() {
  // Hot-energy comet sweeping from pixel 59 toward centre (HOT_START)
  static uint8_t pos = 0;
  static unsigned long lastStep = 0;

  for (int i = HOT_START; i <= HOT_END; i++) {
    leds[i].fadeToBlackBy(40);
  }

  unsigned long now = millis();
  if (now - lastStep >= 25) {
    lastStep = now;
    pos = (pos + 1) % (HOT_END - HOT_START + 1);
  }

  int head = HOT_END - pos;
  leds[head] = CRGB::White;
  if (head + 1 <= HOT_END)
    leds[head + 1] = CRGB(255, 160, 0);
  if (head + 2 <= HOT_END)
    leds[head + 2] = CRGB(220, 60, 0);
  if (head + 3 <= HOT_END)
    leds[head + 3] = CRGB(120, 10, 0);
}

void playFinalEffect() {
  // Nuclear green + purple energy cycling across all 60 pixels
  static uint8_t hueOffset = 0;
  static unsigned long lastStep = 0;

  unsigned long now = millis();
  if (now - lastStep >= 20) {
    lastStep = now;
    hueOffset += 3;
  }

  for (int i = 0; i < NUM_PIXELS; i++) {
    uint8_t wave = sin8((i * 255 / NUM_PIXELS) + hueOffset);
    if (wave > 128) {
      // nuclear green
      leds[i] = CRGB(map(wave, 128, 255, 0, 80), map(wave, 128, 255, 100, 255), 0);
    } else {
      // purple
      leds[i] = CRGB(map(wave, 0, 128, 80, 160), 0, map(wave, 0, 128, 80, 200));
    }
  }
}
