# EchoGlove 生产设计 — EgoGlove V7 对齐（Hand Token v1 + 46 类下游）

> **Date**: 2026-08-10
> **Status**: 设计冻结（已决策，待实施排期）
> **Branch**: `feature/v6-dual-s3p4-flex-lsm6dsv16x`
> **Alignment**: EgoGlove monorepo（V7 设计目标，见 `EgoGlove/docs/V7/STRATEGY.md` + `ARCHITECTURE.md`）

---

## 0. 决策摘要（本设计依据的用户决策）

| # | 决策 | 结论 |
|---|------|------|
| D-A | P0 修复范围 | **B1+B3+B7 完整**（本轮已落地，见 §2） |
| D-B | 生产输出契约 | **Hand Token v1 (79B) + 46 类下游分类** |
| D-C | S3 IMU/Madgwick 时点 | **进生产设计规范，下一工作包实现** |
| D-D | 生产代码归属 | **双轨**：Beta 修 P0 保交付；EgoGlove 开 Lite 生产线 |

---

## 1. 目标架构（双轨）

```
[Beta 交付线 · 立即]                           [EgoGlove 生产线 · 逐步]
S3 (flex+IMU) → ESP-NOW 69B → relay(mock/demo)  S3 Lite → ESP-NOW 79B Hand Token v1 → relay
                                                          → 46 类 SLR (GatedBiCrossAttn)
                  竞赛演示路径不动                      → Hand Token → MANO/OpenXR/Robot (V7 D3)
```

- **Beta 仓库** = 当前交付（竞赛演示 + P0 修复），演示路径 `scripts/demo_server.py`（asl_classifier/rule_classifier）独立于主 relay，不受 P0 修复影响。
- **EgoGlove** = 生产线。复用已实现并单测的 `firmware/shared/hand_token.c`（v1 79B 编解码）+ `relay/hand_token.py`（Python mirror）+ `relay/openxr_adapter.py`。
- 生产输出从「11-dim 特征 → 46 类手势」升级为 **Hand Token v1 中间表示**，46 类 SLR 降级为下游消费头（V7 D3 转向）。

---

## 2. Beta P0 修复（已落地，本设计的前置）

修复清单（均已在 `feature/v6-dual-s3p4-flex-lsm6dsv16x` 落地并通过验证）：

- **B1** `glove_relay/src/models/adapters.py`（新增）：`Tier1Adapter` / `CrossAttnAdapter` 包装裸 `nn.Module`，统一 `BaseModel` 契约（`predict()` 接受 numpy/CPU 输入并转到模型 device）。
- **B1** `model_registry.py`：类映射改指适配器；移除 `ms_tcn_v1`/`stgcn_v1`（stgcn_v1 保留 catalog 注明视觉侧）；warmup 形状 `(1,11)/(1,22)`。
- **B3** `model_config.yaml`：`input_dim 21→11`；`active_l2_model → gated_cross_attn_v1`（补 catalog 条目 + init_kwargs）。
- **B3** `base_model.py` 文档字符串 21→11。
- **B7** `relay_config.yaml`：`mock.enabled: true→false`；`RELAY_MOCK=1` env 覆盖（`config.py`）。
- **顺带** `udp_server.py::_run_l2`：窗口从单手 (T,11) 改双手 (T,22) 拼接，对齐 CrossAttnAdapter 契约。

**验证**：registry 非 mock 加载 + warmup + L1/L2/3D-window predict + 热切换全部通过（8/8）；模型测试（tier1/cross-attn）通过。⚠️ `pytorch_env` 缺 fastapi/uvicorn 等（既有环境缺口，pip 代理断），全 app 启动待 `/setup-env` 后补验。

---

## 3. 线格式契约 — Hand Token v1（79B）

**来源**：`EgoGlove/firmware/shared/hand_token.h/.c`（已实现 + `test_hand_token.c` host 单测）✅

| 字段 | 类型 | Lite 填充 | 状态 |
|------|------|-----------|------|
| magic `"HT"` + version `0x01` | u8×3 | — | ✅ |
| device_id（bit7 product / bit6 hand / bits0-5 serial） | u8 | product=Lite | ✅ |
| timestamp_us（单调，~71min 回绕，relay 处理） | u32 | — | ✅ |
| flex[5]（指间关节角，归一化 0..1，拇→小） | f16×5 | 实际值 | ✅ flex-ADC |
| quat[4]（手掌姿态，SFLP w,x,y,z） | f16×4 | **LSM6DSV16X+Madgwick** | 🟡 驱动待 |
| wrist_6dof[6]、vel[3]、acc[3]、contact[5]、force[5] | f32/f16/u8 | 0 | 🟡/🔬/🌌 |

传输无关：UART / ESP-NOW / BLE / USB-CDC / WiFi-UDP 均可承载；CRC-16/MODBUS 帧尾（与 `uart_frame.h` 同算法）。

**对齐动作（下一工作包）**：S3 Lite 固件用 `hand_token_serialize()` 产出 79B 帧替代 69B GlovePacket；relay 用 `hand_token.py::parse` 替代 protobuf 解析。

---

## 4. IMU 融合 — LSM6DSV16X + Madgwick（规范，代码下一包）

- **接线**（V6 既有规格，`docs/V6/03_wiring_diagram.md` §2）：I²C @0x6A（SA0 LOW），S3 GPIO8 SDA / GPIO9 SCL，400kHz；CS 拉高（LOW=SPI）；SDX/SCX 不接。
- **驱动**：仓库内本地驱动（非 PlatformIO 注册表库）；ODR **120Hz**（芯片无 104Hz 档，120Hz=0b0110 为最接近档；`Task_SensorRead` 同步 120Hz），满量程 ±4g / ±2000dps 起步。
- **融合实现**：**Host Madgwick**（β≈0.1，用户已确认；非芯片内置 SFLP 融合）。**物理限制：yaw（绕重力轴旋转）加速度计不可观**——tilt 收敛，yaw 保持有界但不修正（需磁力计或视觉，属 Pro/roadmap）。
- **姿态**：S3 上跑 **Madgwick**（增益 β≈0.1），输出 **SFLP 四元数**（w,x,y,z）写入 Hand Token `quat[4]`。视觉/VIO 属 Pro/roadmap（D7），S3 不做。
- **特征落位**：euler 由 quat 派生（roll/pitch/yaw），与 5×flex 组成 11 维特征喂 L1；双手拼接 22 维喂 L2（GatedBiCrossAttention）。
- **验证目标**：静止 60s 姿态漂移 < 3°；手翻转 90° 响应 < 200ms。

---

## 5. 通信 — ESP-NOW 79B

- 沿用现有 S3 ESP-NOW 广播链路（✅ 已实现，~2ms 延迟）；ESP-NOW 载荷上限 250B > 79B，无需改动链路层。
- BLE/WiFi（V7 Lite 计划，🟡）为后续：手机/PC 邻接、续航优先场景；本设计不阻塞。

---

## 6. Relay — Hand Token 解析 → 46 类 → 前端

```
USB-CDC / UDP → hand_token.parse → 46 类 SLR (GatedBiCrossAttention) → WS → React
                                        └─ Hand Token 原文透传 → MANO/OpenXR (openxr_adapter)
```

- **46 类 SLR**：`GatedBiCrossAttention`（双手 11 维交叉注意力，已在 Beta `tier2_cross_attn.py`，本轮适配器包好）迁入 EgoGlove `models/slr/`；L1 = `Tier1CNN`（单手 11 维）。
- **Hot-switch**：沿用 Beta `model_registry.py` 模式（本轮修好）→ EgoGlove relay 复用同样结构。
- **NLP/TTS**：语法校正（grammar_corrector）+ edge-tts 作为 SLR 下游展示头，不阻塞。

---

## 7. 数据管道

- **现状**：数据资产几乎为空（`synthetic_dataset.npz` 21 维伪造 + 30 个 `zero_*.csv`）。
- **生产采集**：以 Hand Token v1 帧为单位落盘（CSV + 可选 npz），relay 侧 `data_collector` 支持 **serial + UDP + USB-CDC** 三源（现状仅 serial，UDP 模式 B8 待实现）。
- **训练**：`scripts/train_tier1.py`（已是 11 维）→ 产出 `.pt` checkpoint → INT8 导出供 S3/P4 用。当前无任何 checkpoint（B2，随机权重），本轮不解决；数据量上来后训练是 46 类精度前提。
- **真实性**：synthetic 数据仅能验证管线形状，不能当训练集（V7 真实性分级）。

---

## 8. 硬件现实 — ring 通道 ch3

- **已确认故障**：ring 传感器（GPIO4）分压输出固定 ~114mV，6 次诊断 + 换件均无摆动 → 物理损坏。
- **当前**：竞赛演示 mask ch3，用 4 好通道（A/B/I/L 最小类间距 0.923 >> 噪声 0.1）。
- **生产路径**：① 特征管道 mask ch3（软件，立即）；② 硬件修复（换传感器/补焊/通道重映射，硬件批次）。

---

## 9. V7 决策映射

| V7 决策 | 本设计落位 |
|---------|-----------|
| D1 视觉主导+可穿戴增强 | 视觉=世界态/手套=手态，融合=意图；第一代硬件不进 CV |
| D3 双表示层 | Hand Token 分叉 MANO Layer + Robot Action Layer；46 类 SLR 为下游消费头 |
| D6 Lite/Pro 双产品线 | Lite = flex + 单腕 IMU + ESP-NOW 79B；Pro 多 IMU/力/视觉（roadmap） |
| D7 第一代不进 CV | Pro 预留 EGO Camera 接口；S3 只跑 Madgwick |
| D9 双生态通信 | 消费侧 WiFi/BT + 机器人侧 ROS2/Ethernet（后续） |
| D10/D11 Hand Token v2 Skeleton Layer | v1 永久兼容（本次）；v2 canonical-20/FK-21/TLV 作为后续演进，不阻塞 |
| D12 开放手部运动基础设施 | 厂商手套=外部数据源/适配器；Hand Token v1 是通用中间表示 |

---

## 10. 实施排期（里程碑）

| 里程碑 | 内容 | 归属 | 状态 |
|--------|------|------|------|
| M0 | P0 修复 B1/B3/B7 + 验证 | Beta | ✅ 已落地 |
| M1 | `/setup-env` 补依赖 + 全 app 启动冒烟（pytest 全套） | Beta | 待跑 |
| M2 | S3 Lite 固件：LSM6DSV16X 驱动 + Madgwick → quat 进特征 | EgoGlove/下轮 | 📋 计划已出 (2026-08-11) |
| M3 | S3 输出 Hand Token v1 (79B)，relay 解析 | EgoGlove/下轮 | 待排 |
| M4 | 46 类 SLR 迁 EgoGlove + 数据采集 + 训练 checkpoint | EgoGlove/后续 | 待排 |
| M5 | BLE/WiFi 通信 + Hand Token v2 Skeleton（V7 roadmap） | EgoGlove/后续 | 🌌 |

---

## 11. 真实性总表（四级标注）

| 能力 | 标注 |
|------|------|
| flex-ADC（internal ADC1, 5 通道） | ✅ |
| S3 ESP-NOW 广播（69B） | ✅ |
| Hand Token v1 codec（C + Python + 单测） | ✅（EgoGlove） |
| P4 UART + USB-CDC + mock | ✅ |
| LSM6DSV16X 驱动 + Madgwick 姿态 | 🟡 待实现（M2） |
| Hand Token v1 上链（S3 发射/relay 解析） | 🟡 待实现（M3） |
| 46 类 SLR 真实权重 + 精度 | 🔬 需采集训练数据 |
| BLE/WiFi 通信 | 🟡 |
| VIO / EGO 视觉融合 | 🌌 Pro/roadmap |
