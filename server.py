import socket
import threading
import serial
import argparse
import json
import csv
import os
from datetime import datetime

CSV_FIELDS = ['t_ms', 'seq', 'node', 'sensor_id', 'ax_g', 'ay_g', 'az_g', 'gx_dps', 'gy_dps', 'gz_dps', 'temp_c']


def open_csv(output_dir):
    os.makedirs(output_dir, exist_ok=True)
    filename = os.path.join(output_dir, f"session_{datetime.now().strftime('%Y%m%d_%H%M%S')}.csv")
    f = open(filename, 'w', newline='', encoding='utf-8')
    writer = csv.DictWriter(f, fieldnames=CSV_FIELDS, extrasaction='ignore')
    writer.writeheader()
    print(f"Logging to {filename}")
    return f, writer


def process_line(line, writer, file_handle):
    if not line:
        return
    try:
        msg = json.loads(line)
    except json.JSONDecodeError:
        print(f"[raw] {line}")
        return

    msg_type = msg.get('msg_type', '')

    if msg_type == 'data':
        writer.writerow({
            't_ms':      msg.get('t_ms'),
            'seq':       msg.get('seq'),
            'node':      msg.get('node'),
            'sensor_id': msg.get('sensor_id'),
            'ax_g':      msg.get('ax_g'),
            'ay_g':      msg.get('ay_g'),
            'az_g':      msg.get('az_g'),
            'gx_dps':    msg.get('gx_dps'),
            'gy_dps':    msg.get('gy_dps'),
            'gz_dps':    msg.get('gz_dps'),
            'temp_c':    msg.get('temp_c'),
        })
        file_handle.flush()
    elif msg_type == 'boot':
        print(f"[boot]          {msg.get('node')}  fw={msg.get('fw_version')}")
    elif msg_type == 'config':
        print(f"[config]        {msg.get('sensor_count')} sensor(s) @ {msg.get('sample_rate_hz')} Hz")
    elif msg_type == 'sensor_status':
        print(f"[sensor_status] {msg.get('sensor_id')} → {msg.get('status')}: {msg.get('detail')}")
    elif msg_type == 'heartbeat':
        print(f"[heartbeat]     uptime={msg.get('uptime_s')}s  status={msg.get('status')}")
    elif msg_type == 'error':
        print(f"[error]         {msg.get('sensor_id')} — {msg.get('code')}: {msg.get('detail')}")
    else:
        print(f"[{msg_type}] {line}")


# ── USB / serial mode ────────────────────────────────────────────────────────

def read_serial(port, baudrate, output_dir):
    f, writer = open_csv(output_dir)
    ser = None
    try:
        ser = serial.Serial(port, baudrate, timeout=1)
        print(f"Reading from {port} at {baudrate} baud")
        while True:
            line = ser.readline().decode('utf-8', errors='replace').strip()
            process_line(line, writer, f)
    except KeyboardInterrupt:
        print("\nSerial reader shutting down...")
    except Exception as e:
        print(f"Serial error: {e}")
    finally:
        if ser and ser.is_open:
            ser.close()
        f.close()


# ── WiFi / TCP mode ──────────────────────────────────────────────────────────

def handle_client(client_socket, writer, file_handle):
    buf = ''
    try:
        while True:
            chunk = client_socket.recv(1024).decode('utf-8', errors='replace')
            if not chunk:
                break
            buf += chunk
            while '\n' in buf:
                line, buf = buf.split('\n', 1)
                process_line(line.strip(), writer, file_handle)
    except Exception:
        pass
    finally:
        client_socket.close()


def start_tcp_server(host, port, output_dir):
    f, writer = open_csv(output_dir)
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind((host, port))
    server.listen(5)
    print(f"TCP server listening on {host}:{port}")
    try:
        while True:
            client, addr = server.accept()
            print(f"[connect] {addr}")
            t = threading.Thread(target=handle_client, args=(client, writer, f), daemon=True)
            t.start()
    except KeyboardInterrupt:
        print("\nServer shutting down...")
    finally:
        server.close()
        f.close()


# ── Entry point ──────────────────────────────────────────────────────────────

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="ESP32 IMU data logger")
    parser.add_argument('--mode', choices=['wifi', 'usb'], default='usb',
                        help='Connection mode (default: usb)')
    parser.add_argument('--port', type=int, default=12345,
                        help='TCP port for WiFi mode (default: 12345)')
    parser.add_argument('--serial-port', default='COM3',
                        help='Serial port for USB mode (default: COM3)')
    parser.add_argument('--baudrate', type=int, default=115200,
                        help='Baud rate for USB mode (default: 115200)')
    parser.add_argument('--output-dir', default='data',
                        help='Directory for CSV files (default: data/)')
    args = parser.parse_args()

    if args.mode == 'usb':
        read_serial(args.serial_port, args.baudrate, args.output_dir)
    else:
        start_tcp_server('0.0.0.0', args.port, args.output_dir)
