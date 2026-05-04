#include <Arduino.h>
#include <Wire.h>

static const uint8_t  SDA_PIN      = 21;
static const uint8_t  SCL_PIN      = 22;
static const uint32_t BAUD         = 115200;
static const uint8_t  MPU_ADDR     = 0x68; // change to 0x69 if AD0 is HIGH
static const int      NUM_SAMPLES  = 500;
static const int      SAMPLE_MS    = 5;

static const uint8_t REG_PWR_MGMT_1   = 0x6B;
static const uint8_t REG_ACCEL_XOUT_H = 0x3B;
static const uint8_t REG_GYRO_CONFIG  = 0x1B;
static const uint8_t REG_ACCEL_CONFIG = 0x1C;

bool writeReg(uint8_t addr, uint8_t reg, uint8_t val) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission(true) == 0;
}

bool readMPU(int16_t &ax, int16_t &ay, int16_t &az,
             int16_t &tempRaw,
             int16_t &gx, int16_t &gy, int16_t &gz) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(REG_ACCEL_XOUT_H);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((int)MPU_ADDR, 14, (int)true) != 14) return false;
  ax      = (Wire.read() << 8) | Wire.read();
  ay      = (Wire.read() << 8) | Wire.read();
  az      = (Wire.read() << 8) | Wire.read();
  tempRaw = (Wire.read() << 8) | Wire.read();
  gx      = (Wire.read() << 8) | Wire.read();
  gy      = (Wire.read() << 8) | Wire.read();
  gz      = (Wire.read() << 8) | Wire.read();
  return true;
}

void setup() {
  Serial.begin(BAUD);
  delay(1000);

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(100000);

  writeReg(MPU_ADDR, REG_PWR_MGMT_1, 0x00);
  delay(100);
  writeReg(MPU_ADDR, REG_GYRO_CONFIG, 0x00);
  writeReg(MPU_ADDR, REG_ACCEL_CONFIG, 0x00);
  delay(500);

  Serial.println("Keep sensor still. Collecting samples...");

  long ax_s = 0, ay_s = 0, az_s = 0;
  long gx_s = 0, gy_s = 0, gz_s = 0;
  int  ok   = 0;

  for (int i = 0; i < NUM_SAMPLES; i++) {
    int16_t ax, ay, az, tempRaw, gx, gy, gz;
    if (readMPU(ax, ay, az, tempRaw, gx, gy, gz)) {
      ax_s += ax; ay_s += ay; az_s += az;
      gx_s += gx; gy_s += gy; gz_s += gz;
      ok++;
    }
    delay(SAMPLE_MS);
  }

  if (ok == 0) {
    Serial.println("ERROR: no samples read. Check wiring.");
    return;
  }

  float ax_g  = (ax_s / (float)ok) / 16384.0f;
  float ay_g  = (ay_s / (float)ok) / 16384.0f;
  float az_g  = (az_s / (float)ok) / 16384.0f;
  float gx_dps = (gx_s / (float)ok) / 131.0f;
  float gy_dps = (gy_s / (float)ok) / 131.0f;
  float gz_dps = (gz_s / (float)ok) / 131.0f;

  Serial.println("\n--- Averages ---");
  Serial.print("ax_g  = "); Serial.println(ax_g,  6);
  Serial.print("ay_g  = "); Serial.println(ay_g,  6);
  Serial.print("az_g  = "); Serial.println(az_g,  6);

  Serial.println("\n--- Gyro offsets (copy to main.cpp if needed) ---");
  Serial.print("gx_offset_dps = "); Serial.println(gx_dps, 6);
  Serial.print("gy_offset_dps = "); Serial.println(gy_dps, 6);
  Serial.print("gz_offset_dps = "); Serial.println(gz_dps, 6);

  Serial.println("\nDone.");
}

void loop() {}
