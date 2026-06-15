# EchoGlove V5.2 — Wiring Debug & Multimeter Guide

> **Version**: V5.2.1 | **Date**: 2026-06-12 | **Tool**: DM40B Digital Multimeter
>
> **V3 Users**: V5 does not use TCA9548A MUX or TMAG5273 Hall sensors. V3 guide: `docs/archive/v3/HARDWARE_WIRING_DEBUG_GUIDE_DM40B.md`
>
> **V5.2 Note**: P4 base station (ESP32-P4-Function-EV-Board) is competition-provided — no custom wiring needed. This guide covers glove hardware only.

---

## Quick Reference

| Test | Expected | Tool Setting |
|------|----------|--------------|
| I²C pull-up (SDA→3.3V) | **4.7kΩ** | Ω |
| I²C pull-up (SCL→3.3V) | **4.7kΩ** | Ω |
| SDA voltage (idle) | 3.3V | V DC |
| SCL voltage (idle) | 3.3V | V DC |
| Flex sensor (straight) | ~1.05V at ADC | V DC |
| Flex sensor (bent) | ~2.31V at ADC | V DC |
| Battery voltage | 3.7-4.2V | V DC |
| Regulator output | 3.3V ±0.1V | V DC |

---

## Resistor Value Quick Reference

| Resistor | Color Code | Purpose | Qty |
|----------|------------|---------|-----|
| 4.7kΩ | Yellow-Purple-Red-Gold | I²C pull-up (SDA/SCL → 3.3V) | 2 |
| 47kΩ | Yellow-Purple-Orange-Gold | Flex sensor voltage divider (→ 3.3V) | 5 |

> **⚠️ Do NOT confuse**: 4.7kΩ (4.7 thousand ohms) and 47kΩ (47 thousand ohms) differ by 10×!

---

## V5 I²C Topology (No MUX)

```
ESP32-S3 GPIO8(SDA) GPIO9(SCL)
    │           │
    ├── 4.7kΩ ──┤── 3.3V (pull-ups)
    │           │
    ├───────────┼───────────┬───────────┐
    │           │           │           │
┌───┴───┐  ┌───┴───┐  ┌───┴───┐       │
│ADS1115│  │ADS1115│  │BNO085 │       │
│ #1    │  │ #2    │  │       │       │
│ 0x48  │  │ 0x49  │  │ 0x4B  │       │
└───────┘  └───────┘  └───────┘       │
```

**Key difference from V3**: No TCA9548A MUX. Flat bus with 3 devices.

### I²C Pull-Up Calculation

- I²C Standard-mode (100kHz): rise time t_r ≤ 1000ns
- Formula: R_pull ≤ t_r / (0.8473 × C_bus)
- Bus capacitance estimate: 3 ICs (~60pF) + breadboard wires (~80pF) ≈ 140pF
- R_pull ≤ 1000ns / (0.8473 × 140pF) ≈ 8.4kΩ
- **4.7kΩ** provides ample margin while not over-driving (idle current only 0.7mA/line)

---

## I²C Bus Tests

### Pull-up Resistance (Power OFF)

| Test Point | Expected | Notes |
|------------|----------|-------|
| GPIO8 (SDA) ↔ 3.3V | **4.7kΩ ±5%** | SDA pull-up |
| GPIO9 (SCL) ↔ 3.3V | **4.7kΩ ±5%** | SCL pull-up |
| SDA ↔ SCL | >100kΩ | No short |
| SDA ↔ GND | >100kΩ | No short |

### Bus Voltage (Power ON)

| Test Point | Expected | Notes |
|------------|----------|-------|
| SDA ↔ GND | 3.3V | Idle high |
| SCL ↔ GND | 3.3V | Idle high |

> **If SDA/SCL voltage is 0V**: Pull-up resistors missing or SDA/SCL pulled low by device (bus lockup).

### Device Scan (Firmware)

```bash
pio device monitor -b 115200
# Expected: 3 devices found (0x48, 0x49, 0x4B)
```

---

## ADS1115 ADC Tests

### Address Verification (Power OFF)

| Device | ADDR Pin | Expected Address |
|--------|----------|-----------------|
| ADS1115 #1 | GND | 0x48 |
| ADS1115 #2 | 3.3V | 0x49 |

### ADC Input Voltage

| State | AIN Voltage | Notes |
|-------|-------------|-------|
| Straight | ~1.05V | Low resistance |
| Half bent | ~1.86V | Mid-range |
| Fully bent | ~2.31V | High resistance |

### Voltage Divider Circuit

```
3.3V ── [47kΩ] ──┬── [Flex Sensor] ── GND
                  │
                  └── ADC input
```

- Straight (R_flex=22kΩ): V = 3.3 × 22k/69k = 1.05V
- Half bent (R_flex=60kΩ): V = 3.3 × 60k/107k = 1.86V
- Fully bent (R_flex=110kΩ): V = 3.3 × 110k/157k = 2.31V

> **⚠️ Important**: 47kΩ connects to 3.3V side, flex sensor connects to GND side.
> This way: bent = higher voltage (intuitive).

---

## BNO085 IMU Tests

### RST Pin Check (Critical!)

**Multimeter**: Continuity mode

| Test Point | Expected | Notes |
|------------|----------|-------|
| RST ↔ 3.3V | Continuity (0Ω) | **Must be pulled HIGH** |
| RST ↔ GND | Open (>100kΩ) | Must NOT be pulled low |

> **⚠️ RST floating = BNO085 NOT working**. RST pin MUST connect to 3.3V.

### PS0/PS1 Pin Check

| Test Point | Expected | Notes |
|------------|----------|-------|
| PS0 ↔ 3.3V | Continuity | Selects I2C mode |
| PS1 ↔ GND | Continuity | Selects I2C mode |

### Address Verification

| Test Point | Expected | Notes |
|------------|----------|-------|
| BNO085 SDA ↔ GPIO8 | Continuity | Direct connection |
| BNO085 SCL ↔ GPIO9 | Continuity | Direct connection |
| I²C address | 0x4B | Firmware scan confirms |

### Data Verification

**Multimeter**: V DC

| Test Point | Expected | Notes |
|------------|----------|-------|
| INT ↔ GND | Pulse | Data ready trigger |

**Firmware**: Euler/Gyro data should be non-zero and change with motion.

---

## Flex Sensor Tests

### Resistance (Power OFF, disconnected)

| State | Resistance | Notes |
|-------|------------|-------|
| Straight | ~22kΩ | Low |
| Bent | ~110kΩ | High |

### Voltage Divider Output

| State | AIN Voltage | Notes |
|-------|-------------|-------|
| Straight | ~1.05V | Low resistance |
| Half bent | ~1.86V | Mid-range |
| Fully bent | ~2.31V | High resistance |

---

## Power System Tests

| Test Point | Expected | Notes |
|------------|----------|-------|
| LiPo battery | 3.7-4.2V | Full = 4.2V |
| AMS1117 output | 3.3V ±0.1V | Regulated |
| ESP32-S3 3V3 | 3.3V ±0.1V | Supply |

---

## Fault Quick Reference

| Symptom | Test | Likely Cause | Fix |
|---------|------|--------------|-----|
| No I²C devices | SDA/SCL = 0V | Missing pull-ups | Install 4.7kΩ |
| No I²C devices | Voltage OK | SDA/SCL swapped | Swap GPIO8/9 |
| No I²C devices | Voltage OK, wiring OK | BNO085 RST floating | Connect RST to 3.3V |
| Only 0x4B | — | ADS1115 not connected | Check wiring |
| Scan hangs | SDA = 0V | I²C bus lockup | Power cycle |
| Read err=5 | — | Device no ACK | Check VCC/GND |
| Read err=263 | — | I²C timeout | Check SDA/SCL connection |
| ADC always 0 | AIN = 0V | Missing 47kΩ | Install pull-up to 3.3V |
| ADC always max | AIN = 3.3V | Sensor open circuit | Check solder joints |
| Bend = lower voltage | — | Sensor/resistor swapped | 47kΩ→3.3V, sensor→GND |
| ESP-NOW no data | — | MAC not paired | Check peer config |

---

## I²C Bus Deep Debug

### Scan Hangs

**Symptom**: Firmware I²C scan hangs, no output.

**Cause**: I²C bus locked by device (SDA held low).

**Steps**:
1. Power off
2. Multimeter resistance: SDA↔GND, should be >100kΩ
3. If SDA↔GND = 0Ω: some device has SDA pin shorted
4. Disconnect devices one by one to find the problem device
5. Check breadboard jumper wires for poor contact

### Scan OK but Read Fails

**Symptom**: Scan finds devices (0x48, 0x49, 0x4B), but reads return err=5 or err=263.

**Cause**:
- Device ACKs address byte, but data transfer phase fails
- May be timing issue or excessive bus capacitance

**Steps**:
1. Lower I²C frequency to 50kHz for testing
2. Check decoupling capacitors (100nF at each VDD)
3. Shorten jumper wire lengths
4. Check for multiple pull-up resistors in parallel (value too low)

### Breadboard Common Issues

| Issue | Symptom | Fix |
|-------|---------|-----|
| Poor jumper contact | Intermittent | Re-seat, try new wire |
| Internal breadboard break | Row not connected | Move to different row |
| Power rail short | 3.3V↔GND short | Check breadboard power rails |
| Long jumper wires | Signal degradation | Use shorter wires |

### Systematic Debug Flow

```
1. Power OFF
   ├── Check SDA↔3.3V resistance = 4.7kΩ
   ├── Check SCL↔3.3V resistance = 4.7kΩ
   ├── Check SDA↔SCL > 100kΩ
   ├── Check SDA↔GND > 100kΩ
   ├── Check BNO085 RST↔3.3V = continuity
   ├── Check ADS1115 #1 ADDR↔GND = continuity
   └── Check ADS1115 #2 ADDR↔3.3V = continuity

2. Power ON
   ├── Check SDA voltage = 3.3V
   ├── Check SCL voltage = 3.3V
   ├── Check all VCC = 3.3V
   └── Check all GND = 0V

3. Firmware Test
   ├── I²C scan: should find 3 devices
   ├── ADS1115 read: should return ADC values
   └── BNO085 read: should return IMU data
```

---

> **Version**: 5.2.1 | **Last Updated**: 2026-06-12
