"""
Pre-build script: reads .env and generates include/credentials.h
so WiFi credentials never appear in tracked source files.
"""
Import("env")
import os

def read_dotenv(path):
    result = {}
    try:
        with open(path) as f:
            for line in f:
                line = line.strip()
                if line and not line.startswith('#') and '=' in line:
                    k, _, v = line.partition('=')
                    result[k.strip()] = v.strip()
    except FileNotFoundError:
        print(f"[load_env] WARNING: {path} not found — credentials.h will use empty strings")
    return result

project_dir = env.subst("$PROJECT_DIR")
dotenv      = read_dotenv(os.path.join(project_dir, ".env"))

include_dir = os.path.join(project_dir, "include")
os.makedirs(include_dir, exist_ok=True)

out_path = os.path.join(include_dir, "credentials.h")
with open(out_path, "w") as f:
    f.write("// Auto-generated from .env — do not commit\n")
    f.write("#pragma once\n\n")
    for key in ("WIFI_SSID", "WIFI_PASSWORD", "SERVER_HOST"):
        val = dotenv.get(key, "")
        f.write(f'#define {key}_VAL "{val}"\n')

print(f"[load_env] credentials.h written ({', '.join(dotenv.keys())})")
