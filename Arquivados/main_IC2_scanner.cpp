#include <Arduino.h>
#include <Wire.h>

// ESP32 default I2C pins: SDA=21, SCL=22
// Change these if you wired them differently
#define SDA_PIN 21
#define SCL_PIN 22

void scanI2C() {
  Serial.println("\n--- I2C Scanner ---");
  Serial.printf("SDA: GPIO%d  |  SCL: GPIO%d\n", SDA_PIN, SCL_PIN);

  int found = 0;
  for (byte addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    byte err = Wire.endTransmission();

    if (err == 0) {
      Serial.printf("  Device found at 0x%02X", addr);
      if (addr == 0x68) Serial.print("  <-- MPU6050 (AD0=LOW)");
      if (addr == 0x69) Serial.print("  <-- MPU6050 (AD0=HIGH)");
      Serial.println();
      found++;
    }
  }

  if (found == 0) {
    Serial.println("  No I2C devices found!");
    Serial.println("  Check: SDA/SCL wiring, 3.3V power, and pull-up resistors.");
  } else {
    Serial.printf("  %d device(s) found.\n", found);
  }
  Serial.println("-------------------\n");
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Wire.begin(SDA_PIN, SCL_PIN);
  scanI2C();
}

void loop() {
  scanI2C();
  delay(3000);
}