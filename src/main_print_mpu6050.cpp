#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <MPU6050_light.h>
#include "credentials.h"  // gerado em build-time a partir do .env

// =====================================================
// WiFi / TCP
// =====================================================
static const char    *WIFI_SSID     = WIFI_SSID_VAL;
static const char    *WIFI_PASSWORD = WIFI_PASSWORD_VAL;
static const char    *SERVER_HOST   = SERVER_HOST_VAL;
static const uint16_t SERVER_PORT   = 12345;
static const uint32_t WIFI_TIMEOUT_MS        = 15000;
static const uint32_t TCP_RECONNECT_DELAY_MS = 5000;

// =====================================================
// Identificação do nó
// =====================================================
static const char    *NODE_ID    = "N01";
static const char    *SENSOR_ID  = "IMU01";
static const char    *STREAM_ID  = "N01_IMU01";
static const char    *PROTO_VER  = "0.1";

static const uint32_t SAMPLE_INTERVAL_MS = 200; // 5 Hz

// =====================================================
// WiFi / TCP state
// =====================================================
static WiFiClient tcpClient;
static uint32_t   lastTcpRetryMs = 0;
static uint32_t   seqCounter     = 0;

static void connectWiFi() {
  Serial.printf("[wifi] conectando a %s ", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < WIFI_TIMEOUT_MS) {
    delay(500);
    Serial.print('.');
  }
  if (WiFi.status() == WL_CONNECTED)
    Serial.printf("\n[wifi] IP: %s\n", WiFi.localIP().toString().c_str());
  else
    Serial.println("\n[wifi] falha — continuando sem rede");
}

static void connectTCP() {
  if (WiFi.status() != WL_CONNECTED) return;
  Serial.printf("[tcp] conectando a %s:%u ... ", SERVER_HOST, SERVER_PORT);
  Serial.println(tcpClient.connect(SERVER_HOST, SERVER_PORT) ? "ok" : "falha");
}

static void ensureConnected() {
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
    if (WiFi.status() == WL_CONNECTED) connectTCP();
    return;
  }
  if (!tcpClient.connected()) {
    uint32_t now = millis();
    if (now - lastTcpRetryMs >= TCP_RECONNECT_DELAY_MS) {
      lastTcpRetryMs = now;
      connectTCP();
    }
  }
}

// =====================================================
// Buffer de mensagem — Serial + TCP
// =====================================================
static String _msg;

static void tee(const char *s)   { Serial.print(s); _msg += s; }
static void tee(uint32_t v)      { Serial.print(v); _msg += v; }
static void teeF(float v, int p) { Serial.print(v, p); _msg += String(v, p); }

static void flushMsg() {
  Serial.println();
  if (tcpClient.connected()) tcpClient.println(_msg);
  _msg = "";
}

// =====================================================
// Mensagens JSON
// =====================================================
static void sendBoot() {
  tee("{\"msg_type\":\"boot\",\"node\":\"");  tee(NODE_ID);
  tee("\",\"seq\":");                          tee(++seqCounter);
  tee(",\"t_ms\":");                           tee((uint32_t)millis());
  tee(",\"proto_ver\":\"");                    tee(PROTO_VER);
  tee("\",\"transport\":\"wifi_tcp\"}");
  flushMsg();
}

static void sendData(float ax_g, float ay_g, float az_g,
                     float gx_dps, float gy_dps, float gz_dps,
                     float temp_c) {
  tee("{\"msg_type\":\"data\",\"node\":\"");  tee(NODE_ID);
  tee("\",\"seq\":");                          tee(++seqCounter);
  tee(",\"t_ms\":");                           tee((uint32_t)millis());
  tee(",\"proto_ver\":\"");                    tee(PROTO_VER);
  tee("\",\"sensor_id\":\"");                  tee(SENSOR_ID);
  tee("\",\"stream_id\":\"");                  tee(STREAM_ID);
  tee("\",\"ax_g\":");   teeF(ax_g,   6);
  tee(",\"ay_g\":");     teeF(ay_g,   6);
  tee(",\"az_g\":");     teeF(az_g,   6);
  tee(",\"gx_dps\":"); teeF(gx_dps, 6);
  tee(",\"gy_dps\":"); teeF(gy_dps, 6);
  tee(",\"gz_dps\":"); teeF(gz_dps, 6);
  tee(",\"temp_c\":"); teeF(temp_c,  2);
  tee("}");
  flushMsg();
}

// =====================================================
// Setup / Loop
// =====================================================
MPU6050 mpu(Wire);

void setup() {
  Serial.begin(115200);
  delay(1000);
  Wire.begin();

  connectWiFi();
  connectTCP();

  byte status = mpu.begin();
  Serial.printf("MPU6050 status: %d\n", status);
  if (status != 0) {
    Serial.println("MPU6050 init failed!");
    while (1) delay(10);
  }

  Serial.println("Calibrando...");
  mpu.calcOffsets();
  Serial.println("Pronto.");

  sendBoot();
}

void loop() {
  static uint32_t lastMs = 0;
  uint32_t now = millis();

  ensureConnected();

  if (now - lastMs >= SAMPLE_INTERVAL_MS) {
    lastMs = now;
    mpu.update();
    sendData(
      mpu.getAccX(),  mpu.getAccY(),  mpu.getAccZ(),
      mpu.getGyroX(), mpu.getGyroY(), mpu.getGyroZ(),
      mpu.getTemp()
    );
  }
}
