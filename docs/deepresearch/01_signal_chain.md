# 01 · 信号链：从传感器到 46 类预测的完整数据路径

> **Date**: 2026-08-11 ｜ **依据**: 生产设计 `docs/superpowers/specs/2026-08-10-egoglove-aligned-production-design.md` + 当前代码 ｜ **真实性**: ✅ flex / 🟡 IMU / 🟡 79B 上链

---

## 1. 总览：三级推理拓扑

EchoGlove 采用**三级推理 + 双轨生产**架构：传感器数据在 S3 手套侧归一化为统一中间表示（Hand Token v1），经传输无关链路到基站/PC，由 L1（边缘）/ L2（基站）/ L3（视觉世界态）三个消费头之一产出 46 类手势。

```
 S3 Glove (edge)                        P4 Base / PC Relay                 Frontend
┌───────────────────────┐   ESP-NOW   ┌──────────────────────┐   WS:8765  ┌──────────────┐
│ flex ×5  ADC1  N=16   │   ~2ms 69B  │  L2 22-dim → 46      │──────────►│ React+R3F    │
│ IMU      I²C  120Hz   │────────────►│  L3 ST-GCN (视觉)     │           │ 21-pt 3D hand │
│ 11-dim → L1 (46)      │             │  USB-CDC / UDP        │           └──────────────┘
│ Hand Token 79B        │             └──────────────────────┘
└───────────────────────┘
     │
     └──(wired)──UART 2Mbps 73B frame [0xAA55 | payload | CRC16]──► P4
```

**延迟预算**（性能目标，`CLAUDE.md`）：

$$
t_{\text{E2E}} = t_{\text{sensor}} + t_{\text{fusion}} + t_{\text{comm}} + t_{\text{L1/L2}} + t_{\text{WS}} < 100\,\text{ms}
$$

| 段 | 目标 | 依据 |
|----|------|------|
| 传感器采样 | 100–120 Hz（10–8.3 ms） | `Task_SensorRead` 100 Hz / M2 对齐 120 Hz |
| L1 边缘推理 | < 3 ms | `glove_firmware/lib/Models/` 性能目标 |
| ESP-NOW 通信 | ~2 ms | V6 实测（当前 69B 路径） |
| L2 基站推理 | < 20 ms | P4 Tier2 性能目标 |
| 端到端 | < 100 ms | 竞赛演示红线 |

---

## 2. 传感器层

### 2.1 flex 弯曲传感器 → 内部 ADC1 ✅

5 路 flex 传感器接 **ESP32-S3 内部 ADC1**（GPIO1–5），16 次过采样（N=16），NVS 存储校准确偏置与增益。

- 归一化：$\tilde{f}_i = \mathrm{clip}\left(\frac{V_i - b_i}{g_i},\;0,\,1\right)$，$i=1..5$，$b_i,g_i$ 为 NVS 校准偏置/增益。
- 输出约定：`flex[5]`，拇→小指顺序，0..1 归一化指间关节角（1=弯曲）。
- 现状：✅ 已实现 + 板上验证；NVS 校准 M2 不做（M4）。

```
GPIO1 ─┐ flex1           ┌──────────────────────────────────┐
GPIO2 ─┤ flex2           │  InternalADC1Manager             │
GPIO3 ─┤ flex3  ──►  ADC1 ── N=16 过采样 ──►  NVS 校准 ──►  flex[5] (0..1)
GPIO4 ─┤ flex4           └──────────────────────────────────┘
GPIO5 ─┘ flex5            (ring 通道 ch3 已知物理损坏, 生产路径 mask)
```

> ⚠️ **ring 通道故障**（生产设计 §8）：GPIO4（ch3）分压固定 ~114 mV，6 次诊断+换件无摆动 → 物理损坏。竞赛演示已 mask ch3（4 好通道最小类间距 0.923 >> 噪声 0.1）；生产路径 = 特征管道软件 mask（立即）+ 硬件修复（批次）。

### 2.2 IMU LSM6DSV16X → 姿态四元数 🟡

- **接线**：I²C @0x6A（SA0 LOW），GPIO8 SDA / GPIO9 SCL，400 kHz；CS 拉高（LOW→SPI）。
- **配置**（M2 计划，已对照 datasheet 核实）：ODR **120Hz**（0b0110，芯片无 104Hz 档）；满量程 ±4g / ±2000dps；灵敏度 0.122 mg/LSB、70 mdps/LSB。
- **姿态**：S3 上跑 **Host Madgwick**（非芯片 SFLP），输出 SFLP 四元数 `quat[4]`（w,x,y,z）。详见 `02_imu_fusion_madgwick.md`。
- 现状：驱动 + Madgwick 已在 M2 host 验证（`EgoGlove` 计划 `2026-08-11-lite-lsm6dsv16x-madgwick.md`），**板上待验** 🟡。

```
LSM6DSV16X                     S3 (Host)
┌────────────┐   I²C 400kHz   ┌──────────────────────────────┐
│ ACC ±4g    │──SDA GPIO8───►│  lsm6dsv16x.c (驱动, DRDY 门控) │
│ GYRO±2000dps│──SCL GPIO9───►│  Madgwick (β≈0.1)  quat[4]     │
│ ODR 120Hz  │               └──────────────────────────────┘
└────────────┘
```

### 2.3 特征向量（11 维 / 22 维）✅ 结构

单只手（L1）11 维 = **5 flex + 3 euler + 3 gyro**：

$$
\mathbf{x}_h = [\; \underbrace{\tilde{f}_1 \dots \tilde{f}_5}_{\text{flex}}, \;
                    \underbrace{\varphi_{\text{roll}}, \varphi_{\text{pitch}}, \varphi_{\text{yaw}}}_{\text{姿态 euler(°)/100}}, \;
                    \underbrace{\omega_x, \omega_y, \omega_z}_{\text{gyro(°/s)/2000}} \;]^\top \in \mathbb{R}^{11}
$$

双手（L2）22 维 = `[left(11) ‖ right(11)]` 拼接（`glove_relay/src/models/adapters.py` 的 CrossAttnAdapter 契约：`(batch, 22)` 或 `(batch, T, 22)` 窗口）。

> ⚠️ 现状：`euler`/`gyro` 由 IMU 提供，IMU 未实现前 = 0（V6 现状 IMU=zeros）。真实训练数据依赖 M2 之后的采集。

---

## 3. 中间表示：Hand Token v1（79B）🟡 上链

特征序列化进入**传输无关**的 Hand Token v1 帧（详见 `03_hand_token_protocol.md`）：

```
magic "HT"|ver 0x01|dev_id|t_us(u32)|flex[5]×f16|quat[4]×f16|wrist[6]×f32|vel|acc|contact|force|CRC16
   2 B      1 B      1 B     4 B        10 B         8 B         24 B      6B  6B  5B   10B  2B  = 79 B
```

- 传输承载：UART / ESP-NOW / BLE / USB-CDC / WiFi-UDP 均可（ESP-NOW 载荷上限 250B > 79B，链路层无需改动）。
- 现状：codec（C + Python mirror + host 单测）✅（`EgoGlove/firmware/shared/hand_token.*`）；**S3 发射 / relay 解析 79B 上链属 M3** 🟡。当前 S3 发的是 69B GlovePacket（V6 legacy，竞赛演示路径不动）。

---

## 4. 通信链路（多跳）

| 段 | 协议 | 帧 | 现状 |
|----|------|----|------|
| S3 → P4/PC | ESP-NOW 广播 | 69B（V6）/ 79B（M3）| ✅ 当前活动路径（~2ms） |
| S3 → P4（备选）| 直连 UART | 2 Mbps，`[0xAA 0x55 69B CRC16L CRC16H]` = 73B | 🟡 `WIRED_UART` flag 未进代码 |
| P4 → PC | USB-CDC | JSON | ✅ `usb_cdc_server.py` |
| PC relay → Web | WebSocket | 特征/预测 | ✅ 端口 8765 |

**CRC-16/MODBUS**（`shared/uart_frame.h` 与 `hand_token.h` 同算法）：poly 0xA001，init 0xFFFF，小端帧尾。

---

## 5. 推理消费头（46 类下游）

生产输出契约 = **Hand Token v1 中间表示 + 46 类下游分类**（设计冻结，`specs/2026-08-10` 决策 D-B）：

```
Hand Token v1 ──► L1 Tier1CNN (11→46)        单手边缘        [04]
              ──► L2 GatedBiCrossAttention (22→46) 双手基站 [05]
              ──► L3 ST-GCN 视觉侧 (世界态)             [06]
              ──► NLP 语法校正 + edge-tts（SLR 下游展示头）[08]
              ──► MANO / Robot Action Layer（V7 D3 双表示）[09]
```

- 模型热切换：`BaseModel` 接口 + `model_registry.py` + `model_config.yaml`（适配器包裸 `nn.Module`，见 `adapters.py`）。B1/B3 修复后 8/8 验证通过（registry/warmup/L1/L2/3D-window/hot-switch）。
- ⚠️ 权重现状：所有模型为**随机权重**（B2），46 类精度需真实数据 + 训练（`07_training_quant.md`）。synthetic 数据仅验证管线形状。

---

## 6. 端到端数据流（relay 侧）

```
USB-CDC / UDP ──► udp_server._run_l2 ──► 双手 (T,22) 窗口 ──► CrossAttnAdapter.predict ──► (class, conf)
                                                    │
                                                    └──► WS ──► React (21-pt 3D hand, 手势标签)
```

**L2 窗口契约**（`udp_server.py::_run_l2`，B7 修复后）：双手 11 维特征按时间拼接 `(T, 22)`，交叉注意力模型在时间轴取均值（`tier2_cross_attn.py`）。

---

## 7. 现状 vs 生产（信号链缺口清单）

| 缺口 | 影响 | 归属 |
|------|------|------|
| IMU=zeros（驱动未实现） | euler/gyro 特征为 0 → 11 维实际只有 5 flex 有效 | M2（EgoGlove）🟡 |
| S3 仍发 69B GlovePacket | 未上 Hand Token v1 线 | M3 🟡 |
| ring ch3 物理损坏 | 第 4 指 flex 不可用 | 软件 mask（立即）+ 硬件批次 |
| 无真实训练数据 / checkpoint | 46 类精度无从谈起（随机权重） | M4 采集 + 训练 🔬 |
| P4 视觉 L3 未接 | 世界态融合属 V7 D1 | 🌌 |

---

*真实性：flex-ADC ✅；ESP-NOW 69B ✅；UART/USB-CDC ✅；Hand Token codec ✅；IMU+Madgwick 🟡（M2 host 已验，板上待验）；79B 上链 🟡（M3）；真实权重 46 类 🔬。*
