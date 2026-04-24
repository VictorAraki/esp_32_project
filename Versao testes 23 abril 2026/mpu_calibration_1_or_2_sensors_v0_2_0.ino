#include <Wire.h>

// =====================================================
// Calibração diagnóstica de 1 ou 2 MPU6050
// Versão: 0.2.0
//
// Objetivo:
// - aceitar 1 ou 2 sensores por configuração
// - calcular médias em repouso
// - sugerir offsets do giroscópio
// - mostrar diagnóstico do acelerômetro
//
// Uso:
// - habilite/desabilite os sensores no topo
// - deixe os sensores totalmente parados
// - idealmente: ax~0, ay~0, az~+1g, gx~0, gy~0, gz~0
// =====================================================

// ================================
// Configuração
// ================================
static const uint8_t SDA_PIN = 21;
static const uint8_t SCL_PIN = 22;
static const uint32_t BAUD = 115200;

static const uint8_t MPU1_ADDR = 0x68; // AD0 no GND
static const uint8_t MPU2_ADDR = 0x69; // AD0 no 3.3V

static const bool ENABLE_SENSOR_1 = true;
static const bool ENABLE_SENSOR_2 = true;

static const uint8_t REG_PWR_MGMT_1   = 0x6B;
static const uint8_t REG_WHO_AM_I     = 0x75;
static const uint8_t REG_ACCEL_XOUT_H = 0x3B;
static const uint8_t REG_GYRO_CONFIG  = 0x1B;
static const uint8_t REG_ACCEL_CONFIG = 0x1C;

static const int NUM_SAMPLES = 1000;
static const int SAMPLE_DELAY_MS = 5;

// ================================
// Estruturas
// ================================
struct SensorCalibResult {
  bool enabled = false;
  bool ok = false;

  long ax_sum = 0;
  long ay_sum = 0;
  long az_sum = 0;
  long temp_sum = 0;
  long gx_sum = 0;
  long gy_sum = 0;
  long gz_sum = 0;

  float ax_avg_raw = 0;
  float ay_avg_raw = 0;
  float az_avg_raw = 0;
  float temp_avg_raw = 0;
  float gx_avg_raw = 0;
  float gy_avg_raw = 0;
  float gz_avg_raw = 0;

  float ax_avg_g = 0;
  float ay_avg_g = 0;
  float az_avg_g = 0;
  float temp_avg_c = 0;
  float gx_avg_dps = 0;
  float gy_avg_dps = 0;
  float gz_avg_dps = 0;
};

// ================================
// I2C helpers
// ================================
bool writeRegister8(uint8_t addr, uint8_t reg, uint8_t value) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write(value);
  return (Wire.endTransmission(true) == 0);
}

bool readRegister8(uint8_t addr, uint8_t reg, uint8_t &value) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;

  int n = Wire.requestFrom((int)addr, 1, (int)true);
  if (n != 1) return false;

  value = Wire.read();
  return true;
}

bool i2cResponds(uint8_t addr) {
  Wire.beginTransmission(addr);
  return (Wire.endTransmission(true) == 0);
}

bool checkWhoAmI(uint8_t addr) {
  uint8_t who = 0;
  if (!readRegister8(addr, REG_WHO_AM_I, who)) return false;
  return (who == 0x68);
}

bool initMPU6050(uint8_t addr) {
  if (!writeRegister8(addr, REG_PWR_MGMT_1, 0x00)) return false;
  delay(50);

  if (!writeRegister8(addr, REG_GYRO_CONFIG, 0x00)) return false;  // ±250 dps
  if (!writeRegister8(addr, REG_ACCEL_CONFIG, 0x00)) return false; // ±2g

  if (!checkWhoAmI(addr)) return false;
  return true;
}

bool readMPU14(uint8_t addr,
               int16_t &ax, int16_t &ay, int16_t &az,
               int16_t &tempRaw,
               int16_t &gx, int16_t &gy, int16_t &gz) {
  Wire.beginTransmission(addr);
  Wire.write(REG_ACCEL_XOUT_H);
  if (Wire.endTransmission(false) != 0) return false;

  int n = Wire.requestFrom((int)addr, 14, (int)true);
  if (n != 14) return false;

  ax = (Wire.read() << 8) | Wire.read();
  ay = (Wire.read() << 8) | Wire.read();
  az = (Wire.read() << 8) | Wire.read();
  tempRaw = (Wire.read() << 8) | Wire.read();
  gx = (Wire.read() << 8) | Wire.read();
  gy = (Wire.read() << 8) | Wire.read();
  gz = (Wire.read() << 8) | Wire.read();

  return true;
}

// ================================
// Calibração diagnóstica
// ================================
bool collectAverages(uint8_t addr, SensorCalibResult &res) {
  if (!i2cResponds(addr)) return false;
  if (!checkWhoAmI(addr)) return false;
  if (!initMPU6050(addr)) return false;

  delay(500);

  for (int i = 0; i < NUM_SAMPLES; i++) {
    int16_t ax, ay, az, tempRaw, gx, gy, gz;
    if (!readMPU14(addr, ax, ay, az, tempRaw, gx, gy, gz)) {
      return false;
    }

    res.ax_sum += ax;
    res.ay_sum += ay;
    res.az_sum += az;
    res.temp_sum += tempRaw;
    res.gx_sum += gx;
    res.gy_sum += gy;
    res.gz_sum += gz;

    delay(SAMPLE_DELAY_MS);
  }

  res.ax_avg_raw = (float)res.ax_sum / NUM_SAMPLES;
  res.ay_avg_raw = (float)res.ay_sum / NUM_SAMPLES;
  res.az_avg_raw = (float)res.az_sum / NUM_SAMPLES;
  res.temp_avg_raw = (float)res.temp_sum / NUM_SAMPLES;
  res.gx_avg_raw = (float)res.gx_sum / NUM_SAMPLES;
  res.gy_avg_raw = (float)res.gy_sum / NUM_SAMPLES;
  res.gz_avg_raw = (float)res.gz_sum / NUM_SAMPLES;

  res.ax_avg_g = res.ax_avg_raw / 16384.0f;
  res.ay_avg_g = res.ay_avg_raw / 16384.0f;
  res.az_avg_g = res.az_avg_raw / 16384.0f;

  res.temp_avg_c = (res.temp_avg_raw / 340.0f) + 36.53f;

  res.gx_avg_dps = res.gx_avg_raw / 131.0f;
  res.gy_avg_dps = res.gy_avg_raw / 131.0f;
  res.gz_avg_dps = res.gz_avg_raw / 131.0f;

  res.ok = true;
  return true;
}

void printResult(const char *label, uint8_t addr, const SensorCalibResult &r) {
  Serial.println();
  Serial.println("========================================");
  Serial.print("Sensor: ");
  Serial.println(label);
  Serial.print("Endereco: 0x");
  if (addr < 16) Serial.print("0");
  Serial.println(addr, HEX);

  if (!r.enabled) {
    Serial.println("Status: DESABILITADO POR CONFIGURACAO");
    Serial.println("========================================");
    return;
  }

  if (!r.ok) {
    Serial.println("Status: FALHA");
    Serial.println("========================================");
    return;
  }

  Serial.println("Status: OK");

  Serial.println("--- MEDIAS RAW ---");
  Serial.print("ax_raw = "); Serial.println(r.ax_avg_raw, 3);
  Serial.print("ay_raw = "); Serial.println(r.ay_avg_raw, 3);
  Serial.print("az_raw = "); Serial.println(r.az_avg_raw, 3);
  Serial.print("temp_raw = "); Serial.println(r.temp_avg_raw, 3);
  Serial.print("gx_raw = "); Serial.println(r.gx_avg_raw, 3);
  Serial.print("gy_raw = "); Serial.println(r.gy_avg_raw, 3);
  Serial.print("gz_raw = "); Serial.println(r.gz_avg_raw, 3);

  Serial.println("--- MEDIAS CONVERTIDAS ---");
  Serial.print("ax_g = "); Serial.println(r.ax_avg_g, 6);
  Serial.print("ay_g = "); Serial.println(r.ay_avg_g, 6);
  Serial.print("az_g = "); Serial.println(r.az_avg_g, 6);
  Serial.print("temp_c = "); Serial.println(r.temp_avg_c, 3);
  Serial.print("gx_dps = "); Serial.println(r.gx_avg_dps, 6);
  Serial.print("gy_dps = "); Serial.println(r.gy_avg_dps, 6);
  Serial.print("gz_dps = "); Serial.println(r.gz_avg_dps, 6);

  Serial.println("--- OFFSETS SUGERIDOS ---");
  Serial.println("Gyro (subtrair diretamente da leitura convertida):");
  Serial.print("gx_offset_dps = "); Serial.println(r.gx_avg_dps, 6);
  Serial.print("gy_offset_dps = "); Serial.println(r.gy_avg_dps, 6);
  Serial.print("gz_offset_dps = "); Serial.println(r.gz_avg_dps, 6);

  Serial.println("Accel (diagnostico; nao aplicar automaticamente ainda):");
  Serial.print("ax_bias_g = "); Serial.println(r.ax_avg_g, 6);
  Serial.print("ay_bias_g = "); Serial.println(r.ay_avg_g, 6);
  Serial.print("az_bias_from_plus1g = "); Serial.println(r.az_avg_g - 1.0f, 6);

  Serial.println("========================================");
}

// ================================
// Setup / Loop
// ================================
void setup() {
  Serial.begin(BAUD);
  delay(1500);

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(100000);

  Serial.println();
  Serial.println("=== CALIBRACAO DIAGNOSTICA - 1 ou 2x MPU6050 ===");
  Serial.println("Deixe os sensores totalmente parados.");
  Serial.println("Idealmente: ax~0, ay~0, az~+1g, gx~0, gy~0, gz~0");
  Serial.print("Numero de amostras: ");
  Serial.println(NUM_SAMPLES);
  Serial.println("Iniciando em 3 segundos...");
  delay(3000);

  SensorCalibResult r1, r2;
  r1.enabled = ENABLE_SENSOR_1;
  r2.enabled = ENABLE_SENSOR_2;

  if (ENABLE_SENSOR_1) {
    collectAverages(MPU1_ADDR, r1);
  }
  if (ENABLE_SENSOR_2) {
    collectAverages(MPU2_ADDR, r2);
  }

  printResult("imu_upper_arm", MPU1_ADDR, r1);
  printResult("imu_forearm", MPU2_ADDR, r2);

  Serial.println();
  Serial.println("=== FIM ===");
  Serial.println("Copie os resultados e me envie.");
}

void loop() {
  // Nada a fazer
}
