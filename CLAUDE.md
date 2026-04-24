# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a TCC (undergraduate thesis) project: **3D Human Pose Reconstruction via IMU Sensors** applied to epilepsy monitoring. The system uses ESP32 microcontrollers with MPU6050 IMU sensors to capture motion data, relayed via WiFi or USB to a Python server, with eventual visualization in Unreal Engine 5.

Documentation is primarily in **Portuguese**.

## Architecture

Three-tier pipeline:

```
ESP32 + MPU6050 (C++/Arduino)
        |
   WiFi (TCP :12345) or USB (COM3, 115200 baud)
        |
   server.py (Python)
        |
   [Unreal Engine 5 avatar — not yet integrated]
```

**Data format** (CSV): `ID,Roll,Pitch,Yaw,Timestamp`

### Firmware (`esp_32_project/`)

PlatformIO project targeting `esp32doit-devkit-v1`. The active development version lives in `Versao testes 23 abril 2026/` and supports 1 or 2 MPU6050 sensors on I2C addresses 0x68 and 0x69. Key files:

- `esp32_dual_or_single_mpu_node_v0_5_0.ino` — main node firmware: health checking, recovery logic, 5 Hz sampling, heartbeat every 20s
- `mpu_calibration_1_or_2_sensors_v0_2_0.ino` — calibration utility: collects 1000 samples at 5 ms intervals and suggests gyroscope offsets

The `esp_32_project/src/main.cpp` is an older single-sensor version (10 Hz, no recovery logic).

### Python Server (`server.py`)

Standalone script, no build step. Dual-mode:
- `--mode wifi` — TCP server on port 12345; multi-threaded for multiple clients
- `--mode usb` — serial reader on COM3 at 115200 baud

Dependency: `pyserial` (installed in `.venv`).

## Commands

### ESP32 Firmware (from `esp_32_project/`)

```bash
# Build
platformio run

# Build and upload to board
platformio run --target upload

# Serial monitor (115200 baud)
platformio device monitor
```

### Python Server

```bash
# WiFi mode (ESP32 connects over TCP)
python server.py --mode wifi

# USB/serial mode
python server.py --mode usb
```

## Performance Targets

Per thesis spec (`Roteiro_TCC_Reconstrucao_Pose_IMU.md`):
- Angle RMSE < 10°
- Jitter < 2° RMS
- End-to-end latency < 30 ms
- System uptime ≥ 95% during tests
