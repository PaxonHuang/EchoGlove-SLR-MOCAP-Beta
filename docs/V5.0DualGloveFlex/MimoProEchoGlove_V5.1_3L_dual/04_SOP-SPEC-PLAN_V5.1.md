# EchoGlove V5.1 SOP-SPEC-PLAN — 双手手语识别与动作捕捉系统

> 版本: V5.1 | 三级热切换 + Gated Bi-CrossAttention + 分层ST-GCN + MS-TCN→CTC  
> 分支: `Flex2.2in-BNO085-ADS1115-ESP32S3N16R8-ArduinoFramework`  
> 日期: 2026-06

---

## 目录

1. [项目愿景](#1-项目愿景)
2. [架构决策记录(ADR)](#2-架构决策记录adr)
3. [系统架构总览](#3-系统架构总览)
4. [Phase 1: 单手固件稳定化](#4-phase-1-单手固件稳定化)
5. [Phase 2: 单手数据采集与模型训练](#5-phase-2-单手数据采集与模型训练)
6. [Phase 3: Unity XR可视化](#6-phase-3-unity-xr可视化)
7. [Phase 4: 双手系统升级](#7-phase-4-双手系统升级)
8. [Phase 5: 三级热切换架构](#8-phase-5-三级热切换架构)
9. [Phase 6: 进阶优化](#9-phase-6-进阶优化)
10. [核心模型架构深度设计](#10-核心模型架构深度设计)
11. [全链路迁移影响矩阵](#11-全链路迁移影响矩阵)
12. [已知风险与缓解方案](#12-已知风险与缓解方案)

---

## 1. 项目愿景

### 1.1 项目定义

EchoGlove V5.1 是一套 **Edge-AI 驱动的双手数据手套系统**，面向实时手语翻译与手部动作捕捉场景。系统核心能力如下：

- **实时手语翻译**：支持 46 类基础手势（Phase 2 验证）→ 扩展至 60+ 类双手协同手势（Phase 6）
- **3D 手部动画渲染**：React Three Fiber MVP + Unity XR Hands 1.7 双前端
- **三级推理架构**：手套边缘（Tier1）→ 接收器（Tier2）→ PC（Tier3），热切换无缝过渡
- **双手完整支持**：左右手独立采集 + Gated Bi-CrossAttention 跨手融合

### 1.2 V5.1 核心升级（相比 V5.0）

| 序号 | 升级项 | V5.0 | V5.1 | 收益 |
|------|--------|------|------|------|
| 1 | ST-GCN 图结构 | 统一 42 节点 | 分层 12/42 节点 | 边缘推理减半 |
| 2 | Tier2 模型 | 完整 L2 栈 | 仅 Gated Bi-CrossAttn (~80KB) | 接收器推理更快 |
| 3 | 传感器架构 | Flex+IMU（残留霍尔代码） | Flex+IMU（霍尔完全移除） | 代码清洁，维护简化 |
| 4 | 数据集策略 | 固定 46 类 | 分阶段 46→60+ | 渐进验证，降低风险 |
| 5 | 前端方案 | Unity 单轨 | React3F MVP + Unity XR 并行 | 快速迭代 + 沉浸体验 |

### 1.3 版本演进表

| 版本 | 传感器 | 架构 | L1 方案 | L2 方案 | 创新点 |
|------|--------|------|---------|---------|--------|
| V3.0 | Hall + IMU | 单级 ESP32 | SVM | — | 首次 IMU 融合 |
| V3.1 | Flex(单手) + IMU | 单级 ESP32 | CNN | — | Flex 替代 Hall |
| V4.0 | Flex(双手) + IMU | 双 ESP32 + 接收器 | CNN + Attn | BiLSTM | 双手架构 |
| V5.0 | Flex + IMU | 三级热切换 | CNN + Attn | ST-GCN + MS-TCN | 三级推理 |
| **V5.1** | **Flex + IMU** | **三级热切换（确认版）** | **CNN + SE-Attn** | **分层 ST-GCN + Gated Bi-CrossAttn + CTC** | **分层图 + 门控融合** |

### 1.4 关键性能指标

| 指标 | 目标 | 测量条件 |
|------|------|----------|
| L1 推理延迟 | <30ms | ESP32-S3 240MHz, TFLite int8 |
| L2 推理延迟 | <20ms | GPU (RTX 3060+), PyTorch |
| 端到端延迟（传感器→前端渲染） | <100ms | 全链路含通信 |
| L1 精度 (46 类) | >80% Top-1 | 验证集 |
| L2 精度 (46 类) | >85% Top-1 | 验证集 |
| L3 精度 (46 类) | >92% Top-1 | 验证集, 全模型 |
| 传感器采样率 | 100Hz | ADS1115 + BNO085 |
| 电池续航 | >2小时 | 1000mAh LiPo |
| ESP-NOW 同步延迟 | <5ms | 双手套 tick 配对 |

---

## 2. 架构决策记录(ADR)

> 每条 ADR 遵循标准格式：**状态 · 背景 · 决策 · 选项对比 · 后果 · 论文/项目支撑**

---

### ADR-D1: 传感器选型 — Flex 替代 Hall

- **状态**：已决定（V3.1 起）
- **背景**：霍尔传感器需要磁铁配合，安装复杂，磁场干扰大，线性度差。
- **决策**：全面采用 Flex 传感器（2.2" 弯曲传感器）+ BNO085 IMU。
- **选项对比**：

| 方案 | 精度 | 安装复杂度 | 抗干扰 | 成本 | 体积 |
|------|------|-----------|--------|------|------|
| Hall + 磁铁 | 中 | 高（需磁铁对准） | 低（磁场干扰） | 低 | 大 |
| **Flex 2.2"** | **高** | **低（直接粘贴）** | **高** | **中** | **小** |
| 压力传感器 | 中 | 中 | 高 | 高 | 中 |

- **后果**：所有手指标定改为 Flex 分段线性校准；移除 Hall 相关代码；I2C 地址重新规划。
- **支撑**：StretchSense 公司 Flex 传感器技术白皮书；SignLanguage-Glove 开源项目验证。

---

### ADR-D2: 双 ADC 方案 — ADS1115 × 2

- **状态**：已决定（V4.0 起）
- **背景**：ESP32-S3 内置 ADC 精度不足（12-bit, 非线性），5 路 Flex 需要高精度模拟采集。
- **决策**：使用两片 ADS1115（16-bit I2C ADC），地址 0x48（Flex 0-2）和 0x49（Flex 3-4）。
- **选项对比**：

| 方案 | 精度 | 通道数 | 接口 | 成本 | 布线 |
|------|------|--------|------|------|------|
| ESP32 内置 ADC | 12-bit | 8 | 内部 | 0 | 简单 |
| ADS1115 × 1 | 16-bit | 4 | I2C | ¥8 | 简单 |
| **ADS1115 × 2** | **16-bit** | **8** | **I2C** | **¥16** | **适中** |
| ADS1256 | 24-bit | 8 | SPI | ¥25 | 复杂 |

- **后果**：I2C 总线需 100kHz 限速；引入 0x48/0x49 双地址管理；每通道采样时间 ~1ms。
- **支撑**：ADS1115 datasheet；Adafruit ADS1115 库实测。

---

### ADR-D3: IMU 选型 — BNO085

- **状态**：已决定（V3.0 起）
- **背景**：手部姿态需要 9-DOF 融合（加速度计+陀螺仪+磁力计），传统方案需手动融合。
- **决策**：采用 Bosch BNO085，硬件融合输出四元数/欧拉角/陀螺仪。
- **选项对比**：

| 方案 | 融合方式 | 输出 | 功耗 | 延迟 | 地址 |
|------|----------|------|------|------|------|
| MPU6050 | 软件 DMP | 6-DOF | 低 | 高 | 0x68 |
| ICM-20948 | 软件 AKM | 9-DOF | 中 | 中 | 0x68 |
| **BNO085** | **硬件 SensorHub** | **9-DOF (GRV/Euler/Gyro)** | **中** | **低** | **0x4B** |

- **后果**：使用 GRV 模式（不依赖磁力计，避免磁场干扰）；输出 6 维（euler×3 + gyro×3）。
- **支撑**：BNO085 datasheet；Hillcrest Labs SH-2 协议文档。

---

### ADR-D4: 主控选型 — ESP32-S3-N16R8

- **状态**：已决定（V4.0 起）
- **背景**：双手系统需要 ESP-NOW 通信 + TFLite 推理 + FreeRTOS 多任务。
- **决策**：ESP32-S3-N16R8（16MB Flash, 8MB PSRAM, 双核 240MHz）。
- **选项对比**：

| 方案 | Flash | PSRAM | 核心 | WiFi | BLE | AI加速 |
|------|-------|-------|------|------|-----|--------|
| ESP32-WROOM | 4MB | 0 | 双核 | ✅ | ✅ | ❌ |
| ESP32-S3-N8R2 | 8MB | 2MB | 双核 | ✅ | ✅ | ✅ |
| **ESP32-S3-N16R8** | **16MB** | **8MB** | **双核** | **✅** | **✅** | **✅** |
| ESP32-P4 | 32MB | 32MB | 双核 | ✅ | ✅ | ✅ |

- **后果**：Flash 足够存放 TFLite 模型 + 固件；PSRAM 足够 Tier2 推理缓冲区；ESP-NOW 兼容。
- **支撑**：Espressif ESP32-S3 技术参考手册；TFLite-Micro ESP32-S3 移植指南。

---

### ADR-D5: 通信协议 — ESP-NOW + USB Serial

- **状态**：已决定（V4.0 起）
- **背景**：手套→接收器需要低延迟无线通信；接收器→PC 需要可靠有线传输。
- **决策**：手套↔接收器用 ESP-NOW（~2ms 延迟）；接收器↔PC 用 USB Serial/Protobuf。
- **选项对比**：

| 方案 | 延迟 | 功耗 | 可靠性 | 距离 | 复杂度 |
|------|------|------|--------|------|--------|
| BLE | 10-30ms | 低 | 中 | 10m | 中 |
| **ESP-NOW** | **~2ms** | **低** | **高** | **10m** | **低** |
| WiFi TCP | 5-20ms | 高 | 高 | 30m | 高 |
| NRF24L01 | ~1ms | 低 | 高 | 100m | 中 |

- **后果**：需实现 SYNC_TICK 广播同步；丢包需插值补帧；Protobuf 序列化。
- **支撑**：Espressif ESP-NOW API 文档；Google Protobuf 嵌入式方案。

---

### ADR-D6: 开发框架 — PlatformIO + Arduino Framework

- **状态**：已决定（V3.0 起）
- **背景**：ESP32-S3 开发需要稳定的构建系统和库管理。
- **决策**：PlatformIO 构建 + Arduino Framework（非 ESP-IDF 原生）。
- **选项对比**：

| 方案 | 上手难度 | 库生态 | 调试 | 构建速度 | FreeRTOS支持 |
|------|----------|--------|------|----------|-------------|
| Arduino IDE | 低 | 丰富 | 差 | 慢 | 有限 |
| **PlatformIO + Arduino** | **中** | **丰富** | **好** | **快** | **完整** |
| ESP-IDF 原生 | 高 | 中 | 好 | 快 | 完整 |

- **后果**：platformio.ini 配置管理；库依赖通过 lib_deps 声明；CI/CD 友好。
- **支撑**：PlatformIO 官方文档；ESP32-S3 Arduino Core 维护活跃。

---

### ADR-D7: ST-GCN 分层 — 12 节点 vs 42 节点

- **状态**：已决定（V5.1 新增）
- **背景**：V5.0 统一使用 42 节点 ST-GCN，边缘设备推理过慢（>50ms），且 Tier1/2 不需要完整手部骨架。
- **决策**：分层设计 — Tier1/2 使用 12 节点简化图，Tier3 使用 42 节点完整图。
- **选项对比**：

| 方案 | 节点数 | 边数 | 参数量 | 推理延迟(ESP32) | 精度损失 |
|------|--------|------|--------|-----------------|----------|
| 统一 42 节点 | 42 | 66 | 大 | >50ms | 无 |
| **分层 12/42** | **12/42** | **22/66** | **小/大** | **~25ms/>50ms** | **<2%** |
| 统一 12 节点 | 12 | 22 | 小 | ~25ms | >5% |

- **后果**：需维护两套图邻接矩阵；节点投影层需适配；训练时两套模型需分别训练。
- **支撑**：ST-GCN (CVPR 18) 原文图分区策略；Hand Graph Convolution (ECCV 20) 分层设计。

---

### ADR-D8: Tier2 精简 — 去掉 MS-TCN

- **状态**：已决定（V5.1 新增）
- **背景**：V5.0 Tier2 包含完整 L2 栈（ST-GCN + MS-TCN），在 ESP32-S3 上推理>80ms，超出实时要求。
- **决策**：Tier2 仅保留 Gated Bi-CrossAttn 模块（~80KB int8），去掉 MS-TCN 时序精炼。
- **选项对比**：

| 方案 | 模型大小 | 推理延迟 | 精度 | 适用场景 |
|------|----------|----------|------|----------|
| 完整 L2 栈 | ~250KB | >80ms | ~88% | 仅 PC |
| **仅 Gated CrossAttn** | **~80KB** | **~30ms** | **~85%** | **接收器** |
| 仅 CNN | ~40KB | ~15ms | ~78% | 极简 |

- **后果**：Tier2 精度略降（~3%），但推理速度提升 2.5×；MS-TCN 仅在 Tier3 使用。
- **支撑**：MS-TCN (CVPR 19) 原文延迟分析；Edge TPU 推理 benchmark。

---

### ADR-D9: 双手融合 — Gated Bidirectional CrossAttention

- **状态**：已决定（V4.0 提出, V5.1 确认）
- **背景**：双手手语中约 40% 为单手主导手势，空闲手的噪声会干扰分类。简单的特征拼接无法区分"空闲"与"协同"。
- **决策**：采用 Gated Bidirectional CrossAttention，通过可学习门控动态调节跨手信息流。
- **选项对比**：

| 方案 | 参数量 | 空闲手处理 | 精度(46类) | 复杂度 |
|------|--------|-----------|------------|--------|
| 特征拼接 | 0 | 无 | ~80% | 低 |
| CrossAttention (无门控) | ~8K | 无 | ~84% | 中 |
| **Gated Bi-CrossAttn** | **~16K** | **门控自适应** | **~87%** | **中** |
| Graph Neural Network | ~50K | 图结构 | ~89% | 高 |

- **后果**：门控层需 FP16 混合量化以避免 int8 精度下降；推理增加约 5ms。
- **支撑**：Gated Attention (ICLR 20)；Cross-Modal Attention (ACL 20)；Sign Language Recognition 综述。

---

### ADR-D10: 时序建模 — MS-TCN 多阶段精炼

- **状态**：已决定（V5.0 起, V5.1 仅 Tier3）
- **背景**：连续手语识别需要精确的时序边界检测，单阶段 TCN 感受野不足。
- **决策**：4-stage MS-TCN，每 stage 10 层，dilation 1→512，总感受野 2047 帧。
- **选项对比**：

| 方案 | 感受野 | 参数量 | 边界精度 | 推理延迟 |
|------|--------|--------|----------|----------|
| 单阶段 TCN | 255 帧 | ~50K | 中 | ~5ms |
| **4-stage MS-TCN** | **2047 帧** | **~200K** | **高** | **~15ms** |
| Transformer | 无限 | ~500K | 高 | ~30ms |

- **后果**：仅在 Tier3 使用；Smoothing Loss (λ=0.15) 防止过分割；渐进训练策略。
- **支撑**：MS-TCN (CVPR 19)；ASFormer (ECCV 21) 改进。

---

### ADR-D11: 三阶段 L2 架构 — ST-GCN → MS-TCN → CTC

- **状态**：已决定（V5.0 起）
- **背景**：连续手语识别需要空间建模 + 时序建模 + 序列解码三个阶段的配合。
- **决策**：L2 = ST-GCN（空间）→ MS-TCN（时序精炼）→ CTC（序列解码）。
- **选项对比**：

| 方案 | 空间建模 | 时序建模 | 解码 | 端到端 | 精度 |
|------|----------|----------|------|--------|------|
| CNN + LSTM + Softmax | 弱 | 中 | 帧级 | ❌ | ~82% |
| **ST-GCN + MS-TCN + CTC** | **强** | **强** | **序列级** | **✅** | **~92%** |
| Transformer + CTC | 中 | 强 | 序列级 | ✅ | ~90% |

- **后果**：三阶段可独立训练和优化；CTC blank 类需特殊处理；模型较大 (~3MB)。
- **支撑**：ST-GCN (CVPR 18) + MS-TCN (CVPR 19) + CTC (ICML 06) 经典组合。

---

### ADR-D12: 热切换机制 — 5 帧线性渐变

- **状态**：已决定（V5.0 起）
- **背景**：PC 断开/恢复时，Tier 切换会导致输出概率分布突变，前端动画跳变。
- **决策**：5 帧线性渐变 blend（~50ms），新旧 Tier 输出加权过渡。
- **选项对比**：

| 方案 | 切换延迟 | 平滑度 | 复杂度 |
|------|----------|--------|--------|
| 硬切换 | 0ms | 差（跳变） | 低 |
| **5 帧线性渐变** | **~50ms** | **好** | **中** |
| 温度校准 + 渐变 | ~80ms | 最佳 | 高 |

- **后果**：需维护上一 Tier 的输出缓冲区；blend 期间两 Tier 并行推理；温度校准可选叠加。
- **支撑**：模型集成 (Ensemble) 平滑技术；音频 crossfade 类比。

---

### ADR-D13: 数据集策略 — 分阶段 46→60+

- **状态**：已决定（V5.1 新增）
- **背景**：一次性采集 60+ 类双手手势工作量大、风险高；46 类单手手势已可验证基础架构。
- **决策**：Phase 2 先采集 46 类单手手势验证全流程；Phase 6 扩展到 60+ 类双手协同手势。
- **选项对比**：

| 方案 | 采集周期 | 风险 | 验证速度 | 架构覆盖 |
|------|----------|------|----------|----------|
| 一次性 60+ | 长 | 高 | 慢 | 全覆盖 |
| **分阶段 46→60+** | **短+中** | **低** | **快** | **渐进** |
| 仅 46 类 | 短 | 低 | 快 | 有限 |

- **后果**：Phase 2 仅需单手数据；Phase 6 需重新采集双手数据；模型需支持增量学习。
- **支撑**：增量学习 (Incremental Learning) 研究；手语数据集 WLASL/MS-ASL 分类标准。

---

### ADR-D14: 前端方案 — React3F MVP + Unity XR 并行

- **状态**：已决定（V5.1 新增）
- **背景**：Unity XR Hands 功能强大但开发周期长；React Three Fiber 可快速验证 3D 渲染。
- **决策**：Phase 3 先交付 React3F MVP（Web 端），同时并行开发 Unity XR Hands 版本。
- **选项对比**：

| 方案 | 开发速度 | 3D 质质 | 平台 | XR 支持 | 调试 |
|------|----------|---------|------|---------|------|
| **React Three Fiber** | **快** | **好** | **Web** | **有限** | **易** |
| Unity XR Hands | 慢 | 最佳 | PC/VR | 完整 | 中 |
| Three.js 原生 | 中 | 好 | Web | 无 | 易 |

- **后果**：两套前端代码需维护；数据协议需统一（WebSocket JSON）；React3F 可快速收集用户反馈。
- **支撑**：React Three Fiber 社区活跃；Unity XR Hands 1.7 官方示例。

---

### ADR-D15: CTC 解码策略 — Beam Search

- **状态**：已决定（V5.0 起）
- **背景**：CTC 贪心解码精度低，特别是在连续手语中边界模糊。
- **决策**：Beam Search k=10，blank_idx=0，配合语言模型约束。
- **选项对比**：

| 方案 | 精度 | 速度 | 复杂度 |
|------|------|------|--------|
| 贪心解码 | 低 | 最快 | 低 |
| **Beam Search k=10** | **高** | **快** | **中** |
| Prefix Beam Search | 最高 | 慢 | 高 |

- **后果**：Beam Search 增加约 2ms 延迟；需维护 beam 状态；语言模型可选。
- **支撑**：CTC (Graves, ICML 06)；Beam Search CTC (Hannun et al., 2014)。

---

### ADR-D16: 关节映射 — 5DoF → 26 关节线性耦合

- **状态**：已决定（V3.1 起）
- **背景**：5 个 Flex 传感器仅提供 5 个弯曲自由度，但手部有 26 个关节。
- **决策**：基于解剖学约束的线性耦合映射（拇指特殊处理，其余手指等比缩放）。
- **后果**：映射关系固定，无法捕捉手指间独立运动；Phase 6 可升级为 MANO 参数模型。
- **支撑**：手部解剖学文献；Leap Motion 关节映射参考。

---

### ADR-D17: BiLSTM+CTC 备选方案

- **状态**：已决定（V5.1 预留）
- **背景**：部分部署场景无 GPU，需要 CPU 可行的时序模型。
- **决策**：预留 BiLSTM(28→128×2) → FC → CTC 作为 Tier3 备选，CPU 推理~80ms。
- **后果**：精度略低于 ST-GCN 方案（~88% vs ~92%）；无需 GPU 依赖。
- **支撑**：BiLSTM+CTC 手语识别经典方案 (Camgoz et al., CVPR 18)。

---

### ADR-D18: 温漂补偿 — 自动零点校准

- **状态**：已决定（V3.1 起）
- **背景**：Flex 传感器受温度影响，静态弯曲角度会随温度偏移（典型 ±5°）。
- **决策**：每 30 秒自动零点校准，检测静止状态后记录偏移量。
- **后果**：校准期间需检测"静止"状态（角度变化<2°持续 1 秒）；存储零点偏移数组。
- **支撑**：Flex 传感器温漂特性 datasheet；工业传感器自动校准方案。

---

### ADR-D19: 丢包处理 — 插值补帧

- **状态**：已决定（V4.0 起）
- **背景**：ESP-NOW 无线传输存在丢包（典型 <1%），丢包导致 tick_id 不连续。
- **决策**：接收器检测 tick_id 间隙，使用线性插值补帧。
- **后果**：最多连续补 3 帧（超过则标记数据不可靠）；补帧数据置信度降低。
- **支撑**：实时数据传输插值方案；游戏网络同步技术。

---

### ADR-D20: 模型量化 — int8 + FP16 混合

- **状态**：已决定（V5.0 起）
- **背景**：int8 量化可大幅压缩模型和加速推理，但门控层 sigmoid 精度敏感。
- **决策**：主体 int8 量化，门控层（gate_proj）保留 FP16。
- **后果**：TFLite 需自定义量化配置；模型大小增加约 10%；门控精度保持。
- **支撑**：TensorFlow Lite 混合量化文档；Edge TPU 最佳实践。

---

## 3. 系统架构总览

### 3.1 三级推理架构表

| 层级 | 部署位置 | 模型 | 输入维度 | 输出 | 模型大小 | 推理延迟 | 精度(46类) | 场景 |
|------|----------|------|----------|------|----------|----------|------------|------|
| **Tier1** | ESP32-S3 (手套) | CNN + SE-Attention | 11-dim (单手) | 46 类概率 | ~148KB int8 | ~30ms | ~80% | 离线/边缘 |
| **Tier2** | ESP32-S3 (接收器) | Gated Bi-CrossAttn | 28-dim (双手+相对) | 46 类概率 | ~80KB int8 | ~30ms | ~85% | 无 PC |
| **Tier3** | PC (GPU) | ST-GCN + MS-TCN + CTC | 28-dim 序列 | 连续手语序列 | ~3MB FP32 | ~15ms/GPU | ~92% | 完整功能 |

### 3.2 数据流（完整版）

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                          EchoGlove V5.1 数据流                              │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ┌─────────────┐    ┌─────────────┐                                        │
│  │  左手手套    │    │  右手手套    │                                        │
│  │ ┌─────────┐ │    │ ┌─────────┐ │                                        │
│  │ │ADS1115×2│ │    │ │ADS1115×2│ │   ← 5×Flex (16-bit ADC)               │
│  │ │BNO085   │ │    │ │BNO085   │ │   ← 6-DOF (Euler+Gyro)               │
│  │ └────┬────┘ │    │ └────┬────┘ │                                        │
│  │      ▼      │    │      ▼      │                                        │
│  │ ┌─────────┐ │    │ ┌─────────┐ │                                        │
│  │ │Kalman   │ │    │ │Kalman   │ │   ← 11通道滤波                         │
│  │ │Filter   │ │    │ │Filter   │ │                                        │
│  │ └────┬────┘ │    │ └────┬────┘ │                                        │
│  │      ▼      │    │      ▼      │                                        │
│  │ ┌─────────┐ │    │ ┌─────────┐ │                                        │
│  │ │Tier1    │ │    │ │Tier1    │ │   ← CNN+Attn 单手推理                   │
│  │ │Model    │ │    │ │Model    │ │                                        │
│  │ └────┬────┘ │    │ └────┬────┘ │                                        │
│  │      ▼      │    │      ▼      │                                        │
│  │ ┌─────────┐ │    │ ┌─────────┐ │                                        │
│  │ │ESPNOW   │ │    │ │ESPNOW   │ │   ← GlovePacket (69 bytes)            │
│  │ │Manager  │ │    │ │Manager  │ │                                        │
│  └──────┬──────┘    └──────┬──────┘                                        │
│         │   ESP-NOW (~2ms) │                                                │
│         ▼                  ▼                                                │
│  ┌──────────────────────────────┐                                           │
│  │       接收器 (ESP32-S3)       │                                           │
│  │  ┌────────────────────────┐  │                                           │
│  │  │ SYNC_TICK 广播同步      │  │   ← <2ms 精度                             │
│  │  │ tick_id 配对 + 插值补帧 │  │                                           │
│  │  └───────────┬────────────┘  │                                           │
│  │              ▼               │                                           │
│  │  ┌────────────────────────┐  │                                           │
│  │  │ 相对特征计算            │  │   ← 28-dim = 11+11+6                      │
│  │  │ (delta_pos/orient/ang) │  │                                           │
│  │  └───────────┬────────────┘  │                                           │
│  │              ▼               │                                           │
│  │  ┌────────────────────────┐  │                                           │
│  │  │ Tier2: Gated Bi-XAttn  │  │   ← ~80KB, ~30ms                         │
│  │  └───────────┬────────────┘  │                                           │
│  │              ▼               │                                           │
│  │  ┌────────────────────────┐  │                                           │
│  │  │ USB Serial (Protobuf)  │  │                                           │
│  │  └───────────┬────────────┘  │                                           │
│  └──────────────┼───────────────┘                                           │
│                 │  USB (~1ms)                                               │
│                 ▼                                                            │
│  ┌──────────────────────────────┐                                           │
│  │         PC Relay 服务器       │                                           │
│  │  ┌────────────────────────┐  │                                           │
│  │  │ TierRouter             │  │                                           │
│  │  │  ├─ tier1_left/right   │  │                                           │
│  │  │  ├─ tier2_result       │  │                                           │
│  │  │  ├─ tier3_result       │  │   ← ST-GCN + MS-TCN + CTC               │
│  │  │  └─ blend (5帧渐变)    │  │                                           │
│  │  └───────────┬────────────┘  │                                           │
│  │              ▼               │                                           │
│  │  ┌────────────────────────┐  │                                           │
│  │  │ WebSocket Server       │  │   ← JSON V5.1 格式                       │
│  │  └───────────┬────────────┘  │                                           │
│  └──────────────┼───────────────┘                                           │
│                 │  WebSocket                                                 │
│         ┌───────┴───────┐                                                    │
│         ▼               ▼                                                    │
│  ┌─────────────┐  ┌──────────────┐                                          │
│  │ React 3F    │  │ Unity XR     │                                          │
│  │ 前端 (Web)  │  │ Hands (PC/VR)│                                          │
│  │ 3D手部渲染  │  │ 沉浸式体验   │                                          │
│  └─────────────┘  └──────────────┘                                          │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 3.3 热切换时序

#### 正常模式（PC 可用）

```
手套L ──ESP-NOW──▶ 接收器 ──USB──▶ PC ──Tier3推理──▶ WebSocket ──▶ 前端
手套R ──ESP-NOW──┘              ↑
                                │
                          TierRouter: active_tier = 3
                          blend_alpha = 1.0 (无混合)
```

#### PC 断开过渡

```
t=0ms:   PC 连接断开检测
t=0ms:   TierRouter: active_tier 切换为 2
t=0ms:   blend_alpha = 0.0, 旧输出 = Tier3 结果
t=10ms:  blend_alpha = 0.2, output = 0.8×Tier3 + 0.2×Tier2
t=20ms:  blend_alpha = 0.4, output = 0.6×Tier3 + 0.4×Tier2
t=30ms:  blend_alpha = 0.6, output = 0.4×Tier3 + 0.6×Tier2
t=40ms:  blend_alpha = 0.8, output = 0.2×Tier3 + 0.8×Tier2
t=50ms:  blend_alpha = 1.0, output = Tier2 完全接管
```

#### PC 恢复过渡

```
t=0ms:   PC 连接恢复检测
t=0ms:   Tier3 模型加载 + 预热推理
t=100ms: Tier3 首次推理完成
t=100ms: TierRouter: active_tier 切换为 3
t=100ms: blend_alpha = 0.0, 旧输出 = Tier2 结果
t=110ms: blend_alpha = 0.2, output = 0.8×Tier2 + 0.2×Tier3
...
t=150ms: blend_alpha = 1.0, output = Tier3 完全接管
```

### 3.4 ST-GCN 分层策略

#### Tier1/2: 12 节点简化图（边缘优化）

```
节点定义（每只手 6 个节点，共 12 个）：
  左手: L_Wrist, L_Thumb, L_Index, L_Middle, L_Ring, L_Pinky
  右手: R_Wrist, R_Thumb, R_Index, R_Middle, R_Ring, R_Pinky

邻接矩阵: 22 条边
  每手内部: Wrist→5指尖 (5边) + 指尖间连接 (5边) = 10边/手
  跨手: 无直接连接（由 Gated Bi-CrossAttn 处理）
  总计: 10×2 = 20 边 + 2 条自环 = 22 边

节点特征投影: 28-dim → 12×2 = 24-dim
  每个节点: [flex_angle, imu_feat] 投影为 2-dim 空间坐标
```

#### Tier3: 42 节点完整图（精确建模）

```
节点定义（每只手 21 个关节，共 42 个）：
  每只手: WRIST + 5×(MCP, PIP, DIP, TIP) = 21 节点
  左手 21 + 右手 21 = 42 节点

邻接矩阵: 66 条边
  每手内部: 骨架树结构 (20边) + 跨指连接 (13边) = 33边/手
  跨手: 无直接连接
  总计: 33×2 = 66 边

节点特征投影: 28-dim → 42×2 = 84-dim
  5DoF → 26关节映射 → 投影为 2-dim 空间坐标
```

#### 节点映射关系

```
12节点 → 42节点 映射:
  L_Wrist   → L_WRIST (节点0)
  L_Thumb   → L_THUMB_TIP (节点4)
  L_Index   → L_INDEX_TIP (节点8)
  L_Middle  → L_MIDDLE_TIP (节点12)
  L_Ring    → L_RING_TIP (节点16)
  L_Pinky   → L_PINKY_TIP (节点20)
  (右手同理，偏移+21)
```

---

## 4. Phase 1: 单手固件稳定化

### 4.1 目标

- ✅ 移除所有霍尔传感器相关代码（HallManager, HallCalibrator 等）
- ✅ 实现 ADS1115 + BNO085 稳定采样（100Hz, I2C 100kHz）
- ✅ 输出 11 维特征向量（5 Flex + 6 IMU）
- ✅ 预留 Tier1 推理框架（TFLite-Micro 框架编译通过）

### 4.2 关键模块

| 模块 | 文件路径 | 功能 | 依赖 |
|------|----------|------|------|
| ADS1115Manager | `lib/Sensors/ADS1115Manager.h` | 双 ADC 读取 5 路 Flex 原始电压 | Adafruit_ADS1115 |
| FlexManager | `lib/Sensors/FlexManager.h` | 5 点分段线性校准 + 滑动平均 + 温漂补偿 | — |
| IMUManager | `lib/Sensors/IMUManager.h` | BNO085 GRV 模式 6 轴 (euler×3 + gyro×3) | Adafruit_BNO085 |
| SensorManager | `lib/Sensors/SensorManager.h` | 统一管理所有传感器，输出 11-dim | 上述三者 |
| KalmanFilter | `lib/Filters/KalmanFilter.h` | 11 通道一阶卡尔曼滤波 | — |
| Tier1Model | `lib/Inference/Tier1Model.h` | TFLite-Micro 推理框架（预留） | TFLite_Micro |

### 4.3 ADS1115Manager 详细设计

```cpp
/**
 * ADS1115Manager — 双 ADS1115 ADC 管理器
 * 
 * 硬件配置:
 *   ADC1 (0x48): Flex 0, 1, 2 — 拇指/食指/中指
 *   ADC2 (0x49): Flex 3, 4 — 无名指/小指
 * 
 * I2C: 100kHz, 共享总线 (与 BNO085 0x4B 共存)
 * 采样率: 每通道 ~250SPS, 5通道轮询 → ~100Hz 有效
 */
class ADS1115Manager {
public:
    bool begin();
    bool readAll(float out[5]);        // 读取5路Flex原始电压 (V)
    bool isReady() const;
    uint16_t getSampleRate() const;    // 实际采样率

private:
    Adafruit_ADS1115 adc1;             // 0x48, Flex 0-2
    Adafruit_ADS1115 adc2;             // 0x49, Flex 3-4
    
    // 单端模式, ±4.096V 量程, 128SPS
    static constexpr ads1115_gain_t GAIN = GAIN_FOUR;  // ±1.024V
    static constexpr uint8_t ADDRESS_1 = 0x48;
    static constexpr uint8_t ADDRESS_2 = 0x49;
    
    float raw_voltages[5];
    unsigned long last_read_time;
    
    bool readADC(Adafruit_ADS1115 &adc, uint8_t channel, float &voltage);
};
```

### 4.4 FlexManager 详细设计

```cpp
/**
 * FlexManager — 5路 Flex 传感器管理器
 * 
 * 功能:
 *   1. 5点分段线性校准 (0°, 22.5°, 45°, 67.5°, 90°)
 *   2. 8点滑动平均滤波
 *   3. 自动零点温漂补偿 (每30秒)
 *   4. 输出范围: 0~90° 弯曲角度
 */
class FlexManager {
public:
    bool begin();
    void calibrate();                           // 5点分段线性校准
    void update(const float raw_voltages[5]);   // 输入原始电压
    void getAngles(float out[5]);               // 输出0~90°弯曲角度
    
    bool isCalibrated() const;
    void loadCalibration(const char* path);     // 从NVS加载校准参数
    void saveCalibration(const char* path);     // 保存校准参数到NVS

private:
    float calib_points[5][5];                   // 5传感器×5校准点 (电压值)
    float calib_angles[5];                      // 校准角度 {0, 22.5, 45, 67.5, 90}
    SlidingWindow<float, 8> windows[5];         // 8点滑动平均
    float zero_offsets[5];                      // 温漂零点偏移
    unsigned long last_zero_check;
    static constexpr unsigned long ZERO_CHECK_INTERVAL = 30000; // 30秒
    
    float interpolate(float voltage, uint8_t sensor_idx);
    void autoZeroCalibrate();                   // 检测静止状态后校准零点
    bool isStationary();                        // 角度变化<2°持续1秒
};
```

### 4.5 I2C 配置

```
I2C 总线配置:
  时钟频率: 100kHz (Wire.setClock(100000))
  上拉电阻: 4.7kΩ (板载)

设备地址:
  0x48 — ADS1115 #1 (Flex 0, 1, 2)
  0x49 — ADS1115 #2 (Flex 3, 4)
  0x4B — BNO085   (IMU, GRV模式)

地址冲突检查: 无冲突 ✓
总线仲裁: ESP32-S3 I2C 硬件自动仲裁
```

### 4.6 Protobuf 消息定义

```protobuf
// echoglove_v6.proto
syntax = "proto3";
package echoglove;

// 单手套数据包
message GloveData {
    uint32 timestamp     = 1;   // 毫秒时间戳
    uint32 hand_id       = 2;   // 0=left, 1=right
    repeated float flex_features = 3;  // 5个Flex弯曲角度 (0~90°)
    repeated float imu_features  = 4;  // 6个IMU值 (euler_x/y/z + gyro_x/y/z)
    uint32 l1_gesture_id = 5;   // Tier1 分类结果
    float  l1_confidence = 6;   // Tier1 置信度
    uint32 tick_id       = 7;   // 同步tick (用于双手配对)
    Tier1Result tier1_result = 8;  // Tier1 完整结果
}

// Tier1 推理结果
message Tier1Result {
    repeated float class_probabilities = 1;  // 46类概率
    uint32 top1_class   = 2;
    float  top1_conf    = 3;
    uint32 inference_ms = 4;   // 推理耗时
}

// 接收器转发包
message ReceiverPacket {
    uint32 timestamp         = 1;
    GloveData left_hand      = 2;
    GloveData right_hand     = 3;
    RelativeFeatures relative = 4;  // 双手相对特征
    Tier2Result tier2_result = 5;
    uint32 tick_id           = 6;
    bool left_valid          = 7;
    bool right_valid         = 8;
}

// 相对特征
message RelativeFeatures {
    float delta_pos_x     = 1;  // 右手相对左手 X
    float delta_pos_y     = 2;  // Y
    float delta_pos_z     = 3;  // Z
    float delta_orient_w  = 4;  // 相对方位角
    float delta_orient_diff = 5; // 方位差
    float delta_angular_vel = 6; // 相对角速度
}

// Tier2 推理结果
message Tier2Result {
    repeated float class_probabilities = 1;
    uint32 top1_class   = 2;
    float  top1_conf    = 3;
    uint32 inference_ms = 4;
}
```

### 4.7 FreeRTOS 任务分配

```
Core 1 (传感器核心):
  ┌─────────────────────────────────────────┐
  │ Task: sensor_task                       │
  │   Priority: 5 (最高)                    │
  │   Stack: 4096 bytes                     │
  │   Period: 10ms (100Hz)                  │
  │   功能: ADS1115×2 + BNO085 读取         │
  │         Kalman 滤波                      │
  │         输出 11-dim 特征到队列            │
  └─────────────────────────────────────────┘

Core 0 (推理+通信核心):
  ┌─────────────────────────────────────────┐
  │ Task: tier1_infer_task                  │
  │   Priority: 3 (中)                      │
  │   Stack: 8192 bytes                     │
  │   触发: 从传感器队列读取                  │
  │   功能: TFLite-Micro 推理                │
  │         输出分类结果                      │
  ├─────────────────────────────────────────┤
  │ Task: espnow_task                       │
  │   Priority: 3 (中)                      │
  │   Stack: 4096 bytes                     │
  │   功能: ESP-NOW 数据包发送               │
  │         SYNC_TICK 接收处理               │
  ├─────────────────────────────────────────┤
  │ Task: serial_task                       │
  │   Priority: 2 (低)                      │
  │   Stack: 2048 bytes                     │
  │   功能: USB Serial 数据输出              │
  └─────────────────────────────────────────┘

看门狗:
  Task Watchdog Timer (TWDT): 超时 1000ms
  监控: sensor_task (必须每周期喂狗)
```

### 4.8 验收标准

| 编号 | 验收项 | 通过条件 | 验证方法 |
|------|--------|----------|----------|
| P1-01 | 编译通过 | `pio run -e esp32s3` 无错误 | 命令行 |
| P1-02 | 11维数据输出 | 串口输出稳定 100Hz | 串口监视器 + 频率计 |
| P1-03 | Flex 角度范围 | 0~90°, 无跳变 (>5° 突变) | 物理弯折测试 |
| P1-04 | IMU 数据有效 | euler ∈ [-180,180], gyro 合理 | 静止/旋转测试 |
| P1-05 | I2C 无冲突 | 三设备正常通信 | I2C 扫描 + 日志 |
| P1-06 | Tier1 框架 | TFLite-Micro 编译通过 | `pio run` |
| P1-07 | 功耗 | 工作电流 <200mA | 万用表 |

---

## 5. Phase 2: 单手数据采集与模型训练

### 5.1 目标

- ✅ 采集 46 类手势数据集（每类 10 人×10 次×5 秒）
- ✅ 训练 L1 CNN+SE-Attention 模型（验证精度 >85%）
- ✅ 导出 Tier1 TFLite int8 模型（<200KB, 推理 <50ms）

### 5.2 数据采集工具

**文件**: `tools/collect_dataset.py`

```
功能:
  - pyserial 读取 ESP32 串口 (11-dim, 100Hz)
  - 按手势标签采集, 每标签 10次 × 5秒 = 50秒/类
  - 数据增强 (在线): 时间拉伸 (0.8-1.2×), 高斯噪声 (σ=0.01)
  - 保存格式: CSV + JSON 元数据

46类手势清单:
  01-09: 数字 1-9
  10-19: 字母 A-J
  20-29: 字母 K-T
  30-35: 字母 U-Z
  36-40: 常用词 (你好/谢谢/对不起/是/否)
  41-46: 常用手语 (吃饭/喝水/走/停/帮助/爱)

输出目录结构:
  dataset/
  ├── raw/
  │   ├── gesture_001_digit_1/
  │   │   ├── trial_001.csv
  │   │   ├── trial_001.json
  │   │   └── ...
  │   └── ...
  └── metadata.json
```

### 5.3 数据预处理

**文件**: `tools/preprocess_dataset.py`

```
处理流程:
  1. CSV 加载 → 11-dim 时序数据
  2. Kalman 滤波 (与固件相同的参数)
  3. Z-Score 归一化 (基于训练集统计量)
  4. 滑动窗口: 30帧窗口, 步长10帧
  5. 数据集划分: 训练/验证/测试 = 7:1:2
  6. 保存为 PyTorch .pt 格式

输出:
  processed/
  ├── train.pt    (features, labels)
  ├── val.pt
  ├── test.pt
  └── norm_params.json  (mean, std)
```

### 5.4 L1 模型架构

**文件**: `models/l1_cnn_attention.py`

```python
class L1CNNAttention(nn.Module):
    """
    Tier1 单手模型: CNN + SE-Attention
    
    输入: (batch, 30, 11) — 30帧 × 11维
    输出: (batch, 46) — 46类概率
    
    参数量: ~47K
    Int8 大小: ~148KB
    推理延迟: ~30ms @ ESP32-S3 240MHz
    """
    def __init__(self, input_dim=11, num_classes=46, window_size=30):
        super().__init__()
        
        # 时序特征提取
        self.conv1 = nn.Conv1d(input_dim, 32, kernel_size=3, padding=1)
        self.bn1 = nn.BatchNorm1d(32)
        self.conv2 = nn.Conv1d(32, 64, kernel_size=3, padding=1)
        self.bn2 = nn.BatchNorm1d(64)
        
        # SE-Attention (Squeeze-and-Excitation)
        self.se_pool = nn.AdaptiveAvgPool1d(1)
        self.se_fc1 = nn.Linear(64, 16)  # reduction=4
        self.se_fc2 = nn.Linear(16, 64)
        
        # 分类头
        self.global_pool = nn.AdaptiveAvgPool1d(1)
        self.fc1 = nn.Linear(64, 128)
        self.dropout = nn.Dropout(0.3)
        self.fc2 = nn.Linear(128, num_classes)
    
    def forward(self, x):
        # x: (batch, 30, 11) → (batch, 11, 30)
        x = x.permute(0, 2, 1)
        
        # CNN
        x = F.relu(self.bn1(self.conv1(x)))   # (B, 32, 30)
        x = F.relu(self.bn2(self.conv2(x)))   # (B, 64, 30)
        
        # SE-Attention
        se = self.se_pool(x).squeeze(-1)       # (B, 64)
        se = F.relu(self.se_fc1(se))           # (B, 16)
        se = torch.sigmoid(self.se_fc2(se))    # (B, 64)
        x = x * se.unsqueeze(-1)               # (B, 64, 30)
        
        # 分类
        x = self.global_pool(x).squeeze(-1)    # (B, 64)
        x = F.relu(self.fc1(x))               # (B, 128)
        x = self.dropout(x)
        x = self.fc2(x)                       # (B, 46)
        
        return x
```

### 5.5 训练脚本

**文件**: `train/train_l1.py`

```
训练配置:
  Optimizer:     Adam (lr=1e-3, weight_decay=1e-4)
  Scheduler:     CosineAnnealingLR (T_max=100, eta_min=1e-6)
  Loss:          CrossEntropyLoss (label_smoothing=0.1)
  Epochs:        100
  Batch Size:    64
  EarlyStopping: patience=10, monitor=val_acc
  
数据增强:
  - 时间拉伸: random.uniform(0.8, 1.2)
  - 高斯噪声: torch.randn_like(x) * 0.01
  - 随机裁剪: 随机起始位置, 固定30帧
  - Mixup: α=0.2, 概率0.5

训练日志:
  - TensorBoard 可视化
  - 每 epoch 输出 train_loss, val_loss, val_acc
  - 保存 best_model.pth (最高 val_acc)
```

### 5.6 TFLite 导出

**文件**: `export/export_tier1.py`

```
导出流程:
  1. PyTorch → ONNX (opset=13)
     - 输入: (1, 30, 11) float32
     - 输出: (1, 46) float32
     - torch.onnx.export() + onnxsim 简化
     
  2. ONNX → TensorFlow SavedModel
     - onnx-tf convert
     
  3. TensorFlow → TFLite int8
     - 量化代表性数据集: 训练集随机1000个样本
     - converter.optimizations = [tf.lite.Optimize.DEFAULT]
     - converter.representative_dataset = representative_data_gen
     - converter.target_spec.supported_ops = [TfLiteOpsSet.TFLITE_BUILTINS_INT8]
     
  4. 验证
     - TFLite 推理 vs PyTorch 推理对比
     - 精度损失 <1%
     
  5. 转 C 数组
     - xxd -i tier1_model.tflite > tier1_model_data.cc
     - 预期大小: ~148KB

输出文件:
  export/
  ├── tier1_model.tflite      (~148KB)
  ├── tier1_model_data.cc     (C数组)
  └── tier1_model_info.json   (元数据)
```

### 5.7 验收标准

| 编号 | 验收项 | 通过条件 | 验证方法 |
|------|--------|----------|----------|
| P2-01 | 数据采集 | 20+ 手势样本完整 | 数据集统计 |
| P2-02 | 数据质量 | 无缺失帧, 标签正确 | 抽样检查 |
| P2-03 | 训练精度 | L1 val_acc >85% | 训练日志 |
| P2-04 | 模型大小 | tier1_model.tflite < 200KB | 文件大小 |
| P2-05 | 量化精度 | int8 vs FP32 精度损失 <1% | 推理对比 |
| P2-06 | ESP32 推理 | Tier1 推理 <50ms | 串口计时 |

---

## 6. Phase 3: Unity XR 可视化

### 6.1 目标

- ✅ React Three Fiber MVP 先行（Web 端快速验证）
- ✅ Unity XR Hands 1.7 集成（PC/VR 沉浸体验）
- ✅ 5DoF → 26 关节映射算法实现
- ✅ 双前端统一 WebSocket 数据协议

### 6.2 5DoF → 26 关节映射算法

```
手部关节结构 (OpenXR / Unity XR Hands 标准):
  每只手 26 个关节:
    WRIST (1)
    THUMB: CMC_x, CMC_y, MCP, IP, TIP (5)
    INDEX: MCP, PIP, DIP, TIP (4)
    MIDDLE: MCP, PIP, DIP, TIP (4)
    RING: MCP, PIP, DIP, TIP (4)
    PINKY: MCP, PIP, DIP, TIP (4)
    PALM (1), WRIST_END (1), LITTLE_METACARPAL (1), RING_METACARPAL (1)
    总计: 26

映射算法:
  输入: flex[5] (0~90°), imu_quat(w,x,y,z)
  输出: 26关节旋转四元数

  ┌─ 拇指 (Flex0) ─────────────────────────────────┐
  │  CMC_x_rotation  = Flex0 × 0.8   (范围: 0~72°)  │
  │  CMC_y_rotation  = Flex0 × 0.3   (范围: 0~27°)  │
  │  MCP_rotation    = Flex0 × 0.9   (范围: 0~81°)  │
  │  IP_rotation     = Flex0 × 0.7   (范围: 0~63°)  │
  │  TIP_rotation    = Flex0 × 0.5   (范围: 0~45°)  │
  └──────────────────────────────────────────────────┘
  
  ┌─ 食指 (Flex1) ─────────────────────────────────┐
  │  MCP_rotation = Flex1 × 1.0      (范围: 0~90°)  │
  │  PIP_rotation = Flex1 × 0.67     (范围: 0~60°)  │
  │  DIP_rotation = Flex1 × 0.5      (范围: 0~45°)  │
  │  TIP_rotation = Flex1 × 0.3      (范围: 0~27°)  │
  └──────────────────────────────────────────────────┘
  
  ┌─ 中指 (Flex2) ─────────────────────────────────┐
  │  MCP = Flex2 × 1.0, PIP = Flex2 × 0.67         │
  │  DIP = Flex2 × 0.5,  TIP = Flex2 × 0.3         │
  └──────────────────────────────────────────────────┘
  
  ┌─ 无名指 (Flex3) ───────────────────────────────┐
  │  MCP = Flex3 × 1.0, PIP = Flex3 × 0.67         │
  │  DIP = Flex3 × 0.5,  TIP = Flex3 × 0.3         │
  └──────────────────────────────────────────────────┘
  
  ┌─ 小指 (Flex4) ─────────────────────────────────┐
  │  MCP = Flex4 × 1.0, PIP = Flex4 × 0.67         │
  │  DIP = Flex4 × 0.5,  TIP = Flex4 × 0.3         │
  └──────────────────────────────────────────────────┘
  
  ┌─ 腕部 ─────────────────────────────────────────┐
  │  BNO085 四元数 (w,x,y,z) → Unity 四元数         │
  │  注意: BNO085 输出顺序 (w,x,y,z)                 │
  │        Unity 输入顺序 (x,y,z,w)                  │
  │  转换: unity_quat = (bno.x, bno.y, bno.z, bno.w)│
  └──────────────────────────────────────────────────┘
```

### 6.3 React Three Fiber 前端

**技术栈**:
- React 18 + Vite + TypeScript
- @react-three/fiber + @react-three/drei
- TailwindCSS
- Zustand 状态管理
- PWA (Service Worker + manifest)

**架构**:
```
src/
├── components/
│   ├── Hand3D.tsx          # 3D 手部渲染组件
│   ├── GesturePanel.tsx    # 手势识别结果面板
│   ├── ConnectionStatus.tsx # 连接状态指示
│   └── CalibrationUI.tsx   # 校准界面
├── hooks/
│   ├── useWebSocket.ts     # WebSocket 连接管理
│   └── useHandTracking.ts  # 手部数据处理
├── stores/
│   └── handStore.ts        # Zustand 全局状态
├── utils/
│   ├── jointMapper.ts      # 5DoF→26关节映射
│   └── dataProtocol.ts     # JSON 协议解析
└── App.tsx

WebSocket 消息格式 (与 Unity 共享):
  {
    "timestamp": 1234567890,
    "tier": 3,
    "blend_alpha": 1.0,
    "left_hand": { "flex": [...], "imu": [...], "joints": [...] },
    "right_hand": { "flex": [...], "imu": [...], "joints": [...] },
    "relative": { "delta_pos": [...], "delta_orient": ... },
    "inference": { "gesture_id": 5, "confidence": 0.92 },
    "l2_result": { "sequence": [...], "text": "你好" }
  }
```

### 6.4 Unity XR Hands

**技术栈**:
- Unity 2022.3 LTS
- XR Hands 1.7
- OpenXR Plugin
- Universal Render Pipeline (URP)

**核心脚本**:
```csharp
// EchoGloveDriver.cs — 数据接收驱动
public class EchoGloveDriver : MonoBehaviour {
    // 串口/WebSocket 数据源选择
    public enum DataSource { Serial, WebSocket }
    public DataSource source = DataSource.WebSocket;
    
    // 接收 GloveData
    public GloveData leftHand;
    public GloveData rightHand;
    
    void Update() {
        ReadData();
        MapToJoints();
    }
}

// HandPoseMapper.cs — 5DoF→26关节映射
public class HandPoseMapper : MonoBehaviour {
    // 映射系数 (与 React 版共享)
    private static readonly float[][] FLEX_RATIOS = { ... };
    
    public void MapToJoints(float[] flex, Quaternion imuQuat, 
                            out Quaternion[] jointRotations) {
        // 拇指特殊映射
        // 四指等比映射
        // 腕部四元数转换 (w,x,y,z → x,y,z,w)
    }
}

// DualHandManager.cs — 双手管理
public class DualHandManager : MonoBehaviour {
    public HandPoseMapper leftMapper;
    public HandPoseMapper rightMapper;
    
    // 同步双手渲染
    public void UpdateHands(GloveData left, GloveData right) {
        leftMapper.ApplyPose(left);
        rightMapper.ApplyPose(right);
    }
}
```

### 6.5 验收标准

| 编号 | 验收项 | 通过条件 | 验证方法 |
|------|--------|----------|----------|
| P3-01 | React3F 渲染 | 3D 手部实时跟随, 延迟 <50ms | 目视 + 计时 |
| P3-02 | Unity 渲染 | XR Hands 1.7 手部跟随 | VR 头显测试 |
| P3-03 | 关节映射 | 26 关节运动自然, 无异常翻转 | 物理手对比 |
| P3-04 | WebSocket | 数据稳定, 无断连 | 长时间测试 (>30min) |
| P3-05 | 双前端一致 | React 和 Unity 渲染结果一致 | 同数据对比 |

---

## 7. Phase 4: 双手系统升级

### 7.1 目标

- ✅ ESP-NOW 双手通信（同步延迟 <5ms）
- ✅ Gated Bi-CrossAttn 双手融合（精度 >87%）
- ✅ 28-dim 特征向量定义与实现
- ✅ 双手数据集采集工具

### 7.2 ESP-NOW 通信设计

```
GlovePacket 结构 (69 bytes):
  ┌───────────────────────────────────────┐
  │ Field           │ Size    │ Offset    │
  ├─────────────────┼─────────┼───────────┤
  │ magic           │ 2B      │ 0         │
  │ version         │ 1B      │ 2         │
  │ hand_id         │ 1B      │ 3         │
  │ tick_id         │ 4B      │ 4         │
  │ timestamp       │ 4B      │ 8         │
  │ flex[5]         │ 20B     │ 12        │  (float32 × 5)
  │ imu[6]          │ 24B     │ 32        │  (float32 × 6)
  │ l1_gesture_id   │ 4B      │ 56        │
  │ l1_confidence   │ 4B      │ 60        │
  │ checksum        │ 2B      │ 64        │
  │ reserved        │ 3B      │ 66        │
  └─────────────────┴─────────┴───────────┘

同步机制:
  1. 接收器每 10ms 广播 SYNC_TICK (包含全局 tick_id)
  2. 手套收到 SYNC_TICK 后, 在下一个采样周期使用该 tick_id
  3. 接收器配对: left.tick_id == right.tick_id 视为同步帧
  4. 同步精度: <2ms (ESP-NOW 延迟 ~1ms + 采样抖动 ~1ms)

丢包处理:
  检测: tick_id 不连续 (gap > 1)
  处理: 线性插值补帧 (最多连续 3 帧)
  标记: 补帧数据 confidence × 0.8
```

### 7.3 Gated Bi-CrossAttention 核心设计

```python
class GatedCrossAttention(nn.Module):
    """
    门控交叉注意力: 解决空闲手噪声问题
    
    核心思想:
      当一只手为主导手时, gate→0, 跨手注意力被抑制
      当双手协同时, gate→1, 跨手信息被充分利用
    
    参数量: ~8,256 (d_model=64, num_heads=4)
    """
    def __init__(self, d_model=64, num_heads=4, dropout=0.1):
        super().__init__()
        self.cross_attn = nn.MultiheadAttention(
            d_model, num_heads, dropout=dropout, batch_first=True
        )
        self.gate_proj = nn.Linear(d_model * 2, d_model)
        self.norm = nn.LayerNorm(d_model)
        self.dropout = nn.Dropout(dropout)
    
    def forward(self, x_self, x_cross):
        """
        x_self:  (batch, seq_self, d_model)  自身特征
        x_cross: (batch, seq_cross, d_model)  对侧特征
        """
        # 交叉注意力
        attn_out, attn_weights = self.cross_attn(
            query=x_self, key=x_cross, value=x_cross
        )
        
        # 门控计算: gate = σ(W · [x_self; attn_out])
        gate_input = torch.cat([x_self, attn_out], dim=-1)
        gate = torch.sigmoid(self.gate_proj(gate_input))
        
        # 门控融合: output = gate × attn + (1-gate) × self
        output = gate * attn_out + (1 - gate) * x_self
        
        # 残差连接 + LayerNorm
        return self.norm(output + self.dropout(x_self))


class L1GatedBiCrossAttn(nn.Module):
    """
    V5.1 双手融合模型 (Tier1/Tier2 共享架构)
    
    输入: left 11-dim + right 11-dim + relative 6-dim = 28-dim
    输出: 46 类概率
    
    参数量: ~35K (Tier1) / ~35K (Tier2)
    """
    def __init__(self, input_dim=11, num_classes=46, d_model=64):
        super().__init__()
        
        # 单手特征提取
        self.left_encoder = nn.Sequential(
            nn.Conv1d(input_dim, d_model, kernel_size=3, padding=1),
            nn.BatchNorm1d(d_model),
            nn.ReLU(),
        )
        self.right_encoder = nn.Sequential(
            nn.Conv1d(input_dim, d_model, kernel_size=3, padding=1),
            nn.BatchNorm1d(d_model),
            nn.ReLU(),
        )
        
        # 双向门控交叉注意力
        self.cross_attn_lr = GatedCrossAttention(d_model)
        self.cross_attn_rl = GatedCrossAttention(d_model)
        
        # 相对特征编码
        self.relative_encoder = nn.Sequential(
            nn.Linear(6, d_model),
            nn.ReLU(),
        )
        
        # 分类头
        self.classifier = nn.Sequential(
            nn.AdaptiveAvgPool1d(1),
            nn.Flatten(),
            nn.Linear(d_model * 3, 128),  # left + right + relative
            nn.ReLU(),
            nn.Dropout(0.3),
            nn.Linear(128, num_classes),
        )
    
    def forward(self, left_feat, right_feat, relative_feat):
        """
        left_feat:     (batch, 30, 11)
        right_feat:    (batch, 30, 11)
        relative_feat: (batch, 6)
        """
        # 编码
        left_enc = self.left_encoder(left_feat.permute(0, 2, 1))    # (B, 64, 30)
        right_enc = self.right_encoder(right_feat.permute(0, 2, 1)) # (B, 64, 30)
        
        # 转置为 (B, 30, 64) 用于 attention
        left_enc = left_enc.permute(0, 2, 1)
        right_enc = right_enc.permute(0, 2, 1)
        
        # 双向门控交叉注意力
        left_enhanced = self.cross_attn_lr(left_enc, right_enc)   # (B, 30, 64)
        right_enhanced = self.cross_attn_rl(right_enc, left_enc)  # (B, 30, 64)
        
        # 相对特征
        rel_enc = self.relative_encoder(relative_feat)  # (B, 64)
        
        # 拼接 + 分类
        left_pool = left_enhanced.mean(dim=1)    # (B, 64)
        right_pool = right_enhanced.mean(dim=1)  # (B, 64)
        combined = torch.cat([left_pool, right_pool, rel_enc], dim=-1)  # (B, 192)
        
        return self.classifier(combined.unsqueeze(-1))  # (B, 46)
```

### 7.4 门控机制详解

```
问题分析:
  约 40% 的手语手势为单手主导 (如数字 1-5, 字母 A-Z)
  空闲手的噪声 (自然摆动) 会干扰分类
  简单拼接/注意力无法自适应调节

门控公式:
  gate = σ(W_gate · [X_self; Attn_cross] + b_gate)
  
  其中:
    X_self:    自身特征 (batch, seq, d_model)
    Attn_cross: 交叉注意力输出 (batch, seq, d_model)
    W_gate:    可学习权重 (2*d_model, d_model)
    σ:         Sigmoid 函数

门控行为:
  ┌──────────────────────────────────────────────────────┐
  │ 场景          │ gate 值  │ 行为                       │
  ├───────────────┼──────────┼───────────────────────────┤
  │ 单手主导      │ → 0      │ 退化为自注意力, 忽略空闲手  │
  │ 双手协同      │ → 1      │ 充分利用跨手信息            │
  │ 过渡状态      │ 0.3~0.7  │ 混合权重                   │
  └──────────────────────────────────────────────────────┘

参数增量:
  gate_proj: (2×64, 64) = 8,192 参数 + 64 偏置 = 8,256
  相比无门控版本, 仅增加 ~10% 参数
```

### 7.5 28-dim 特征向量定义

```
28-dim 特征向量结构:

索引  维度  来源        含义              范围
─────────────────────────────────────────────────────
0     F     左手 Flex0  拇指弯曲角度      0~90°
1     F     左手 Flex1  食指弯曲角度      0~90°
2     F     左手 Flex2  中指弯曲角度      0~90°
3     F     左手 Flex3  无名指弯曲角度    0~90°
4     F     左手 Flex4  小指弯曲角度      0~90°
5     F     左手 Euler_X  X轴欧拉角     -180~180°
6     F     左手 Euler_Y  Y轴欧拉角     -90~90°
7     F     左手 Euler_Z  Z轴欧拉角     -180~180°
8     F     左手 Gyro_X   X轴角速度     -2000~2000°/s
9     F     左手 Gyro_Y   Y轴角速度     -2000~2000°/s
10    F     左手 Gyro_Z   Z轴角速度     -2000~2000°/s
─────────────────────────────────────────────────────
11-21 F     右手 ...      (同左手结构)   ...
─────────────────────────────────────────────────────
22    F     delta_pos_x   右手相对左手 X  cm
23    F     delta_pos_y   右手相对左手 Y  cm
24    F     delta_pos_z   右手相对左手 Z  cm
25    F     delta_orient_w 相对方位角     0~360°
26    F     delta_orient_diff 方位差      -180~180°
27    F     delta_angular_vel 相对角速度  °/s
─────────────────────────────────────────────────────
```

### 7.6 双手数据集采集

```
更新 collect_dataset.py:
  - 双手同步采集 (通过接收器汇聚)
  - 每标签: 10人 × 10次 × 5秒 × 2手
  - 保存: left.csv, right.csv, relative.csv, metadata.json

新增双手协同手势 (Phase 6 预留, 47-60+ 类):
  47: 拍手 (双掌相击)
  48: 鼓掌
  49: 双手比心
  50: 打招呼 (双手挥手)
  51-60: 更多双手协同手势...
```

### 7.7 验收标准

| 编号 | 验收项 | 通过条件 | 验证方法 |
|------|--------|----------|----------|
| P4-01 | ESP-NOW 同步 | 双手 tick 配对延迟 <5ms | 日志计时 |
| P4-02 | 丢包处理 | 连续丢包 ≤3 帧时正常工作 | 模拟丢包测试 |
| P4-03 | Bi-CrossAttn 精度 | 双手 46 类精度 >87% | 验证集 |
| P4-04 | 门控行为 | 单手时 gate→0, 双手时 gate→1 | 门控值日志 |
| P4-05 | 28-dim 特征 | 各维度范围正确 | 数据检查 |
| P4-06 | 3D 渲染同步 | 双手渲染无错位 | 目视测试 |

---

## 8. Phase 5: 三级热切换架构

### 8.1 目标

- ✅ Tier1 (手套): CNN+Attn, 11-dim, ~148KB, ~30ms, ~80%
- ✅ Tier2 (接收器): Gated Bi-CrossAttn only, 28-dim, ~80KB, ~30ms, ~85%
- ✅ Tier3 (PC): 全链路 ST-GCN+MS-TCN+CTC, 28-dim, ~3MB, ~15ms(GPU), ~92%
- ✅ 热切换: 5 帧线性渐变 (~50ms), 无跳变

### 8.2 Tier2 模型（精简版）

```python
class Tier2Model(nn.Module):
    """
    接收器端模型: 仅 Gated Bi-CrossAttn, 无 MS-TCN
    
    设计目标:
      - 模型大小 < 100KB (int8)
      - 推理延迟 < 40ms @ ESP32-S3 240MHz
      - 精度 > 85% (46类)
    
    与 Tier3 区别:
      - 无 MS-TCN 时序精炼
      - 无 CTC 解码
      - 使用 12 节点 ST-GCN (非 42 节点)
      - 仅帧级分类, 非序列识别
    """
    def __init__(self, input_dim=11, num_classes=46, d_model=64):
        super().__init__()
        
        # 单手编码 (与 L1GatedBiCrossAttn 共享结构)
        self.left_encoder = nn.Sequential(
            nn.Conv1d(input_dim, d_model, 3, padding=1),
            nn.BatchNorm1d(d_model),
            nn.ReLU(),
        )
        self.right_encoder = nn.Sequential(
            nn.Conv1d(input_dim, d_model, 3, padding=1),
            nn.BatchNorm1d(d_model),
            nn.ReLU(),
        )
        
        # 双向门控交叉注意力
        self.cross_lr = GatedCrossAttention(d_model)
        self.cross_rl = GatedCrossAttention(d_model)
        
        # 相对特征
        self.rel_encoder = nn.Linear(6, d_model)
        
        # 分类头 (精简: 无 MS-TCN)
        self.pool = nn.AdaptiveAvgPool1d(1)
        self.fc1 = nn.Linear(d_model * 3, 128)
        self.fc2 = nn.Linear(128, num_classes)
        self.dropout = nn.Dropout(0.2)
    
    def forward(self, left, right, relative):
        """同 L1GatedBiCrossAttn.forward()"""
        left_enc = self.left_encoder(left.permute(0, 2, 1)).permute(0, 2, 1)
        right_enc = self.right_encoder(right.permute(0, 2, 1)).permute(0, 2, 1)
        
        left_out = self.cross_lr(left_enc, right_enc)
        right_out = self.cross_rl(right_enc, left_enc)
        
        rel_out = F.relu(self.rel_encoder(relative))
        
        combined = torch.cat([
            left_out.mean(1), right_out.mean(1), rel_out
        ], dim=-1)
        
        x = F.relu(self.fc1(self.dropout(combined)))
        return self.fc2(x)
    
    # 预期指标:
    #   参数量: ~22K
    #   Int8 大小: ~80KB
    #   推理延迟: ~30ms @ ESP32-S3 240MHz
    #   精度: ~85% (46类)
```

### 8.3 接收器主循环

```cpp
/**
 * 接收器主循环 — 双手汇聚 + Tier2 推理 + 转发
 * 
 * 运行在 ESP32-S3 接收器板上
 * Core 0: 主循环
 * Core 1: ESP-NOW 接收 + USB 转发
 */

// 全局状态
GloveData left_hand, right_hand;
RelativeFeatures relative;
Tier2Model tier2;  // ~80KB int8
uint32_t global_tick = 0;

void setup() {
    Serial.begin(115200);
    espnow_init();           // 初始化 ESP-NOW
    tier2.loadModel();       // 加载 TFLite 模型
    sync_tick_init();        // 初始化同步广播
}

void loop() {
    // 1. 广播同步 Tick
    broadcastSyncTick(global_tick++);
    
    // 2. 等待双手数据 (超时 5ms)
    if (waitForBothHands(5)) {
        // 2a. 计算相对特征
        computeRelativeFeatures(left_hand, right_hand, &relative);
        
        // 2b. Tier2 推理
        unsigned long t0 = micros();
        Tier2Result result = tier2.infer(
            left_hand.features, 
            right_hand.features, 
            relative
        );
        unsigned long infer_time = (micros() - t0) / 1000;
        
        // 2c. 转发到 PC
        forwardToPC(left_hand, right_hand, relative, result);
        
    } else {
        // 3. 超时: 转发可用数据
        forwardAvailable();
    }
    
    // 4. 检查 PC 连接状态
    checkPCConnection();
}

/**
 * 计算双手相对特征 (6-dim)
 */
void computeRelativeFeatures(const GloveData& left, 
                             const GloveData& right,
                             RelativeFeatures* rel) {
    // 相对位置 (简化: 基于 IMU 推算)
    rel->delta_pos_x = right.pos_x - left.pos_x;
    rel->delta_pos_y = right.pos_y - left.pos_y;
    rel->delta_pos_z = right.pos_z - left.pos_z;
    
    // 相对方位
    rel->delta_orient_w = right.euler_y - left.euler_y;
    rel->delta_orient_diff = abs(right.euler_y - left.euler_y);
    
    // 相对角速度
    rel->delta_angular_vel = right.gyro_z - left.gyro_z;
}
```

### 8.4 PC Relay 三级融合 — TierRouter

```python
class TierRouter:
    """
    三级推理路由与融合
    
    职责:
      1. 维护三个 Tier 的推理结果
      2. 检测 PC 连接状态, 自动切换活跃 Tier
      3. 线性渐变 blend, 平滑过渡
      4. 温度校准对齐不同 Tier 的概率分布
    """
    
    def __init__(self, tier1_model, tier3_model):
        # 模型实例
        self.tier1_model = tier1_model    # TFLite (备用)
        self.tier3_model = tier3_model    # PyTorch GPU
        
        # 状态
        self.active_tier = 3              # 当前活跃 Tier
        self.prev_tier = 3                # 上一个 Tier
        self.blend_alpha = 1.0            # 混合系数 0→1
        self.blend_frames = 5             # 渐变帧数
        self.blend_counter = 0
        
        # 缓冲区
        self.tier1_left = None
        self.tier1_right = None
        self.tier2_result = None
        self.tier3_result = None
        self.prev_output = None           # 上一帧输出 (用于 blend)
        
        # 温度校准参数
        self.temperature_t1 = 1.5   # Tier1 温度 (较平滑)
        self.temperature_t2 = 1.2   # Tier2 温度
        self.temperature_t3 = 1.0   # Tier3 温度 (原始)
    
    def update(self, packet: ReceiverPacket):
        """更新路由状态"""
        # 更新各 Tier 结果
        self.tier1_left = packet.left_hand.tier1_result
        self.tier1_right = packet.right_hand.tier1_result
        self.tier2_result = packet.tier2_result
        
        # 检测 PC 连接, 更新活跃 Tier
        self._update_active_tier()
    
    def _update_active_tier(self):
        """根据 PC 连接状态切换活跃 Tier"""
        if self.pc_connected:
            target_tier = 3
        elif self.receiver_available:
            target_tier = 2
        else:
            target_tier = 1
        
        if target_tier != self.active_tier:
            self.prev_tier = self.active_tier
            self.active_tier = target_tier
            self.blend_counter = 0
            self.prev_output = self._get_current_output()
    
    def get_output(self) -> dict:
        """获取融合后的输出"""
        current = self._get_current_output()
        
        if self.blend_counter < self.blend_frames:
            # 线性渐变
            alpha = self.blend_counter / self.blend_frames
            output = self._blend(self.prev_output, current, alpha)
            self.blend_counter += 1
        else:
            output = current
        
        return {
            "tier": self.active_tier,
            "blend_alpha": self.blend_counter / self.blend_frames,
            "probabilities": output,
            "gesture_id": int(np.argmax(output)),
            "confidence": float(np.max(output)),
        }
    
    def _get_current_output(self) -> np.ndarray:
        """获取当前活跃 Tier 的输出"""
        if self.active_tier == 3 and self.tier3_result is not None:
            return self._calibrate_temperature(
                self.tier3_result.probabilities, self.temperature_t3
            )
        elif self.active_tier == 2 and self.tier2_result is not None:
            return self._calibrate_temperature(
                self.tier2_result.class_probabilities, self.temperature_t2
            )
        else:
            # Tier1: 左右手结果取平均
            left = self._calibrate_temperature(
                self.tier1_left.class_probabilities, self.temperature_t1
            )
            right = self._calibrate_temperature(
                self.tier1_right.class_probabilities, self.temperature_t1
            )
            return (left + right) / 2
    
    def _blend(self, old, new, alpha):
        """线性渐变混合"""
        return (1 - alpha) * old + alpha * new
    
    def _calibrate_temperature(self, logits, temperature):
        """温度校准: 平滑不同 Tier 的概率分布"""
        exp_logits = np.exp(logits / temperature)
        return exp_logits / np.sum(exp_logits)
```

### 8.5 WebSocket 消息格式 V5.1

```json
{
  "version": "6.0",
  "timestamp": 1685678901234,
  "tier": 3,
  "blend_alpha": 1.0,
  
  "left_hand": {
    "flex": [45.2, 12.3, 67.8, 23.1, 8.5],
    "imu": {
      "euler": [12.3, -45.6, 78.9],
      "gyro": [0.12, -0.34, 0.56]
    },
    "l1_gesture": {"id": 5, "confidence": 0.87}
  },
  
  "right_hand": {
    "flex": [0.0, 0.0, 0.0, 0.0, 0.0],
    "imu": {
      "euler": [0.0, 0.0, 0.0],
      "gyro": [0.0, 0.0, 0.0]
    },
    "l1_gesture": {"id": 0, "confidence": 0.95}
  },
  
  "relative": {
    "delta_pos": [15.2, -3.4, 8.7],
    "delta_orient": 12.5,
    "delta_angular_vel": 0.23
  },
  
  "inference": {
    "gesture_id": 5,
    "confidence": 0.92,
    "probabilities": [0.01, 0.02, ...]
  },
  
  "l2_result": {
    "sequence": ["你好", "谢谢"],
    "text": "你好 谢谢",
    "segment_boundaries": [0, 30, 60]
  },
  
  "nlp_text": "你好，谢谢！"
}
```

### 8.6 验收标准

| 编号 | 验收项 | 通过条件 | 验证方法 |
|------|--------|----------|----------|
| P5-01 | Tier3→Tier2 切换 | <100ms, 无跳变 | 断开 PC 测试 |
| P5-02 | Tier2→Tier1 切换 | <50ms, 无跳变 | 断开接收器测试 |
| P5-03 | Tier2 推理延迟 | <40ms @ ESP32-S3 | 串口计时 |
| P5-04 | 渐变平滑度 | 输出无突变 (Δ<0.1/frame) | 日志分析 |
| P5-05 | 恢复切换 | PC 恢复后自动切回 Tier3 | 重连测试 |
| P5-06 | 长时间稳定性 | >1小时无崩溃 | 持续运行测试 |

---

## 9. Phase 6: 进阶优化

### 9.1 数据集扩展到 60+ 类双手协同手势

```
扩展策略:
  Phase 2 已验证 46 类单手手势
  Phase 6 新增 15-20 类双手协同手势

新增手势类别 (示例):
  47: 拍手
  48: 鼓掌
  49: 双手比心
  50: 挥手打招呼
  51: 抱歉 (双手合十)
  52: 感谢 (双手前伸)
  53: 拥抱手势
  54: 跳绳手势
  55: 弹钢琴
  56: 打字
  ...

增量学习:
  - 冻结 Tier1 单手模型
  - 仅微调 Gated Bi-CrossAttn 分类头
  - 防止灾难性遗忘 (EWC 正则化)
```

### 9.2 MANO 参数化模型（替换线性耦合）

```
当前方案: 5DoF → 26 关节线性耦合
升级方案: MANO (hand Model with Articulated and Non-rigid dEformations)

MANO 优势:
  - 参数化手部模型, 78 维参数 (pose + shape)
  - 可捕捉手指间独立运动
  - 物理约束保证关节合理性
  - 可与视觉数据 (MediaPipe) 对齐

实现路径:
  1. 训练 5DoF → MANO 参数回归网络
  2. MANO 渲染 26 关节
  3. 替换线性耦合映射
```

### 9.3 多模态视觉融合（MediaPipe 补全）

```
方案: 手套传感器 + RGB 摄像头双模态

融合策略:
  - 手套: 高精度弯曲角度, 无遮挡问题
  - MediaPipe: 手部姿态估计, 补全手套无法测量的特征
  - 融合层: 门控注意力选择可靠模态

应用场景:
  - 复杂手语需要指尖位置精度
  - 手套传感器故障时降级到纯视觉
```

### 9.4 BiLSTM+CTC 备选（无 GPU 场景）

```python
class BiLSTMCTC(nn.Module):
    """
    备选 Tier3 模型: BiLSTM + CTC (CPU 友好)
    
    适用场景: 无 GPU 的 PC 部署
    推理延迟: ~80ms @ CPU (Intel i5)
    精度: ~88% (略低于 ST-GCN 的 ~92%)
    """
    def __init__(self, input_dim=28, hidden_dim=128, num_classes=46):
        super().__init__()
        self.lstm = nn.LSTM(
            input_dim, hidden_dim, num_layers=2,
            bidirectional=True, batch_first=True, dropout=0.3
        )
        self.fc = nn.Linear(hidden_dim * 2, num_classes + 1)  # +1 for CTC blank
    
    def forward(self, x):
        # x: (batch, seq_len, 28)
        lstm_out, _ = self.lstm(x)        # (batch, seq_len, 256)
        output = self.fc(lstm_out)         # (batch, seq_len, 47)
        return output
```

### 9.5 低功耗优化

```
目标: 电池续航 >2小时

优化项:
  1. 动态频率调节: 空闲时降至 80MHz
  2. 传感器按需唤醒: 无手势时降至 10Hz
  3. ESP-NOW 功耗: 发送间隔优化
  4. ADC 低功耗模式: ADS1115 单次转换模式
  5. BNO085 低功耗模式: 降低采样率至 50Hz
```

### 9.6 系统集成测试

```
测试矩阵:
  1. 单手基本功能: 46 类手势识别
  2. 双手协同: 60+ 类手势识别
  3. 热切换: Tier3→Tier2→Tier1→Tier2→Tier3 循环
  4. 长时间运行: >4 小时稳定性
  5. 多用户: 不同手型/手势习惯
  6. 环境干扰: 温度变化、电磁干扰
  7. 前端一致性: React3F 与 Unity 渲染对比
```

---

## 10. 核心模型架构深度设计

### 10.1 L1: CNN+SE-Attention（单手 Tier1）

```
完整架构:

输入: (batch, 30, 11)

Layer 1:  Conv1d(11→32, k=3, p=1) + BatchNorm1d(32) + ReLU
          输出: (batch, 32, 30)
          参数: 11×32×3 + 32 = 1,088

Layer 2:  Conv1d(32→64, k=3, p=1) + BatchNorm1d(64) + ReLU
          输出: (batch, 64, 30)
          参数: 32×64×3 + 64 = 6,208

SE-Attention:
  Squeeze: AdaptiveAvgPool1d(1) → (batch, 64)
  Excitation: Linear(64→16) + ReLU + Linear(16→64) + Sigmoid
  参数: 64×16 + 16 + 16×64 + 64 = 2,128
  Scale: 输出 × scale → (batch, 64, 30)

Layer 3:  AdaptiveAvgPool1d(1) → (batch, 64)
Layer 4:  Linear(64→128) + ReLU
          参数: 64×128 + 128 = 8,320
Layer 5:  Dropout(0.3)
Layer 6:  Linear(128→46)
          参数: 128×46 + 46 = 5,934

总参数量: 1,088 + 6,208 + 2,128 + 8,320 + 5,934 = 23,678 (~24K)
Int8 大小: ~24KB (仅权重) + TFLite 开销 → ~148KB (含元数据)
推理延迟: ~30ms @ ESP32-S3 240MHz (TFLite-Micro)
精度: ~80% Top-1 (46类, 验证集)
```

### 10.2 L1: Gated Bidirectional CrossAttention（双手融合）

```
完整架构:

输入: left(batch,30,11) + right(batch,30,11) + relative(batch,6)

左手编码:
  Conv1d(11→64, k=3, p=1) + BN + ReLU → (batch, 64, 30) → permute → (batch, 30, 64)
  参数: 11×64×3 + 64 = 2,176

右手编码: (共享权重或独立)
  参数: 2,176

GatedCrossAttention (左→右):
  MultiheadAttention(d=64, heads=4): Q,K,V 投影 = 64×64×4 = 16,384
  GateProj(128→64): 128×64 + 64 = 8,256
  LayerNorm(64): 128
  小计: ~24,768

GatedCrossAttention (右→左): (权重独立)
  小计: ~24,768

相对特征编码:
  Linear(6→64) + ReLU: 6×64 + 64 = 448

分类头:
  AdaptiveAvgPool1d(1): 0
  Linear(192→128) + ReLU: 192×128 + 128 = 24,704
  Dropout(0.3): 0
  Linear(128→46): 128×46 + 46 = 5,934

总参数量: 2,176 + 2,176 + 24,768 + 24,768 + 448 + 24,704 + 5,934 = ~85K
Int8 大小: ~85KB (仅权重) + 开销 → ~80KB (门控层 FP16 混合量化后)
推理延迟: ~30ms @ ESP32-S3 (Tier2 版本)
精度: ~85% Top-1 (46类, 双手)
```

### 10.3 L2: ST-GCN 空间图卷积

```
分层图设计:

┌─ 12 节点图 (Tier1/2) ──────────────────────────────────┐
│                                                          │
│  节点定义 (每手 6 个, 共 12 个):                          │
│    左手: L_Wrist, L_Thumb, L_Index, L_Middle, L_Ring,   │
│          L_Pinky                                         │
│    右手: R_Wrist, R_Thumb, R_Index, R_Middle, R_Ring,   │
│          R_Pinky                                         │
│                                                          │
│  邻接矩阵: 22 条边                                      │
│    每手: Wrist↔5指尖 (5边) + 指尖间 (5边) = 10边/手     │
│    跨手: 无直接连接                                      │
│    总计: 10×2 + 2(自环) = 22 边                          │
│                                                          │
│  图卷积:                                                 │
│    GCNLayer: A_norm × X × W                              │
│    A_norm = D^{-1/2} × A × D^{-1/2}                     │
│    X: (batch, 12, 28) → 投影 → (batch, 12, 64)          │
│                                                          │
│  ST-GCN Block × 3:                                       │
│    Spatial GCN → Temporal Conv1d(k=9) → BN → ReLU       │
│    参数: ~50K/block × 3 = ~150K                          │
└──────────────────────────────────────────────────────────┘

┌─ 42 节点图 (Tier3) ────────────────────────────────────┐
│                                                          │
│  节点定义 (每手 21 个关节, 共 42 个):                     │
│    WRIST (1) + 5×(MCP, PIP, DIP, TIP) = 21/手           │
│                                                          │
│  邻接矩阵: 66 条边                                      │
│    每手: 骨架树 (20边) + 跨指 (13边) = 33边/手           │
│    跨手: 无直接连接                                      │
│    总计: 33×2 = 66 边                                    │
│                                                          │
│  图卷积:                                                 │
│    GCNLayer: A_norm × X × W                              │
│    X: (batch, 42, 28) → 投影 → (batch, 42, 128)         │
│                                                          │
│  ST-GCN Block × 6:                                       │
│    Spatial GCN → Temporal Conv1d(k=9) → BN → ReLU       │
│    参数: ~200K/block × 6 = ~1.2M                         │
└──────────────────────────────────────────────────────────┘

节点投影层:
  28-dim 特征 → N_nodes × 2 空间坐标
  使用可学习的线性投影: Linear(28, N_nodes×2)
```

### 10.4 L2: MS-TCN 多阶段时序精炼

```
架构: 4-stage Multi-Scale TCN

每个 Stage:
  ┌─────────────────────────────────────────┐
  │ 10 层 Temporal Convolution               │
  │                                         │
  │ 每层:                                    │
  │   Conv1d(C→C, k=3, d=dilation)          │
  │   + BatchNorm + ReLU + Dropout(0.3)     │
  │   + 残差连接                             │
  │                                         │
  │ Dilation: 1, 2, 4, 8, 16, 32, 64, 128, │
  │           256, 512                       │
  │                                         │
  │ 感受野: 3×(512×2-1) = 3069 帧           │
  │ (修正: 每层 k=3, 总 RF = 1+2×Σd =       │
  │  1+2×(1+2+...+512) = 1+2×1023 = 2047)   │
  └─────────────────────────────────────────┘

Stage 间连接:
  Stage 1 输出 → FC → Stage 2 输入
  Stage 2 输出 → FC → Stage 3 输入
  Stage 3 输出 → FC → Stage 4 输入

Smoothing Loss:
  L_smooth = λ × Σ|p_t - p_{t-1}|²
  λ = 0.15, 防止过分割

参数量:
  每层: C×C×3 + C (Conv + BN) = 64×64×3 + 64 = 12,352
  每 stage: 12,352 × 10 = 123,520
  4 stages: 123,520 × 4 = ~494K
  FC 连接: ~50K
  总计: ~544K (仅 MS-TCN)
```

### 10.5 L2: CTC 解码

```
CTC (Connectionist Temporal Classification):

输入: MS-TCN 输出序列 (batch, seq_len, num_classes+1)
输出: 手语标签序列

Beam Search 配置:
  beam_width: k = 10
  blank_idx: 0
  top_paths: 1

解码流程:
  1. 对每个时间步, 取 log_softmax
  2. 维护 k 个候选序列
  3. 每步扩展: 重复标签合并 + blank 处理
  4. 返回最高概率路径

语言模型约束 (可选):
  beam_score = ctc_score + β × lm_score
  β = 0.3 (权重)
```

### 10.6 BiLSTM+CTC 备选

```
架构:
  BiLSTM(input=28, hidden=128, layers=2, bidirectional=True)
  → FC(256→47)  # 46类 + 1 blank
  → CTC Loss / Beam Search

参数量:
  LSTM: 4 × (28×128 + 128×128 + 128) × 2 (双向) × 2 (层) = ~262K
  FC: 256×47 + 47 = ~12K
  总计: ~274K

推理延迟:
  GPU: ~5ms
  CPU (Intel i5): ~80ms
  ESP32-S3: 不适用 (内存不足)

精度: ~88% (46类, 略低于 ST-GCN 的 ~92%)

适用场景:
  - 无 GPU 的 PC 部署
  - 嵌入式 Tier3 不推荐
```

### 10.7 模型参数总览表

| 模型 | 部署位置 | 参数量 | FP32 大小 | Int8 大小 | 推理延迟 | 精度(46类) | 备注 |
|------|----------|--------|-----------|-----------|----------|------------|------|
| L1 CNN+SE-Attn | 手套 (ESP32-S3) | ~24K | ~96KB | ~148KB | ~30ms | ~80% | Tier1 |
| L1 Gated Bi-XAttn | 手套/接收器 | ~85K | ~340KB | ~80KB(FP16混合) | ~30ms | ~85% | Tier1/2 |
| Tier2 精简版 | 接收器 (ESP32-S3) | ~22K | ~88KB | ~80KB | ~30ms | ~85% | Tier2 |
| ST-GCN (12节点) | 接收器/PC | ~150K | ~600KB | ~200KB | ~25ms | ~83% | 简化 |
| ST-GCN (42节点) | PC (GPU) | ~1.2M | ~4.8MB | ~1.5MB | ~8ms | ~90% | 完整 |
| MS-TCN (4-stage) | PC (GPU) | ~544K | ~2.2MB | ~600KB | ~5ms | +2% | 精炼 |
| **Tier3 完整栈** | **PC (GPU)** | **~1.8M** | **~7.2MB** | **~3MB** | **~15ms** | **~92%** | **ST-GCN+MS-TCN+CTC** |
| BiLSTM+CTC (备选) | PC (CPU) | ~274K | ~1.1MB | ~300KB | ~80ms | ~88% | 无GPU备选 |

---

## 11. 全链路迁移影响矩阵

### 11.1 V3.0(霍尔) → V5.1(Flex) 完整变更

| 层级 | 变更项 | V3.0 | V5.1 | 影响度 | 具体变更 |
|------|--------|------|------|--------|----------|
| **硬件层** | 传感器 | Hall × 5 + MPU6050 | Flex × 5 + BNO085 | 🔴 高 | 完全替换传感器 |
| **硬件层** | ADC | ESP32 内置 | ADS1115 × 2 | 🟡 中 | 新增 I2C ADC |
| **硬件层** | 主控 | ESP32-WROOM | ESP32-S3-N16R8 | 🟡 中 | 升级主控 |
| **固件层** | 采集代码 | HallManager | ADS1115Manager + FlexManager | 🔴 高 | 完全重写 |
| **固件层** | 校准 | 磁场校准 | Flex 5点分段线性 | 🔴 高 | 校准方案重设计 |
| **固件层** | IMU | MPU6050 DMP | BNO085 SH-2 | 🟡 中 | 驱动替换 |
| **固件层** | 通信 | BLE | ESP-NOW | 🟡 中 | 协议替换 |
| **模型层** | 特征维度 | 14-dim | 11-dim | 🟡 中 | 输入维度变更 |
| **模型层** | L1 模型 | SVM | CNN+SE-Attn | 🔴 高 | 完全重训练 |
| **模型层** | L2 模型 | 无 | ST-GCN+MS-TCN | 🔴 高 | 全新模型 |
| **数据层** | 数据集 | Hall 数据 | Flex 数据 | 🔴 高 | 完全重新采集 |
| **前端层** | 渲染 | 简单 3D | R3F + Unity XR | 🟡 中 | 前端重设计 |

### 11.2 V5.0 → V5.1 变更

| 变更项 | V5.0 | V5.1 | 影响 | 迁移工作量 |
|--------|------|------|------|-----------|
| Tier2 模型 | 完整 L2 栈 (ST-GCN+MS-TCN) | 仅 Gated Bi-CrossAttn | 🟡 中 | 移除 MS-TCN, 保留 CrossAttn |
| ST-GCN 图 | 统一 42 节点 | 分层 12/42 节点 | 🟡 中 | 新增 12 节点图定义 |
| 数据集 | 固定 46 类 | 分阶段 46→60+ | 🟢 低 | 策略调整, 非代码变更 |
| 前端 | Unity 单轨 | R3F + Unity 双轨 | 🟡 中 | 新增 React3F 前端 |
| 霍尔代码 | 残留 | 完全移除 | 🟢 低 | 代码清理 |
| 门控层 | 无特殊处理 | FP16 混合量化 | 🟢 低 | 量化配置调整 |
| 接收器固件 | 完整推理 | 精简推理 | 🟡 中 | 模型替换 + 代码精简 |

---

## 12. 已知风险与缓解方案

### 12.1 硬件风险

| 风险 | 概率 | 影响 | 缓解方案 | 负责人 |
|------|------|------|----------|--------|
| Flex 传感器温漂 | 高 | 读数偏移 ±5° | 自动零点校准每 30 秒；温度补偿系数表 | 固件 |
| I2C 总线冲突 | 低 | 数据读取错误 | 100kHz 限速；地址扫描验证；错误重试机制 | 固件 |
| ESP-NOW 丢包 | 中 | 数据不连续 | 线性插值补帧（≤3帧）；置信度标记；重传机制 | 通信 |
| BNO085 磁干扰 | 中 | 姿态漂移 | GRV 模式（不使用磁力计）；定期重初始化 | 固件 |
| Flex 传感器老化 | 低 | 灵敏度下降 | 定期重新校准；传感器寿命监测 | 硬件 |
| ESP32-S3 内存不足 | 低 | 推理失败 | PSRAM 分配监控；模型大小限制；内存池管理 | 固件 |

### 12.2 模型风险

| 风险 | 概率 | 影响 | 缓解方案 |
|------|------|------|----------|
| Gated CrossAttn 门控退化 | 中 | 所有样本 gate→0 或 gate→1 | 门控正则化: loss += λ·\|gate-0.5\|²; 门控值监控 |
| MS-TCN 梯度消失 | 中 | 深层训练不收敛 | 渐进训练: Stage1→Stage1+2→...→全阶段; 梯度裁剪 |
| CTC blank 类过多 | 中 | 输出全为 blank | 调整 blank 权重; 增大训练 epoch; 语言模型约束 |
| int8 量化门控精度下降 | 中 | 门控行为异常 | 门控层 FP16 混合量化; 量化后精度验证 |
| 过拟合 (小数据集) | 高 | 验证精度远低于训练 | 数据增强; Dropout; EarlyStopping; 交叉验证 |
| 类别不平衡 | 中 | 少数类精度低 | 加权交叉熵; 过采样; Focal Loss |

### 12.3 系统风险

| 风险 | 概率 | 影响 | 缓解方案 |
|------|------|------|----------|
| 接收器 PSRAM 不足 | 低 | Tier2 推理失败 | Tier2 已精简到 ~80KB；PSRAM 使用监控 |
| 热切换输出跳变 | 中 | 前端动画卡顿 | 延长 blend 至 10 帧 (100ms)；温度校准对齐 |
| Tier 间概率分布不对齐 | 高 | 切换后分类结果不一致 | 验证集温度校准；KL 散度 <0.1 目标 |
| WebSocket 连接断开 | 中 | 前端数据中断 | 自动重连机制；断线期间使用缓存数据 |
| USB Serial 带宽不足 | 低 | 数据包丢失 | Protobuf 压缩；降低非必要字段频率 |
| 多设备同频干扰 | 低 | ESP-NOW 通信异常 | 频道选择；设备 ID 过滤 |

---

## 附录

### A. 术语表

| 术语 | 全称 | 含义 |
|------|------|------|
| ST-GCN | Spatial-Temporal Graph Convolutional Network | 时空图卷积网络 |
| MS-TCN | Multi-Scale Temporal Convolutional Network | 多尺度时序卷积网络 |
| CTC | Connectionist Temporal Classification | 时序连接分类 |
| SE-Attention | Squeeze-and-Excitation Attention | 压缩激励注意力 |
| Bi-CrossAttn | Bidirectional Cross-Attention | 双向交叉注意力 |
| MANO | hand Model with Articulated and Non-rigid dEformations | 参数化手部模型 |
| GRV | Game Rotation Vector | 游戏旋转向量 (无磁力计) |
| TFLite | TensorFlow Lite | TensorFlow 轻量推理框架 |
| ESP-NOW | Espressif Now | 乐鑫低延迟无线协议 |

### B. 参考文献

1. Yan et al., "Spatial Temporal Graph Convolutional Networks for Skeleton-Based Action Recognition", CVPR 2018
2. Farha & Gall, "MS-TCN: Multi-Stage Temporal Convolutional Network for Action Segmentation", CVPR 2019
3. Graves et al., "Connectionist Temporal Classification", ICML 2006
4. Hu et al., "Squeeze-and-Excitation Networks", CVPR 2018
5. Vaswani et al., "Attention Is All You Need", NeurIPS 2017
6. Romero et al., "Embodied Hands: Modeling and Capturing Hands and Bodies Together", SIGGRAPH Asia 2017
7. Pavllo et al., "3D Human Pose Estimations in Video with Temporal Convolutions and Semi-Supervised Training", CVPR 2019

### C. 文件清单

```
echoglove-v5.1/
├── firmware/                    # ESP32-S3 固件
│   ├── src/
│   │   └── main.cpp
│   ├── lib/
│   │   ├── Sensors/
│   │   │   ├── ADS1115Manager.h/.cpp
│   │   │   ├── FlexManager.h/.cpp
│   │   │   ├── IMUManager.h/.cpp
│   │   │   └── SensorManager.h/.cpp
│   │   ├── Filters/
│   │   │   └── KalmanFilter.h/.cpp
│   │   ├── Inference/
│   │   │   ├── Tier1Model.h/.cpp
│   │   │   └── tier1_model_data.cc
│   │   └── Communication/
│   │       └── ESPNOWManager.h/.cpp
│   └── platformio.ini
├── receiver/                    # 接收器固件
│   ├── src/main.cpp
│   ├── lib/
│   │   └── Tier2Model.h/.cpp
│   └── platformio.ini
├── models/                      # 模型训练
│   ├── l1_cnn_attention.py
│   ├── l1_gated_bicrossattn.py
│   ├── tier2_model.py
│   ├── st_gcn.py
│   ├── ms_tcn.py
│   └── bi_lstm_ctc.py
├── train/                       # 训练脚本
│   ├── train_l1.py
│   ├── train_tier2.py
│   └── train_tier3.py
├── export/                      # 模型导出
│   ├── export_tier1.py
│   └── export_tier2.py
├── tools/                       # 工具
│   ├── collect_dataset.py
│   └── preprocess_dataset.py
├── frontend/                    # React Three Fiber 前端
│   └── ...
├── unity/                       # Unity XR Hands
│   └── ...
├── proto/                       # Protobuf 定义
│   └── echoglove_v6.proto
└── docs/                        # 文档
    └── 04_SOP-SPEC-PLAN_V5.1.md  ← 本文档
```

---

> **文档版本**: V5.1  
> **最后更新**: 2026-06  
> **维护者**: EchoGlove Architecture Team  
> **状态**: ✅ 确认版 (Confirmed)
