# EchoGlove 科研级原理文档索引

> **Date**: 2026-08-11
> **Status**: 分批滚动发布（首批：01 / 02 / 03）
> **依据**: V6 生产设计 `docs/superpowers/specs/2026-08-10-egoglove-aligned-production-design.md`（设计冻结）+ 当前代码（`glove_relay/`、`glove_firmware/`、`EgoGlove/firmware/shared/`）
> **真实性标注**: ✅ 已实现 · 🟡 工程可实现（6–12 月）· 🔬 需研发验证 · 🌌 长期方向（与 `EgoGlove/docs/V7/ARCHITECTURE.md` §8 同分级）

---

## 文档全景（系统信号流）

```
┌─────────────────────────────── S3 手套（边缘，L1） ───────────────────────────────┐
│                                                                                  │
│   flex ×5 ──ADC1(GP1-5)──┐                                                    │
│   归一化 0..1            ├──► 11 维特征 ──► L1 Tier1CNN(11→46)   [01][04]         │
│                          │      [5 flex, 3 euler, 3 gyro]                       │
│   LSM6DSV16X ──I²C@0x6A──┘      ↑                                                │
│   (120Hz, ±4g/±2000dps)      Madgwick quat ──euler──┘        [02]               │
│                                                                                  │
│   特征 ──► Hand Token v1 (79B 固定帧) ──► ESP-NOW/UART      [03]                 │
└──────────────────────────────────────────────────────────────────────────────────┘
                                     │
                                     ▼
┌─────────────────────────── P4 基站 / PC Relay（L2/L3） ──────────────────────────┐
│   UART 2Mbps (73B) → USB-CDC → relay                                            │
│                                                                                  │
│   L2 GatedBiCrossAttention 双手 22 维 ──► 46 类 SLR        [05]                 │
│   L3 ST-GCN 视觉侧 (12/42 节点) 世界态 ──► 46 类           [06]                 │
│   ──► NLP 语法校正 ──► edge-tts TTS（下游展示头）            [08]                 │
│   ──► 训练 + INT8 量化（当前无真实 checkpoint）             [07]                 │
└──────────────────────────────────────────────────────────────────────────────────┘
                                     │
                                     ▼
                        Hand Token 双表示分叉（V7 D3）
        ┌──────────────────────────────┬──────────────────────────────┐
        ▼                              ▼                              ▼
   MANO Layer                Robot Action Layer              FK→MANO 双表示
   (数字人 21 点)             (遥操作 / 机械手)                 [09]
        [09]
```

---

## 目录

| # | 文档 | 主题 | 核心公式 / 关键结论 | 真实性 |
|---|------|------|---------------------|--------|
| 01 | `01_signal_chain.md` | 信号链：flex ADC + IMU I²C → 11 维特征 → Hand Token → 三跳链路 | 特征归一化、E2E 延迟预算 <100ms | flex ✅ / IMU 🟡 / 79B 上链 🟡 |
| 02 | `02_imu_fusion_madgwick.md` | LSM6DSV16X 寄存器 + Host Madgwick 梯度下降 AHRS | Madgwick 梯度 Jᵀf、**2026-08-11 三处符号修正决策**、yaw 不可观、离散不动点 | 驱动 🟡 / 算法 ✅(host) / SFLP 🟡 |
| 03 | `03_hand_token_protocol.md` | Hand Token v1 (79B) 线格式：canonical 布局、f16/f32、CRC-16/MODBUS、v2 TLV | 79B 偏移表、f16 编解码、CRC poly 0xA001 | codec ✅ / 上链 🟡 / v2 🌌 |
| 04 | `04_l1_edge_cnn.md` | L1 Tier1CNN + SEBlock 边缘推理（11→46 类） | MLP+SE 等价公式、INT8 部署预算 | 结构 ✅ / 权重 🔬 |
| 05 | `05_l2_gated_bi_cross_attn.md` | L2 双手门控双向交叉注意力（22→46 类） | Q/K/V 双向交叉、门控融合 | 结构 ✅ / 权重 🔬 |
| 06 | `06_l3_stgcn_vision.md` | L3 视觉侧 ST-GCN（12/42 节点世界态） | 图卷积归一化、时空块 | 结构 ✅ / 视觉 🌌 |
| 07 | `07_training_quant.md` | 训练 + INT8 量化（数据管道、checkpoint、TFLite/INT8 导出） | 量化误差界、校准集 | 🔬（无真实数据/权重） |
| 08 | `08_nlp_tts.md` | SLR 下游：规则语法校正 + edge-tts | 规则表、音素时长模型 | 结构 ✅ / 精度 🔬 |
| 09 | `09_fk_mano_dual_rep.md` | FK→MANO 双表示层（Hand Token 分叉） | FK 关节角→21 点、MANO 层 | 🌌 / FK 🟡 |

---

## 真实性总表（跨文档）

| 能力 | 标注 | 依据 |
|------|------|------|
| flex 内部 ADC1（GPIO1–5, N=16, NVS 校准） | ✅ | `glove_firmware/lib/Sensors/InternalADCManager` + 板上验证 |
| S3 ESP-NOW 广播（69B GlovePacket） | ✅ | `glove_firmware/src/main.cpp`（当前活动通信路径） |
| P4 UART RX + USB-CDC + mock 验证 | ✅ | `glove_firmware/p4_base_station/`（`CONFIG_P4_INTERNAL_MOCK=y` 单机验证） |
| Hand Token v1 codec（C + Python + host 单测） | ✅ | `EgoGlove/firmware/shared/hand_token.{h,c}` + `firmware/shared/test/` |
| L1/L2/L3 模型结构（Tier1CNN / GatedBiCross / ST-GCN） | ✅ 结构 | `glove_relay/src/models/`（随机权重） |
| LSM6DSV16X 驱动 + Host Madgwick 姿态 | 🟡 | `EgoGlove` M2 计划（2026-08-11）已 host 验证，板上待验 |
| Hand Token v1 上链（S3 发射 / relay 解析 79B） | 🟡 | M3（EgoGlove 下轮） |
| 46 类 SLR 真实权重 + 精度（>90/95% Top-1） | 🔬 | 需采集训练数据 + 训练 checkpoint（当前无） |
| ST-GCN 视觉侧融合（世界态） | 🌌 | V7 D1 视觉主导，第一代硬件不进 CV |
| NLP 语法校正 + edge-tts | ✅ 结构 / 🔬 精度 | `glove_relay/src/nlp/` + `tts/`（展示头，不阻塞） |
| FK→MANO / Robot Action Layer 双表示 | 🌌 | V7 D3，Hand Token v1 是中间表示 |

---

## 关键决策记录（跨会话）

- **2026-08-11 — Madgwick 梯度三处符号修正**：M2 计划 `madgwick.c` 的 `s = Jᵀf` 原实现非标准（f1 公式 + s0/s1 的 f1 Jacobian 系数各一处符号错），导致重力 −y 收敛到 roll +90° 而非物理真值 −90°。已对照真 Jacobian 与部署参考实现（arduino-libraries/MadgwickAHRS `updateIMU`）修正，host 全 5 测试通过。详见 `02_imu_fusion_madgwick.md` §决策记录。
- **2026-08-11 — IMU ODR 120Hz**：芯片无 104Hz 档（0b0110=120Hz 为最接近），与 `Task_SensorRead` 120Hz 对齐。修正生产设计 §4（原 104Hz）。

---

## 写作约定

1. **公式**：LaTeX 语法（`$$` 块 / `$` 行内），变量名与代码一致。
2. **网络图**：科研级 ASCII，标注张量维度 `(B, T, N, C)`。
3. **真实性**：每能力段首标注四级符号；未核实内容显式写「待人工核对」。
4. **引用**：代码路径用仓库相对路径（`file:line` 可点击）。
