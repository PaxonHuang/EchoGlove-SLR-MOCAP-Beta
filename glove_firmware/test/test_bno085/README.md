# BNO085 Test Suite

Test files for GY-BNO085 IMU module on ESP32-S3-DevKitC-1 N16R8.

## Wiring (ESP32-S3-DevKitC-1 N16R8)

| BNO085 Pin | ESP32-S3 Pin | Notes |
|------------|--------------|-------|
| VCC | 3.3V | **NOT 5V!** |
| GND | GND | |
| SDA | GPIO8 | 4.7kΩ pull-up to 3.3V |
| SCL | GPIO9 | 4.7kΩ pull-up to 3.3V |
| CS | 3.3V | HIGH for I2C mode |
| PS0 | GND | **GND=I2C, 3.3V=SPI** |
| PS1 | GND | Must be GND |
| ADO | 3.3V | HIGH=0x4B, LOW=0x4A |
| RST | GPIO10 | Optional: 3.3V if not needed |
| INT | Floating | Not used |

**Critical Notes:**
- PS0/PS1 are latched at power-up. PS0=3.3V selects SPI mode and can damage the module!
- GPIO8/9 works on N16R8 (unlike N8 variant where GPIO8/9 conflict with internal flash)
- Adafruit BNO08x v1.2.5 uses `begin_I2C()` not `begin()`
- `sh2_SensorValue_t` uses `.sensorId` not `.type`

## Test Files

### test_bno085_diagnostic.ino
- I2C scan (0x4B detection)
- RST pin state check
- Wiring recommendations

### test_bno085_sensor_data.ino
- I2C scan + RST check
- BNO085 initialization (Adafruit library)
- Rotation vector, accelerometer, gyroscope data streaming

## Usage

### Upload diagnostic test:
```bash
cd glove_firmware
# Edit platformio.ini: change build_src_filter to test_bno085_diagnostic.ino
pio run -e bno085-diag -t upload
pio device monitor -e bno085-diag
```

### Upload sensor data test:
```bash
cd glove_firmware
# Edit platformio.ini: change build_src_filter to test_bno085_sensor_data.ino
pio run -e bno085-diag -t upload
pio device monitor -e bno085-diag
```

## Expected Output

### Diagnostic Test:
```
========================================
  BNO085 Diagnostic Test
========================================

[1] I2C scan...
  Found: 0x4B [BNO085]

[2] RST pin check (GPIO10):
  GPIO10 = HIGH
  GPIO10 (with pull-up) = HIGH
  GPIO10 (with pull-down) = HIGH
  [WARN] RST is HARD HIGH — likely connected to 3.3V!
```

### Sensor Data Test:
```
========================================
  BNO085 Sensor Data Test
========================================

[1] I2C scan...
  Found: 0x4B [BNO085]

[2] RST pin check (GPIO10):
  GPIO10 with pull-down = HIGH
  [WARN] RST is HARD HIGH — software reset unavailable

[3] Initializing BNO085 (Adafruit library)...
[OK] BNO085 initialized!

[4] Enabling sensors...
  [OK] Rotation vector (200Hz)
  [OK] Accelerometer (100Hz)
  [OK] Gyroscope (100Hz)

[5] Reading sensor data (Ctrl+C to stop)...

[ROT] r=(0.461, -0.043, 0.017, -0.886) accuracy=0.05
[ACC] x=0.613 y=-0.691 z=9.652 m/s^2
[GYR] x=0.000 y=0.000 z=0.000 rad/s
```

## Troubleshooting

### BNO085 not found (I2C scan fails):
1. Check PS0/PS1 = GND (not 3.3V!)
2. Check SDA/SCL wiring (GPIO8/9)
3. Check VCC = 3.3V (not 5V!)
4. Power cycle: disconnect VCC for 10 seconds

### BNO085 init fails (Adafruit library):
1. Power cycle: disconnect VCC for 10 seconds
2. Check CS = 3.3V
3. Check ADO = 3.3V (for 0x4B)

### RST pin HARD HIGH:
- RST has strong internal pull-up
- Software reset not available — use power cycle instead
- If RST connected to 3.3V, disconnect and connect to GPIO10

### Data shows all zeros:
- Wait 1-2 seconds for sensor initialization
- Check accuracy field — should increase over time
- Move sensor to see data changes

## History

- 2026-06-21: Initial test suite — PS0/PS1 wiring correction, GPIO8/9 on N16R8, Adafruit library requirements
