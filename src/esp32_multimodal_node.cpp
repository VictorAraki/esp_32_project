#include <Arduino.h>
#include <Wire.h>

// =====================================================
// Nó ESP32 + 1 ou 2 MPU6050 — padrão técnico de identificação
// Versão: 0.6.0
//
// O que esta versão adiciona:
// - suporte a 1 ou 2 sensores por configuração
// - sensor pode ser habilitado/desabilitado no topo do arquivo
// - nó não entra em "degraded" por sensor desabilitado intencionalmente
// - heartbeat reflete apenas sensores habilitados
// - health check, recovery, warmup, LED de estado e offset de gyro por sensor
// - identificação técnica desacoplada da posição anatômica: N01, IMU01, IMU02, stream_id
//
// Regras de uso:
// - para usar 1 sensor: deixe ENABLE_SENSOR_1 = true e ENABLE_SENSOR_2 = false
//   ou o contrário.
// - para usar 2 sensores: deixe ambos true.
// - se um sensor estiver habilitado, mas fisicamente ausente, o nó ficará degradado.
// =====================================================

// =====================================================
// Configuração geral do nó
// =====================================================
static const char *NODE_ID = "N01";
static const char *PROTO_VER = "0.1";
static const char *FW_NAME = "ictaltech_sensor_node";
static const char *FW_VERSION = "0.6.0";

static const uint8_t I2C_SDA_PIN = 21;
static const uint8_t I2C_SCL_PIN = 22;
static const uint32_t SERIAL_BAUD = 115200;

static const float SAMPLE_RATE_HZ = 5.0f;
static const uint32_t SAMPLE_INTERVAL_MS = 200;      // 5 Hz por nó
static const uint32_t HEARTBEAT_INTERVAL_MS = 20000; // 20 s

// Warmup / recovery
static const uint32_t RECOVERY_DELAY_MS = 200;
static const uint8_t WARMUP_SAMPLES_TO_DISCARD = 5;

// =====================================================
// Habilitação de sensores
// =====================================================
// Deixe true/false conforme o hardware realmente conectado.
static const bool ENABLE_SENSOR_1 = false;  // IMU01 - endereco 0x68
static const bool ENABLE_SENSOR_2 = true;   // IMU02 - endereco 0x69

// =====================================================
// LED onboard
// =====================================================
#ifndef LED_BUILTIN
#define LED_BUILTIN 2
#endif
static const uint8_t LED_PIN = LED_BUILTIN;

// =====================================================
// MPU6050 registradores
// =====================================================
static const uint8_t REG_PWR_MGMT_1   = 0x6B;
static const uint8_t REG_WHO_AM_I     = 0x75;
static const uint8_t REG_ACCEL_XOUT_H = 0x3B;
static const uint8_t REG_GYRO_CONFIG  = 0x1B;
static const uint8_t REG_ACCEL_CONFIG = 0x1C;
static const uint8_t REG_SMPLRT_DIV   = 0x19;
static const uint8_t REG_CONFIG       = 0x1A;

// =====================================================
// Endereços I2C
// =====================================================
static const uint8_t MPU_ADDR_1 = 0x68; // AD0 no GND
static const uint8_t MPU_ADDR_2 = 0x69; // AD0 no 3.3V

// =====================================================
// Estado por sensor
// =====================================================
struct SensorState {
  // Identidade técnica
  // Importante: não usar significado anatômico aqui.
  // Ex.: posição no corpo deve ser mapeada no backend:
  // N01_IMU01 -> upper_arm/right, N01_IMU02 -> forearm/right etc.
  const char *sensor_model;  // ex.: "mpu6050"
  const char *sensor_type;   // ex.: "IMU"
  const char *sensor_id;     // ex.: "IMU01"
  uint8_t addr;              // endereço físico I2C, não usar como ID principal
  bool enabled;

  // Saúde / presença
  bool detected;
  bool healthy;
  bool wasHealthy;

  // Recovery / warmup
  bool inWarmup;
  uint8_t warmupDiscardRemaining;
  uint32_t recoveryStartMs;

  // Contadores
  uint32_t consecutiveFailures;
  uint32_t totalFailures;
  uint32_t totalReadsOk;

  // Offsets de giroscópio, já calibrados
  float gxOffsetDps;
  float gyOffsetDps;
  float gzOffsetDps;
};

// Offsets de gyro obtidos na sua calibração diagnóstica
SensorState sensor1 = {
  "mpu6050",
  "IMU",
  "IMU01",
  MPU_ADDR_1,
  ENABLE_SENSOR_1,
  false, false, false,
  false, 0, 0,
  0, 0, 0,
  -1.721931f,  1.020237f, -2.985832f
};

SensorState sensor2 = {
  "mpu6050",
  "IMU",
  "IMU02",
  MPU_ADDR_2,
  ENABLE_SENSOR_2,
  false, false, false,
  false, 0, 0,
  0, 0, 0,
  -0.613496f,  0.714382f,  0.122443f
};

// =====================================================
// Estado global
// =====================================================
uint32_t seqCounter = 0;
uint32_t lastSampleMs = 0;
uint32_t lastHeartbeatMs = 0;

// =====================================================
// Utilidades JSON
// =====================================================
uint32_t nextSeq() {
  seqCounter++;
  return seqCounter;
}

void printBasePrefix(const char *msgType) {
  Serial.print("{\"msg_type\":\"");
  Serial.print(msgType);
  Serial.print("\",\"node\":\"");
  Serial.print(NODE_ID);
  Serial.print("\",\"seq\":");
  Serial.print(nextSeq());
  Serial.print(",\"t_ms\":");
  Serial.print(millis());
  Serial.print(",\"proto_ver\":\"");
  Serial.print(PROTO_VER);
  Serial.print("\"");
}

void printStreamId(const SensorState &s) {
  Serial.print(NODE_ID);
  Serial.print("_");
  Serial.print(s.sensor_id);
}

void printSensorCommon(const SensorState &s) {
  Serial.print(",\"sensor_model\":\"");
  Serial.print(s.sensor_model);
  Serial.print("\",\"sensor_type\":\"");
  Serial.print(s.sensor_type);
  Serial.print("\",\"sensor_id\":\"");
  Serial.print(s.sensor_id);
  Serial.print("\",\"stream_id\":\"");
  printStreamId(s);
  Serial.print("\",\"hw_address\":\"0x");
  if (s.addr < 16) Serial.print("0");
  Serial.print(s.addr, HEX);
  Serial.print("\"");
}

void sendBoot() {
  printBasePrefix("boot");
  Serial.print(",\"fw_name\":\"");
  Serial.print(FW_NAME);
  Serial.print("\",\"fw_version\":\"");
  Serial.print(FW_VERSION);
  Serial.print(",\"board\":\"ESP32_WROOM\"");
  Serial.print(",\"chip\":\"ESP32\"");
  Serial.print(",\"transport\":\"serial_usb\"}");
  Serial.println();
}

void printSensorEnabledField(const SensorState &s) {
  Serial.print(",\"");
  Serial.print(s.sensor_id);
  Serial.print("_enabled\":");
  Serial.print(s.enabled ? "true" : "false");
}

void printGyroOffsets(const SensorState &s) {
  Serial.print(",\"");
  printStreamId(s);
  Serial.print("_gyro_offsets_dps\":{\"gx\":");
  Serial.print(s.gxOffsetDps, 6);
  Serial.print(",\"gy\":");
  Serial.print(s.gyOffsetDps, 6);
  Serial.print(",\"gz\":");
  Serial.print(s.gzOffsetDps, 6);
  Serial.print("}");
}

void sendConfig() {
  uint8_t sensorCount = 0;
  if (sensor1.enabled) sensorCount++;
  if (sensor2.enabled) sensorCount++;

  printBasePrefix("config");
  Serial.print(",\"sample_rate_hz\":");
  Serial.print(SAMPLE_RATE_HZ, 3);
  Serial.print(",\"i2c_sda\":");
  Serial.print(I2C_SDA_PIN);
  Serial.print(",\"i2c_scl\":");
  Serial.print(I2C_SCL_PIN);
  Serial.print(",\"serial_baud\":");
  Serial.print(SERIAL_BAUD);
  Serial.print(",\"send_format\":\"ndjson\"");
  Serial.print(",\"heartbeat_interval_s\":");
  Serial.print(HEARTBEAT_INTERVAL_MS / 1000.0f, 1);
  Serial.print(",\"node_id_standard\":\"ictaltech_sensor_identification_v1\"");
  Serial.print(",\"sensor_count\":");
  Serial.print(sensorCount);
  Serial.print(",\"recovery_delay_ms\":");
  Serial.print(RECOVERY_DELAY_MS);
  Serial.print(",\"warmup_samples_to_discard\":");
  Serial.print(WARMUP_SAMPLES_TO_DISCARD);

  printSensorEnabledField(sensor1);
  printSensorEnabledField(sensor2);

  if (sensor1.enabled) {
    printGyroOffsets(sensor1);
  }

  if (sensor2.enabled) {
    printGyroOffsets(sensor2);
  }

  Serial.print("}");
  Serial.println();
}

void sendSensorStatus(const SensorState &s, bool detected, const char *status, const char *detail) {
  printBasePrefix("sensor_status");
  printSensorCommon(s);
  Serial.print(",\"bus\":\"i2c\"");
  Serial.print(",\"enabled\":");
  Serial.print(s.enabled ? "true" : "false");
  Serial.print(",\"detected\":");
  Serial.print(detected ? "true" : "false");
  Serial.print(",\"status\":\"");
  Serial.print(status);
  Serial.print("\",\"detail\":\"");
  Serial.print(detail);
  Serial.print("\"}");
  Serial.println();
}

void sendError(const SensorState &s, const char *code, const char *detail) {
  printBasePrefix("error");
  printSensorCommon(s);
  Serial.print(",\"code\":\"");
  Serial.print(code);
  Serial.print("\",\"detail\":\"");
  Serial.print(detail);
  Serial.print("\",\"consecutive_failures\":");
  Serial.print(s.consecutiveFailures);
  Serial.print(",\"total_failures\":");
  Serial.print(s.totalFailures);
  Serial.print("}");
  Serial.println();
}

void sendHeartbeat() {
  uint8_t enabledCount = 0;
  uint8_t healthyEnabledCount = 0;
  bool anyWarmup = false;

  SensorState *sensors[2] = {&sensor1, &sensor2};
  for (uint8_t i = 0; i < 2; i++) {
    SensorState *s = sensors[i];
    if (!s->enabled) continue;

    enabledCount++;
    if (s->healthy) healthyEnabledCount++;
    if (s->inWarmup) anyWarmup = true;
  }

  bool globalOk = (enabledCount > 0) && (healthyEnabledCount == enabledCount) && !anyWarmup;

  printBasePrefix("heartbeat");
  Serial.print(",\"uptime_s\":");
  Serial.print(millis() / 1000UL);
  Serial.print(",\"status\":\"");
  Serial.print(globalOk ? "ok" : "degraded");
  Serial.print("\"");
  Serial.print(",\"enabled_sensor_count\":");
  Serial.print(enabledCount);

  // sensor 1
  Serial.print(",\"sensor1_enabled\":");
  Serial.print(sensor1.enabled ? "true" : "false");
  Serial.print(",\"sensor1_ok\":");
  Serial.print(sensor1.healthy ? "true" : "false");
  Serial.print(",\"sensor1_warmup\":");
  Serial.print(sensor1.inWarmup ? "true" : "false");
  Serial.print(",\"sensor1_total_failures\":");
  Serial.print(sensor1.totalFailures);
  Serial.print(",\"sensor1_total_reads_ok\":");
  Serial.print(sensor1.totalReadsOk);

  // sensor 2
  Serial.print(",\"sensor2_enabled\":");
  Serial.print(sensor2.enabled ? "true" : "false");
  Serial.print(",\"sensor2_ok\":");
  Serial.print(sensor2.healthy ? "true" : "false");
  Serial.print(",\"sensor2_warmup\":");
  Serial.print(sensor2.inWarmup ? "true" : "false");
  Serial.print(",\"sensor2_total_failures\":");
  Serial.print(sensor2.totalFailures);
  Serial.print(",\"sensor2_total_reads_ok\":");
  Serial.print(sensor2.totalReadsOk);

  Serial.print("}");
  Serial.println();
}

void sendData(const SensorState &s,
              float ax_g, float ay_g, float az_g,
              float gx_dps, float gy_dps, float gz_dps,
              float temp_c) {
  printBasePrefix("data");
  printSensorCommon(s);

  Serial.print(",\"ax_g\":");
  Serial.print(ax_g, 6);
  Serial.print(",\"ay_g\":");
  Serial.print(ay_g, 6);
  Serial.print(",\"az_g\":");
  Serial.print(az_g, 6);

  Serial.print(",\"gx_dps\":");
  Serial.print(gx_dps, 6);
  Serial.print(",\"gy_dps\":");
  Serial.print(gy_dps, 6);
  Serial.print(",\"gz_dps\":");
  Serial.print(gz_dps, 6);

  Serial.print(",\"temp_c\":");
  Serial.print(temp_c, 2);
  Serial.print("}");
  Serial.println();
}

// =====================================================
// I2C helpers
// =====================================================
bool i2cDeviceResponds(uint8_t addr) {
  Wire.beginTransmission(addr);
  uint8_t err = Wire.endTransmission(true);
  return (err == 0);
}

bool readRegister8(uint8_t addr, uint8_t reg, uint8_t &value) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  uint8_t err = Wire.endTransmission(false);
  if (err != 0) return false;

  int n = Wire.requestFrom((int)addr, 1, (int)true);
  if (n != 1) return false;

  value = Wire.read();
  return true;
}

bool writeRegister8(uint8_t addr, uint8_t reg, uint8_t value) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write(value);
  uint8_t err = Wire.endTransmission(true);
  return (err == 0);
}

bool checkWhoAmI(uint8_t addr) {
  uint8_t who = 0x00;
  if (!readRegister8(addr, REG_WHO_AM_I, who)) return false;
  return (who == 0x68);
}

bool initMPU6050(uint8_t addr) {
  // Full device reset — clears any stuck state from prior session.
  if (!writeRegister8(addr, REG_PWR_MGMT_1, 0x80)) return false;
  delay(100);

  // PLL with X-gyro reference: stabler clock than internal 8 MHz oscillator,
  // which on GY-521 clones causes intermittent all-zero frames.
  if (!writeRegister8(addr, REG_PWR_MGMT_1, 0x01)) return false;
  delay(50);

  if (!writeRegister8(addr, REG_SMPLRT_DIV, 0x00)) return false;   // gyro rate / 1
  if (!writeRegister8(addr, REG_CONFIG, 0x00)) return false;       // DLPF off
  if (!writeRegister8(addr, REG_GYRO_CONFIG, 0x00)) return false;  // ±250 dps
  if (!writeRegister8(addr, REG_ACCEL_CONFIG, 0x00)) return false; // ±2g

  if (!checkWhoAmI(addr)) return false;
  return true;
}

bool readMPURaw14(uint8_t addr,
                  int16_t &ax, int16_t &ay, int16_t &az,
                  int16_t &tempRaw,
                  int16_t &gx, int16_t &gy, int16_t &gz) {
  Wire.beginTransmission(addr);
  Wire.write(REG_ACCEL_XOUT_H);
  uint8_t err = Wire.endTransmission(false);
  if (err != 0) return false;

  int bytesRead = Wire.requestFrom((int)addr, 14, (int)true);
  if (bytesRead != 14) return false;

  ax = (Wire.read() << 8) | Wire.read();
  ay = (Wire.read() << 8) | Wire.read();
  az = (Wire.read() << 8) | Wire.read();
  tempRaw = (Wire.read() << 8) | Wire.read();
  gx = (Wire.read() << 8) | Wire.read();
  gy = (Wire.read() << 8) | Wire.read();
  gz = (Wire.read() << 8) | Wire.read();
  return true;
}

// =====================================================
// Warmup / recovery
// =====================================================
bool looksLikeAllZeroFrame(int16_t ax, int16_t ay, int16_t az,
                           int16_t tempRaw,
                           int16_t gx, int16_t gy, int16_t gz) {
  return (ax == 0 && ay == 0 && az == 0 &&
          tempRaw == 0 &&
          gx == 0 && gy == 0 && gz == 0);
}

void beginWarmup(SensorState &s) {
  s.inWarmup = true;
  s.warmupDiscardRemaining = WARMUP_SAMPLES_TO_DISCARD;
  s.recoveryStartMs = millis();
  s.healthy = false;
}

// =====================================================
// I2C Bus reset (unsticks hung bus after failed reads)
// =====================================================
void resetI2CBus() {
  // Release the bus by setting pins to input with pull-ups
  pinMode(I2C_SDA_PIN, INPUT_PULLUP);
  pinMode(I2C_SCL_PIN, INPUT_PULLUP);
  delay(10);

  // Reinitialize Wire
  Wire.end();
  delay(50);
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(100000); // 100 kHz for robustness
}

bool tryRecoverSensor(SensorState &s) {
  // Don't tear down the shared I2C bus: the sensor still ACKs, only its
  // internal state is bad. initMPU6050 issues a device reset, which is
  // enough. resetI2CBus() is kept for explicit bus-fault handling only.
  if (!i2cDeviceResponds(s.addr)) return false;
  if (!initMPU6050(s.addr)) return false;

  beginWarmup(s);
  sendSensorStatus(s, true, "recovered", "sensor_recovered_reinitialized");
  return true;
}

// =====================================================
// LED de estado global
// =====================================================
static const uint8_t LED_STATE_BOOTING    = 0;
static const uint8_t LED_STATE_OK         = 1;
static const uint8_t LED_STATE_DEGRADED   = 2;
static const uint8_t LED_STATE_WARMUP     = 3;
static const uint8_t LED_STATE_INIT_ERROR = 4;

uint8_t getLedState() {
  uint8_t enabledCount = 0;
  bool anyWarmup = false;
  bool allHealthy = true;
  bool anyDetectedEnabled = false;

  SensorState *sensors[2] = {&sensor1, &sensor2};
  for (uint8_t i = 0; i < 2; i++) {
    SensorState *s = sensors[i];
    if (!s->enabled) continue;

    enabledCount++;
    if (s->inWarmup) anyWarmup = true;
    if (!s->healthy) allHealthy = false;
    if (s->detected) anyDetectedEnabled = true;
  }

  if (enabledCount == 0) return LED_STATE_INIT_ERROR;
  if (!anyDetectedEnabled && !allHealthy) return LED_STATE_INIT_ERROR;
  if (anyWarmup) return LED_STATE_WARMUP;
  if (allHealthy) return LED_STATE_OK;
  return LED_STATE_DEGRADED;
}

void updateLed() {
  static uint32_t lastBlinkMs = 0;
  static bool ledOn = false;
  uint32_t now = millis();
  uint8_t st = getLedState();

  switch (st) {
    case LED_STATE_BOOTING:
      if (now - lastBlinkMs >= 150) {
        lastBlinkMs = now;
        ledOn = !ledOn;
        digitalWrite(LED_PIN, ledOn ? HIGH : LOW);
      }
      break;

    case LED_STATE_OK:
      digitalWrite(LED_PIN, HIGH);
      break;

    case LED_STATE_DEGRADED:
      if (now - lastBlinkMs >= 500) {
        lastBlinkMs = now;
        ledOn = !ledOn;
        digitalWrite(LED_PIN, ledOn ? HIGH : LOW);
      }
      break;

    case LED_STATE_WARMUP:
      if (now - lastBlinkMs >= 200) {
        lastBlinkMs = now;
        ledOn = !ledOn;
        digitalWrite(LED_PIN, ledOn ? HIGH : LOW);
      }
      break;

    case LED_STATE_INIT_ERROR:
      if (now - lastBlinkMs >= 100) {
        lastBlinkMs = now;
        ledOn = !ledOn;
        digitalWrite(LED_PIN, ledOn ? HIGH : LOW);
      }
      break;
  }
}

// =====================================================
// Inicialização e processamento por sensor
// =====================================================
void initializeSensorAtBoot(SensorState &s) {
  if (!s.enabled) {
    sendSensorStatus(s, false, "disabled", "sensor_disabled_by_configuration");
    return;
  }

  s.detected = i2cDeviceResponds(s.addr);

  if (!s.detected) {
    s.healthy = false;
    s.wasHealthy = false;
    sendSensorStatus(s, false, "init_error", "sensor_not_detected_at_boot");
    return;
  }

  if (!checkWhoAmI(s.addr)) {
    s.healthy = false;
    s.wasHealthy = false;
    sendSensorStatus(s, false, "init_error", "who_am_i_mismatch");
    return;
  }

  if (!initMPU6050(s.addr)) {
    s.healthy = false;
    s.wasHealthy = false;
    sendSensorStatus(s, false, "init_error", "sensor_init_failed");
    return;
  }

  s.detected = true;
  s.healthy = true;
  s.wasHealthy = true;
  s.inWarmup = false;
  s.warmupDiscardRemaining = 0;

  sendSensorStatus(s, true, "ok", "sensor_detected_and_initialized");
}

void processSensor(SensorState &s) {
  if (!s.enabled) return;

  int16_t ax, ay, az, tempRaw, gx, gy, gz;
  bool readOk = readMPURaw14(s.addr, ax, ay, az, tempRaw, gx, gy, gz);

  if (!readOk) {
    s.consecutiveFailures++;
    s.totalFailures++;

    bool currentlyResponding = i2cDeviceResponds(s.addr);
    s.detected = currentlyResponding;

    if (s.wasHealthy) {
      s.healthy = false;
      s.wasHealthy = false;
      s.inWarmup = false;
      s.warmupDiscardRemaining = 0;
      sendSensorStatus(s, false, "lost", "sensor_lost_during_runtime");
    }

    if (currentlyResponding) {
      sendError(s, "i2c_read_failed", "expected_14_bytes_or_bus_error");
    } else {
      sendError(s, "i2c_read_failed", "sensor_still_not_responding");
    }
    return;
  }

  // Leitura bruta respondeu
  s.detected = true;

  // Se não estava saudável e não está em warmup, tentar recovery com reinit
  if (!s.healthy && !s.inWarmup) {
    if (!tryRecoverSensor(s)) {
      s.consecutiveFailures++;
      s.totalFailures++;
      sendError(s, "recovery_failed", "sensor_detected_but_reinit_failed");
      return;
    }
    return; // próxima rodada fará o warmup
  }

  // Warmup após recovery
  if (s.inWarmup) {
    if ((millis() - s.recoveryStartMs) < RECOVERY_DELAY_MS) {
      return;
    }

    if (looksLikeAllZeroFrame(ax, ay, az, tempRaw, gx, gy, gz)) {
      s.consecutiveFailures++;
      s.totalFailures++;
      sendError(s, "warmup_invalid_frame", "all_zero_frame_during_warmup");
      return;
    }

    if (s.warmupDiscardRemaining > 0) {
      s.warmupDiscardRemaining--;
      if (s.warmupDiscardRemaining == 0) {
        s.inWarmup = false;
        s.healthy = true;
        s.wasHealthy = true;
        s.consecutiveFailures = 0;
        sendSensorStatus(s, true, "ok", "warmup_completed_sensor_ready");
      }
      return;
    }
  }

  // Frame suspeito em operação normal
  if (looksLikeAllZeroFrame(ax, ay, az, tempRaw, gx, gy, gz)) {
    s.consecutiveFailures++;
    s.totalFailures++;
    s.healthy = false;
    s.wasHealthy = false;
    sendError(s, "invalid_frame", "all_zero_frame_detected");
    return;
  }

  // Conversão de unidades
  float ax_g = ax / 16384.0f;
  float ay_g = ay / 16384.0f;
  float az_g = az / 16384.0f;
  float temp_c = (tempRaw / 340.0f) + 36.53f;

  // Aplicação de offset de gyro calibrado por sensor
  float gx_dps = (gx / 131.0f) - s.gxOffsetDps;
  float gy_dps = (gy / 131.0f) - s.gyOffsetDps;
  float gz_dps = (gz / 131.0f) - s.gzOffsetDps;

  s.healthy = true;
  s.wasHealthy = true;
  s.consecutiveFailures = 0;
  s.totalReadsOk++;

  sendData(s, ax_g, ay_g, az_g, gx_dps, gy_dps, gz_dps, temp_c);
}

// =====================================================
// Setup / Loop
// =====================================================
void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Serial.begin(SERIAL_BAUD);
  delay(1000);

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(100000); // 100 kHz para robustez

  sendBoot();
  sendConfig();

  initializeSensorAtBoot(sensor1);
  initializeSensorAtBoot(sensor2);

  lastSampleMs = millis();
  lastHeartbeatMs = millis();
}

void loop() {
  uint32_t now = millis();

  updateLed();

  if ((now - lastSampleMs) >= SAMPLE_INTERVAL_MS) {
    lastSampleMs = now;

    processSensor(sensor1);
    processSensor(sensor2);
  }

  if ((now - lastHeartbeatMs) >= HEARTBEAT_INTERVAL_MS) {
    lastHeartbeatMs = now;
    sendHeartbeat();
  }
}
