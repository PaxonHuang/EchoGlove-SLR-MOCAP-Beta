# EchoGlove V5.2 — Wiring Debug & Multimeter Guide

> **Version**: V5.2 | **Date**: 2026-06-10 | **Tool**: DM40B Digital Multimeter
>
> **V3 Users**: V5 does not use TCA9548A MUX or TMAG5273 Hall sensors. V3 guide: `docs/archive/v3/HARDWARE_WIRING_DEBUG_GUIDE_DM40B.md`
>
> **V5.2 Note**: P4 base station (ESP32-P4-Function-EV-Board) is competition-provided — no custom wiring needed. This guide covers glove hardware only.

---

## Quick Reference

| Test | Expected | Tool Setting |
|------|----------|--------------|
| I²C pull-up (SDA→3.3V) | 2.2kΩ | Ω |
| I²C pull-up (SCL→3.3V) | 2.2kΩ | Ω |
| SDA voltage (idle) | 3.3V | V DC |
| SCL voltage (idle) | 3.3V | V DC |
| Flex sensor (straight) | ~0.3V at ADC | V DC |
| Flex sensor (bent) | ~2.8V at ADC | V DC |
| Battery voltage | 3.7-4.2V | V DC |
| Regulator output | 3.3V ±0.1V | V DC |

---

## V5 I²C Topology (No MUX)

```
ESP32-S3 GPIO8(SDA) GPIO9(SCL)
    │           │
    ├── 2.2kΩ ──┤── 3.3V (pull-ups)
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

---

## I²C Bus Tests

### Pull-up Resistance (Power OFF)

| Test Point | Expected | Notes |
|------------|----------|-------|
| GPIO8 (SDA) ↔ 3.3V | 2.2kΩ ±5% | SDA pull-up |
| GPIO9 (SCL) ↔ 3.3V | 2.2kΩ ±5% | SCL pull-up |
| SDA ↔ SCL | >100kΩ | No short |

### Bus Voltage (Power ON)

| Test Point | Expected | Notes |
|------------|----------|-------|
| SDA ↔ GND | 3.3V | Idle high |
| SCL ↔ GND | 3.3V | Idle high |

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
| Straight | ~0.3V | Low resistance |
| Half bent | ~1.5V | Mid-range |
| Fully bent | ~2.8V | High resistance |

---

## Flex Sensor Tests

### Resistance (Power OFF, disconnected)

| State | Resistance | Notes |
|-------|------------|-------|
| Straight | ~10kΩ | Low |
| Bent | ~100kΩ | High |

### Voltage Divider Output

```
3.3V ─── [Flex] ──┬── [47kΩ] ─── GND
                   │
                   └── ADC input
```

- Straight: V_out ≈ 3.3V × 10k/(10k+47k) ≈ 0.58V
- Bent: V_out ≈ 3.3V × 100k/(100k+47k) ≈ 2.24V

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
| No I²C devices | SDA/SCL = 0V | Missing pull-ups | Install 2.2kΩ |
| No I²C devices | Voltage OK | SDA/SCL swapped | Swap GPIO8/9 |
| Only 0x4B | — | ADS1115 not connected | Check wiring |
| ADC always 0 | AIN = 0V | Missing 47kΩ | Install pull-down |
| ADC always max | AIN = 3.3V | Sensor open circuit | Check solder joints |
| ESP-NOW no data | — | MAC not paired | Check peer config |

---

> **Version**: 5.0.0 | **Last Updated**: 2026-06-03
