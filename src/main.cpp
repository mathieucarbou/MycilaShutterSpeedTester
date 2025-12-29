// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2023-2025 Mathieu Carbou
 */
#include <Arduino.h>
#include <M5Unified.h>
#include <Preferences.h>

// #define DEBUG 1

// #define LIGHT_SENSOR_PIN 1 // Light sensor connected directly to the unit
#define LIGHT_SENSOR_PIN 8 // GPIO8 for PortABC Port B

// Number of measurements to keep in history (depends on display size)
#define HISTORY_SIZE 6

// Calibration notes:
// - Used Boling BL-P1 light at 8500K at 100% with diffuser close to camera lens or lens mount
// - leaf shutter calibrated with a Rollei 35S
// - focal-plane shutter calibrated with a Canon EOS 3000
#define DEFAULT_THRESHOLD_LEAF        3200
#define DEFAULT_THRESHOLD_FOCAL_PLANE 3800

// we consider that there is no light when the raw value is below this threshold
#define DEFAULT_THRESHOLD_DARK 500

enum class State : uint8_t {
  LEAF_MEASUREMENT = 0,
  FOCAL_PLANE_MEASUREMENT = 1,
  LEAF_CALIBRATION = 2,
  FOCAL_PLANE_CALIBRATION = 3
};

struct Reading {
    uint32_t shutterOpenTime = 0;
    uint32_t shutterDuration = 0;
};

static Preferences preferences;

static Reading history[HISTORY_SIZE] = {};
static size_t historyIndex = 0;

static Reading reading = {0, 0};

static uint16_t threshold;
static State state;
static uint32_t lastDisplayUpdate;

static uint16_t read() {
  const uint16_t raw = 4095 - analogRead(LIGHT_SENSOR_PIN);
  return raw >= DEFAULT_THRESHOLD_DARK ? raw : 0;
}

static void displayHistory() {
  if (reading.shutterDuration > 0) {
    if (reading.shutterDuration >= 1000000) {
      Serial.printf("%.1f s\n", reading.shutterDuration / 1000000.0f);
    } else {
      Serial.printf("1/%.0f s\n", 1000000.0f / reading.shutterDuration);
    }
  }

  M5.Display.clear();
  M5.Display.setCursor(0, 8);

  M5.Display.setTextSize(1.6);
  M5.Display.setTextColor(BLUE);

  M5.Display.print(state == State::LEAF_MEASUREMENT ? "LEAF SHUTTER" : "FOCAL-PLANE");
  M5.Display.println();
  M5.Display.print("Measurements:");

  M5.Display.setTextSize(1.8);
  M5.Display.setTextColor(WHITE);

  M5.Display.println();

  for (size_t i = historyIndex % HISTORY_SIZE, pos = 1, rounds = 0; rounds < HISTORY_SIZE; rounds++, i = (i + 1) % HISTORY_SIZE) {
    if (history[i].shutterDuration > 0) {
      if (history[i].shutterDuration >= 1000000) {
        M5.Display.printf("%d. %.1f s\n", pos, history[i].shutterDuration / 1000000.0f);
      } else {
        M5.Display.printf("%d. 1/%.0f s\n", pos, 1000000.0f / history[i].shutterDuration);
      }
      pos++;
    }
  }
}

static void displayRawValue() {
  const uint16_t raw = read();

#ifdef DEBUG
  if (raw >= DEFAULT_THRESHOLD_DARK) {
    Serial.print(">raw:");
    Serial.print(raw);
    Serial.print("\r\n");
  }

#else
  if (lastDisplayUpdate && millis() - lastDisplayUpdate < 200)
    return;

  if (raw)
    Serial.printf("%c %d\n", raw >= threshold ? '>' : '<', raw);

  M5.Display.clear();
  M5.Display.setCursor(0, 8);
  M5.Display.setTextSize(1.6);
  M5.Display.setTextColor(BLUE);
  M5.Display.print(state == State::LEAF_CALIBRATION ? "LEAF SHUTTER" : "FOCAL-PLANE");
  M5.Display.println();
  M5.Display.print("Calibration:");
  M5.Display.setTextSize(1.8);
  M5.Display.setTextColor(WHITE);
  M5.Display.println();
  M5.Display.println();
  M5.Display.println("Threshold:");
  M5.Display.println(threshold);
  M5.Display.println();
  M5.Display.setTextColor(raw >= threshold ? YELLOW : RED);
  M5.Display.println(raw);

  lastDisplayUpdate = millis();
#endif
}

static void updateThreshold() {
  if (millis() - lastDisplayUpdate < 200) {
    return;
  }

  threshold = threshold < 4075 ? threshold + 25 : 0;

  Serial.printf("Threshold: %d\n", threshold);

  switch (state) {
    case State::LEAF_CALIBRATION:
      preferences.putUShort("t_leaf", threshold);
      break;
    case State::FOCAL_PLANE_CALIBRATION:
      preferences.putUShort("t_focal_plane", threshold);
      break;
    default:
      break;
  }

  displayRawValue();
}

static void resetCalibration() {
  Serial.println("Reset Calibration");

  switch (state) {
    case State::LEAF_CALIBRATION:
      threshold = DEFAULT_THRESHOLD_LEAF;
      preferences.putUShort("t_leaf", DEFAULT_THRESHOLD_LEAF);
      break;
    case State::FOCAL_PLANE_CALIBRATION:
      threshold = DEFAULT_THRESHOLD_FOCAL_PLANE;
      preferences.putUShort("t_focal_plane", DEFAULT_THRESHOLD_FOCAL_PLANE);
      break;
    default:
      break;
  }
}

static void resetHistory() {
  Serial.println("Reset History");

  reading = {0, 0};

  historyIndex = 0;
  for (size_t i = 0; i < HISTORY_SIZE; i++) {
    history[i] = {0};
  }

  displayHistory();
}

static void doShutterMeasurement() {
  // analogRead() on ESP32 (which M5Stack devices use) returns a value between 0 and 4095 by default.
  const uint16_t raw = read();

  if (reading.shutterOpenTime == 0 && raw >= threshold) {
    // shutter is opening: start recording time
    reading.shutterOpenTime = micros();
    return;
  }

  if (reading.shutterDuration == 0 && reading.shutterOpenTime > 0 && raw < threshold) {
    reading.shutterDuration = micros() - reading.shutterOpenTime;
    return;
  }

  if (reading.shutterDuration) {
    history[historyIndex] = reading;
    historyIndex++;
    if (historyIndex >= HISTORY_SIZE) {
      historyIndex = 0;
    }

    displayHistory();

    reading = {0, 0};
  }
}

static void switchState(State newState) {
  state = newState;
  reading = {0, 0};
  lastDisplayUpdate = 0;
  preferences.putUChar("mode", static_cast<uint8_t>(state));

  switch (state) {
    case State::LEAF_MEASUREMENT:
      Serial.println("Leaf Shutter Measurement Mode");
      threshold = preferences.getUShort("t_leaf", DEFAULT_THRESHOLD_LEAF);
      resetHistory();
      break;
    case State::FOCAL_PLANE_MEASUREMENT:
      Serial.println("Focal Plane Shutter Measurement Mode");
      threshold = preferences.getUShort("t_focal_plane", DEFAULT_THRESHOLD_FOCAL_PLANE);
      resetHistory();
      break;
    case State::LEAF_CALIBRATION:
      Serial.println("Leaf Shutter Calibration Mode");
      threshold = preferences.getUShort("t_leaf", DEFAULT_THRESHOLD_LEAF);
      displayRawValue();
      break;
    case State::FOCAL_PLANE_CALIBRATION:
      Serial.println("Focal Plane Shutter Calibration Mode");
      threshold = preferences.getUShort("t_focal_plane", DEFAULT_THRESHOLD_FOCAL_PLANE);
      displayRawValue();
      break;
    default:
      break;
  }
}

void setup() {
  esp_log_level_set("*", ESP_LOG_INFO);
  // esp_log_level_set("esp_core_dump_elf", ESP_LOG_INFO);
  // esp_log_level_set("esp_core_dump_port", ESP_LOG_INFO);
  // esp_log_level_set("esp_netif_lwip", ESP_LOG_INFO);
  // esp_log_level_set("LGFX", ESP_LOG_INFO);
  // esp_log_level_set("M5GFX", ESP_LOG_INFO);
  // esp_log_level_set("nvs", ESP_LOG_VERBOSE);
  // esp_log_level_set("ARDUINO", ESP_LOG_VERBOSE);

  Serial.begin(115200);
#if ARDUINO_USB_CDC_ON_BOOT
  Serial.setTxTimeoutMs(0);
  delay(100);
#else
  while (!Serial)
    yield();
#endif

  preferences.begin("MSST", false);

  M5.begin();
  M5.Display.setRotation(0);
  M5.Display.setBrightness(128);

  pinMode(LIGHT_SENSOR_PIN, INPUT);
  analogSetAttenuation(ADC_11db);

  Serial.println("Shutter Speed Tester Ready");

  switchState(static_cast<State>(preferences.getUChar("mode", static_cast<uint8_t>(State::LEAF_MEASUREMENT))));
}

void loop() {
  M5.update();

  if (M5.BtnA.wasSingleClicked()) {
    switch (state) {
      case State::LEAF_MEASUREMENT:
      case State::FOCAL_PLANE_MEASUREMENT:
        resetHistory();
        break;
      case State::LEAF_CALIBRATION:
      case State::FOCAL_PLANE_CALIBRATION:
        resetCalibration();
        break;
      default:
        break;
    }
    return;
  }

  if (M5.BtnA.wasDoubleClicked()) {
    switchState(static_cast<State>((static_cast<uint8_t>(state) + 1) % 4));
    return;
  }

  if (M5.BtnA.isHolding()) {
    switch (state) {
      case State::LEAF_CALIBRATION:
      case State::FOCAL_PLANE_CALIBRATION:
        updateThreshold();
        break;
      default:
        break;
    }
    return;
  }

  switch (state) {
    case State::LEAF_MEASUREMENT:
    case State::FOCAL_PLANE_MEASUREMENT:
      doShutterMeasurement();
      break;
    case State::LEAF_CALIBRATION:
    case State::FOCAL_PLANE_CALIBRATION:
      displayRawValue();
      break;
    default:
      break;
  }
}
