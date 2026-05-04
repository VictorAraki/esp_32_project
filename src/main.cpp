#include <Arduino.h>
#include <Wire.h>
#include <MPU6050_light.h>

static const char    *NODE_ID     = "esp32_node_01";
static const char    *FW_VERSION  = "0.1.0";
static const char    *SENSOR_ID   = "imu_01";
static const uint32_t BAUD        = 115200;
static const uint32_t INTERVAL_MS = 100; // 10 Hz

MPU6050 mpu(Wire);
uint32_t seq    = 0;
uint32_t lastMs = 0;

void setup() {
  Serial.begin(BAUD);
  delay(1000);
  Wire.begin();

  byte status = mpu.begin();
  if (status != 0) {
    Serial.print("{\"msg_type\":\"error\",\"node\":\"");
    Serial.print(NODE_ID);
    Serial.println("\",\"detail\":\"MPU6050 init failed\"}");
    while (1) delay(10);
  }

  mpu.calcOffsets();

  Serial.print("{\"msg_type\":\"boot\",\"node\":\"");
  Serial.print(NODE_ID);
  Serial.print("\",\"fw_version\":\"");
  Serial.print(FW_VERSION);
  Serial.println("\",\"detail\":\"ready\"}");

  lastMs = millis();
}

void loop() {
  uint32_t now = millis();
  if (now - lastMs < INTERVAL_MS) return;
  lastMs = now;

  mpu.update();
  seq++;

  Serial.print("{\"msg_type\":\"data\",\"node\":\"");
  Serial.print(NODE_ID);
  Serial.print("\",\"seq\":");
  Serial.print(seq);
  Serial.print(",\"t_ms\":");
  Serial.print(now);
  Serial.print(",\"sensor_id\":\"");
  Serial.print(SENSOR_ID);
  Serial.print("\"");
  Serial.print(",\"ax_g\":");   Serial.print(mpu.getAccX(),  4);
  Serial.print(",\"ay_g\":");   Serial.print(mpu.getAccY(),  4);
  Serial.print(",\"az_g\":");   Serial.print(mpu.getAccZ(),  4);
  Serial.print(",\"gx_dps\":"); Serial.print(mpu.getGyroX(), 4);
  Serial.print(",\"gy_dps\":"); Serial.print(mpu.getGyroY(), 4);
  Serial.print(",\"gz_dps\":"); Serial.print(mpu.getGyroZ(), 4);
  Serial.print(",\"temp_c\":"); Serial.print(mpu.getTemp(),  2);
  Serial.println("}");
}
