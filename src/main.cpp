// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2023-2025 Mathieu Carbou
 */
#include <Arduino.h>
#include <M5Unified.h>

// #define LIGHT_SENSOR_PIN 1 // Light sensor connected directly to the unit
#define LIGHT_SENSOR_PIN 8 // GPIO8 for PortABC Port B
#define LINES            7

// Calibration notes:
// - Used Boling BL-P1 light at 8500K without diffuser close to camera lens or lens mount
// - leaf shutter calibrated with a Rollei 35S
// - focal-plane shutter calibrated with a Canon EOS 3000
#define THRESHOLD_LEAF        800
#define THRESHOLD_FOCAL_PLANE 225

enum class ShutterMode {
  Leaf,
  FocalPlane
};

struct Reading {
    uint32_t shutterOpenTime = 0;
    uint32_t shutterDuration = 0;
};

static Reading reading = {0, 0};
static Reading history[LINES] = {};
static size_t historyIndex = 0;

static ShutterMode currentMode = ShutterMode::Leaf;
static int threshold = currentMode == ShutterMode::Leaf ? THRESHOLD_LEAF : THRESHOLD_FOCAL_PLANE;

static void reset() {
  for (size_t i = 0; i < LINES; i++) {
    history[i] = {0};
  }
  historyIndex = 0;

  reading = {0, 0};

  M5.Display.clear();
  M5.Display.setCursor(0, 8);
  M5.Display.println(currentMode == ShutterMode::Leaf ? "Leaf" : "Focal-plane");

  Serial.println("Shutter Speed Tester Ready");
  Serial.println("Shutter Mode: " + String(currentMode == ShutterMode::Leaf ? "Leaf" : "Focal-plane"));
}

static void switchMode() {
  currentMode = currentMode == ShutterMode::Leaf ? ShutterMode::FocalPlane : ShutterMode::Leaf;
  threshold = currentMode == ShutterMode::Leaf ? THRESHOLD_LEAF : THRESHOLD_FOCAL_PLANE;
  Serial.printf("Switched to %s shutter mode (threshold=%d)\n", currentMode == ShutterMode::Leaf ? "Leaf" : "Focal-plane", threshold);

  reset();
}

void setup() {
  Serial.begin(115200);
#if ARDUINO_USB_CDC_ON_BOOT
  Serial.setTxTimeoutMs(0);
  delay(100);
#else
  while (!Serial)
    yield();
#endif

  M5.begin();
  M5.Display.setRotation(0);
  M5.Display.setBrightness(128);
  M5.Display.setTextSize(1.9);
  M5.Display.setTextColor(WHITE);

  pinMode(LIGHT_SENSOR_PIN, INPUT);
  analogSetAttenuation(ADC_11db);

  reset();
}

void loop() {
  M5.update();

  if (M5.BtnA.wasSingleClicked()) {
    reset();
  }

  if (M5.BtnA.wasDoubleClicked()) {
    switchMode();
  }

  const int raw = analogRead(LIGHT_SENSOR_PIN);

  // print only relevant values
  // if (raw <= 3500)
  //   Serial.printf("raw:%d\n", raw);

  if (reading.shutterOpenTime == 0 && raw < threshold) {
    // shutter is opening: start recording time
    reading.shutterOpenTime = micros();
    return;
  }

  if (reading.shutterDuration == 0 && reading.shutterOpenTime > 0 && raw > threshold) {
    reading.shutterDuration = micros() - reading.shutterOpenTime;
    return;
  }

  if (reading.shutterDuration) {
    history[historyIndex] = reading;

    Serial.printf("Shutter: %" PRIu32 " us => %.0f ms => ", reading.shutterDuration, reading.shutterDuration / 1000.0f);
    if (history[historyIndex].shutterDuration >= 1000000) {
      Serial.printf("%.1f s\n", history[historyIndex].shutterDuration / 1000000.0f);
    } else {
      Serial.printf("1/%.0f s\n", 1000000.0f / history[historyIndex].shutterDuration);
    }
    Serial.println("----");

    M5.update();
    M5.Display.clear();
    M5.Display.setCursor(0, 8);

    M5.Display.println(currentMode == ShutterMode::Leaf ? "Leaf" : "Focal-plane");
    for (size_t i = (historyIndex + 1) % LINES, pos = 1, rounds = 0; rounds < LINES; rounds++, i = (i + 1) % LINES) {
      if (history[i].shutterDuration > 0) {
        if (history[i].shutterDuration >= 1000000) {
          M5.Display.printf("%d. %.1f s\n", pos, history[i].shutterDuration / 1000000.0f);
        } else {
          M5.Display.printf("%d. 1/%.0f s\n", pos, 1000000.0f / history[i].shutterDuration);
        }
        pos++;
      }
    }

    // prepare mext entry
    reading = {0, 0};
    historyIndex++;
    if (historyIndex >= LINES) {
      historyIndex = 0;
    }
  }
}
