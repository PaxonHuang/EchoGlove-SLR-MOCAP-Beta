#!/usr/bin/env bash
set -euo pipefail

# EchoGlove Smoke Test
# Tests project structure, verifies key files, and documents how to run the system

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
SCREENSHOT_DIR="$SCRIPT_DIR/screenshots"
mkdir -p "$SCREENSHOT_DIR"

echo "=== EchoGlove Smoke Test ==="
echo "Project root: $PROJECT_ROOT"
echo ""

# 1. Verify project structure
echo "[1/6] Verifying project structure..."
for dir in glove_firmware glove_relay glove_web docs; do
    if [ -d "$PROJECT_ROOT/$dir" ]; then
        echo "  ✓ $dir/ exists"
    else
        echo "  ✗ $dir/ missing"
        exit 1
    fi
done

# 2. Check relay server files
echo "[2/6] Checking relay server files..."
cd "$PROJECT_ROOT/glove_relay"
for file in src/main.py requirements.txt pyproject.toml; do
    if [ -f "$file" ]; then
        echo "  ✓ $file exists"
    else
        echo "  ✗ $file missing"
        exit 1
    fi
done

# 3. Check web frontend files
echo "[3/6] Checking web frontend files..."
cd "$PROJECT_ROOT/glove_web"
for file in package.json src/App.tsx vite.config.ts; do
    if [ -f "$file" ]; then
        echo "  ✓ $file exists"
    else
        echo "  ✗ $file missing"
        exit 1
    fi
done

# 4. Check firmware files
echo "[4/6] Checking firmware files..."
cd "$PROJECT_ROOT/glove_firmware"
for file in src/main.cpp platformio.ini; do
    if [ -f "$file" ]; then
        echo "  ✓ $file exists"
    else
        echo "  ✗ $file missing"
        exit 1
    fi
done

# 5. Check test files
echo "[5/6] Checking test files..."
cd "$PROJECT_ROOT/glove_relay"
test_count=$(find tests -name "test_*.py" 2>/dev/null | wc -l)
if [ "$test_count" -gt 0 ]; then
    echo "  ✓ Found $test_count test files in glove_relay/tests/"
else
    echo "  ✗ No test files found in glove_relay/tests/"
fi

cd "$PROJECT_ROOT/glove_firmware"
if [ -d "test" ]; then
    echo "  ✓ test/ directory exists"
else
    echo "  ✗ test/ directory missing"
fi

# 6. Check documentation
echo "[6/6] Checking documentation..."
cd "$PROJECT_ROOT"
for file in CLAUDE.md PROGRESS.md README.md; do
    if [ -f "$file" ]; then
        echo "  ✓ $file exists"
    else
        echo "  ✗ $file missing"
    fi
done

echo ""
echo "=== Smoke Test Complete ==="
echo "All project structure checks passed."
echo ""
echo "To run the full system:"
echo ""
echo "1. Start relay server:"
echo "   cd glove_relay"
echo "   python3 -m venv venv"
echo "   source venv/bin/activate"
echo "   pip install -r requirements.txt"
echo "   uvicorn src.main:app --host 0.0.0.0 --port 8765 --reload"
echo ""
echo "2. Start web frontend:"
echo "   cd glove_web"
echo "   npm install"
echo "   npm run dev"
echo ""
echo "3. Flash firmware (requires ESP32-S3 hardware):"
echo "   cd glove_firmware"
echo "   pio run -t upload"
echo ""
echo "4. Run tests:"
echo "   cd glove_relay && python -m pytest tests/"
echo "   cd glove_firmware && pio test"
echo ""
echo "Note: Firmware tests require ESP32-S3 hardware connected via USB."
