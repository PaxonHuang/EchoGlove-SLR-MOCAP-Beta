# I2C Bus Scanner — EchoGlove V5 Diagnostic

## What It Does

Scans multiple SDA/SCL GPIO pairs on the ESP32-S3 to find which pins can
communicate with the three expected I2C devices:

| Address | Device          |
|---------|-----------------|
| 0x48    | ADS1115 ADC #1  |
| 0x49    | ADS1115 ADC #2  |
| 0x4B    | BNO085 IMU      |

Also reports PSRAM size — if it reads 0 MB, the board JSON likely has an
incorrect `memory_type` setting that conflicts with the GPIO pins used for I2C.

### Why GPIO8/9 May Fail

On ESP32-S3 N16R8 boards with **Octal PSRAM**, GPIO8 and GPIO9 are used by
the PSRAM interface. When `memory_type` is set to `qio_opi` in the board JSON,
the firmware initializes Octal PSRAM which takes over GPIO8/9, making them
unavailable for I2C. This explains both the I2C timeout (error 5) and the 0 MB
PSRAM report (if the PSRAM init fails because the config is wrong).

## Pin Pairs Tested

| SDA | SCL | Notes                          |
|-----|-----|--------------------------------|
|  8  |  9  | Current wiring (likely broken with OPI PSRAM) |
|  1  |  2  |                                |
|  4  |  5  |                                |
|  6  |  7  |                                |
| 15  | 16  |                                |
| 17  | 18  |                                |
| 20  | 21  |                                |

## How to Run

```bash
cd glove_firmware

# Option 1: Run as a PlatformIO test
pio test -e esp32-s3-devkitc-1-n16r8 --filter test_i2c_scan

# Option 2: Build and upload as regular firmware, then open serial monitor
pio run -e esp32-s3-devkitc-1-n16r8 -t upload
pio device monitor -b 115200
```

> **Note:** If using `pio test`, PlatformIO may expect Unity test assertions.
> This diagnostic is a standalone Arduino sketch. If `pio test` complains about
> missing Unity, use Option 2 instead, or configure `test_build_src = yes` in
> `platformio.ini`.

## Output

The scanner prints a table like this at the end:

```
  GPIO Pair    | Devices | 0x48 | 0x49 | 0x4B | Status
  -------------|---------|------|------|------|-------
  SDA= 8 SCL= 9 |   0     |  --  |  --  |  --  | expected (GPIO8/9 conflict)
  SDA= 1 SCL= 2 |   3     |  OK  |  OK  |  OK  | *** ALL 3 DEVICES ***
```

If a working pair is found, it prints the `build_flags` to add to `platformio.ini`.

## Troubleshooting

If no pair finds any devices:

1. **Check wiring** — verify continuity from ESP32-S3 pins to sensor breakout boards
2. **Pull-ups** — SDA and SCL each need a 4.7k ohm pull-up to 3.3V
3. **BNO085 mode pins** — PS0 must be GND (I2C mode), not 3.3V (SPI mode)
4. **ADS1115 ADDR pin** — GND = 0x48, VCC = 0x49, SDA = 0x4A, SCL = 0x4B
5. **Power cycle BNO085** — disconnect VCC for 10 seconds, then reconnect
6. **Voltage** — all devices must be on 3.3V, NOT 5V
