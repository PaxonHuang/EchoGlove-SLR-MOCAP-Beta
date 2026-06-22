---
name: run-echoglove
description: Build and run EchoGlove relay server and web frontend, test with curl/chromium-cli
---

# EchoGlove Run Skill

Multi-component ESP32 data glove system: relay server (Python FastAPI) + web frontend (React + Vite) + firmware (ESP32-S3).

## Quick Start

```bash
# From project root
bash .claude/skills/run-echoglove/smoke.sh
```

## Prerequisites

```bash
# Python 3.9+ with pip
sudo apt-get install -y python3 python3-pip python3-venv

# Node.js 18+ (for web frontend)
curl -fsSL https://deb.nodesource.com/setup_18.x | sudo -E bash -
sudo apt-get install -y nodejs

# PlatformIO (for firmware)
pip install platformio

# chromium-cli (for web testing)
npm install -g chromium-cli
```

## Build & Run

### Relay Server (glove_relay)

```bash
cd glove_relay
python3 -m venv venv
source venv/bin/activate
pip install -r requirements.txt

# Start relay (WebSocket on :8765, UDP on :8888)
uvicorn src.main:app --host 0.0.0.0 --port 8765 --reload
```

### Web Frontend (glove_web)

```bash
cd glove_web
npm install
npm run dev  # Dev server on http://localhost:5173
```

### Firmware (glove_firmware)

```bash
cd glove_firmware
pio run -t upload  # Requires ESP32-S3 hardware connected via USB
pio device monitor  # Serial monitor at 115200 baud
```

## Agent Path (smoke.sh)

The smoke script verifies project structure and documents how to run the system:

1. Verifies project structure (glove_firmware, glove_relay, glove_web, docs)
2. Checks relay server files (src/main.py, requirements.txt, pyproject.toml)
3. Checks web frontend files (package.json, src/App.tsx, vite.config.ts)
4. Checks firmware files (src/main.cpp, platformio.ini)
5. Checks test files (14 test files in glove_relay/tests/)
6. Checks documentation (CLAUDE.md, PROGRESS.md, README.md)

Run with: `bash .claude/skills/run-echoglove/smoke.sh`

## Testing

### Relay Tests

```bash
cd glove_relay
python -m pytest tests/  # 14 test files, 88 tests total
```

### Firmware Tests

```bash
cd glove_firmware
pio test  # Requires ESP32-S3 hardware connected via USB
```

## Gotchas

- **Port conflicts**: Relay uses 8765 (WebSocket) + 8888 (UDP). Web uses 5173.
- **USB CDC**: Relay tries to open `/dev/ttyACM0` for P4 base station. Will fail without hardware.
- **Models**: Relay loads ML models on startup. May fail if model files missing.
- **CORS**: Web frontend connects to `ws://localhost:8765` by default.
- **Firmware tests**: Require ESP32-S3 hardware connected via USB. Will timeout in container.
- **Network issues**: npm install may fail behind proxy. Configure npm proxy settings.

## Troubleshooting

| Symptom | Fix |
|---------|-----|
| `ModuleNotFoundError: No module named 'src'` | Run from `glove_relay/` directory |
| `EADDRINUSE: address already in use 8765` | Kill existing relay: `pkill -f uvicorn` |
| `chromium-cli: command not found` | `npm install -g chromium-cli` |
| `pio test timeout` | Ensure ESP32-S3 hardware is connected via USB |
| `npm install ECONNRESET` | Configure proxy: `npm config set proxy http://proxy:port` |
