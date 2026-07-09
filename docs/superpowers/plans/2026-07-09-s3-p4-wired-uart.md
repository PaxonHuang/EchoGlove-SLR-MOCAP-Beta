# S3→P4 UART Wired Communication — Implementation Plan

**日期**: 2026-07-09
**分支**: `feature/v6-dual-s3p4-flex-lsm6dsv16x`
**Tag**: `v5.3-wired-dev` (待打)
**设计文档**: `docs/superpowers/specs/2026-07-08-s3-p4-wired-uart-design.md`

## 执行摘要

将 S3 手套→P4 基站的通信从 ESP-NOW 无线临时切换为 UART 有线直连，绕过 C6 ESP-Hosted 协处理器不支持 ESP-NOW 的架构限制。本计划采用 **TDD 方式**，分 3 阶段执行，确保每一步可验证。

---

## 阶段 0: 准备工作

### 0.1 打 Tag 标记当前状态

```bash
git tag -a v5.3-wired-dev -m "Pre-wired-UART baseline: A3 verified, C6 deferred, ESP-NOW active"
git push origin v5.3-wired-dev
```

### 0.2 确认硬件连接

| S3 DevKit | P4 EV Board | 线缆 |
|-----------|-------------|------|
| GPIO 6 (TX) | GPIO 38 (RX) | 1× 跳线 |
| GND | GND | 1× 跳线 |

**注意**: P4 GPIO 37 是 UART0 TX，GPIO 38 是 UART0 RX（已在 p4_firmware 中配置）。

### 0.3 确认引脚无冲突

**S3 GPIO 审计结果**:
- ✅ GPIO 6: FREE，可用作 UART1 TX
- ❌ GPIO 1–5: V6 内部 ADC（弯曲传感器）
- ❌ GPIO 8–9: I2C（保留给 LSM6DSV16X）
- ❌ GPIO 11–17: SPI Flash
- ❌ GPIO 19–20: USB CDC（Serial 调试）
- ❌ GPIO 26–32: OPI PSRAM

---

## 阶段 1: S3 侧 UARTTransmitter 实现（TDD）

### 1.1 创建测试骨架

**文件**: `glove_firmware/test/test_uart_transmitter.cpp` (native)

```cpp
#include <unity.h>
#include "UARTTransmitter.h"
#include "uart_frame.h"
#include "data_structures.h"

void test_uart_transmitter_encode_frame() {
    UARTTransmitter tx;
    uint8_t buf[128];
    GlovePacket pkt;
    pkt.magic[0] = 0x45; pkt.magic[1] = 0x47;
    pkt.version = 5;
    pkt.hand_id = HAND_LEFT;
    pkt.tick_id = 1;
    pkt.computeChecksum();

    size_t len = tx.encodeFrame(pkt, buf, sizeof(buf));
    TEST_ASSERT_EQUAL(73, len);  // 2 magic + 69 payload + 2 CRC
    TEST_ASSERT_EQUAL_UINT8(0xAA, buf[0]);
    TEST_ASSERT_EQUAL_UINT8(0x55, buf[1]);
}

void test_uart_transmitter_buffer_too_small() {
    UARTTransmitter tx;
    uint8_t buf[10];
    GlovePacket pkt;
    size_t len = tx.encodeFrame(pkt, buf, sizeof(buf));
    TEST_ASSERT_EQUAL(0, len);  // error
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_uart_transmitter_encode_frame);
    RUN_TEST(test_uart_transmitter_buffer_too_small);
    return UNITY_END();
}
```

### 1.2 实现 UARTTransmitter

**文件**: `glove_firmware/lib/Comms/UARTTransmitter.h`

```cpp
#pragma once
#include <Arduino.h>
#include "data_structures.h"
#include "uart_frame.h"

class UARTTransmitter {
public:
    bool begin(int tx_pin, int baud = 2000000) {
        Serial1.begin(baud, SERIAL_8N1, -1, tx_pin);  // RX unused, TX only
        return true;
    }

    size_t encodeFrame(const GlovePacket& pkt, uint8_t* buf, size_t buf_len) {
        return uart_frame_encode(&pkt, sizeof(GlovePacket), buf, buf_len);
    }

    void send(const GlovePacket& pkt) {
        uint8_t frame[UART_FRAME_MAX_SIZE];
        size_t len = encodeFrame(pkt, frame, sizeof(frame));
        if (len > 0) {
            Serial1.write(frame, len);
        }
    }
};
```

### 1.3 Native 测试通过

```bash
cd glove_firmware
pio test -e native -f test_uart_transmitter
```

**验证**: 2/2 测试通过。

---

## 阶段 2: S3 main.cpp 集成

### 2.1 添加编译宏控制

**文件**: `glove_firmware/platformio.ini` (在 `[env:esp32-s3-devkitc-1-n16r8]` 中)

```ini
; Wired UART fallback (parallel to ESP-NOW)
-DWIRED_UART=1
-DUART_TX_PIN=6
```

### 2.2 修改 Task_Comms

**文件**: `glove_firmware/src/main.cpp`

```cpp
#include "UARTTransmitter.h"

static UARTTransmitter uart_tx;

void setup() {
    // ... existing init ...
    
#if WIRED_UART
    uart_tx.begin(UART_TX_PIN, 2000000);
    Serial.printf("[WIRED] UART TX on GPIO %d @ 2Mbps\n", UART_TX_PIN);
#endif
}

static void Task_Comms(void *pvParameters) {
    while (1) {
        GlovePacket pkt;
        if (xQueueReceive(comms_queue, &pkt, pdMS_TO_TICKS(20)) == pdTRUE) {
            // ESP-NOW broadcast (primary)
            esp_err_t err = esp_now_send(BROADCAST_ADDR,
                                          (uint8_t*)&pkt, sizeof(pkt));
            
#if WIRED_UART
            // Wired UART fallback (parallel)
            uart_tx.send(pkt);
#endif
        }
    }
}
```

### 2.3 构建验证

```bash
pio run -e esp32-s3-devkitc-1-n16r8
```

**验证**: 编译通过，无错误。

---

## 阶段 3: P4 侧配置

### 3.1 禁用内部 Mock

**文件**: `glove_firmware/p4_base_station/p4_firmware/sdkconfig.defaults`

```diff
- CONFIG_P4_INTERNAL_MOCK=y
+ # CONFIG_P4_INTERNAL_MOCK=y  (disabled for real UART input)
```

### 3.2 重新构建 + 烧录

```bash
cd glove_firmware/p4_base_station/p4_firmware
idf.py set-target esp32p4
idf.py build
idf.py -p /dev/ttyACM0 flash
```

---

## 阶段 4: 硬件接线 + 端到端验证

### 4.1 接线

```
S3 DevKit          P4 EV Board
┌─────────┐        ┌───────────┐
│ GPIO 6  ├───────►│ GPIO 38   │  (TX → RX)
│ GND     ├────────┤ GND       │
└─────────┘        └───────────┘
```

### 4.2 烧录 S3

```bash
cd glove_firmware
pio run -e esp32-s3-devkitc-1-n16r8 -t upload
```

### 4.3 验证 P4 接收

**预期日志** (`/dev/ttyACM0`):

```
I (xxx) uart_rx: UART0 init: TX=37 RX=38 baud=2000000
I (xxx) p4_main: Pair tick=0 feat[0]=0.xxx feat[27]=0.xxx
I (xxx) p4_main: Tier2 #10: gesture=X conf=0.xxx time=xxx us
```

**失败排查**:
- 无 `Pair tick` 日志 → 检查 S3 TX → P4 RX 连线
- `UART RX count: 0` → 检查波特率是否匹配（必须 2Mbps）
- 帧解码失败 → 检查 GND 连接（CRC 校验失败通常由电气噪声引起）

---

## 阶段 5: 双手扩展（可选，后续 session）

### 5.1 P4 第二 UART

- P4 UART2 可用引脚: 待确认（参考 ESP32-P4 数据手册）
- 需要第二个 `uart_receiver` 实例

### 5.2 接线

```
S3 (L) GPIO 6 ──► P4 GPIO 38 (UART0 RX)
S3 (R) GPIO 6 ──► P4 GPIO ?? (UART2 RX)
```

---

## 验收标准

| 阶段 | 验收标准 |
|------|----------|
| 1 | Native 测试 2/2 通过 |
| 2 | S3 构建成功，Serial1 初始化日志出现 |
| 3 | P4 构建 + 烧录成功，无内部 mock 日志 |
| 4 | P4 接收到真实 GlovePacket（Pair tick 日志出现），FramePairer 正确配对 |

---

## 风险与缓解

| 风险 | 概率 | 缓解措施 |
|------|------|----------|
| GPIO 6 被其他外设占用 | 低 | 已审计，GPIO 6 FREE |
| 波特率不匹配 | 中 | 强制 2Mbps，S3/P4 端统一 |
| 电平不兼容（3.3V） | 低 | 两边均为 3.3V，兼容 |
| 帧同步失败 | 中 | uart_frame.h 已有 magic + CRC，抗噪声 |
| 双手总线竞争 | — | 单手阶段无竞争；双手用双 UART 物理隔离 |

---

## 回滚计划

若 UART 方案失败，恢复到 ESP-NOW 无线:

```bash
# S3: 移除 WIRED_UART 宏，重新烧录
# P4: 恢复 CONFIG_P4_INTERNAL_MOCK=y，或等待 C6 Wi-Fi 集成
```

---

## 参考

- 设计文档: `docs/superpowers/specs/2026-07-08-s3-p4-wired-uart-design.md`
- GPIO 审计: 本文档 §0.3
- uart_frame.h: `glove_firmware/shared/uart_frame.h`
- P4 uart_receiver: `glove_firmware/p4_base_station/p4_firmware/main/uart_receiver.cpp`