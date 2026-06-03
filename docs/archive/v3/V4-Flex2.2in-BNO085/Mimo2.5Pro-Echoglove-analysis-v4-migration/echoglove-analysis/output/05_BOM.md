# EchoGlove V4 Bill of Materials (BOM)

> **Version:** 4.0  
> **Date:** 2026-06-01  

---

## 1. Component Table

| # | Component | Specification | Qty | Unit Price (USD) | Total | Source |
|---|---|---|---|---|---|---|
| 1 | ESP32-S3-WROOM-1 | 8MB PSRAM, 8MB Flash, dual-core 240MHz | 1 | $3.50 | $3.50 | [LCSC](https://www.lcsc.com/) / [Taobao](https://www.taobao.com/) |
| 2 | BNO085 IMU Breakout | 9-DOF, I2C 0x4B, SH2 Game RV mode | 1 | $8.00 | $8.00 | [SparkFun](https://www.sparkfun.com/) / [DigiKey](https://www.digikey.com/) |
| 3 | ADS1115 ADC Module | 16-bit, I2C, 860SPS, PGA | 2 | $2.00 | $4.00 | [LCSC](https://www.lcsc.com/) / [Taobao](https://www.taobao.com/) |
| 4 | Flex Sensor 2.2" | SpectraFlex or equivalent, 25–125kΩ | 5 | $2.50 | $12.50 | [Spectra Symbol](https://www.spectrasymbol.com/) / Taobao |
| 5 | Resistor 47kΩ | 0805, ±1%, 1/8W | 5 | $0.02 | $0.10 | [LCSC](https://www.lcsc.com/) |
| 6 | Capacitor 100nF | 0805, 16V, X7R | 5 | $0.03 | $0.15 | [LCSC](https://www.lcsc.com/) |
| 7 | Resistor 4.7kΩ | 0805, ±5%, 1/8W (I2C pull-up) | 2 | $0.02 | $0.04 | [LCSC](https://www.lcsc.com/) |
| 8 | Resistor 10kΩ | 0805, ±5%, 1/8W (pull-up) | 2 | $0.02 | $0.04 | [LCSC](https://www.lcsc.com/) |
| 9 | LiPo Battery | 3.7V, 500mAh, JST-PH 2.0 | 1 | $4.00 | $4.00 | [Taobao](https://www.taobao.com/) |
| 10 | TP4056 Charger Module | Micro-USB/USB-C, 1A charge | 1 | $0.50 | $0.50 | [Taobao](https://www.taobao.com/) |
| 11 | WS2812B LED | 5050, addressable RGB | 1 | $0.15 | $0.15 | [LCSC](https://www.lcsc.com/) |
| 12 | Tactile Button | 6×6mm, 4-pin, through-hole | 1 | $0.05 | $0.05 | [LCSC](https://www.lcsc.com/) |
| 13 | MOSFET (AO3400) | N-channel, SOT-23, logic level | 1 | $0.10 | $0.10 | [LCSC](https://www.lcsc.com/) |
| 14 | Glove Substrate | Nylon/spandex, size M/L | 1 | $3.00 | $3.00 | [Taobao](https://www.taobao.com/) |
| 15 | Perfboard/PCB | 40×60mm, single-side | 1 | $1.00 | $1.00 | [Taobao](https://www.taobao.com/) |
| 16 | JST-PH Connector | 2-pin, male+female | 2 | $0.20 | $0.40 | [LCSC](https://www.lcsc.com/) |
| 17 | Silicone Wire | 26AWG, stranded, various colors | 2m | $0.50 | $1.00 | [Taobao](https://www.taobao.com/) |
| 18 | Heat Shrink Tube | 3mm, various colors | 0.5m | $0.20 | $0.20 | [Taobao](https://www.taobao.com/) |
| 19 | Velcro Strap | 20mm, adhesive back | 0.3m | $0.50 | $0.50 | [Taobao](https://www.taobao.com/) |
| | | | | **TOTAL** | **$39.13** | |

---

## 2. V3 vs V4 Cost Comparison

| Component | V3 Qty | V3 Price | V4 Qty | V4 Price | Delta |
|---|---|---|---|---|---|
| ESP32-S3-WROOM-1 | 1 | $3.50 | 1 | $3.50 | $0.00 |
| BNO085 breakout | 1 | $8.00 | 1 | $8.00 | $0.00 |
| TMAG5273 Hall sensor | 5 | $12.50 | 0 | $0.00 | **-$12.50** |
| N52 magnet beads (6mm) | 5 | $2.50 | 0 | $0.00 | **-$2.50** |
| TCA9548A MUX | 1 | $1.50 | 0 | $0.00 | **-$1.50** |
| Flex sensor 2.2" | 0 | $0.00 | 5 | $12.50 | **+$12.50** |
| ADS1115 breakout | 0 | $0.00 | 2 | $4.00 | **+$4.00** |
| Resistors (various) | 5 | $0.25 | 9 | $0.22 | -$0.03 |
| Capacitors | 5 | $0.25 | 5 | $0.15 | -$0.10 |
| LiPo battery | 1 | $4.00 | 1 | $4.00 | $0.00 |
| TP4056 charger | 1 | $0.50 | 1 | $0.50 | $0.00 |
| WS2812B LED | 1 | $0.30 | 1 | $0.15 | -$0.15 |
| Glove substrate | 1 | $3.00 | 1 | $3.00 | $0.00 |
| PCB/Perfboard | 1 | $1.00 | 1 | $1.00 | $0.00 |
| Connectors/wires | — | $2.50 | — | $2.10 | -$0.40 |
| **TOTAL** | | **$39.80** | | **$39.13** | **-$0.67** |

**Result: V4 is $0.67 cheaper than V3** despite adding ADS1115 modules, because removing Hall sensors, magnets, and MUX saves more than the flex sensors and ADCs cost.

---

## 3. 国产替代 vs Spectra Symbol Price Comparison

### 3.1 Flex Sensor Options

| Option | Brand | Length | Price/pc | Total (5 pcs) | Quality | Notes |
|---|---|---|---|---|---|---|
| **SpectraFlex** | Spectra Symbol (US) | 2.2" | $2.50 | $12.50 | ★★★★★ | Gold standard, 100K+ cycles |
| **SpectraFlex Long** | Spectra Symbol (US) | 4.5" | $3.50 | $17.50 | ★★★★★ | Better for longer fingers |
| **国产替代 A** | 深圳柔性传感器 | 2.2" | $1.00 | $5.00 | ★★★☆☆ | Budget option, ~50K cycles |
| **国产替代 B** | 东莞传感科技 | 2.2" | $1.50 | $7.50 | ★★★★☆ | Good quality, 80K+ cycles |
| **国产替代 C** | 苏州微纳传感 | 55mm | $0.80 | $4.00 | ★★★☆☆ | Very budget, ~30K cycles |

### 3.2 ADS1115 Module Options

| Option | Price/pc | Total (2 pcs) | Quality | Notes |
|---|---|---|---|---|
| **TI ADS1115IDR** (original) | $3.50 | $7.00 | ★★★★★ | TI original, LCSC |
| **GY-ADS1115 module** | $2.00 | $4.00 | ★★★★☆ | Generic breakout, Taobao |
| **国产 ADS1115 module** | $1.50 | $3.00 | ★★★☆☆ | Budget, may have noise |

### 3.3 Recommendation

| Budget | Flex Sensor | ADS1115 | Total BOM |
|---|---|---|---|
| **Premium** | SpectraFlex 2.2" ($12.50) | TI original ($7.00) | ~$43 |
| **Standard** (recommended) | 国产替代 B ($7.50) | GY-ADS1115 ($4.00) | ~$37 |
| **Budget** | 国产替代 C ($4.00) | 国产 ADS1115 ($3.00) | ~$33 |

---

## 4. Recommended Suppliers

### 4.1 Chinese Domestic (Taobao / LCSC)

| Supplier | Platform | Components | Lead Time |
|---|---|---|---|
| **LCSC (立创商城)** | lcsc.com | ESP32, ADS1115, passives, MOSFET | 2–5 days |
| **Taobao shops** | taobao.com | Flex sensors, battery, glove, wire | 3–7 days |
| **Bilibili DIY shops** | taobao.com | BNO085 breakout, ESP32 devkit | 3–5 days |
| **SZLCSC (深圳立创)** | szlcsc.com | PCB fabrication + assembly | 5–10 days |

### 4.2 International

| Supplier | Components | Lead Time |
|---|---|---|
| **DigiKey** | BNO085, TI ADS1115 | 3–7 days |
| **SparkFun** | BNO085 breakout | 3–7 days |
| **Spectra Symbol** | Flex sensors (official) | 5–10 days |
| **Mouser** | TI ADS1115, passives | 3–7 days |

### 4.3 Specific Part Numbers

| Component | LCSC Part # | Taobao Search |
|---|---|---|
| ESP32-S3-WROOM-1-N8R8 | C5316215 | "ESP32-S3 开发板" |
| ADS1115IDR | C81146 | "ADS1115 模块" |
| 0805 47kΩ resistor | C17414 | "0805 47K 电阻" |
| 0805 100nF capacitor | C49678 | "0805 100NF 电容" |
| AO3400 MOSFET | C15127 | "AO3400 MOS管" |
| TP4056 module | — | "TP4056 充电模块" |
| Flex sensor 2.2" | — | "弯曲传感器 2.2寸" |
| BNO085 breakout | — | "BNO085 九轴模块" |

---

## 5. Assembly Notes

### 5.1 Flex Sensor Mounting

1. **Orientation:** Flex sensors mount on the **dorsal (back)** side of each finger
2. **Adhesive:** Use thin double-sided tape or silicone adhesive
3. **Routing:** Wire runs along the finger to the back of the hand PCB
4. **Strain relief:** Leave 2mm slack at each end to prevent wire fatigue
5. **Avoid:** Do not crease the sensor — minimum bend radius is 5mm

### 5.2 Voltage Divider Placement

1. Mount 47kΩ resistors on the perfboard near the ADS1115 inputs
2. Keep analog traces short (<5cm) to minimize noise
3. Add 100nF capacitor at each ADS1115 input for noise filtering
4. Route analog traces away from digital signals (I2C, SPI)

### 5.3 I2C Bus Wiring

1. Use short, direct connections from ESP32 to ADS1115 and BNO085
2. Place 4.7kΩ pull-up resistors near the ESP32 (not at the far end)
3. Keep total I2C bus length <30cm
4. Use twisted pair for SDA/SCL if running >10cm

### 5.4 Battery & Power

1. TP4056 module handles charging; connect battery to B+ and B-
2. Output to ESP32 3.3V regulator or directly to 3.3V rail (if using module with regulator)
3. Add 100µF electrolytic capacitor on battery rail for current spikes
4. MOSFET (GPIO47) controls flex sensor power for sleep mode

### 5.5 Glove Integration

1. Sew flex sensors into finger channels on the dorsal side
2. PCB + battery mount on the back of the hand (wrist area)
3. Use velcro straps for secure but removable mounting
4. Ensure flex sensors can bend freely without binding

---

*End of BOM*
