#include <Arduino.h>
#include <Wire.h>
#include <MPU6050_light.h>

MPU6050 mpu(Wire);

void setup() {
  Serial.begin(115200);
  delay(1000);
  Wire.begin();

  byte status = mpu.begin();
  Serial.printf("MPU6050 status: %d\n", status);
  if (status != 0) {
    Serial.println("MPU6050 init failed!");
    while (1) delay(10);
  }

  Serial.println("MPU6050 connected! Calibrating...");
  mpu.calcOffsets();
  Serial.println("Done.");
}

void loop() {
  mpu.update();

  Serial.printf("Accel X: %.2f  Y: %.2f  Z: %.2f m/s²\n",
    mpu.getAccX(), mpu.getAccY(), mpu.getAccZ());
  Serial.printf("Gyro  X: %.2f  Y: %.2f  Z: %.2f deg/s\n",
    mpu.getGyroX(), mpu.getGyroY(), mpu.getGyroZ());
  Serial.printf("Angle X: %.1f  Y: %.1f  Z: %.1f°\n\n",
    mpu.getAngleX(), mpu.getAngleY(), mpu.getAngleZ());

  delay(500);
}