# Hardware Wiring Debug Guide — DM40B Multimeter

## Overview
This guide helps verify physical connections for PCA9548A I2C multiplexer and GY-BNO085 IMU before troubleshooting I2C communication issues.

## Required Equipment
- DM40B multimeter (or equivalent digital multimeter)
- ESP32-S3-DevKitC-1 N16R8
- PCA9548A I2C multiplexer
- GY-BNO085 IMU module
- Jumper wires (breadboard or DuPont)
- 2kΩ resistors (×2 for main I2C bus pull-ups)

## Safety Precautions
⚠️ **CRITICAL**: Always measure resistance with **power OFF**. Never measure resistance on powered circuits.

---

## Step 1: Power Supply Verification (ESP32-S3 OFF)

### 1.1 Verify 3.3V Power Rail
1. Set multimeter to **DC Voltage mode** (20V range)
2. Power ON ESP32-S3
3. Measure:
   - **3V3 pin** → **GND pin**: Should read **3.25-3.35V**
4. If voltage is wrong, check:
   - USB cable connection
   - DevKit board power selector (should be USB)

### 1.2 Verify PCA9548A Power Connections
**ESP32-S3 OFF**, measure continuity:

| Test Point | Expected Resistance | Multimeter Mode | Notes |
|------------|---------------------|-----------------|-------|
| PCA9548A **VCC** → ESP32-S3 **3V3** | 0Ω (short) | Resistance (200Ω range) | Direct connection |
| PCA9548A **GND** → ESP32-S3 **GND** | 0Ω (short) | Resistance (200Ω range) | Direct connection |
| PCA9548A **VCC** → **GND** | >10kΩ (no short) | Resistance (20kΩ range) | Should NOT be shorted |

✅ **Pass criteria**: VCC/GND connected correctly, no short between VCC/GND

---

## Step 2: I2C Pull-Up Resistor Verification (ESP32-S3 OFF)

### 2.1 Verify Pull-Up Placement
Your configuration uses **2kΩ pull-ups on main bus only**:

```
3.3V ──[2kΩ]── GPIO8 (SDA)
3.3V ──[2kΩ]── GPIO9 (SCL)
```

Measure with **ESP32-S3 OFF**, multimeter in **Resistance mode (20kΩ range)**:

| Test Point | Expected Resistance | Meaning |
|------------|---------------------|---------|
| **GPIO8** → **3.3V** | ~2kΩ | SDA pull-up present |
| **GPIO9** → **3.3V** | ~2kΩ | SCL pull-up present |
| **GPIO8** → **GND** | >10kΩ (open) | No short to ground |
| **GPIO9** → **GND** | >10kΩ (open) | No short to ground |

✅ **Pass criteria**: Both pull-ups measure ~2kΩ, no shorts to ground

⚠️ **Note**: Ch5 (GY-BNO085) sub-bus has **5.1kΩ** pull-ups installed (SD5/SC5→3.3V). Ch0-4 (TMAG5273) sub-bus pull-ups (4.7kΩ) NOT installed yet. Main bus 2kΩ also passes through PCA9548A internal switches — Ch0-4 sub-buses are functional for testing without dedicated pull-ups.

---

## Step 3: I2C Bus Connectivity (ESP32-S3 OFF)

### 3.1 Verify GPIO → PCA9548A Connections
Measure continuity with **ESP32-S3 OFF**, multimeter in **Resistance mode (200Ω range)**:

| Test Point | Expected Resistance | Meaning |
|------------|---------------------|---------|
| **GPIO8** (ESP32) → **SDA** (PCA9548A) | 0Ω (short) | SDA bus connected |
| **GPIO9** (ESP32) → **SCL** (PCA9548A) | 0Ω (short) | SCL bus connected |
| **GPIO8** → **GPIO9** | >10kΩ (open) | No short between SDA/SCL |

✅ **Pass criteria**: Both lines connected, no cross-short

---

## Step 4: PCA9548A Reset Pin (ESP32-S3 OFF)

### 4.1 Verify /RST Connection
PCA9548A has an active-low reset pin. It must be pulled HIGH for normal operation.

| Test Point | Expected Resistance | Notes |
|------------|---------------------|-------|
| PCA9548A **/RST** → **3.3V** | 0Ω (short) or 10kΩ pull-up | Must be HIGH to operate |
| PCA9548A **/RST** → **GND** | >10kΩ (open) | Should NOT be grounded |

✅ **Pass criteria**: /RST connected to 3.3V or pulled up

---

## Step 5: BNO085 Connections (ESP32-S3 OFF)

### 5.1 Verify BNO085 Power (CH5)
Measure continuity with **ESP32-S3 OFF**:

| Test Point | Expected Resistance | Meaning |
|------------|---------------------|---------|
| BNO085 **VCC** → **3.3V** | 0Ω (short) | Power connected |
| BNO085 **GND** → **GND** | 0Ω (short) | Ground connected |
| BNO085 **VCC** → **GND** | >10kΩ (open) | No power short |

### 5.2 Verify BNO085 I2C Bus (CH5)
Measure continuity:

| Test Point | Expected Resistance | Meaning |
|------------|---------------------|---------|
| PCA9548A **SD5** → BNO085 **SDA** | 0Ω (short) | Channel 5 downstream bus |
| PCA9548A **SC5** → BNO085 **SCL** | 0Ω (short) | Channel 5 downstream bus |
| BNO085 **SDA** → **SCL** | >10kΩ (open) | No cross-short on CH5 |

### 5.3 Verify BNO085 Address Pins
BNO085 I2C address is determined by PS0/PS1 pins:

| PS0 | PS1 | I2C Address | Notes |
|-----|-----|-------------|-------|
| GND | GND | 0x4A | **Default (you should use this)** |
| GND | VCC | 0x4B | Alternate |
| VCC | GND | 0x4C | Alternate |
| VCC | VCC | 0x4D | Alternate |

Measure resistance:

| Test Point | Expected Resistance | Meaning |
|------------|---------------------|---------|
| BNO085 **PS0** → **GND** | 0Ω (short) | Address bit 0 = 0 |
| BNO085 **PS1** → **GND** | 0Ω (short) | Address bit 1 = 0 |

✅ **Pass criteria**: Address pins grounded → address = 0x4A

### 5.4 Verify BNO085 INT Pin
BNO085 interrupt pin (optional, but you connected it):

| Test Point | Expected Resistance | Meaning |
|------------|---------------------|---------|
| BNO085 **INT** → **GPIO21** (ESP32) | 0Ω (short) | Interrupt connected |

---

## Step 6: I2C Bus Idle State Verification (ESP32-S3 ON)

### 6.1 Measure Idle Voltage Levels
**Power ON ESP32-S3**, multimeter in **DC Voltage mode (20V range)**:

| Test Point | Expected Voltage | Meaning |
|------------|------------------|---------|
| **GPIO8** (SDA) vs GND | 3.3V (HIGH) | Pull-up working, bus idle |
| **GPIO9** (SCL) vs GND | 3.3V (HIGH) | Pull-up working, bus idle |
| **GPIO8** → **GPIO9** voltage difference | <0.1V | Both pulled up equally |

✅ **Pass criteria**: Both lines HIGH at 3.3V when idle

⚠️ **If voltage is ~1.5V**: Likely a device is holding the line LOW (bus stuck)
⚠️ **If voltage is ~0V**: Short to ground or device pulling LOW continuously

### 6.2 Check PCA9548A Channel 5 Idle State
**ESP32-S3 ON**, measure downstream bus after mux initialization attempt:

| Test Point | Expected Voltage | Meaning |
|------------|------------------|---------|
| PCA9548A **SD5** vs GND | 3.3V (HIGH) | CH5 bus idle |
| PCA9548A **SC5** vs GND | 3.3V (HIGH) | CH5 bus idle |

⚠️ **Note**: These measurements are only valid if PCA9548A responds to I2C. If mux doesn't respond, downstream channels may not be activated.

---

## Troubleshooting Checklist

### If I2C scanner shows "No device at 0x70 (PCA9548A)"

1. ✅ Check PCA9548A VCC/GND power connections (Step 1.2)
2. ✅ Check GPIO8/9 → PCA9548A SDA/SCL continuity (Step 3.1)
3. ✅ Check 2kΩ pull-up resistors (Step 2.1)
4. ✅ Check PCA9548A /RST pin connection to 3.3V (Step 4.1)
5. ✅ Measure GPIO8/9 idle voltage = 3.3V (Step 6.1)

### If I2C scanner shows PCA9548A OK but BNO085 fails

1. ✅ Check BNO085 VCC/GND power connections (Step 5.1)
2. ✅ Check PCA9548A SD5/SC5 → BNO085 SDA/SCL (Step 5.2)
3. ✅ Check BNO085 PS0/PS1 address pins (Step 5.3)
4. ✅ Verify PCA9548A CH5 is being selected in firmware (`MuxChannels::BNO085_IMU` = 5)

### If all wiring tests pass but I2C still fails

**Likely causes**:
- Device not powered (check module's own power LED if present)
- Wrong I2C address (verify with datasheet)
- Device firmware requires initialization sequence before responding
- I2C speed mismatch (try 100kHz instead of 400kHz)
- Damaged component (try replacement module)

---

## Quick Reference: Your Current Wiring

| Connection | Wire Color (if labeled) | Test Point |
|------------|------------------------|------------|
| ESP32-S3 **GPIO8** → PCA9548A **SDA** | — | Step 3.1 |
| ESP32-S3 **GPIO9** → PCA9548A **SCL** | — | Step 3.1 |
| 3.3V → 2kΩ → GPIO8 | — | Step 2.1 |
| 3.3V → 2kΩ → GPIO9 | — | Step 2.1 |
| PCA9548A **SD5** → BNO085 **SDA** | — | Step 5.2 |
| PCA9548A **SC5** → BNO085 **SCL** | — | Step 5.2 |
| BNO085 **INT** → GPIO21 | — | Step 5.4 |

---

## Multimeter Safety Reminder

| Mode | When to Use | Power State |
|------|-------------|-------------|
| **Resistance** | Continuity, pull-up verification | **OFF** |
| **DC Voltage** | Power rails, idle bus voltage | **ON** |
| **Current** | Power consumption (advanced) | **ON** (series connection) |

⚠️ **Never measure resistance on powered circuits** — it can damage the multimeter or circuit.

---

## Next Steps After Wiring Verification

1. If all wiring tests pass → I2C issue is likely software/config
2. If any test fails → Fix wiring first before firmware debugging
3. After wiring fixed → Re-run I2C scanner and SensorManager init
4. If PCA9548A still fails → Simulation mode allows development to continue

---

Generated: 2026-05-20
Project: EdgeAI Data Glove V3
Hardware: ESP32-S3-DevKitC-1 N16R8 + PCA9548A + BNO085