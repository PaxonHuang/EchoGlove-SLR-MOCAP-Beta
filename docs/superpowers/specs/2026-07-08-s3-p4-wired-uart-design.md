# EchoGlove V5.3 — S3↔P4 有线 UART 开发方案

**日期**: 2026-07-08
**分支**: `feature/v6-dual-s3p4-flex-lsm6dsv16x`
**状态**: 设计中 (待硬件审计完成)

## 背景与动机

### 问题
P4 EV Board 上的 C6 是 **ESP-Hosted Wi-Fi/BT 协处理器**（SDIO 总线，出厂预烧 slave 固件），**不支持 ESP-NOW**。原 `c6_firmware` mock-ESP-NOW bridge 与此硬件不兼容。详见 [[p4-ev-board-c6-esp-hosted]]。

### 现状（代码审计结论）
- **S3 固件**: 仅有 ESP-NOW 一个数据出口（`esp_now_send` 50Hz，raw 69 字节无帧封装）
- **P4 固件**: `uart_receiver` 已完整可用（2Mbps，CRC-16/MODBUS 帧解码）
- **共享协议**: `uart_frame.h` 已就绪（73 字节帧 = 2 magic + 69 payload + 2 CRC），但 **未接入 S3**
- **P4 EV Board USB 端口**（官方文档）:
  - USB Full-speed Port（#15）— 供电/通信
  - USB Serial/JTAG Port（#16）— 烧录/调试/JTAG，`/dev/ttyACM0`
  - USB 2.0 Type-C Port（#17）— USB 2.0 OTG High-Speed，P4 作 Device（TinyUSB CDC 已用此口）
  - USB 2.0 Type-A Port（#18）— 同 OTG High-Speed，P4 作 Host，500mA 输出（#17/#18 二选一）

## 方案选择

**选定: S3→P4 UART 直连**（用户决策 2026-07-08）

### 为什么选 UART 而非 USB CDC
1. **代码改动最小**: P4 侧 `uart_receiver` 零改动；S3 仅需加一个 UART TX 模块
2. **协议已就绪**: `uart_frame.h` 帧格式直接复用，与未来 C6→P4 UART 路径完全一致
3. **无 USB 端口冲突**: 不占用 P4 唯一的 USB 2.0 OTG 口（已被 TinyUSB CDC 占用）
4. **低延迟**: 2Mbps UART 传输 73 字节 ≈ 0.36ms，远低于 ESP-NOW ~2ms 目标
5. **可保留到生产**: 作为 ESP-NOW 的 wired fallback

### 为什么不立即做 USB CDC（S3→P4 USB Host）
- P4 侧需写 USB Host CDC 驱动（非 trivial，工作量大）
- 占用 P4 唯一 USB OTG 口，与 TinyUSB CDC 冲突
- 双手场景需两个 USB 口，硬件不支持

## 架构设计

### 数据流（开发阶段，单手）

```
S3 Glove (L)                    P4 Base Station
┌──────────────┐   UART 2Mbps   ┌──────────────────────┐
│ Task_Sensor  │   TX→RX         │ uart_receiver        │
│ Task_Comms   │─────────────────│   ↓                  │
│  + UART TX   │  [AA 55 69B CRC]│ FramePairer          │
│              │   1根TX + GND   │   ↓                  │
└──────────────┘                 │ inference + LVGL +   │
                                 │ USB CDC + TTS        │
                                 └──────────────────────┘
```

### 双手总线竞争问题（关键）

**问题**: 两个 S3 手套若共用同一 UART 总线，TX 信号会冲突。

**解决方案**: P4 双 UART 接收（待硬件审计确认可用 UART 后定稿）
```
S3 Glove (L) ──UART1 TX──→ P4 UART1 RX (GPIO 38)
S3 Glove (R) ──UART2 TX──→ P4 UART2 RX (GPIO 待定)
```
- 每只手套独占一根 TX 线 + GND，物理隔离，**无总线竞争**
- P4 侧两个 `uart_receiver` 实例，分别 feed `FramePairer`（按 `hand_id` 配对）
- 单手阶段先用 UART1，双手阶段加 UART2

### S3 侧改动（最小化）

新增 `glove_firmware/lib/Comms/UARTTransmitter.h`:
```cpp
class UARTTransmitter {
public:
    bool begin(HardwareSerial& port, int tx_pin, int baud = 2000000);
    void send(const GlovePacket& pkt);  // uart_frame_encode + write
};
```

`main.cpp` `Task_Comms` 改动: **并行发送**（ESP-NOW + UART 同时发），不替换：
```cpp
// 开发阶段: ESP-NOW + UART 双发
esp_now_send(BROADCAST_ADDR, (uint8_t*)&pkt, sizeof(pkt));
uart_tx.send(pkt);  // 有线 fallback
```
- 用编译宏 `WIRED_UART` 控制是否启用 UART（默认开）
- ESP-NOW 失败时（无 C6）UART 仍工作 → 有线为主路径

### P4 侧改动

**单手阶段**: 零改动。`CONFIG_P4_INTERNAL_MOCK=n` + `uart_receiver_init(0, 37, 38, 2000000)` 已就绪。
- 仅需把 `sdkconfig.defaults` 的 `CONFIG_P4_INTERNAL_MOCK=y` 改回 `=n`

**双手阶段**: `uart_receiver` 重构为多实例（或新增第二个 receiver），`main.cpp` 双路 feed FramePairer。

## 重构风险评估

| 风险点 | 评估 | 缓解 |
|--------|------|------|
| S3 UART TX 模块新写 | 🟢 低 | 复用 `uart_frame.h`，<100 行代码，可 native 测试 |
| P4 侧改动 | 🟢 低（单手）/ 🟡 中（双手） | 单手零改动；双手需 uart_receiver 多实例化 |
| GPIO 冲突 | 🟡 待审计 | 需确认 S3/P4 各有可用 UART TX/RX 引脚 |
| 双手总线竞争 | 🟢 已解决 | 双 UART 物理隔离，无竞争 |
| 帧同步 | 🟢 低 | `uart_frame.h` 已有 magic + CRC，抗噪声 |
| 生产保留 | 🟢 低 | UART TX 作为 ESP-NOW fallback，编译宏控制 |

## 实施计划（分步走）

### 阶段 1: 单手 UART 直连（1-2 session）
1. 确认 S3 可用 TX 引脚（硬件审计）
2. 写 `UARTTransmitter.h` + native 测试
3. S3 `main.cpp` 接入并行 UART TX
4. P4 `sdkconfig.defaults`: `CONFIG_P4_INTERNAL_MOCK=n`
5. 硬件接线: S3 TX → P4 GPIO38 (RX) + GND
6. 烧录 S3 + P4，验证 P4 收到真实 GlovePacket

### 阶段 2: 双手双 UART（后续 session）
1. 确认 P4 第二可用 UART + RX 引脚
2. `uart_receiver` 多实例化
3. `main.cpp` 双路 feed FramePairer
4. 接第二只手套，验证配对

### 阶段 3: C6 ESP-Hosted Wi-Fi 集成（生产）
1. P4 加 `espressif/esp_hosted` + `espressif/esp_wifi_remote` 组件
2. C6 作为 Wi-Fi 协处理器（出厂固件即可用）
3. S3 通过 Wi-Fi（UDP/WS）而非 ESP-NOW 发送到 P4
4. UART TX 保留为 wired fallback（编译宏 `WIRED_UART`）

## C6 后期爆发风险预防

**风险**: 若后期才发现 C6 路径走不通（已确认 ESP-Hosted 不支持 ESP-NOW），会阻塞整个无线化。

**预防**:
- ✅ 已确认 C6 = ESP-Hosted 协处理器，**不走 ESP-NOW 路径**
- ✅ 生产无线方案明确: C6 作 Wi-Fi 协处理器，S3→P4 走 Wi-Fi（UDP/WS）
- ✅ UART 有线方案是**独立可用**的开发路径，不依赖 C6
- ✅ ESP-Hosted 出厂即用，Wi-Fi 集成风险可控（官方 iperf demo 已验证）

## 结论

**推荐执行**: S3→P4 UART 直连方案，分步走，单手先跑通。

**核心优势**:
- 不误删任何现有代码（ESP-NOW 保留）
- P4 单手阶段零改动
- 双手总线竞争通过物理双 UART 解决
- C6 无线问题已隔离，不阻塞开发
- 有线代码可保留到生产作 fallback
