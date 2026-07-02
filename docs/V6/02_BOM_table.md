# EchoGlove V6.0 — Bill of Materials (BOM)

> **Version**: V6.0
> **Date**: 2026-06-23
> **Status**: Draft
> **Reference Spec**: `docs/V6/04_SOP-SPEC-PLAN_V6.md`

---

## Table of Contents

1. [Per-Glove BOM](#1-per-glove-bom)
2. [Base Station BOM](#2-base-station-bom)
3. [Development / Prototyping BOM](#3-development--prototyping-bom)
4. [Cost Comparison: V5 (BNO085) vs V6 (LSM6DSV16X)](#4-cost-comparison-v5-bno085-vs-v6-lsm6dsv16x)
5. [Recommended Suppliers](#5-recommended-suppliers)
6. [LSM6DSV16X Breakout Board Options](#6-lsm6dsv16x-breakout-board-options)
7. [Notes](#7-notes)

---

## 1. Per-Glove BOM

Each glove requires the following components. Quantities below are **per single glove**.

| # | Component | Model / Part Number | Qty | Interface | Unit Price (CNY) | Total (CNY) | Notes |
|---|-----------|---------------------|-----|-----------|------------------|-------------|-------|
| 1 | MCU | ESP32-S3-DevKitC-1 N16R8 | 1 | — | ¥28 | ¥28 | 8MB Flash + 8MB PSRAM, USB-C, antenna on PCB |
| 2 | IMU | **LSM6DSV16X** (ST) | 1 | I2C 0x6A | **¥3** | **¥3** | 6-axis, LGA-14L 2.5x3x0.83mm, needs breakout board |
| 3 | ADC | **ESP32-S3 internal ADC1** (no external chip) | 0 | ADC1 GPIO1-5 | ¥0 | ¥0 | **V6: replaces 2× ADS1115.** 5 flex sensors on ADC1_CH0-4/GPIO1-5, see `07_internal_adc_migration.md`. Saves ¥8/glove. |
| 4 | Flex Sensor | SpectraFlex 2.2" / 国产弯曲传感器 2.2" | 5 | Analog (ADC1) | ¥3 | ¥15 | Resistance range ~25kΩ (flat) to ~125kΩ (bent) |
| 5 | Pull-down Resistor | 47kΩ 0603 / 插件47kΩ | 5 | — | ¥0.1 | ¥0.5 | Voltage divider with flex sensor (unchanged from V5) |
| 6 | Decoupling Capacitor | 100nF 0603 ceramic | 3 | — | ¥0.05 | ¥0.15 | 1x per IC power pin (LSM6DSV16X VDD, VDDIO, ESP32-S3 3.3V) — ADS1115×2 removed |
| 7 | Pull-up Resistor | 4.7kΩ 0603 | 2 | — | ¥0.1 | ¥0.2 | I2C SDA/SCL pull-ups (may be on DevKitC already; include for breakout) |
| | | | | | | | |
| | **Per-Glove Total** | | | | | **~¥47** | V6: −¥8 vs V5 (ADS1115×2 removed → internal ADC1); see `07_internal_adc_migration.md` |

**Wiring / Connectors (development phase, amortized across gloves)**:

| # | Item | Qty (per glove) | Unit Price (CNY) | Total (CNY) | Notes |
|---|------|-----------------|------------------|-------------|-------|
| 8 | Dupont wires (M-M, M-F, F-F) | ~20 wires | ¥0.15 | ¥3 | For breadboard prototyping |
| 9 | Small breadboard (170 tie-points) | 1 | ¥5 | ¥5 | Compact enough for glove form factor |
| 10 | Glove (knit/stretch fabric) | 1 | ¥5 | ¥5 | Sensor mounting substrate |
| | **Wiring subtotal** | | | **~¥13** | |
| | **Grand Total per Glove (with wiring)** | | | **~¥68** | |

---

## 2. Base Station BOM

The base station is shared between left and right gloves. Components are unchanged from V5.2.

| # | Component | Model / Part Number | Qty | Unit Price (CNY) | Total (CNY) | Notes |
|---|-----------|---------------------|-----|------------------|-------------|-------|
| 1 | P4 Main Board | ESP32-P4-Function-EV-Board v1.5.2 | 1 | Competition-provided | ¥0 | 400MHz RV32, 32MB PSRAM, USB HS, LVGL display |
| 2 | C6 Co-processor | ESP32-C6-MINI-1 (ESP32-C6-MINI-1U for external antenna) | 1 | ¥12 | ¥12 | ESP-NOW receiver + BLE 5.0 provisioning relay |
| 3 | C6 Dev Board | ESP32-C6-DevKitC-1 (for prototyping) | 1 | ¥25 | ¥25 | Alternative to bare C6-MINI-1 module; includes USB-UART bridge |
| 4 | MicroSD Card | 16GB Class 10 / UHS-I | 1 | ¥15 | ¥15 | TTS PCM audio storage, FAT32 formatted |
| 5 | USB-C Cable | USB 2.0 High-Speed (480Mbps) | 1 | ¥10 | ¥10 | P4 to PC connection; must support USB 2.0 HS |
| 6 | USB-A to USB-C Cable | USB 2.0 | 1 | ¥8 | ¥8 | For C6 flashing/debug (UART bridge) |
| 7 | UART Wiring | Dupont wires (C6 TX→P4 RX) | 4 | ¥0.15 | ¥0.6 | C6 GPIO43→P4 GPIO38, C6 GPIO44←P4 GPIO37, +GND |
| | | | | | | |
| | **Base Station Total** | | | | **~¥71** | P4 board not counted (competition-provided) |

---

## 3. Development / Prototyping BOM

These items are needed for the development bench, shared across the project. Not per-glove costs.

| # | Category | Item | Model / Description | Qty | Unit Price (CNY) | Total (CNY) | Notes |
|---|----------|------|---------------------|-----|------------------|-------------|-------|
| 1 | Breadboard | Full-size breadboard (830 tie-points) | MB-102 or equivalent | 4 | ¥8 | ¥32 | 2x for glove prototyping, 2x for base station |
| 2 | Wiring | Dupont wire kit (M-M, M-F, F-F, 20cm) | 120-piece kit | 2 | ¥12 | ¥24 | Various lengths needed |
| 3 | Wiring | Jumper wire kit (assorted lengths) | 140-piece box | 1 | ¥15 | ¥15 | Neater than Dupont for semi-permanent builds |
| 4 | USB | USB-C data cables (1m) | USB 2.0, not charge-only | 4 | ¥8 | ¥32 | For ESP32-S3 + ESP32-P4 connections |
| 5 | USB | USB-UART bridge | CP2102 or CH340G module | 2 | ¥8 | ¥16 | Serial debug for bare C6 module |
| 6 | Power | Breadboard power supply module | MB-V2, 3.3V/5V switchable | 2 | ¥10 | ¥20 | With 9V barrel jack adapter |
| 7 | Power | USB power bank (5V/2A) | Any 10000mAh+ | 1 | ¥50 | ¥50 | Portable power for glove testing |
| 8 | Multimeter | Digital multimeter | UNI-T UT61E or equivalent | 1 | ¥120 | ¥120 | I2C voltage verification, continuity checks |
| 9 | Logic Analyzer | 8ch 24MHz USB logic analyzer | Saleae clone / Kingst LA1010 | 1 | ¥60 | ¥60 | I2C protocol debugging, timing analysis |
| 10 | Soldering | Soldering iron + tip set | TS101 or FH-100 | 1 | ¥150 | ¥150 | For breakout board header soldering |
| 11 | Soldering | Solder wire (0.8mm, lead-free) | Sn99.3/Cu0.7 | 1 | ¥25 | ¥25 | |
| 12 | Soldering | Flux (no-clean) | Rosin-based, 10ml syringe | 1 | ¥15 | ¥15 | LGA-14L breakout soldering aid |
| 13 | Soldering | Solder paste + heat gun | Low-temp paste (138°C) | 1 | ¥30 | ¥30 | For LGA-14L reflow if hand-soldering fails |
| 14 | Enclosure | 3D printer filament | PLA or TPU (for flex mounts) | 1 | ¥80 | ¥80 | Sensor housing, cable management clips |
| 15 | Testing | Flex sensor test fixture | 3D-printed jig for calibration | 1 | ¥20 | ¥20 | Repeatable bend angles for data collection |
| | | | | | | | |
| | **Dev Tools Total** | | | | | **~¥689** | One-time investment |

---

## 4. Cost Comparison: V5 (BNO085) vs V6 (LSM6DSV16X)

### 4.1 Per-Glove IMU Cost

| Item | V5 (BNO085) | V6 (LSM6DSV16X) | Delta |
|------|-------------|------------------|-------|
| IMU unit price | ¥15-25 | ¥3 | **-¥12 to -¥22** |
| Breakout board (if needed) | ¥5-10 (included in BNO085 module) | ¥3-8 (LGA-14L to DIP) | -¥2 to -¥2 |
| Driver library | Adafruit BNO08x (external dep) | ST official C driver (local) | No cost difference |
| **Total IMU cost per glove** | **¥20-35** | **¥6-11** | **-¥14 to -¥24** |

### 4.2 Dual-Glove System Cost (2 gloves + base station)

| Component | V5 Total (CNY) | V6 Total (CNY) | Savings (CNY) |
|-----------|----------------|----------------|---------------|
| 2x IMU (per pair) | ¥40-70 | ¥12-22 | **¥28-48** |
| 2x ESP32-S3 N16R8 | ¥56 | ¥56 | ¥0 |
| 4x ADS1115 | ¥32 | **¥0** (removed — internal ADC1) | **¥32** |
| 10x Flex sensors | ¥30 | ¥30 | ¥0 |
| 10x 47kΩ resistors | ¥1 | ¥1 | ¥0 |
| Wiring (2 gloves) | ¥26 | ¥26 | ¥0 |
| Base station (C6 + SD + cables) | ¥71 | ¥71 | ¥0 |
| **System Total** | **¥256-286** | **¥196-226** | **¥60-80** |

### 4.3 Cost Savings Summary

| Metric | Value |
|--------|-------|
| Per-glove IMU savings | ¥14-24 |
| Per-pair IMU savings | ¥28-48 |
| Per-glove ADC savings (ADS1115 removed) | ¥8 |
| Per-pair ADC savings | **¥32** |
| Per-pair total savings (IMU + ADC) | **¥60-80** |
| Per-pair savings (USD equivalent) | **$8-11** |
| Percentage reduction (IMU + ADC) | **~24-30%** |
| Full system cost reduction | **~24-30%** |

### 4.4 Non-Cost Advantages of LSM6DSV16X

| Advantage | Detail |
|-----------|--------|
| SFLP embedded fusion | No MCU-side quaternion computation; 0.65mA total |
| Faster calibration | 0.8s auto-convergence vs BNO085 manual calibration |
| Lower power | 0.65mA (accel+gyro) vs BNO085 ~12mA |
| Smaller package | 2.5x3mm LGA vs BNO085 ~5x5mm module |
| Built-in temperature sensor | Enables flex sensor drift compensation |
| MLC + FSM | Future on-chip gesture recognition (16 classes) |
| Better supply chain | ST is a major foundry; fewer fakes than Adafruit BNO085 |

---

## 5. Recommended Suppliers

### 5.1 LSM6DSV16X Breakout Boards

| Platform | Search Term | Typical Price | Notes |
|----------|-------------|---------------|-------|
| Taobao (淘宝) | "LSM6DSV16X 模块" or "LSM6DSV16X breakout" | ¥5-15 | Search for pre-soldered LGA-14L to DIP/SIP adapter boards |
| Taobao (淘宝) | "LSM6DSV16X 开发板 6轴传感器" | ¥8-20 | Some sellers include pin headers |
| AliExpress | "LSM6DSV16X breakout board" | $1-3 (~¥7-22) | Check seller ratings; 2-4 week shipping |
| AliExpress | "LSM6DSV16X IMU 6DOF module" | $2-4 (~¥14-29) | Often labeled with generic "6DOF" description |
| LCSC (立创商城) | Part: LSM6DSV16XTR | ¥15-20 (chip only) | Need own breakout PCB or LGA adapter |
| Mouser / DigiKey | LSM6DSV16X | ~$2.50-4 (chip only) | Reliable sourcing; need breakout PCB |

### 5.2 ESP32-S3 N16R8

| Platform | Search Term | Typical Price | Notes |
|----------|-------------|---------------|-------|
| Taobao | "ESP32-S3-DevKitC-1 N16R8" | ¥25-35 | Official Espressif or compatible clones |
| AliExpress | "ESP32-S3 DevKitC N16R8" | $4-6 (~¥29-43) | Check for USB-C and external antenna option |
| LCSC | ESP32-S3-DevKitC-1-N16R8 | ¥30-35 | Official Espressif distributor |

### 5.3 ADS1115 — removed in V6

ADS1115 removed in V6 — flex on internal ADC1 (GPIO1-5). No vendor part. See `07_internal_adc_migration.md`.

### 5.4 Flex Sensors

| Platform | Search Term | Typical Price | Notes |
|----------|-------------|---------------|-------|
| Taobao | "弯曲传感器 2.2寸" or "柔性弯曲传感器" | ¥2-5 | Domestic alternatives for development |
| Taobao | "Spectra Symbol flex sensor 2.2" | ¥15-25 | Original Spectra Symbol; better linearity |
| AliExpress | "flex sensor 2.2 inch bend sensor" | $1-3 (~¥7-22) | Quality varies; test before bulk order |

### 5.5 General Components (Resistors, Capacitors, Wires)

| Platform | Notes |
|----------|-------|
| Taobao | Search "贴片电阻 0603 47kΩ" — buy in reels of 100+ (¥2-5/reel) |
| LCSC (立创商城) | Best for SMD passives; ¥0.01-0.05 per piece, free shipping >¥20 order |
| AliExpress | Component kits available; slower shipping but no minimum order |

---

## 6. LSM6DSV16X Breakout Board Options

The LSM6DSV16X comes in a **LGA-14L package (2.5 x 3 x 0.83mm)** — too small for direct breadboard use. Several breakout strategies exist:

### 6.1 Option A: Commercial Breakout Board (Recommended for Development)

| Supplier | Product | Price | Features |
|----------|---------|-------|----------|
| ST NUCLEO ecosystem | X-NUCLEO-IKS4A1 | ~¥150 | Includes LSM6DSV16X + other ST sensors; Arduino headers; overkill for single sensor |
| AliExpress/Taobao generic | "LSM6DSV16X breakout DIP" | ¥5-15 | Small PCB with LGA-14L pre-soldered, 2.54mm pin header pads |
| OSH Park / JLCPCB | Custom PCB (see Option C) | ¥5-10 for 5pcs | Full control over layout |

**Recommended**: Search Taobao/AliExpress for pre-made LSM6DSV16X breakout boards with 2.54mm pitch headers. These are the most cost-effective for prototyping.

### 6.2 Option B: LGA-14L to DIP Adapter PCB

If no commercial breakout is available, use a generic LGA-to-DIP adapter:

| Adapter Type | Description | Where to Buy |
|--------------|-------------|--------------|
| LGA-14L to DIP-14 adapter | Thin PCB with LGA pads on top, DIP pins on sides | Taobao: "LGA14 转 DIP 适配板" (~¥3-5) |
| QFP/LGA universal adapter | Multi-footprint adapter board | Taobao: "LGA 转 DIP 万能转接板" (~¥5) |

**Soldering notes**:
- Apply flux to LGA pads on adapter
- Place LSM6DSV16X chip (pin 1 marker = dot on package)
- Reflow with hot air gun at 230-250°C for 30-60 seconds
- Alternatively: hand-solder with fine-tip iron + flux + 0.5mm solder
- Verify continuity with multimeter after soldering

### 6.3 Option C: Custom PCB (For Production / Semi-Production)

Design a minimal breakout PCB using KiCad or EasyEDA:

```
Schematic: LSM6DSV16X + 2x 100nF decoupling caps + 2.54mm header
Board size: ~15mm x 10mm (2-layer)
Fab: JLCPCB 5pcs/¥5 + shipping
Components: ~¥5 per board
```

**Recommended for**: Final integration onto a flex PCB (FPC) glove form factor.

### 6.4 Pin Mapping for Breakout Board

| LSM6DSV16X Pin | Breakout Header | Connection | Notes |
|----------------|-----------------|------------|-------|
| 1 (CS) | Pin 1 | 3.3V (HIGH = I2C mode) | **Must be HIGH for I2C** |
| 2 (SCL/SCLK) | Pin 2 | GPIO9 via 4.7kΩ pull-up | I2C clock |
| 3 (SDA/SDI) | Pin 3 | GPIO8 via 4.7kΩ pull-up | I2C data |
| 4 (SDO/SA0) | Pin 4 | GND | LOW = address 0x6A |
| 5 (VDDIO) | Pin 5 | 3.3V | I/O supply |
| 6 (VDD) | Pin 6 | 3.3V | Core supply |
| 7 (GND) | Pin 7 | GND | Ground |
| 8 (INT1) | Pin 8 | GPIO10 (optional) | Data-ready interrupt |
| 9-14 | NC or GND | See datasheet | Reserved / no connect |

---

## 7. Notes

### 7.1 I2C Address Summary (V6 Flat Bus)

| Device | Address | SDO/SA0 / ADDR Pin | Conflict? |
|--------|---------|---------------------|-----------|
| LSM6DSV16X | 0x6A | SDO/SA0 = GND | No |
| LSM6DSV16X (alt) | 0x6B | SDO/SA0 = VDD | Not used |

Total: 1 device on flat I2C bus (V6 removed ADS1115 @ 0x48/0x49 — flex on internal ADC1, see `07`). No MUX needed.

### 7.2 Power Budget (Per Glove)

| Component | Active Current | Sleep Current | Notes |
|-----------|---------------|---------------|-------|
| ESP32-S3 (WiFi TX) | ~240mA | ~10µA (deep sleep) | WiFi not used in glove mode; ESP-NOW only |
| ESP32-S3 (ESP-NOW TX) | ~130mA | — | Active mode, ESP-NOW broadcast |
| LSM6DSV16X | 0.65mA | 0.17µA (power-down) | Accel+Gyro combo at 104Hz |
| Flex sensors (passive) | ~0.07mA total | 0mA | Voltage divider, V/R = 3.3V/(25k+47k) |
| **Total per glove** | **~131mA** | — | At 3.3V = **~0.43W** (V6: ADS1115×2 removed, internal ADC1 — see `07`) |

**Estimated battery life**: 500mAh LiPo / 131mA = **~3.8 hours** continuous use.

### 7.3 Flex Sensor Voltage Divider

```
3.3V ──┤Flex Sensor (25k-125kΩ)├── ADC Input ──┤47kΩ├── GND
```

| Bend State | Flex Resistance | Voltage at ADC | ADC1 reading (internal, 12-bit) |
|------------|-----------------|----------------|--------------------------|
| Flat (0°) | ~25kΩ | 3.3V × 47k/(25k+47k) = 2.16V | ~2850 |
| 45° | ~50kΩ | 3.3V × 47k/(50k+47k) = 1.60V | ~2110 |
| 90° (full bend) | ~125kΩ | 3.3V × 47k/(125k+47k) = 0.90V | ~1190 |

### 7.4 Minimum Order Quantities

For a **single development prototype** (2 gloves + base station):

| Component | Min Order Qty | Needed | Surplus |
|-----------|---------------|--------|---------|
| LSM6DSV16X breakout | 1 | 2 | — |
| (internal ADC1 — no module) | — | — | — |
| ESP32-S3 N16R8 | 1 | 2 | — |
| Flex sensor | 1 | 10 | — |
| 47kΩ resistor (reel) | 100 | 10 | 90 (keep for spares) |
| 4.7kΩ resistor (reel) | 100 | 4 | 96 (keep for spares) |
| 100nF capacitor (reel) | 100 | 10 | 90 (keep for spares) |

### 7.5 Total Project Budget Estimate

| Category | Cost (CNY) | Cost (USD approx.) |
|----------|------------|---------------------|
| 2x Per-Glove BOM (components + wiring) | ¥136 | $19 |
| Base Station BOM | ¥71 | $10 |
| Dev Tools (one-time) | ¥689 | $95 |
| **Total Project Cost** | **¥896** | **~$124** |

Excludes: P4 board (competition-provided), 3D printer (assumed available), PC for relay software.
