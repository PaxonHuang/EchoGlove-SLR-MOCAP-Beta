# EchoGlove V6.0 Wiring Diagram

> **Branch**: `feature/v6-dual-s3p4-flex-lsm6dsv16x` (active)
> **Date**: 2026-06-23 (updated 2026-07-10)
> **MCU**: ESP32-S3-DevKitC-1 N16R8 (per glove)
> **IMU**: BNO085 (0x4B) → ST LSM6DSV16X (0x6A) — **driver not implemented yet (IMU=zeros in current code); flex on internal ADC1 is implemented**

---

## 1. Per-Glove I2C Bus Diagram

Flat bus topology, no MUX. In V6 the I²C bus carries a single device (LSM6DSV16X); the two ADS1115 ADCs were removed and flex sensors now use the ESP32-S3 internal ADC1 (see `07_internal_adc_migration.md`). GPIO8 (SDA) / GPIO9 (SCL) at 400 kHz.

```
                        ESP32-S3-DevKitC-1 N16R8
                      ┌──────────────────────────────┐
                      │                                │
                      │   GPIO8 (SDA) ──┬──── 4.7kΩ pull-up ── 3.3V
                      │                  │
                      │   GPIO9 (SCL) ──┬┘
                      │                  │
                      │              4.7kΩ (pull-up to 3.3V)
                      │                  │
                      └──────────────────┼─┘
                                         │
                  ┌──────────────────────┘
                  │
          ┌───────┴───────────────┐
          │  LSM6DSV16X           │
          │  (IMU)                │
          │  ADDR = 0x6A          │
          │  (SDO/SA0 = GND)      │
          └───────────────────────┘

  I2C Address Map (V6 — single device):
  ┌───────────────────┬───────────┬────────────────────────┐
  │ Device            │ Address   │ Notes                  │
  ├───────────────────┼───────────┼────────────────────────┤
  │ LSM6DSV16X        │ 0x6A      │ SDO/SA0=GND            │
  └───────────────────┴───────────┴────────────────────────┘

  V6: ADS1115×2 removed — flex on internal ADC1 (GPIO1-5), see 07.
  (V5, removed: BNO085 @ 0x4B; ADS1115 @ 0x48 / 0x49.)
```

---

## 2. LSM6DSV16X Pin Connection Table

14-pin LGA package. All references are to ESP32-S3-DevKitC-1 N16R8 GPIO numbers.

```
  LSM6DSV16X 14-pin LGA (top view, pin 1 indicator at top-left)
  ┌─────────────────────┐
  │ 1 (CS)    14 (VDD)  │
  │ 2 (SA0)   13 (GND)  │
  │ 3 (RES)   12 (VDDIO)│
  │ 4 (GND)   11 (RES)  │
  │ 5 (SDA)   10 (RES)  │
  │ 6 (SCL)    9 (INT1) │
  │ 7 (RES)    8 (INT2) │
  └─────────────────────┘
```

| Pin | Name      | Connect To          | Voltage | Notes                                  |
|-----|-----------|---------------------|---------|----------------------------------------|
| 1   | CS        | 3.3V                | 3.3V    | HIGH = I2C mode (CS bar active LOW)    |
| 2   | SDO/SA0   | GND                 | 0V      | I2C address = 0x6A (LOW)               |
| 3   | Reserved  | NC                  | --      | Leave unconnected                      |
| 4   | GND       | GND                 | 0V      | Ground                                 |
| 5   | SDA/SDI   | ESP32-S3 GPIO8      | 3.3V    | I2C data, 4.7k pull-up to 3.3V        |
| 6   | SCL/SCLK  | ESP32-S3 GPIO9      | 3.3V    | I2C clock, 4.7k pull-up to 3.3V       |
| 7   | Reserved  | NC                  | --      | Leave unconnected                      |
| 8   | INT2      | NC or GPIO11        | 3.3V    | Optional: freefall/wakeup interrupt    |
| 9   | INT1      | ESP32-S3 GPIO10     | 3.3V    | Optional: data-ready / significant motion |
| 10  | Reserved  | NC                  | --      | Leave unconnected                      |
| 11  | Reserved  | NC                  | --      | Leave unconnected                      |
| 12  | VDDIO     | 3.3V                | 3.3V    | I/O supply voltage                     |
| 13  | GND       | GND                 | 0V      | Ground (connect both GND pins)         |
| 14  | VDD       | 3.3V                | 3.3V    | Main supply voltage                    |

**Power Decoupling**: Place 100nF ceramic capacitor between VDD (pin 14) and GND (pin 13), as close to the module as possible. Place 10nF between VDDIO (pin 12) and GND (pin 4).

---

## 3. Flex Sensor Circuit

Each flex sensor forms a voltage divider with a 47k ohm pull-down resistor. Five channels total, read by the ESP32-S3 internal ADC1 (GPIO1-5). The voltage divider is unchanged from V5; only the ADC front-end changed (ADS1115 removed). See `07_internal_adc_migration.md`.

```
  Voltage Divider (per channel):

       3.3V
        │
        │
   ┌────┴────┐
   │  Flex   │   Variable resistance: ~25k ohm (straight) to ~125k ohm (bent)
   │  Sensor │
   └────┬────┘
        │
        ├──────────── To ESP32-S3 ADC1 input (GPIO1-5)
        │
   ┌────┴────┐
   │  47kΩ   │   Fixed pull-down resistor (1% tolerance recommended)
   │  (fixed)│
   └────┬────┘
        │
       GND

  Voltage Range:
  ┌──────────────────┬──────────────┬──────────────┐
  │ Flex State       │ Resistance   │ ADC Voltage  │
  ├──────────────────┼──────────────┼──────────────┤
  │ Straight (0 deg) │ ~25k ohm     │ ~1.13V       │
  │ 45 deg           │ ~47k ohm     │ ~1.65V       │
  │ Fully bent 90deg │ ~125k ohm    │ ~2.39V       │
  └──────────────────┴──────────────┴──────────────┘

  V_out = 3.3V * (47k / (R_flex + 47k))

  Internal ADC1 settings (V6):
    - ADC_ATTEN_DB_12  (0-~2.5V usable range w/ 12dB attenuation)
    - analogReadMilliVolts() for calibrated mV
    - N=16 oversample per channel for noise reduction
```

---

## 4. Flex Sensor → ADC1 Channel Assignment (V6)

V6 removed the two ADS1115 ADCs. Flex sensors connect directly to ESP32-S3 ADC1 GPIO pins. The voltage divider (flex + 47kΩ pull-down) is unchanged from V5. See `07_internal_adc_migration.md`.

| Finger  | ADC1 Channel | GPIO | Connect To        | Notes                      |
|---------|--------------|------|-------------------|----------------------------|
| Thumb   | ADC1_CH0     | GPIO1| Flex divider out  | analogReadMilliVolts, N=16 |
| Index   | ADC1_CH1     | GPIO2| Flex divider out  | analogReadMilliVolts, N=16 |
| Middle  | ADC1_CH2     | GPIO3| Flex divider out  | analogReadMilliVolts, N=16 |
| Ring    | ADC1_CH3     | GPIO4| Flex divider out  | analogReadMilliVolts, N=16 |
| Pinky   | ADC1_CH4     | GPIO5| Flex divider out  | analogReadMilliVolts, N=16 |

**Internal ADC1 Configuration**:
- Attenuation: `ADC_ATTEN_DB_12` (0-~2.5V usable range)
- Read: `analogReadMilliVolts(pin)` for factory-calibrated mV
- Oversample: N=16 samples averaged per channel per 100Hz tick
- Driver: `InternalADCManager.h` (implements `IFlexSensor`), replaces V5 `ADS1115Manager.h`
- Wiring: flex divider output → GPIO1-5 directly (no I²C, no external ADC chip)

---

## 5. S3 ↔ P4 Direct UART Wiring (V5.3 Wired Dev Path — Designed, code-pending)

> **Status (2026-07-10, code-verified)**: **Designed, not yet implemented in firmware.** S3 currently uses ESP-NOW broadcast; the `WIRED_UART` compile flag does **not exist in code yet**. The on-board C6 is an ESP-Hosted Wi-Fi/BT co-processor (pre-flashed slave firmware, SDIO bus) and does **not** support ESP-NOW pass-through, so the planned dev path bypasses C6: S3 → direct UART → P4. C6 is deferred to a future Wi-Fi integration phase. See `docs/superpowers/specs/2026-07-08-s3-p4-wired-uart-design.md`.

The ESP32-S3 glove transmits GlovePacket data directly to the ESP32-P4 base station over UART at 2 Mbps, reusing the same CRC-16/MODBUS framing as the (deferred) C6→P4 path. This is a **unidirectional** link (S3 TX → P4 RX) — no back-channel is needed since the P4 only consumes telemetry.

```
  ESP32-S3-DevKitC-1 N16R8            ESP32-P4 EV Board
  ┌──────────────────┐                ┌──────────────────┐
  │                  │                │                  │
  │  GPIO6  (TX)     ├────────────────┤ GPIO38 (RX)      │
  │                  │    2 Mbps 8N1  │                  │
  │  GND             ├────────────────┤ GND              │
  │                  │                │                  │
  └──────────────────┘                └──────────────────┘
   (per glove, one TX line)            (UART0 RX, already configured)
```

| Signal | S3 GPIO | P4 GPIO | Direction | Baud Rate | Config |
|--------|---------|---------|-----------|-----------|--------|
| TX     | GPIO6   | GPIO38  | S3 → P4   | 2,000,000 | 8N1   |
| GND    | GND     | GND     | —         | —         | Common ground required |

**S3 GPIO6 selection rationale** (audited 2026-07-09):
- GPIO6 is FREE on the S3 DevKit (not strapping, not flash, not USB, not PSRAM, not ADC1).
- ADC1 uses GPIO1–5 (flex); I²C uses GPIO8–9; flash uses GPIO11–17; USB CDC uses GPIO19–20; OPI PSRAM uses GPIO26–32.
- GPIO6/7 are the cleanest free pair for UART1 (TX=6, optional RX=7).

**Dual-hand wiring (no bus contention)**:
Each glove gets its own dedicated TX line into a separate P4 UART — physical isolation eliminates any bus contention:

```
  S3 (Left)  GPIO6 (TX) ──► P4 GPIO38 (UART0 RX)
  S3 (Right) GPIO6 (TX) ──► P4 GPIO?? (UART2 RX, TBD)
```

**Notes**:
- Common ground is mandatory for UART at 2 Mbps.
- No external pull-ups needed for UART lines (TX-only, P4 RX has internal config).
- 73-byte frame = 2 magic + 69 payload + 2 CRC (same `uart_frame.h` protocol as C6 path).
- **Target**: S3 sends ESP-NOW **and** UART in parallel (compile flag `WIRED_UART=1`, planned). UART is the wired fallback. **Currently only ESP-NOW is implemented** — wired UART TX is pending `UARTTransmitter.h` (TDD, see plan `2026-07-09-s3-p4-wired-uart.md`).

---

## 6. C6 ↔ P4 UART Wiring (Deferred — Production Wi-Fi Path)

> **Status (2026-07-09)**: **Deferred.** Kept for reference; the on-board C6 cannot run our mock-ESP-NOW firmware (ESP-Hosted doesn't support ESP-NOW). In production, C6 will serve as a Wi-Fi co-processor via ESP-Hosted, not as a UART relay. See [[p4-ev-board-c6-esp-hosted]].

The (future production) ESP32-C6 co-processor relays Wi-Fi-received glove data to the ESP32-P4 main processor via 2 Mbps UART with CRC-16/MODBUS framing. This section is retained for the eventual Wi-Fi integration phase.

```
  ESP32-C6-MINI-1                    ESP32-P4
  ┌──────────────┐                  ┌──────────────┐
  │              │                  │              │
  │  GPIO43 (TX) ├──────────────────┤ GPIO38 (RX)  │
  │              │    2 Mbps 8N1    │              │
  │  GPIO44 (RX) ├──────────────────┤ GPIO37 (TX)  │
  │              │                  │              │
  │  GND         ├──────────────────┤ GND          │
  │              │                  │              │
  └──────────────┘                  └──────────────┘

  UART Frame (73 bytes total):
  ┌──────┬──────┬─────────────────┬─────────┬─────────┐
  │ SYNC │ SYNC │    PAYLOAD      │ CRC_L   │ CRC_H   │
  │ 0xAA │ 0x55 │  69 bytes       │ MODBUS  │ MODBUS  │
  └──────┴──────┴─────────────────┴─────────┴─────────┘
  Total: 1 + 1 + 69 + 1 + 1 = 73 bytes
```

| Signal | C6 GPIO | P4 GPIO | Direction | Baud Rate | Config |
|--------|---------|---------|-----------|-----------|--------|
| TX     | GPIO43  | GPIO38  | C6 --> P4 | 2,000,000 | 8N1   |
| RX     | GPIO44  | GPIO37  | P4 --> C6 | 2,000,000 | 8N1   |
| GND    | GND     | GND     | --        | --        | Common ground required |

**Notes**:
- TX/RX are crossed: C6 TX goes to P4 RX, and vice versa.
- Common ground is mandatory for UART communication at 2 Mbps.
- No external pull-ups needed for UART lines.

---

## 7. P4 Base Station Connections

```
  ┌─────────────────────────────────────────────────────────────────┐
  │                    ESP32-P4 Base Station                        │
  │                                                                 │
  │   ┌──────────┐    ┌──────────┐    ┌──────────┐                 │
  │   │ S3 UART  │    │ MIPI-DSI │    │ I2S Audio│                 │
  │   │ GPIO37/38│    │ Display  │    │ ES8311   │                 │
  │   └──────────┘    └──────────┘    └──────────┘                 │
  │                                                                 │
  │   ┌──────────┐    ┌──────────┐    ┌──────────┐                 │
  │   │ USB HS   │    │ SD Card  │    │ SPI Flash│                 │
  │   │ OTG      │    │ Slot     │    │ (prog)   │                 │
  │   └──────────┘    └──────────┘    └──────────┘                 │
  └─────────────────────────────────────────────────────────────────┘
```

### 7.1 MIPI-DSI Display

| Signal       | P4 GPIO / Pin | Notes                          |
|--------------|---------------|--------------------------------|
| DSI CLK+     | DSI_CLK_P     | Differential clock pair        |
| DSI CLK-     | DSI_CLK_N     |                                |
| DSI D0+      | DSI_D0_P      | Data lane 0                    |
| DSI D0-      | DSI_D0_N      |                                |
| DSI D1+      | DSI_D1_P      | Data lane 1 (if 2-lane)        |
| DSI D1-      | DSI_D1_N      |                                |
| Backlight    | GPIO (PWM)    | Brightness control             |
| Reset        | GPIO          | Display reset (active LOW)     |
| Power        | 3.3V / 1.8V   | Per display module spec        |

### 7.2 I2S Audio (ES8311 Codec)

| Signal   | P4 GPIO       | Direction | Notes                  |
|----------|---------------|-----------|------------------------|
| I2S_BCK  | I2S_BCLK      | P4 --> Codec | Bit clock          |
| I2S_WS   | I2S_WS        | P4 --> Codec | Word select (LRCK)  |
| I2S_DOUT | I2S_DO        | P4 --> Codec | Data to speaker     |
| I2S_DIN  | I2S_DI        | Codec --> P4 | Data from mic       |
| I2C SDA  | I2C_SDA       | P4 --> Codec | Codec config (I2C)  |
| I2C SCL  | I2C_SCL       | P4 --> Codec | Codec config (I2C)  |

### 7.3 USB High-Speed OTG

| Signal   | P4 Pin   | Connect To    | Notes                    |
|----------|----------|---------------|--------------------------|
| USB D+   | USB_DP   | PC USB port   | High-Speed 480 Mbps      |
| USB D-   | USB_DM   | PC USB port   |                          |
| USB VBUS | VBUS     | 5V supply     | Or from PC USB port      |
| USB GND  | GND      | Common ground |                          |
| USB ID   | GND      | GND           | OTG host mode (tie to GND) |

### 7.4 SD Card Slot

| Signal    | P4 GPIO    | Notes                  |
|-----------|------------|------------------------|
| SD_CMD    | SD_CMD     | MOSI / command         |
| SD_CLK    | SD_CLK     | Clock                  |
| SD_D0     | SD_D0      | MISO / data line 0     |
| SD_D1     | SD_D1      | Data line 1 (4-bit)    |
| SD_D2     | SD_D2      | Data line 2 (4-bit)    |
| SD_D3     | SD_D3      | Data line 3 / CS       |
| SD_VDD    | 3.3V       | Power supply           |
| SD_GND    | GND        | Ground                 |

---

## 8. Critical Wiring Notes

### 8.1 Voltage Levels

- **Entire I2C bus is 3.3V.** Never connect 5V to any I2C pin.
- LSM6DSV16X VDD and VDDIO must both be 3.3V.
- Flex sensor voltage divider output is 0-3.3V range, within ADC1 usable range with `ADC_ATTEN_DB_12` (V6 removed the ADS1115; see `07`).

### 8.2 I2C Pull-ups

- Required: 4.7k ohm pull-up resistors on both SDA (GPIO8) and SCL (GPIO9) to 3.3V.
- If using a breakout board with built-in pull-ups, verify they are 4.7k ohm and not duplicated (parallel pull-ups reduce effective resistance).
- Total bus capacitance should stay below 400pF for 400 kHz operation.

### 8.3 LSM6DSV16X CS and SDO/SA0 Behavior

- **CS (pin 1) = 3.3V**: Forces I2C mode. If CS is left floating, the chip may enter SPI mode on power-up, causing I2C communication failure.
- **SDO/SA0 (pin 2) = GND**: Sets I2C address to 0x6A. If connected to 3.3V, address becomes 0x6B.
- CS and SDO/SA0 are latched at power-up. Changing them after power-on has no effect. Power cycle the module if changing I2C/SPI mode or address.

### 8.4 BNO085 PS0/PS1 (Historical -- No Longer Applies)

The V6 design removes BNO085 entirely. The following notes are retained for reference during migration:

- BNO085 PS0=GND selected I2C mode. PS0=3.3V would select SPI mode and could damage the module.
- BNO085 ADO=3.3V set address to 0x4B. ADO=GND would set 0x4A.
- BNO085 RST had a strong internal pull-up; GPIO10 could not reliably pull it LOW.

### 8.5 Flex Sensor Mounting

- Flex sensors should be mounted on the glove with the conductive side facing the finger.
- Solder wires to the flex sensor pads; do not use crimp connectors (unreliable at flex sensor thickness).
- Keep wire runs from flex sensor to the ESP32-S3 ADC1 GPIO pin as short as possible to reduce noise.
- Consider 100nF ceramic capacitor at each ADC1 analog input (GPIO1-5) for noise filtering.

### 8.6 Common Ground

- All devices (both gloves, P4) must share a common ground reference.
- When connecting S3 to P4 via UART (V5.3 wired path), the GND wire is mandatory.
- ESP-NOW (wireless, when re-enabled via a standalone C6 or ESP-Hosted Wi-Fi) does not require a shared ground between gloves.

### 8.7 Power Budget (Per Glove)

| Component     | Typical Current | Max Current | Voltage |
|---------------|-----------------|-------------|---------|
| ESP32-S3      | 80 mA           | 500 mA      | 3.3V    |
| LSM6DSV16X    | 0.55 mA         | 1.0 mA      | 3.3V    |
| Flex sensors  | ~0.07 mA each   | ~0.14 mA    | 3.3V    |
| **Total**     | **~81 mA**      | **~501 mA** | **3.3V**|

> V6: ADS1115 #1/#2 rows removed (ADC now internal to ESP32-S3). See `07_internal_adc_migration.md`.

Recommend powering from a 3.7V LiPo with 3.3V LDO regulator (e.g., AP2112K-3.3).

---

## 9. BNO085 --> LSM6DSV16X Migration Pin Mapping

This table shows what changed between V5 (BNO085) and V6 (LSM6DSV16X) on the ESP32-S3 glove.

| ESP32-S3 Pin | V5: BNO085         | V6: LSM6DSV16X     | Change?   |
|--------------|--------------------|--------------------|-----------|
| GPIO8 (SDA)  | I2C SDA            | I2C SDA            | No change |
| GPIO9 (SCL)  | I2C SCL            | I2C SCL            | No change |
| GPIO10       | RST (attempted)    | INT1 (optional)    | **Changed** -- was RST (non-functional), now interrupt input |
| GPIO11       | Not used           | INT2 (optional)    | New (optional) |
| 3.3V         | VCC                | VDD + VDDIO + CS   | **Changed** -- now powers 3 pins instead of 1 |
| GND          | GND                | GND (pin 4 + pin 13) | **Changed** -- 2 GND pins instead of 1 |

### Key Differences

| Aspect             | V5: BNO085                    | V6: LSM6DSV16X                  |
|--------------------|-------------------------------|----------------------------------|
| I2C Address        | 0x4B (ADO=HIGH)              | 0x6A (SA0=LOW)                  |
| Mode Select Pins   | PS0=GND, PS1=GND (I2C)      | CS=3.3V (I2C)                   |
| Address Select Pin  | ADO=3.3V                     | SDO/SA0=GND                     |
| Reset Pin          | RST=GPIO10 (non-functional)  | No RST pin (power cycle only)   |
| Interrupt Pins     | INT=floating (not used)      | INT1=GPIO10, INT2=optional      |
| Package            | 28-pin LGA                   | 14-pin LGA (smaller)            |
| Power Pins         | VCC + GND                    | VDD + VDDIO + 2x GND           |
| Decoupling         | 100nF on VCC                 | 100nF on VDD + 10nF on VDDIO   |

### GPIO10 Reassignment

On V5, GPIO10 was connected to BNO085 RST but could not reliably pull it LOW due to the strong internal pull-up on the BNO085 RST pin. This connection was effectively non-functional.

On V6, GPIO10 is connected to LSM6DSV16X INT1, which is an active-high interrupt output from the IMU. This can be used for:
- Data-ready notification (avoids polling)
- Significant motion detection
- Wake-on-motion to save power

If INT1 is not needed, GPIO10 can be left unconnected and the LSM6DSV16X INT1 pin left floating or tied to GND via 10k ohm resistor.

---

## Appendix: Complete Per-Glove Wiring Summary

```
  ESP32-S3-DevKitC-1 N16R8
  ┌─────────────────────────────────────────────┐
  │                                             │
  │  3.3V ──┬──────────┬──────────┬─────────┐   │
  │         │          │          │         │   │
  │        100nF      10nF    4.7kΩ x2     │   │
  │         │          │          │         │   │
  │  GND ───┼────┬─────┼──────────┼─────────┤   │
  │         │    │     │          │         │   │
  │         │    │     │     ┌────┴────┐    │   │
  │         │    │     │     │ Pull-ups │    │   │
  │         │    │     │     └────┬────┘    │   │
  │         │    │     │          │         │   │
  │  GPIO8 (SDA)─┼─────┼──────────┘         │   │
  │  GPIO9 (SCL)─┼─────┘                    │   │
  │         │    │                           │   │
  │         │    ├── LSM6DSV16X VDD (pin14)  │   │
  │         │    ├── LSM6DSV16X VDDIO(pin12) │   │
  │         │    ├── LSM6DSV16X CS (pin1)    │   │
  │         │    │                           │   │
  │         ├─── ├── LSM6DSV16X GND (pin4)   │   │
  │         ├─── ├── LSM6DSV16X GND (pin13)  │   │
  │         │    │                           │   │
  │  GPIO10 ──────── LSM6DSV16X INT1 (pin9)  │   │
  │                                             │
  │  Flex Sensors (5x) → ADC1 (V6, no ADS1115): │
  │    Thumb  --> GPIO1 (ADC1_CH0)              │
  │    Index  --> GPIO2 (ADC1_CH1)              │
  │    Middle --> GPIO3 (ADC1_CH2)              │
  │    Ring   --> GPIO4 (ADC1_CH3)              │
  │    Pinky  --> GPIO5 (ADC1_CH4)              │
  │    (47kΩ pull-down to GND, divider unchanged)│
  │                                             │
  │  UART1 TX → P4 (V5.3 wired dev path):       │
  │    GPIO6 (TX) --> P4 GPIO38 (RX), 2Mbps     │
  │    GND ---------> P4 GND                    │
  │                                             │
  └─────────────────────────────────────────────┘
```
