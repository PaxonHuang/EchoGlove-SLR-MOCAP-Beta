# EchoGlove V6.0 -- System Architecture Diagrams

> **Version**: V6.0
> **Date**: 2026-06-23 (updated 2026-07-09)
> **Status**: Draft
> **Supersedes**: V5.0 DualGloveFlex + V5.2 P4 Base Station

> **⚠️ Architecture Update (2026-07-09, V5.3 wired dev path)**: The on-board C6 is an ESP-Hosted Wi-Fi/BT co-processor (SDIO bus, pre-flashed slave firmware) and does **not** support ESP-NOW pass-through. During development, S3 gloves connect **directly** to the P4 over UART (2 Mbps, CRC-16/MODBUS framing), bypassing C6. C6 is deferred to a future Wi-Fi integration phase. The diagrams below still depict the original C6→P4 ESP-NOW relay topology for reference; treat the "C6 Relay → UART → P4" segment as replaced by "S3 → UART → P4 (direct, wired)". See `docs/superpowers/specs/2026-07-08-s3-p4-wired-uart-design.md` + `docs/superpowers/plans/2026-07-09-s3-p4-wired-uart.md`.

---

## Table of Contents

1. [System Architecture Overview](#1-system-architecture-overview)
2. [Data Flow Diagram](#2-data-flow-diagram)
3. [I2C Bus Topology](#3-i2c-bus-topology)
4. [FreeRTOS Task Architecture](#4-freertos-task-architecture)
5. [Three-Tier Inference Pipeline](#5-three-tier-inference-pipeline)
6. [Communication Stack](#6-communication-stack)
7. [Model Hot-Switch Architecture](#7-model-hot-switch-architecture)
8. [Dual-Hand Feature Computation Flow](#8-dual-hand-feature-computation-flow)

---

## 1. System Architecture Overview

系统总览 -- 展示所有节点及其连接关系。

```
+===========================================================================+
|                      EchoGlove V6.0 System Architecture                    |
|                                                                           |
|  Glove Layer (ESP32-S3)                Base Station        PC Layer       |
|                                                                           |
|  +-----------------+                   +-----------+       +-----------+  |
|  |  Left Glove     |   ESP-NOW 69B    |  C6 Relay | UART  |  P4 Base  |  |
|  |  ESP32-S3 N16R8 | --~2ms---------> | ESP32-C6  |--2M-->| ESP32-P4  |  |
|  |                 |                   |  MINI-1   | bps   | 400MHz    |  |
|  |  LSM6DSV16X     |                   +-----------+       | RV32      |  |
|  |  ADC1 (GPIO1-5) |                                       | Dual-Core |  |
|  |  5x Flex        |                                       +-----+-----+  |
|  |  Tier1 CNN      |                                             |         |
|  |  11-dim feat    |                                         USB | HS      |
|  +-----------------+                                        480Mbps        |
|                                                                   |        |
|  +-----------------+   ESP-NOW 69B                                v        |
|  |  Right Glove    | --~2ms---------> (same C6)        +-----------------+ |
|  |  ESP32-S3 N16R8 |                                   |   PC Relay      | |
|  |                 |                                   |   FastAPI        | |
|  |  LSM6DSV16X     |                                   |   Tier3 ST-GCN  | |
|  |  ADC1 (GPIO1-5) |                                   |   NLP + TTS     | |
|  |  5x Flex        |                                   +--------+--------+ |
|  |  Tier1 CNN      |                                            |          |
|  |  11-dim feat    |                                      WS:8765| JSON     |
|  +-----------------+                                            v          |
|                                                        +-----------------+ |
|                                                        |   Frontend       | |
|                                                        |   React3F / R3F  | |
|                                                        |   Unity XR Hands | |
|                                                        +-----------------+ |
+===========================================================================+

  Standalone Mode (no PC):  P4 runs Tier2 + LVGL 7" display + TTS audio
  Fallback Mode (no P4):    C6 forwards directly to PC via USB
```

### 1.1 Node Summary

| Node | MCU | Clock | Memory | Role |
|------|-----|-------|--------|------|
| Left Glove | ESP32-S3 N16R8 | 240MHz dual-core | 8MB Flash + 8MB PSRAM | Sensor sampling, Tier1 inference, ESP-NOW TX |
| Right Glove | ESP32-S3 N16R8 | 240MHz dual-core | 8MB Flash + 8MB PSRAM | Sensor sampling, Tier1 inference, ESP-NOW TX |
| C6 Relay | ESP32-C6-MINI-1 | 160MHz single-core | 4MB Flash + 512KB SRAM | ESP-NOW RX, BLE 5.0, UART relay |
| P4 Base | ESP32-P4 | 400MHz RV32 dual-core | 32MB PSRAM + 7" LCD | Tier2 inference, LVGL UI, TTS audio, USB HS |
| PC Relay | x86/x64 | N/A | N/A | Tier3 inference, NLP, TTS, WebSocket server |
| Frontend | Browser/Unity | N/A | N/A | 3D hand skeleton rendering |

### 1.2 V6 Key Changes from V5

| Item | V5 | V6 |
|------|----|----|
| IMU | BNO085 @ 0x4B ($15-25) | LSM6DSV16X @ 0x6A ($2-4) |
| Fusion | BNO085 hardware fusion | LSM6DSV16X embedded SFLP |
| Calibration | BNO085 dynamic calibration | SFLP auto-converge 0.8s |
| Feature vector | 11-dim (unchanged) | 11-dim (unchanged) |
| Downstream code | -- | Zero changes |

---

## 2. Data Flow Diagram

数据流图 -- 从传感器采样到前端渲染的完整路径。

```
+========================= Glove (per hand) =========================+
|                                                                    |
|  LSM6DSV16X (0x6A)          ADC1 (GPIO1-5)                       |
|  +---------------+         +-------------+                        |
|  | Accel + Gyro  |         | Ch0: Thumb  |                        |
|  | SFLP Quat     |         | Ch1: Index  |                        |
|  +-------+-------+         | Ch2: Middle |                        |
|          |                 | Ch3: Ring   |                        |
|          v                 | Ch4: Pinky  |                        |
|  QuaternionToEuler()       +------+------+                         |
|          |                        |                                |
|          v                        v                                |
|  euler[3] + gyro[3]       FlexManager (5-point piecewise cal)     |
|          |                        |                                |
|          v                        v                                |
|                            flex[5] normalized [0,1]                |
|                       (analogReadMilliVolts, N=16 oversample)      |
|          |                      |                                  |
|          +----------+-----------+                                  |
|                     v                                              |
|              SensorData (11-dim)                                   |
|                     |                                              |
|              Kalman Filter (11-channel, 1D)                        |
|                     |                                              |
|              SlidingWindow (30 frames, ring buffer)                |
|                     |                                              |
|              Tier1 CNN+SE-Attention                                |
|              Input: 11-dim  Output: 46-class probs                |
|                     |                                              |
|              GlovePacket (69 bytes)                                |
|                     |                                              |
+=====================|==============================================+
                      v
               ESP-NOW Broadcast
               (every 10ms, ~2ms latency)
                      |
+=====================v============================================+
| C6 Relay (ESP32-C6)                                              |
|                                                                    |
|  ESP-NOW RX  ------>  FramePairer  ------>  UART TX              |
|  (L+R packets)        (match tick_id)       (2 Mbps, 73B frame)  |
|                                                                    |
+=====================|=============================================+
                      v
               UART 2 Mbps (GPIO43/44)
               [0xAA 0x55 LEN_H LEN_L PAYLOAD CRC16]
                      |
+=====================v============================================+
| P4 Base Station (ESP32-P4)                                       |
|                                                                    |
|  UART RX  ------>  FramePairer  ------>  RelativeFeatures        |
|  (73B frames)      (L+R by tick_id)      (compute 6-dim delta)   |
|                                              |                     |
|                                              v                     |
|                                     28-dim feature vector         |
|                                     (L11 + R11 + Rel6)            |
|                                              |                     |
|                          +-------------------+------------------+  |
|                          |                   |                  |  |
|                          v                   v                  v  |
|                    Tier2 Inference     LVGL Display       TTS     |
|                    Gated Bi-CrossAttn  7" touchscreen     Audio   |
|                    ~30ms, 46-class     10Hz update         I2S    |
|                          |                   |                  |  |
|                          +-------------------+------------------+  |
|                                              |                     |
|                                        USB HS CDC (480Mbps)       |
|                                        Protobuf encoded           |
+=====================|=============================================+
                      v
               USB High Speed
                      |
+=====================v============================================+
| PC Relay (FastAPI, Python)                                       |
|                                                                    |
|  USB CDC RX  ----->  ConfidenceRouter                             |
|                      |                                             |
|           +----------+----------+                                  |
|           |          |          |                                  |
|           v          v          v                                  |
|      Tier3-L1   Tier3-L2    NLP Pipeline                          |
|      BiCrossAttn ST-GCN     Grammar correction                    |
|      +MS-TCN     +MS-TCN     + TTS synthesis                      |
|      46-class    +CTC decode                                       |
|           |          |          |                                  |
|           +----+-----+----+----+                                  |
|                v          v                                        |
|          Confidence    Text + Audio                                |
|          Selection                                                |
|                |                                                   |
|                v                                                   |
|          WebSocket Server (port 8765, JSON)                        |
|                |                                                   |
+================|==================================================+
                 v
          WebSocket JSON
          (tier, blend_alpha, hands, inference, nlp_text)
                 |
+================v==================================================+
| Frontend                                                           |
|                                                                    |
|  React3F (Web MVP)  /  Unity XR Hands (Pro)                      |
|  +----------------+   +------------------+                        |
|  | 21-keypoint    |   | 26-joint OpenXR  |                        |
|  | MediaPipe topo |   | skeleton         |                        |
|  | R3F rendering  |   | XR device support|                        |
|  +----------------+   +------------------+                        |
|                                                                    |
+===================================================================+
```

> V6: ADS1115×2 removed — flex on internal ADC1 (GPIO1-5), not on I²C. I²C bus now carries only LSM6DSV16X @ 0x6A. See `07_internal_adc_migration.md`.

### 2.1 Latency Budget

| Stage | Latency | Notes |
|-------|---------|-------|
| Sensor sampling | 10ms | 100Hz I2C read |
| Kalman filter | <0.1ms | 11-channel 1D |
| Tier1 inference | <3ms | CNN+SE-Attention, INT8 |
| ESP-NOW TX | ~1ms | 69-byte broadcast |
| C6 relay | <0.5ms | UART forwarding |
| UART C6->P4 | ~0.3ms | 73 bytes @ 2Mbps |
| Tier2 inference | ~30ms | Gated Bi-CrossAttn, INT8 |
| USB HS transfer | <0.5ms | 480Mbps |
| Tier3 inference | ~15ms | ST-GCN + MS-TCN |
| WebSocket | <1ms | JSON, localhost |
| **Edge total (Tier1)** | **~15ms** | Glove to P4 display |
| **Full chain (Tier3)** | **~60ms** | Glove to frontend |
| **Target E2E** | **<100ms** | Including rendering |

---

## 3. I2C Bus Topology

I2C 总线拓扑 -- 每只手套的传感器连接。

```
                    ESP32-S3 (N16R8)
                   +-----------------+
                   |                 |
                   |   GPIO8 (SDA)  |----+---- 4.7kΩ pull-up ---- 3.3V
                   |   GPIO9 (SCL)  |--+ |
                   |                 |  | |
                   |   400kHz I2C    |  | |
                   +-----------------+  | |
                                        | |
               +------------------------+-+------------------------+
               |                  I2C Flat Bus                      |
               |       (single device — no MUX, V6 removed ADS1115) |
               |                                                    |
     +---------+---------+                                          |
     |                   |                                          |
     v                   v                                          |
+----------+                                                       |
|LSM6DSV16X|                                                       |
|  @ 0x6A  |                                                       |
|          |                                                       |
| SDO=GND  |                                                       |
| CS=3.3V  |                                                       |
| (I2C)    |                                                       |
+----------+                                                       |
|                                                                  |
| Accel + Gyro                                                     |
| SFLP Quat                                                        |
|                                                                  |
| Outputs:                                                         |
| - quaternion                                                     |
| - accel (g)                                                      |
| - gyro (dps)                                                     |
| - temperature                                                    |
+------------------------------------------------------------------+
```

> V6: ADS1115×2 removed — flex on internal ADC1 (see `07_internal_adc_migration.md`). I²C bus now has ONE device: LSM6DSV16X @ 0x6A.

### 3.1 I2C Address Map

| Device | Address | SDO/ADDR Pin | Config |
|--------|---------|-------------|--------|
| LSM6DSV16X | 0x6A | SDO/SA0 = GND | CS = 3.3V (I2C mode) |

> V6: ADS1115 @ 0x48 / 0x49 removed — flex on internal ADC1 (see `07`). (V5, removed: BNO085 @ 0x4B.)

### 3.2 Flex Sensor Wiring (per channel)

```
3.3V ---[ Flex Sensor ]---+--- ADC1 GPIO1-5 (internal)
                           |
                       [ 47kΩ ]
                           |
                          GND

Voltage divider: V_out = 3.3V * R_flex / (R_flex + 47kΩ)
ADC_ATTEN_DB_12: ~0–2500 mV usable, 12-bit; see `07_internal_adc_migration.md`
Normalized: flex_norm = (raw - cal_min) / (cal_max - cal_min)
```

### 3.3 LSM6DSV16X Wiring

| Pin | Connection | Notes |
|-----|-----------|-------|
| VDD | 3.3V | 1.71-3.6V operating range |
| VDDIO | 3.3V | I/O supply (tie to VDD) |
| GND | GND | |
| SDA/SDI | GPIO8 | 4.7kΩ pull-up to 3.3V |
| SCL/SCLK | GPIO9 | 4.7kΩ pull-up to 3.3V |
| SDO/SA0 | GND | LOW = 0x6A (latched at power-up) |
| CS | 3.3V | HIGH = I2C mode |
| INT1 | GPIO10 | Optional: data-ready interrupt |
| INT2 | Floating | Not connected |

### 3.4 Timing Budget (I2C 400kHz)

| Device | Bytes | Time | Notes |
|--------|-------|------|-------|
| LSM6DSV16X | 14 (quat+gyro+accel) | ~0.35ms | SFLP output + raw gyro |
| ADC1 (5ch flex) | 5 (N=16 oversample) | ~0.8ms | internal ADC, see `07` |
| **Total per sample** | | **~1.15ms** | Well within 10ms budget |

---

## 4. FreeRTOS Task Architecture

FreeRTOS 任务架构 -- 手套端 + P4 基站端。

### 4.1 Glove Tasks (ESP32-S3, per glove)

```
+============================ ESP32-S3 ==============================+
|                                                                    |
|  Core 1 (sensor core)                                             |
|  +--------------------------------------------------------------+ |
|  | Task_SensorRead  [Priority: 3 (highest),  Freq: 100Hz]      | |
|  |                                                                | |
|  |   I2C Read LSM6DSV16X  ----+                                  | |
|  |   ADC1 read 5ch (GPIO1-5) ---+--> SensorData (11-dim)          | |
|  |   (internal ADC, see 07)    |          |                        | |
|  |   ~1.15ms total                      v                        | |
|  |                              Kalman Filter (11-ch, 1D)        | |
|  |                                      |                        | |
|  |                              SlidingWindow.push()             | |
|  |                              (30-frame ring buffer)           | |
|  +--------------------------------------------------------------+ |
|                                                                    |
|  Core 0 (comms + inference core)                                  |
|  +--------------------------------------------------------------+ |
|  | Task_Inference  [Priority: 2,  Freq: ~30Hz]                  | |
|  |                                                                | |
|  |   SlidingWindow.get() --> Tier1 CNN+SE-Attention              | |
|  |   Input: 11-dim x 30 frames                                   | |
|  |   Output: gesture_id + confidence (46 classes)                | |
|  |   Latency: <3ms                                                | |
|  +--------------------------------------------------------------+ |
|                                                                    |
|  +--------------------------------------------------------------+ |
|  | Task_Comms  [Priority: 1,  Freq: 100Hz]                      | |
|  |                                                                | |
|  |   Build GlovePacket (69 bytes)                                | |
|  |   ESP-NOW broadcast  ----->  C6 relay                         | |
|  |   BLE provisioning   <---->  Phone (idle mode only)           | |
|  +--------------------------------------------------------------+ |
|                                                                    |
|  +--------------------------------------------------------------+ |
|  | Task_Watchdog  [Priority: 0 (lowest),  Freq: 1Hz]            | |
|  |                                                                | |
|  |   Feed hardware watchdog                                      | |
|  |   Health check: sensor OK, comms OK, heap OK                  | |
|  |   Log uptime, error count                                     | |
|  +--------------------------------------------------------------+ |
|                                                                    |
+====================================================================+
```

### 4.2 P4 Base Station Tasks (ESP32-P4)

```
+============================ ESP32-P4 ==============================+
|                                                                    |
|  Core 0 (IO + display core)                                       |
|  +--------------------------------------------------------------+ |
|  | Task_UART_Receive  [Priority: 3 (highest), Interrupt-driven] | |
|  |                                                                | |
|  |   UART RX interrupt --> ring_buffer (4KB)                     | |
|  |   Frame parser: [0xAA 0x55 LEN PAYLOAD CRC16]                | |
|  |   Protobuf decode --> GlovePacket                             | |
|  |   FramePairer: match L+R by tick_id                           | |
|  +--------------------------------------------------------------+ |
|                                                                    |
|  +--------------------------------------------------------------+ |
|  | Task_Display  [Priority: 1,  30fps render / 10Hz data]       | |
|  |                                                                | |
|  |   LVGL UI update                                              | |
|  |   Show: gesture name, confidence, dual-hand skeleton          | |
|  |   7" 1024x600 LCD via MIPI-DSI                               | |
|  +--------------------------------------------------------------+ |
|                                                                    |
|  +--------------------------------------------------------------+ |
|  | Task_Audio  [Priority: 1,  Event-driven]                     | |
|  |                                                                | |
|  |   TTS PCM playback via I2S (ES8311 codec)                    | |
|  |   Audio files stored on MicroSD (16GB Class10)               | |
|  |   Triggered by inference result change                        | |
|  +--------------------------------------------------------------+ |
|                                                                    |
|  +--------------------------------------------------------------+ |
|  | Task_USB  [Priority: 1,  ~100Hz]                             | |
|  |                                                                | |
|  |   USB HS CDC data forwarding to PC                           | |
|  |   Protobuf encoded ReceiverPacket                             | |
|  |   Includes: L+R GloveData + 28-dim features + Tier2 result   | |
|  +--------------------------------------------------------------+ |
|                                                                    |
|  Core 1 (inference core)                                          |
|  +--------------------------------------------------------------+ |
|  | Task_Inference  [Priority: 2,  ~30Hz]                        | |
|  |                                                                | |
|  |   Receive 28-dim from FramePairer                             | |
|  |   Tier2 Gated Bi-CrossAttn inference                         | |
|  |   Input: 28-dim (L11 + R11 + Rel6)                           | |
|  |   Output: gesture_id + confidence (46 classes)               | |
|  |   Latency: ~30ms (INT8, TFLite Micro)                        | |
|  +--------------------------------------------------------------+ |
|                                                                    |
+====================================================================+
```

### 4.3 Task Interaction Diagram

```
  Glove (ESP32-S3)                         P4 (ESP32-P4)
  ================                         ==============

  Task_SensorRead (Core 1, 100Hz)
       |
       | SensorData[11]
       v
  Kalman Filter --> SlidingWindow
       |                    |
       |                    v
       |            Task_Inference (Core 0, ~30Hz)
       |                    |
       |            GestureResult (id + confidence)
       |                    |
       v                    v
  Task_Comms (Core 0, 100Hz)        Task_UART_Receive (Core 0, IRQ)
       |                                    |
       | GlovePacket (69B)                 | FramePairer (L+R match)
       | ESP-NOW broadcast                 | 28-dim feature vector
       v                                    |
  +=========+                               +----+------+------+
  |   C6    |                                    |      |      |
  | ESP-NOW |                                    v      v      v
  |   RX    |                              Task_     Task_  Task_
  |   |     |                              Inference Display Audio
  | UART TX |                              (Core 1)  (Core 0)(Core 0)
  +=========+                                 |
       |                                      | Result
       +---------- UART 2Mbps --------------- v
                                           Task_USB (Core 0)
                                              |
                                           USB HS --> PC
```

---

## 5. Three-Tier Inference Pipeline

三级推理流水线 -- 从边缘到云端的分级推理。

```
+======================= Tier 1: Edge (Glove ESP32-S3) =======================+
|                                                                             |
|  Input: 11-dim x 30 frames (SlidingWindow)                                 |
|  Model: CNN + SE-Attention                                                  |
|  Size: ~80KB (TFLite INT8)                                                 |
|  Latency: <3ms                                                             |
|  Output: 46-class probability vector + gesture_id + confidence             |
|                                                                             |
|  +-----------+    +--------+    +--------+    +--------+    +-----------+  |
|  | Input     |    | Conv1D |    | Conv1D |    |   SE   |    |   FC +    |  |
|  | 11x30     |--->|  32ch  |--->|  64ch  |--->| Attn   |--->| Softmax   |  |
|  | window    |    |  +BN   |    |  +BN   |    | Squeeze|    | 46-class  |  |
|  +-----------+    +--------+    +--------+    +--------+    +-----------+  |
|                                                                             |
|  Runs on EVERY glove. Always available. <3ms latency.                      |
+============================+===============================================+
                             |
           If P4 available: ESP-NOW --> C6 --> UART --> P4
                             |
+============================v===============================================+
|                                                                             |
|                    Tier 2: P4 Base Station (ESP32-P4)                      |
|                                                                             |
|  Input: 28-dim (L11 + R11 + Relative6) x 1 frame                          |
|  Model: Gated Bi-CrossAttention                                            |
|  Size: ~80KB (TFLite INT8)                                                 |
|  Latency: ~30ms                                                            |
|  Output: 46-class probability vector + gesture_id + confidence             |
|                                                                             |
|  +--------+     +--------+     +--------+     +--------+                   |
|  |  Left  |     | Cross  |     | Gate   |     |  FC +  |                   |
|  | 11-dim |--+->| Attn   |--+->| Fusion |--+->| Softmax|--+--> 46-class   |
|  +--------+  |  | (L<->R)|  |  | sigmoid|  |  +--------+  |               |
|  +--------+  |  +--------+  |  +--------+  |              |               |
|  | Right  |--+              +--------------+              |               |
|  | 11-dim |                                                |               |
|  +--------+     +--------+                                |               |
|                 | Relative|-------------------------------+               |
|                 | 6-dim   |                                                |
|                 +--------+                                                |
|                                                                             |
|  Standalone mode: P4 displays result on LVGL + plays TTS audio.           |
|  Connected mode:  P4 forwards to PC via USB HS.                           |
+============================+===============================================+
                             |
           If PC available: USB HS --> PC Relay
                             |
+============================v===============================================+
|                                                                             |
|                         Tier 3: PC (x86 + GPU)                             |
|                                                                             |
|  +---------------------------------------------------------------------+  |
|  | Tier3-L1: Gated Bi-CrossAttn + MS-TCN 4-stage                      |  |
|  |                                                                       |  |
|  |  Input: 28-dim x 30 frames (sliding window)                          |  |
|  |  Model: Bi-CrossAttn spatial + MS-TCN temporal                       |  |
|  |  Size: ~500KB (FP32)                                                  |  |
|  |  Latency: ~15ms                                                       |  |
|  |  Output: 46-class per-frame (isolated gesture)                       |  |
|  +---------------------------------------------------------------------+  |
|                                                                             |
|  +---------------------------------------------------------------------+  |
|  | Tier3-L2: ST-GCN(42 nodes) + MS-TCN 4-stage + CTC decode           |  |
|  |                                                                       |  |
|  |  Input: 28-dim x 30 frames --> 42-node graph (21 per hand)           |  |
|  |  Model: ST-GCN spatial-temporal + MS-TCN segmentation                |  |
|  |  Size: ~3MB (FP32)                                                    |  |
|  |  Latency: ~15ms                                                       |  |
|  |  Output: Continuous character/word sequence (CTC beam search)        |  |
|  +---------------------------------------------------------------------+  |
|                                                                             |
|  +---------------------------------------------------------------------+  |
|  | Confidence Router                                                     |  |
|  |                                                                       |  |
|  |  Always:  Tier1 result (lowest latency, always available)            |  |
|  |  If P4:   Tier2 result (cross-hand attention, better accuracy)       |  |
|  |  If PC:   Tier3-L1 (isolated) or Tier3-L2 (continuous)              |  |
|  |  Blend:   5-frame linear crossfade (50ms @ 100Hz)                    |  |
|  +---------------------------------------------------------------------+  |
|                                                                             |
|  +---------------------------------------------------------------------+  |
|  | NLP + TTS Pipeline                                                    |  |
|  |                                                                       |  |
|  |  Phase 1: gesture_id --> grammar correction --> text                  |  |
|  |  Phase 2: CTC sequence --> beam search --> NLP --> fluent sentence    |  |
|  |  TTS: edge-tts or pre-recorded PCM --> audio stream                  |  |
|  +---------------------------------------------------------------------+  |
|                                                                             |
+============================+===============================================+
                             |
                      WebSocket :8765
                      JSON (tier, blend_alpha, hands, inference, nlp_text)
                             |
+============================v===============================================+
|                                                                             |
|                           Frontend                                         |
|                                                                             |
|  React3F (21-keypoint)    /    Unity XR Hands (26-joint)                  |
|                                                                             |
+=============================================================================+
```

### 5.1 Tier Comparison

| Property | Tier1 | Tier2 | Tier3-L1 | Tier3-L2 |
|----------|-------|-------|----------|----------|
| Location | Glove S3 | P4 base | PC GPU | PC GPU |
| Input dim | 11 | 28 | 28 x 30 | 28 x 30 |
| Model | CNN+SE | Bi-CrossAttn | Bi-CrossAttn+MS-TCN | ST-GCN+MS-TCN+CTC |
| Size | ~80KB | ~80KB | ~500KB | ~3MB |
| Quantization | INT8 | INT8 | FP32 | FP32 |
| Latency | <3ms | ~30ms | ~15ms | ~15ms |
| Accuracy | >85% | >90% | >95% | >95% |
| Output | Isolated | Isolated | Isolated | Continuous |
| Availability | Always | With P4 | With PC | With PC |

### 5.2 Hot-Swap Transition

```
   Tier1 only          Tier1 + Tier2         Tier1 + Tier2 + Tier3
   ===========         ================      ======================

   output = T1         output = blend(T1,T2) output = blend(T2,T3)

   blend_alpha = 0     blend_alpha: 0->1     blend_alpha: 0->1
                       over 5 frames          over 5 frames
                       (~50ms @ 100Hz)        (~50ms @ 100Hz)

   If higher tier goes offline:
   - 3 missed heartbeats (300ms) --> downgrade
   - Reverse blend: blend_alpha: 1->0 over 5 frames
```

---

## 6. Communication Stack

通信协议栈 -- 各层协议详解。

```
+===========================================================================+
|                    EchoGlove V6.0 Communication Stack                      |
|                                                                           |
|  Layer 5: Application                                                     |
|  +-----------------------------------------------------------------------+|
|  |  React3F / Unity XR   <--- JSON --- WebSocket :8765                   ||
|  |  {tier, blend_alpha, left_hand, right_hand, inference, nlp_text}      ||
|  +-----------------------------------------------------------------------+|
|                                    ^                                      |
|  Layer 4: WebSocket               | JSON                                 |
|  +-----------------------------------------------------------------------+|
|  |  FastAPI (Python)                                                     ||
|  |  Port 8765, JSON encoding, auto-reconnect                            ||
|  +-----------------------------------------------------------------------+|
|                                    ^                                      |
|  Layer 3: USB                     | Protobuf                             |
|  +-----------------------------------------------------------------------+|
|  |  USB 2.0 High Speed (480Mbps)                                         ||
|  |  CDC ACM class, P4 <-> PC                                             ||
|  |  Frame: Protobuf ReceiverPacket (L+R GloveData + relative features)   ||
|  +-----------------------------------------------------------------------+|
|                                    ^                                      |
|  Layer 2: UART                    | Raw bytes                            |
|  +-----------------------------------------------------------------------+|
|  |  2 Mbps, 8N1                                                          ||
|  |  TX: GPIO43 (C6) --> GPIO38 (P4)                                     ||
|  |  RX: GPIO44 (C6) <-- GPIO37 (P4)                                     ||
|  |  Frame: [0xAA][0x55][LEN_H][LEN_L][PAYLOAD...][CRC16_H][CRC16_L]     ||
|  |  Frame size: 73 bytes (69B payload + 4B header/footer)                ||
|  |  CRC: CRC-16/MODBUS                                                   ||
|  +-----------------------------------------------------------------------+|
|                                    ^                                      |
|  Layer 1: ESP-NOW                 | 69-byte GlovePacket                  |
|  +-----------------------------------------------------------------------+|
|  |  ESP-NOW broadcast, ~1ms latency                                      ||
|  |  No pairing required, both gloves broadcast simultaneously            ||
|  |  C6 broadcasts SYNC_TICK every 10ms for frame alignment              ||
|  |  GlovePacket: 69 bytes (see struct below)                             ||
|  +-----------------------------------------------------------------------+|
|                                    ^                                      |
|  Layer 0: RF                      | 2.4 GHz                              |
|  +-----------------------------------------------------------------------+|
|  |  Wi-Fi PHY (802.11 b/g/n)                                            ||
|  |  ESP-NOW uses Action Frame (vendor-specific)                         ||
|  |  Range: ~30m indoor, ~100m outdoor                                    ||
|  +-----------------------------------------------------------------------+|
|                                                                           |
+===========================================================================+
```

### 6.1 GlovePacket Structure (69 bytes)

```
Offset  Size  Field               Description
------  ----  ------------------  ----------------------------------------
0       2     magic               {0x45, 0x47} = "EG"
2       1     version             6 (V6)
3       1     hand_id             0=left, 1=right
4       4     tick_id             Receiver broadcast sync tick
8       4     timestamp_us        Microsecond timestamp
12      20    flex[5]             5 x float32, normalized [0,1]
32      24    imu[6]              6 x float32, euler[3] + gyro[3]
56      2     l1_gesture_id       uint16, Tier1 result
58      4     l1_confidence       float32, Tier1 confidence
62      1     status              STREAMING / CALIBRATING / ERROR
63      2     checksum            CRC16 of bytes 0-62
65      4     reserved            Future use
------  ----  ------------------  ----------------------------------------
Total:  69 bytes
```

### 6.2 UART Frame Structure (73 bytes)

```
+------+--------+--------+---------+------+------+
| 0xAA |  0x55  | LEN_H  | LEN_L   | PAY  | CRC  |
|      |        | (0x00) | (0x45)  | LOAD |  16  |
| 1B   |  1B    | 1B     | 1B      | 69B  | 2B   |
+------+--------+--------+---------+------+------+

Magic:   0xAA 0x55
Length:  69 bytes (0x0045)
Payload: GlovePacket (69 bytes)
CRC:     CRC-16/MODBUS over bytes [2..70]
Total:   73 bytes
```

### 6.3 Throughput Analysis

| Link | Rate | Packet Size | Frequency | Bandwidth | Headroom |
|------|------|-------------|-----------|-----------|----------|
| ESP-NOW (per glove) | ~1Mbps | 69B | 100Hz | 55.2 kbps | 18x |
| ESP-NOW (total) | ~1Mbps | 69B x 2 | 100Hz | 110.4 kbps | 9x |
| UART (C6->P4) | 2 Mbps | 73B x 2 | 100Hz | 116.8 kbps | 17x |
| USB HS (P4->PC) | 480 Mbps | Protobuf | 100Hz | ~50 kbps | 9600x |
| WebSocket (PC->FE) | ~100 Mbps | JSON | 30Hz | ~30 kbps | 3300x |

### 6.4 Packet Loss Handling

```
  Frame N:    [valid]  [drop]  [drop]  [drop]  [valid]
                  |       |       |       |       |
  Strategy:       |   interpolate  |   hold last |  resume
                  |   (1-3 gap)    |   (4-10 gap)|

  1-3 dropped:  Linear interpolation between last_valid and next_valid
  4-10 dropped: Hold last valid value + set stale_flag
  >10 dropped:  Mark hand as offline, Tier1 fallback only
```

---

## 7. Model Hot-Switch Architecture

模型热切换架构 -- 运行时动态加载和切换模型。

```
+======================== C++ (Glove Firmware) ========================+
|                                                                      |
|  BaseModel (abstract interface)                                      |
|  +----------------------------------------------------------------+ |
|  |  virtual bool load(const char* model_path) = 0;                | |
|  |  virtual bool infer(const float* input, float* output) = 0;    | |
|  |  virtual int  getInputDim() = 0;                                | |
|  |  virtual int  getOutputDim() = 0;                               | |
|  |  virtual const char* getName() = 0;                             | |
|  +----------------------------------------------------------------+ |
|          ^                    ^                    ^                 |
|          |                    |                    |                 |
|  +-------+-------+   +-------+-------+   +--------+------+         |
|  | TFLiteModel   |   | EIModel       |   | MockModel     |         |
|  | (TFLite Micro)|   | (Edge Impulse)|   | (testing)     |         |
|  +---------------+   +---------------+   +---------------+         |
|                                                                      |
|  ModelRegistry                                                       |
|  +----------------------------------------------------------------+ |
|  |  map<string, BaseModel*> models_;                               | |
|  |  BaseModel* active_;                                            | |
|  |  void register(name, model);                                    | |
|  |  void switchTo(name);          // hot-switch                    | |
|  |  bool infer(input, output);     // delegates to active_         | |
|  +----------------------------------------------------------------+ |
|                                                                      |
|  Config: platformio.ini --> model_config.yaml                        |
|  Switch: runtime, no restart required                                |
|                                                                      |
+======================================================================+

+======================== Python (PC Relay) ==========================+
|                                                                      |
|  BaseModel (abstract interface)                                      |
|  +----------------------------------------------------------------+ |
|  |  def load(self, config: dict) -> None                           | |
|  |  def predict(self, features: np.ndarray) -> dict                | |
|  |  def get_name(self) -> str                                      | |
|  +----------------------------------------------------------------+ |
|          ^                    ^                    ^                 |
|          |                    |                    |                 |
|  +-------+-------+   +-------+-------+   +--------+------+         |
|  | STGCNModel    |   | BiCrossAttn   |   | MockModel     |         |
|  | (ST-GCN L2)   |   | (Tier3-L1)    |   | (testing)     |         |
|  +---------------+   +---------------+   +---------------+         |
|                                                                      |
|  ModelRegistry                                                       |
|  +----------------------------------------------------------------+ |
|  |  _models: dict[str, BaseModel]                                  | |
|  |  _active: BaseModel                                             | |
|  |  register(name, model_cls)                                      | |
|  |  switch_to(name)              # hot-switch                      | |
|  |  predict(features)            # delegates to _active            | |
|  +----------------------------------------------------------------+ |
|                                                                      |
|  Config: glove_relay/configs/model_config.yaml                       |
|  Switch: runtime via API endpoint or YAML reload                     |
|                                                                      |
+======================================================================+
```

### 7.1 Model Registry Flow

```
  Startup                    Runtime
  =======                    =======

  Load YAML config           API call or threshold event
       |                           |
       v                           v
  Register models            switch_to("new_model")
  into registry                   |
       |                           v
       v                    Hot-switch: active_ = new_model
  Set active model                |
  from config                     v
       |                    Next infer() call uses new model
       v                    (no restart, no reload)
  Ready to infer
```

### 7.2 Confidence-Based Auto-Switch

```
  Tier1 confidence < 0.6  -->  Request Tier2 from P4
  Tier2 confidence < 0.5  -->  Request Tier3 from PC
  Tier3 unavailable       -->  Fallback to Tier2
  Tier2 unavailable       -->  Fallback to Tier1

  Heartbeat protocol:
    - Each tier sends heartbeat every 100ms
    - 3 missed heartbeats (300ms) = tier offline
    - Automatic downgrade with 5-frame blend transition
```

---

## 8. Dual-Hand Feature Computation Flow

双手特征计算流程 -- 从单手 11 维到双手 28 维的完整路径。

```
+==================== Left Glove (ESP32-S3) ====================+
|                                                                |
|  LSM6DSV16X              ADC1 (GPIO1-5)                      |
|  +----------+            +----------+                          |
|  | SFLP Quat|            | Flex 0-4 |                          |
|  | Gyro     |            | (5 ch)   |                          |
|  +----+-----+            +----+-----+                          |
|       |                       |                                |
|       v                       v                                |
|  quat -> euler[3]        flex_norm[5]                          |
|  gyro_raw[3]                                                 |
|       |                       |                                |
|       +-------+---------------+                                |
|               v                                                |
|     Left_SensorData (11-dim)                                   |
|     = [flex0..flex4, euler_x..z, gyro_x..z]                   |
|               |                                                |
|     Kalman Filter (11-ch)                                      |
|               |                                                |
|     ESP-NOW broadcast (69B GlovePacket, hand_id=0)             |
+===============|================================================+
                |
+===============v================================================+
| C6 Relay                                                         |
| ESP-NOW RX: receive L + R packets                              |
| UART TX: forward both to P4                                    |
+===============|================================================+
                |
+===============v================================================+ +=================== Right Glove (ESP32-S3) ===================+
|                                                                |
|  LSM6DSV16X              ADC1 (GPIO1-5)                      |
|  +----------+            +----------+                          |
|  | SFLP Quat|            | Flex 0-4 |                          |
|  | Gyro     |            | (5 ch)   |                          |
|  +----+-----+            +----+-----+                          |
|       |                       |                                |
|       v                       v                                |
|  quat -> euler[3]        flex_norm[5]                          |
|  gyro_raw[3]                                                 |
|       |                       |                                |
|       +-------+---------------+                                |
|               v                                                |
|     Right_SensorData (11-dim)                                  |
|     = [flex0..flex4, euler_x..z, gyro_x..z]                   |
|               |                                                |
|     Kalman Filter (11-ch)                                      |
|               |                                                |
|     ESP-NOW broadcast (69B GlovePacket, hand_id=1)             |
+===============|================================================+
                v
+===============v================================================+
| P4 Base Station (ESP32-P4)                                     |
|                                                                |
|  FramePairer: match L+R by tick_id                             |
|       |                   |                                    |
|       v                   v                                    |
|  Left[11]            Right[11]                                |
|  = [f0..f4,          = [f0..f4,                               |
|     ex,ey,ez,           ex,ey,ez,                              |
|     gx,gy,gz]           gx,gy,gz]                             |
|       |                   |                                    |
|       +--------+----------+                                    |
|                v                                                |
|  Relative Features (6-dim):                                    |
|  +-----------------------------------------------------------+ |
|  |  DeltaEuler[3]                                             | |
|  |    dE[i] = L.euler[i] - R.euler[i]     for i in (x,y,z)   | |
|  |                                                            | |
|  |  DeltaQuatDist[1]                                          | |
|  |    dQ = 2 * arccos(|q_L . q_R|)                           | |
|  |    (quaternion angular distance)                           | |
|  |                                                            | |
|  |  DeltaGyroNorm[1]                                          | |
|  |    dGN = ||gyro_L|| - ||gyro_R||                           | |
|  |    (angular speed difference)                              | |
|  |                                                            | |
|  |  DeltaGyroAxis[1]                                          | |
|  |    dGA = argmax(|gyro_L - gyro_R|) / 3                     | |
|  |    (dominant axis, normalized)                             | |
|  +-----------------------------------------------------------+ |
|                |                                                |
|                v                                                |
|  28-dim Feature Vector:                                        |
|  +-----------------------------------------------------------+ |
|  |  Left[11]  |  Right[11]  |  Relative[6]                   | |
|  |  0..10     |  11..21     |  22..27                        | |
|  +-----------------------------------------------------------+ |
|                |                                                |
|                v                                                |
|  Tier2 Gated Bi-CrossAttn Inference                            |
|  (or forward 28-dim to PC via USB for Tier3)                   |
|                                                                |
+================================================================+
```

> V6: ADS1115×2 removed per glove — flex on internal ADC1 (GPIO1-5), not on I²C. See `07_internal_adc_migration.md`.

### 8.1 Feature Vector Layout (28-dim)

```
Index   Field               Source      Range
-----   ------------------   ---------   ---------
0       flex_thumb           Left ADS    [0, 1]
1       flex_index           Left ADS    [0, 1]
2       flex_middle          Left ADS    [0, 1]
3       flex_ring            Left ADS    [0, 1]
4       flex_pinky           Left ADS    [0, 1]
5       euler_x (roll)       Left IMU    [-180, 180] deg
6       euler_y (pitch)      Left IMU    [-90, 90] deg
7       euler_z (yaw)        Left IMU    [-180, 180] deg
8       gyro_x               Left IMU    [-4000, 4000] dps
9       gyro_y               Left IMU    [-4000, 4000] dps
10      gyro_z               Left IMU    [-4000, 4000] dps
-----   ------------------   ---------   ---------
11      flex_thumb           Right ADS   [0, 1]
12      flex_index           Right ADS   [0, 1]
13      flex_middle          Right ADS   [0, 1]
14      flex_ring            Right ADS   [0, 1]
15      flex_pinky           Right ADS   [0, 1]
16      euler_x (roll)       Right IMU   [-180, 180] deg
17      euler_y (pitch)      Right IMU   [-90, 90] deg
18      euler_z (yaw)        Right IMU   [-180, 180] deg
19      gyro_x               Right IMU   [-4000, 4000] dps
20      gyro_y               Right IMU   [-4000, 4000] dps
21      gyro_z               Right IMU   [-4000, 4000] dps
-----   ------------------   ---------   ---------
22      delta_euler_x        Computed    [-360, 360] deg
23      delta_euler_y        Computed    [-180, 180] deg
24      delta_euler_z        Computed    [-360, 360] deg
25      delta_quat_dist      Computed    [0, 180] deg
26      delta_gyro_norm      Computed    [-8000, 8000] dps
27      delta_gyro_axis      Computed    [0, 1] (normalized)
```

### 8.2 Relative Feature Computation (Pseudocode)

```python
def compute_relative_features(left: SensorData, right: SensorData) -> float[6]:
    """Compute 6-dim relative features from paired glove data."""

    # Delta Euler (roll, pitch, yaw)
    delta_euler = [
        left.euler[0] - right.euler[0],   # roll difference
        left.euler[1] - right.euler[1],   # pitch difference
        left.euler[2] - right.euler[2],   # yaw difference
    ]

    # Quaternion angular distance
    dot = abs(left.quat[0]*right.quat[0] +
              left.quat[1]*right.quat[1] +
              left.quat[2]*right.quat[2] +
              left.quat[3]*right.quat[3])
    dot = min(dot, 1.0)  # clamp for acos
    delta_quat_dist = 2.0 * math.acos(dot)  # radians

    # Gyro norm difference
    gyro_L_norm = math.sqrt(left.gyro[0]**2 + left.gyro[1]**2 + left.gyro[2]**2)
    gyro_R_norm = math.sqrt(right.gyro[0]**2 + right.gyro[1]**2 + right.gyro[2]**2)
    delta_gyro_norm = gyro_L_norm - gyro_R_norm

    # Dominant axis (normalized)
    diff = [abs(left.gyro[i] - right.gyro[i]) for i in range(3)]
    delta_gyro_axis = max(diff) / 3.0

    return [*delta_euler, delta_quat_dist, delta_gyro_norm, delta_gyro_axis]
```

### 8.3 Frame Pairing (P4 FramePairer)

```
  Tick ID:  t=100    t=101    t=102    t=103    t=104

  Left:     [L@100]  [L@101]  [drop]   [L@103]  [L@104]
  Right:    [R@100]  [drop]   [R@102]  [R@103]  [R@104]

  Pairs:
  t=100:  L@100 + R@100  --> compute 28-dim --> Tier2 inference
  t=101:  L@101 + R@101? --> R missing, hold R@100, stale flag
  t=102:  L@102? + R@102 --> L missing, hold L@101, stale flag
  t=103:  L@103 + R@103  --> both fresh --> compute 28-dim
  t=104:  L@104 + R@104  --> both fresh --> compute 28-dim

  Stale timeout: >10 frames (100ms) --> mark hand offline
  Recovery: first fresh packet after stale --> immediate pair
```

---

## Appendix A: Glossary

| Term | Definition |
|------|-----------|
| SFLP | Sensor Fusion Low Power -- LSM6DSV16X embedded quaternion fusion |
| Bi-CrossAttn | Gated Bi-CrossAttention -- dual-hand cross-attention mechanism |
| ST-GCN | Spatial-Temporal Graph Convolutional Network |
| MS-TCN | Multi-Stage Temporal Convolutional Network |
| CTC | Connectionist Temporal Classification -- continuous sequence decoding |
| INT8 | 8-bit integer quantization (TFLite Micro) |
| ESP-NOW | Espressif connectionless Wi-Fi protocol, ~1ms latency |
| LVGL | Light and Versatile Graphics Library -- P4 touchscreen UI |
| TTS | Text-to-Speech audio synthesis |
| CDC | Communications Device Class -- USB serial emulation |
| PGA | Programmable Gain Amplifier (ADS1115) |

---

## Appendix B: Version History

| Date | Version | Changes |
|------|---------|---------|
| 2026-06-23 | V6.0 | Initial architecture diagrams for LSM6DSV16X migration |
