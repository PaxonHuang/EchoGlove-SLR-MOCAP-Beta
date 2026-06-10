# EchoGlove V5.2 — ESP32-P4 Smart Base Station Design Spec

> **Date**: 2026-06-10
> **Branch**: V5-DualGloveFlex
> **Status**: Approved
> **Timeline**: 3 weeks (deadline: June 2026)
> **Supersedes**: V5.2 full spec (deferred UWB + camera to post-competition)

---

## 1. Summary

Upgrade the V5.0 receiver (ESP32-S3 USB relay) to a **P4 smart base station** using the ESP32-P4-Function-EV-Board v1.5.2 (competition-provided). The base station runs Tier2 inference locally, displays results on a 7" touchscreen, and plays TTS audio — all independent of a PC.

**Key decisions**:
- ESP32-P4 as smart base station (replaces S3 receiver)
- ESP-NOW + BLE 5.0 dual-mode via ESP32-C6 co-processor
- 28-dim feature vector (no UWB — deferred to V5.3)
- Tier2 inference on P4 (TFLite Micro, INT8, ~80KB)
- LVGL 7" touchscreen UI + ES8311 TTS audio
- 3-week development cycle
- ESP-IDF v5.4+ (P4 support required)

---

## 2. System Architecture

### 2.1 Hardware Topology

| Node | MCU | Role |
|------|-----|------|
| Left Glove | ESP32-S3 N16R8 | Sensor sampling + Tier1 inference + ESP-NOW TX |
| Right Glove | ESP32-S3 N16R8 | Sensor sampling + Tier1 inference + ESP-NOW TX |
| Base Station (C6) | ESP32-C6-MINI-1 | ESP-NOW RX + BLE 5.0 + UART relay |
| Base Station (P4) | ESP32-P4 (400MHz RV32 dual-core) | Tier2 inference + LVGL display + TTS audio + USB HS |
| PC | x86 | Tier3 inference + NLP + React3F/Unity rendering |

### 2.2 Data Flow

```
Glove L/S3 ──ESP-NOW (~2ms)──→ C6 ──UART 2Mbps (~0.5ms)──→ P4
Glove R/S3 ──ESP-NOW (~2ms)──↗                                  │
                                                               ├── Tier2 inference (~30ms)
                                                               ├── LVGL display (10Hz update)
                                                               ├── TTS audio playback
                                                               └── USB HS 480Mbps → PC Relay
```

### 2.3 Glove Firmware — Zero Changes

The existing V5.0 ESP32-S3 glove firmware is unchanged:
- Sensor sampling: 100Hz (BNO085 + ADS1115)
- Tier1 inference: CNN + Attention, ~80KB INT8
- ESP-NOW broadcast: 28-dim features + metadata
- Protobuf encoding: existing `GlovePacket` format

### 2.4 PC Relay — Minimal Changes

| Module | Change | Scope |
|--------|--------|-------|
| `udp_server.py` | Add USB CDC data source | Small |
| `protobuf_parser.py` | Parse `BaseStationInfo` fields | Small |
| `confidence_router.py` | Route Tier2 P4 results | Small |
| `main.py` | USB CDC as alternative input | Small |
| `stgcn_model.py` | None | — |
| `tts_engine.py` | None | — |
| `nlp/` | None | — |

---

## 3. C6 Co-Processor Firmware

### 3.1 Responsibilities

1. **ESP-NOW reception**: Receive data from both gloves (MAC-bound)
2. **BLE 5.0**: OTA firmware upgrade + optional phone pairing
3. **UART relay**: Forward received data to P4 via UART 2Mbps
4. **Time-division multiplexing**: 90% ESP-NOW / 10% BLE

### 3.2 UART Protocol

| Parameter | Value |
|-----------|-------|
| Baud rate | 2 Mbps |
| TX pin | GPIO43 (C6) → GPIO38 (P4) |
| RX pin | GPIO44 (C6) ← GPIO37 (P4) |
| Format | 8N1 |
| Frame | `[0xAA][0x55][LEN_H][LEN_L][PAYLOAD...][CRC16_H][CRC16_L]` |

### 3.3 Data Throughput

| Stream | Rate | Size | Bandwidth |
|--------|------|------|-----------|
| Left glove | 50Hz | ~20B | ~1 KB/s |
| Right glove | 50Hz | ~20B | ~1 KB/s |
| Total | — | — | ~2 KB/s |
| UART capacity | — | — | ~200 KB/s |
| **Headroom** | — | — | **100×** |

---

## 4. P4 Main Processor Firmware

### 4.1 FreeRTOS Task Architecture

| Task | Core | Priority | Frequency | Purpose |
|------|------|----------|-----------|---------|
| Task_UART_Receive | 0 | 3 (highest) | Interrupt-driven | UART RX → ring buffer → Protobuf decode |
| Task_Inference | 1 | 2 | ~30Hz | Tier2 model inference (dedicated core) |
| Task_Display | 0 | 1 | 30fps render, 10Hz data | LVGL UI update |
| Task_Audio | 0 | 1 | Event-driven | TTS PCM playback via I2S |
| Task_USB | 0 | 1 | ~100Hz | USB HS CDC data forwarding |

### 4.2 Task_UART_Receive

```
UART RX interrupt → ring_buffer (4KB)
    → frame_parser (detect magic + CRC)
    → protobuf_decode (GlovePacket)
    → frame_pairer (match L+R by sequence_id)
    → feature_assembler (28-dim vector)
    → inference_queue (send to Task_Inference)
```

### 4.3 Task_Inference (Tier2 on P4)

**Model**: Gated Bidirectional CrossAttention (same architecture as `glove_relay/src/models/stgcn_model.py`)

| Parameter | Value |
|-----------|-------|
| Input | 28-dim × T=30 frames |
| Model size | ~80KB (INT8 quantized) |
| Arena memory | ~16KB (PSRAM) |
| Inference latency | ~30ms @ 400MHz RV32 |
| Classes | 46 |
| Framework | TFLite Micro (initial) → ESP-DL (optimization) |

**Migration pipeline**:
```
PyTorch model (.py) → ONNX export → TFLite convert → INT8 quantization → C header → P4 firmware
```

### 4.4 Task_Display (LVGL UI)

**Display**: 7" MIPI-DSI, 1024×600, capacitive touch (GT911)

**Layout**:
- Top status bar: C6 connection | Battery L/R | Active Tier | FPS
- Left half: Left hand gesture + confidence bar + 5-finger flex bar chart
- Right half: Right hand gesture + confidence bar + 5-finger flex bar chart
- Bottom: Last 10 recognition results scroll log

**Performance**: 30fps render, 10Hz data update from inference results

### 4.5 Task_Audio (TTS)

**Hardware**: ES8311 codec (I2S) → NS4150 speaker (1W)

| Parameter | Value |
|-----------|-------|
| Sample rate | 16,000 Hz |
| Bit depth | 16-bit |
| Channels | Mono |
| I2S pins | Pre-wired on EV Board — use ESP-IDF `esp_codec_dev` component (auto-detects board I2S pinout) |

**TTS strategy**:
1. Pre-record 46 gesture names as raw PCM files
2. Store on MicroSD: `/tts/{gesture_id}.pcm`
3. On recognition: load PCM → I2S DMA playback
4. Dedup: same gesture within 2 seconds is not re-played

### 4.6 Task_USB (PC Connection)

**USB 2.0 HS OTG**: 480 Mbps (40× improvement over V5.0's 12 Mbps)

- Output: Full Protobuf `DualGloveData` message to PC
- Compatible with existing Relay `udp_server.py` (add USB CDC source)
- Bidirectional: PC can send configuration commands to P4

---

## 5. Three-Tier Hot-Switch

| State | Inference | Precision | Latency | Scenario |
|-------|-----------|-----------|---------|----------|
| Full chain | PC Tier3 | ~95% (60+ classes) | ~50ms | Normal operation |
| PC disconnected | P4 Tier2 | ~90% (46 classes) | ~30ms | **Standalone demo** |
| Base station disconnected | Glove Tier1 | ~85% (~20 classes) | ~5ms | Offline/fallback |

**Transition**: 5-frame linear blend (50ms)
```
output_t = α * output_new + (1 - α) * output_old
α: 0.2 → 0.4 → 0.6 → 0.8 → 1.0
```

**Heartbeat**: 100ms intervals. 3 missed = downgrade.

**Competition highlight**: System works fully standalone without PC — P4 base station runs inference + display + audio independently.

---

## 6. Deferred to Post-Competition (V5.3)

| Feature | V5.2 Spec | This Design | V5.3 Plan |
|---------|-----------|-------------|-----------|
| UWB DW3000 | Included | **Deferred** | After competition |
| 29-dim features | Included | **28-dim** | Add UWB distance dim |
| MIPI-CSI camera | Included | **Deferred** | Visual supplement |
| Tier2+ visual | Included | **Deferred** | Camera fusion |
| EKF fusion | Included | **Deferred** | IMU+UWB filter |
| Model retraining | Included | **No retrain** | 29-dim retrain |

---

## 7. Development Plan (3 Weeks)

### Week 1 (Jun 10-16): P4 Base Station Foundation

| Day | Task | Deliverable |
|-----|------|-------------|
| 1-2 | ESP-IDF P4 project setup + C6 ESP-NOW receive | C6 receives glove data |
| 3-4 | C6→P4 UART relay + Protobuf decode on P4 | P4 parses glove packets |
| 5-6 | LVGL basic UI + USB HS forwarding to PC | Screen shows data, PC receives |
| 7 | End-to-end integration test | Full data path verified |

### Week 2 (Jun 17-23): Tier2 Inference + Display

| Day | Task | Deliverable |
|-----|------|-------------|
| 1-3 | Tier2 model export (PyTorch→ONNX→TFLite INT8) | model_data.h ready |
| 4-5 | TFLite Micro integration on P4 | Tier2 inference running on P4 |
| 6-7 | LVGL full UI with inference results | Screen shows gesture names + confidence |

### Week 3 (Jun 24-30): Audio + Integration + Polish

| Day | Task | Deliverable |
|-----|------|-------------|
| 1-2 | ES8311 I2S audio + TTS PCM playback | Audio output working |
| 3-4 | Three-tier hot-switch integration test | Auto-downgrade verified |
| 5-6 | End-to-end latency optimization + stability | <100ms total latency |
| 7 | Competition demo rehearsal | Demo script ready |

---

## 8. BOM Impact

| Item | Cost | Status |
|------|------|--------|
| ESP32-P4-Function-EV-Board v1.5.2 | ¥0 (competition-provided) | **Have it** |
| MicroSD 16GB Class10 | ~¥15 | Need to buy |
| USB-C cable (P4 to PC) | ~¥5 | Likely included |
| Ethernet cable (optional backup) | ~¥5 | Optional |
| **Total additional cost** | **~¥20-25** | — |

No new hardware for gloves. No UWB modules needed.

---

## 9. Risk Assessment

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| P4 ESP-IDF ecosystem maturity | Medium | High | Use stable ESP-IDF 5.x features only |
| Tier2 model TFLite conversion | Low | Medium | Model architecture is simple (FC + Conv1D) |
| C6 UART stability | Low | Medium | 2Mbps with CRC, proven protocol |
| LVGL MIPI-DSI driver | Medium | Medium | P4 EV Board has official examples |
| ES8311 audio driver | Low | Low | EV Board has onboard codec, examples available |
| 3-week timeline | **High** | **Critical** | Strict scope control, defer non-essentials |

---

## 10. Innovation Points (Competition Scoring)

| # | Innovation | Level | Description |
|---|-----------|-------|-------------|
| 1 | Three-tier inference hot-switch | System | Glove→P4→PC auto-downgrade |
| 2 | P4 smart base station | System | Inference + display + audio + USB, no PC needed |
| 3 | Gated Bi-CrossAttention | Academic | Dual-hand cross-attention for sign language |
| 4 | ESP-NOW + BLE 5.0 dual-mode | Engineering | Real-time data + OTA/pairing |
| 5 | Standalone sign language translator | System | Complete hand-sign→text+voice without PC |
| 6 | 28-dim dual-hand features | Engineering | Relative pose features for cross-hand gestures |
