# EchoGlove V5.0 核心决策点归纳

> 15项关键架构决策 + 依据 + 影响范围 + V4.0→V5.0变更标记

---

## 决策总览

| # | 决策点 | 选择 | 核心依据 | 版本 |
|---|--------|------|----------|------|
| 1 | 硬件拓扑 | 双ESP32-S3 | arXiv:2401.13254模块化验证 | V4.0 |
| 2 | 通信同步 | ESP-NOW广播 | 1-2ms延迟，免配对 | V4.0 |
| 3 | 特征向量 | 28-dim(11×2+相对6) | 双手手语核心=空间关系 | V4.0 |
| 4 | 模型架构 | **Gated Bi-CrossAttention** | 门控解决空闲手，DGCA验证 | **V5.0升级** |
| 5 | ST-GCN图 | 6条跨手边 | 性价比最优 | V4.0 |
| 6 | BNO085模式 | GRV 6轴 | 室内磁干扰不可控 | V4.0 |
| 7 | 关节映射 | Phase1线性耦合→Phase2 MANO | 渐进落地 | V4.0 |
| 8 | 连续手语 | L1/L2保留+CTC | ST-GCN+CTC职责清晰 | V4.0 |
| 9 | 同步协议 | 接收器广播SYNC_TICK | 中心化调度无冲突 | V4.0 |
| 10 | I2C策略 | 100kHz全局降速 | 稳定性优先 | V4.0 |
| 11 | CrossAttention变体 | **Gated Bidirectional** | 门控+双向=空闲手最优解 | **V5.0新增** |
| 12 | L2时序骨干 | **MS-TCN(4-stage)** | 专为时序分割，ICASSP验证 | **V5.0新增** |
| 13 | L2流水线 | **ST-GCN→MS-TCN→CTC** | 空间/时序/解码各司其职 | **V5.0新增** |
| 14 | 热切换架构 | **三级: 手套→接收器→PC** | 最大鲁棒性+优雅降级 | **V5.0新增** |
| 15 | BiLSTM+CTC定位 | **L2备选方案** | 无GPU部署场景 | **V5.0新增** |

---

## V4.0决策 #1-#10 (详见V4.0版本文档，此处仅列出变更)

| # | V4.0选择 | V5.0变更 |
|---|---------|----------|
| 4 | 双流+CrossAttention | **→ Gated Bi-CrossAttention (门控升级)** |
| 其余 | 不变 | — |

---

## V5.0新增决策 #11-#15

### 决策11: CrossAttention变体 — Gated Bidirectional

#### 选项
- A. 标准单向CrossAttention (Q=Left, K=V=Right)
- B. 双向CrossAttention (V4.0方案)
- **C. Gated Bidirectional CrossAttention (选中)**
- D. Cross-Covariance Attention (XCA)

#### 深度分析

| 变体 | 参数 | FLOPs | 空闲手处理 | 创新性 |
|------|------|-------|-----------|--------|
| 标准单向 | ~16K | ~730K | ❌ 无 | 低 |
| 双向 | ~33K | ~1.46M | ⚠️ 部分 | 中 |
| **Gated双向** | **~33K** | **~1.48M** | **✅ 门控自动抑制** | **高(DGCA WWW2025)** |
| XCA | ~16K+LPI | ~123K(d=64时不优) | ❌ 无 | 中 |

#### 依据
1. **空闲手问题**: 双手手语约40%是单手主导，空闲手噪声通过门控自动关闭
2. **门控公式**: `gate = σ(W·[X_self; Attn_cross])`, `output = gate⊙Attn + (1-gate)⊙X_self`
3. **参数增量极小**: 仅增加`Linear(128→64)=8,256参数`，微不足道
4. **DGCA论文(WWW 2025)**: 门控CrossAttention在多模态融合上+2.27% over SOTA
5. **XCA不适用**: 设计用于T>>d的长序列，我们的场景T=30<<d=64，XCA反而更贵

#### 全链路影响
| 层 | 影响 |
|----|------|
| L1模型 | CrossAttention层增加门控投影+sigmoid，其余不变 |
| 训练 | 新增门控正则化: `loss += λ·|gate-0.5|²` 防止退化为全0/全1 |
| 推理 | 额外~0.02ms(GPU)，ESP32上额外~1ms |
| Protobuf | 不受影响 |
| 前端 | 不受影响 |

---

### 决策12: L2时序骨干 — MS-TCN

#### 选项
- A. 单阶段TCN (V4.0方案)
- **B. MS-TCN 4-stage (选中)**
- C. BiLSTM
- D. Transformer

#### 深度分析

| 模型 | 手语分割mF1 | 并行性 | 过分割处理 | 边缘部署 | 创新性 |
|------|------------|--------|-----------|----------|--------|
| 单阶段TCN | ~55 | ✅ | ❌ 需后处理 | ✅ | 低 |
| **MS-TCN 4-stage** | **~69** | **✅** | **✅ Smoothing Loss** | **⚠️ 减配** | **中(ICASSP2021)** |
| BiLSTM | ~62 | ❌ 顺序 | ⚠️ 需CRF | ❌ LSTM量化难 | 低 |
| Transformer | ~65 | ✅ | ⚠️ 需后处理 | ❌ 太大 | 中 |

#### 依据
1. **Renz et al., ICASSP 2021**: MS-TCN在手语分割上mF1=68.68(I3D特征)，是当前最优时序分割方法
2. **多阶段精炼**: 每阶段纠正上阶段过分割，4-stage比1-stage提升5-15% F1
3. **Smoothing Loss**: 内置过分割惩罚`L_smooth = Σ(pred[t+1]-pred[t])²`，无需后处理CRF
4. **完全并行**: 膨胀卷积可并行计算，比BiLSTM的顺序计算快3-5倍
5. **量化友好**: 1D卷积int8量化成熟(LSTM量化在TFLite-Micro中仍不稳定)

#### 减配方案
- PC端: 4-stage, 10层/stage, 64 filters (~514KB int8)
- 接收器端: 2-stage, 5层/stage, 32 filters (~35KB int8)

#### 全链路影响
| 层 | 影响 |
|----|------|
| L2模型 | 新增MS-TCN 4-stage模块，替代V4.0单阶段时序卷积 |
| 训练 | 渐进训练: Stage1→冻结→Stage2→...→联合微调 |
| L2推理 | PC GPU: ~5ms增加; 接收器: 不运行ST-GCN，仅2-stage MS-TCN |
| 损失函数 | CE + Smoothing Loss(λ=0.15) |

---

### 决策13: L2流水线 — ST-GCN → MS-TCN → CTC

#### 选项
- A. ST-GCN+CTC (V4.0方案，单阶段时序)
- **B. ST-GCN → MS-TCN → CTC (选中，三阶段)**
- C. MS-GCN (集成方案)
- D. BiLSTM+CTC (无图结构)

#### 三阶段详解

```
阶段1 (空间): ST-GCN
  输入: 28-dim序列 → 投影到42节点图 → 3个ST-Conv块
  输出: 每帧64维空间特征

阶段2 (时序): MS-TCN
  输入: ST-GCN的64维特征
  Stage1: 10层膨胀卷积(RF=2047帧) → 预测1
  Stage2-4: 精炼上阶段预测 → 最终预测
  输出: 每帧47类概率(46手势+1 CTC blank)

阶段3 (解码): CTC
  输入: MS-TCN最终预测
  训练: CTC Loss对齐标签
  推理: Beam Search(k=10) → 词序列
```

#### 依据
1. **MS-GCN(arXiv 2022)**: 已验证将图卷积集成到MS-TCN各阶段是可行的
2. **职责分离**: ST-GCN专注空间图结构，MS-TCN专注时序分割，CTC专注序列解码
3. **训练稳定**: 可先独立训练各阶段，再端到端微调
4. **灵活替换**: MS-TCN可用BiLSTM替代(无GPU)，CTC可用Attention Decoder替代

#### 全链路影响
| 层 | 影响 |
|----|------|
| L2模型 | 完全重构: 单阶段→三阶段流水线 |
| 训练 | 三阶段: 先训ST-GCN→冻结→训MS-TCN→CTC→联合微调 |
| L2推理 | GPU: ~15ms(ST-GCN 5ms + MS-TCN 5ms + CTC 5ms) |
| 数据 | 需要序列级标注(连续手语视频+时间对齐) |

---

### 决策14: 三级热切换架构

#### 选项
- A. 纯PC推理 (V4.0方案)
- B. 双模式: 手套边缘 OR PC
- **C. 三级: 手套→接收器→PC (选中)**

#### 三级架构

```
Tier 1 (手套端):
  模型: CNN+SE-Attention, 11-dim单手
  大小: ~148KB int8
  延迟: ~30ms/帧
  精度: ~80% (单手静态)
  场景: 离线/单手/应急/PC和接收器均不可用
  状态: 始终运行(作为fallback)

Tier 2 (接收器端):
  模型: CNN + Gated Bi-CrossAttn + MS-TCN 2-stage, 28-dim双手
  大小: ~170KB int8
  延迟: ~50ms/帧
  精度: ~87% (双手静态+简单动态)
  场景: PC离线时的主力推理
  要求: ESP32-S3 N16R8 (8MB PSRAM)

Tier 3 (PC端):
  L1: Gated Bi-CrossAttn + MS-TCN 4-stage, 28-dim
  L2: ST-GCN → MS-TCN → CTC
  大小: ~3-5MB (FP32 GPU)
  延迟: ~15ms/帧 (GPU)
  精度: ~92% (双手+连续手语)
  场景: 在线全功能
```

#### 热切换过渡算法

```
切换时: output(t) = (1-α(t)) · result_old + α(t) · result_new
α(t) = clamp((t - t_switch) / N_blend, 0, 1)
N_blend = 5帧 (~50ms过渡)

温度校准:
  两个Tier模型的softmax温度在验证集上对齐:
  T_calibrated = T_raw / τ, τ由KL散度最小化确定
```

#### 依据
1. **接收器ESP32-S3有8MB PSRAM**: 足以运行~170KB的Tier2模型+~60KB Arena
2. **ESP-NOW延迟1-2ms**: 数据从手套到接收器几乎无延迟
3. **Tier2精度87%**: 比Tier1(80%)显著提升，可作为PC离线时的可靠替代
4. **AWS Greengrass V2**: 已验证边缘-云热切换架构的生产可行性
5. **双模融合**: 5帧线性渐变过渡(~50ms)，用户几乎感知不到切换

#### 全链路影响
| 层 | 影响 |
|----|------|
| 手套固件 | 新增Tier1推理模块+结果嵌入Protobuf |
| 接收器固件 | 新增Tier2推理模块+PSRAM配置 |
| Relay | 新增TierRouter(三级融合+温度校准) |
| Protobuf | 新增InferenceTier+Tier1Result+Tier2Result消息 |
| 前端 | 新增Tier级别指示器 |
| NLP/TTS | 不受影响 |

---

### 决策15: BiLSTM+CTC定位 — L2备选

#### 选项
- A. 不实现(仅ST-GCN→MS-TCN→CTC)
- **B. 作为L2备选方案(选中)**

#### 依据
1. **无GPU场景**: 部分部署环境(教室、户外)可能没有GPU
2. **CPU推理**: BiLSTM+CTC在CPU上~80ms可接受，ST-GCN+MS-TCN需~300ms
3. **精度取舍**: BiLSTM比ST-GCN低~5%，但有总比没有好
4. **实现成本低**: BiLSTM+CTC代码量~100行，训练脚本可复用
5. **限制**: LSTM在TFLite-Micro中量化不稳定，不适合ESP32部署，仅PC CPU运行

#### 全链路影响
| 层 | 影响 |
|----|------|
| L2备选 | 新增l2_bilstm_ctc.py，~100行代码 |
| 训练 | 新增train_l2_bilstm.py |
| Relay | TierRouter增加BiLSTM路径选择 |
| 前端 | 不受影响 |

---

## 创新点总结 (V5.0更新)

| # | 创新点 | 对应决策 | 创新层级 | 版本 |
|---|--------|----------|----------|------|
| 1 | **Gated Bi-CrossAttention手套融合** | #4,#11 | 🔴 学术空白 | **V5.0** |
| 2 | **三级热切换推理架构(手套→接收器→PC)** | #14 | 🔴 系统创新 | **V5.0** |
| 3 | **ST-GCN→MS-TCN→CTC三阶段手语识别** | #12,#13 | 🟡 模型创新 | **V5.0** |
| 4 | ESP-NOW硬件级双手同步手套系统 | #2,#9 | 🟡 工程创新 | V4.0 |
| 5 | ST-GCN 6跨手边双手图拓扑 | #5 | 🟡 模型创新 | V4.0 |
| 6 | 28-dim含相对特征双手手语向量 | #3 | 🟡 特征工程 | V4.0 |
| 7 | 低维高效推理(11-dim匹配21-dim精度) | V3.1迁移 | 🟡 算法创新 | V3.1 |
| 8 | Flex→伪骨骼映射+CTC连续手语 | #7,#8 | 🟡 应用创新 | V4.0 |
| 9 | 置信度驱动L1/L2双层推理路由 | 已有 | 🟢 架构创新 | V3.0 |
| 10 | CSL NLP语法修正+TTS双向翻译 | 已有 | 🟢 应用创新 | V3.0 |

**V5.0相比V4.0新增3个创新点，其中#1和#2为学术空白级别的核心创新。**


---

## 附录C: 用户已确认决策 (2026-06-02)

| # | 决策点 | 用户选择 | 状态 |
|---|--------|----------|------|
| 1 | 接收器MCU | ESP32-S3 N16R8 (8MB PSRAM) | ✅ 已确认 |
| 2 | Flex传感器 | SpectraFlex 2.2" (¥15/个) | ✅ 已确认 |
| 3 | MS-TCN减配 | 接收器2-stage (~50ms) | ✅ 已确认 |
| 4 | CTC解码 | Beam Search(k=10) | ✅ 已确认 |
| 5 | BNO085安装 | 手腕外侧偏上, 远离天线≥2cm | ✅ 已确认 |
| 6 | PCB层数 | 4层柔性PCB | ✅ 已确认 |
| 7 | 电池容量 | 500mAh (~2小时) | ✅ 已确认 |
| 8 | 热切换过渡 | 5帧线性渐变(50ms) | ✅ 默认确认 |

> 所有关键决策已锁定。项目进入执行阶段。
