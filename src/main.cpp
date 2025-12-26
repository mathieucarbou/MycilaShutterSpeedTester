#include <Arduino.h>
#include <M5Unified.h>

// #define LIGHT_SENSOR_PIN 1 // Light sensor connected directly to the unit
#define LIGHT_SENSOR_PIN 8 // GPIO8 for PortABC Port B
#define LINES            9

static uint32_t lastShutterOpen = 0; // last time the shutter was opened
static float readings[LINES] = {0};
static size_t readIndex = 0;

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
  M5.Display.setTextSize(1.7);
  M5.Display.setTextColor(WHITE);

  pinMode(LIGHT_SENSOR_PIN, INPUT);
  analogSetAttenuation(ADC_11db);
}

void loop() {
  const int raw = analogRead(LIGHT_SENSOR_PIN);
#if DEBUG
  Serial.printf("raw:%d\n", raw);
#endif

  if (lastShutterOpen == 0 && raw < 2000) {
    // shutter is opening: start recording time
    lastShutterOpen = micros();
    return;
  }

  if (lastShutterOpen && raw > 2000) {
    // shutter is closing: calculate elapsed time
    uint32_t shutterTime = micros() - lastShutterOpen;
    lastShutterOpen = 0;

    if (shutterTime < 1500) {
      // ignore very short times (if shutter opens slowly then ESP32 can read different values around 2000)
      return;
    }

    readings[readIndex] = shutterTime / 1000.0f;
    // Serial.println(readings[readIndex]);

    if (readings[readIndex] >= 1000.0f) {
      Serial.printf("%.3f s\n", readings[readIndex] / 1000.0f);
    } else {
      Serial.printf("1/%.0f s\n", 1000.0f / readings[readIndex]);
    }

    M5.update();
    M5.Display.clear();
    M5.Display.setCursor(0, 8);

    for (size_t i = (readIndex + 1) % LINES, pos = 1, rounds = 0; rounds < LINES; rounds++, i = (i + 1) % LINES) {
      if (readings[i] > 0) {
        if (readings[readIndex] >= 1000.0f) {
          M5.Display.printf("%d. %.3f s\n", pos, readings[i] / 1000.0f);
        } else {
          M5.Display.printf("%d. 1/%.0f s\n", pos, 1000.0f / readings[i]);
        }
        pos++;
      }
    }

    readIndex++;
    if (readIndex >= LINES) {
      readIndex = 0;
    }

    return;
  }

  if (!lastShutterOpen) {
    M5.update();
    if (M5.BtnA.wasPressed()) {
      for (size_t i = 0; i < LINES; i++) {
        readings[i] = 0;
      }
      readIndex = 0;
      M5.Display.clear();
    }
  }

#if DEBUG
  delay(100);
#endif
}
