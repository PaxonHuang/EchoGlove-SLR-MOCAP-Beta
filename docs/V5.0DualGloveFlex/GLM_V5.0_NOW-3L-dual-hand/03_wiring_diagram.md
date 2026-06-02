# EchoGlove V5.0 接线图与注意事项

> V5.0与V4.0接线完全相同，仅接收器增加状态LED指示Tier级别

---

## 1. 单手套完整接线图（与V4.0相同）

```
                    ESP32-S3 N16R8
                   ┌──────────────────┐
                   │                  │
    BNO085 SDA ────┤ GPIO8 (SDA)      │
    BNO085 SCL ────┤ GPIO9 (SCL)      │
                   │                  │       USB-C
    ADS1115#1 SDA──┤ GPIO8 (共用SDA)  │───────┤ (烧录/调试)
    ADS1115#1 SCL──┤ GPIO9 (共用SCL)  │       │
                   │                  │
    ADS1115#2 SDA──┤ GPIO8 (共用SDA)  │
    ADS1115#2 SCL──┤ GPIO9 (共用SCL)  │
                   │                  │
    LED_STATUS ────┤ GPIO2            │
    BTN_CALIBRATE──┤ GPIO1            │
                   │                  │
    VBAT_SENSE ────┤ GPIO4 (ADC)      │
                   │                  │
                   │  ESP-NOW Antenna │── 内置2.4GHz
                   └──────────────────┘
                          │
                    I2C Bus (100kHz)
                    4.7kΩ上拉至3.3V
                          │
        ┌─────────────────┼──────────────────┐
        │                 │                  │
   ┌────┴────┐      ┌────┴────┐       ┌─────┴─────┐
   │ ADS1115 │      │ ADS1115 │       │  BNO085   │
   │ 0x48    │      │ 0x49    │       │  0x4B     │
   │ (Ch0-3) │      │ (Ch0-1) │       │  (GRV)    │
   └────┬────┘      └────┬────┘       └───────────┘
        │                 │
   AIN0 AIN1 AIN2 AIN3   AIN0 AIN1
    │    │    │    │       │    │
   Flex0 Flex1 Flex2 N/C  Flex3 Flex4
   (拇) (食) (中)       (无名) (小)

   Flex传感器分压电路:
   3.3V → Flex Sensor → ├→ ADS1115 AINx
                          │
                       10kΩ → GND
```

---

## 2. V5.0接收器接线（新增状态LED）

```
    接收器 ESP32-S3 N16R8
    ┌──────────────────┐
    │                  │
    │  ESP-NOW 接收    │ ← 左手套数据
    │  (内置2.4GHz)    │ ← 右手套数据
    │                  │
    │  SYNC_TICK 广播  │ → 左右手套
    │                  │
    │  USB-C           │ → PC (串口 115200bps)
    │                  │
    │  GPIO2 ──────[R330Ω]──── WS2812B LED  │  ← V5.0新增
    │                  │     颜色指示:
    │  GPIO3 ←────── BTN │   绿=Tier1  黄=Tier2  红=Tier3
    └──────────────────┘

    ⚠️ 接收器无需I2C传感器
    ⚠️ USB供电(5V→AMS1117→3.3V)
    ⚠️ PSRAM(8MB)用于Tier2模型推理
```

---

## 3. 电源系统（与V4.0相同）

```
    3.7V Li-Po (500mAh)
         │
         ├──→ TP4056 充电保护 → USB-C (充电口)
         │           │
         │       AMS1117-3.3
         │           │
         │       3.3V 系统电源
         │       ┌───┼───────┐
         │    ESP32-S3  ADS1115×2  BNO085
         │
         └──→ VBAT_SENSE (GPIO4 + 分压电阻)
```

---

## 4. ADS1115地址配置（与V4.0相同）

```
    ADS1115 #1: ADDR→GND = 0x48 (Flex 0-2)
    ADS1115 #2: ADDR→VDD = 0x49 (Flex 3-4)
    BNO085:     PS0=VDD  = 0x4B
    → 三片设备无地址冲突 ✅
```

---

## 5. V5.0新增接线注意事项

### 5.1 接收器Tier2推理相关

| 注意事项 | 说明 |
|----------|------|
| **PSRAM必须使能** | PlatformIO中`CONFIG_SPIRAM=y`，Tier2模型需8MB PSRAM |
| **Flash分区** | 需分配≥512KB给模型数据，partition table需调整 |
| **PSRAM时序** | ESP32-S3 PSRAM默认40MHz，Tier2推理时可提至80MHz |
| **USB CDC** | 使用USB-Serial-JTAG或外部CH340N，确保115200稳定 |
| **LED指示** | WS2812B需5V信号，3.3V GPIO需电平转换或用普通LED替代 |

### 5.2 Tier2模型部署要点

| 要点 | 说明 |
|------|------|
| 模型格式 | TFLite Micro int8量化 |
| Arena大小 | ~60KB PSRAM |
| 推理线程 | 独立FreeRTOS任务，优先级低于ESP-NOW接收 |
| 帧率降级 | Tier2推理时ESP-NOW接收仍100Hz，推理可降至20Hz |
| 看门狗 | 模型推理可能触发看门狗，需配置task watchdog超时>1s |

### 5.3 通用注意事项（与V4.0相同）

| 项目 | 要点 |
|------|------|
| I2C时钟 | 强制100kHz，不要400kHz |
| Flex安装 | 沿手指背侧，弯曲面朝手心 |
| Flex引线 | 根部加热缩管保护，最易折断 |
| BNO085 | 手腕外侧偏上，远离ESP32天线≥2cm |
| ESP-NOW | 同一WiFi通道，广播模式 |
| PCB | 4层板，天线净空≥15mm |
