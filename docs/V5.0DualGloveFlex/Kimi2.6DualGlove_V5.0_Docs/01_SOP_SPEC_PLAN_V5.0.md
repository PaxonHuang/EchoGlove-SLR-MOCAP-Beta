# EchoGlove V5.0 SOP-SPEC-PLAN

> 版本: V5.0 | 三级热切换 + Gated Bi-CrossAttention + ST-GCN→MS-TCN→CTC
> 分支: `Flex2.2in-BNO085-ADS1115-ESP32S3N16R8-ArduinoFramework`
> 日期: 2026-06-02

---

## 目录

1. [项目愿景与核心升级](#1-项目愿景与核心升级)
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

## 1. 项目愿景与核心升级

### 1.1 为什么要放弃霍尔传感器方案？

**你的怀疑完全正确。** 经过全球全网调研验证，5根指尖上的N52小磁珠与指根的TMAG5273霍尔传感器以及BNO085 IMU之间存在**严重的磁干扰耦合**：

| 干扰源 | 受影响设备 | 机制 | 严重程度 |
|--------|-----------|------|---------|
| N52磁珠(指尖) | TMAG5273(指根) | 近场强磁场饱和±40mT量程 | 🔴 致命 |
| N52磁珠(指尖) | BNO085磁力计 | 硬铁干扰，校准失效 | 🔴 致命 |
| TMAG5273×5 | 互相串扰 | 多源磁场叠加，非线性 | 🟡 严重 |
| 手部运动 | 磁场方向变化 | 动态干扰，无法滤波消除 | 🟡 严重 |

**BNO085官方文档明确指出**: "Magnetic interference from nearby motors, speakers, or metal objects can corrupt magnetometer readings... use the IMU-only mode that excludes magnetometer data."

**你的V3霍尔方案的问题本质**:
- TMAG5273量程±40mT，N52磁珠表面场强可达~500mT，在<2cm距离内必然饱和
- BNO085的9轴融合依赖磁力计做航向参考，磁珠干扰导致姿态解算漂移
- TCA9548A I2C多路复用器增加了通信延迟和故障点
- 5个霍尔传感器+磁珠的机械结构复杂，佩戴舒适度差

### 1.2 V5.0核心升级

V5.0在V3/V4基础上实现三大升级：

1. **Gated Bidirectional CrossAttention**: 替代V4.0的简单CrossAttention，门控机制解决"空闲手"问题
2. **MS-TCN时序精炼**: 替代V4.0的单阶段时序卷积，多阶段迭代精炼提升手语边界分割精度
3. **三级热切换**: 手套端→接收器端→PC端无缝切换，优雅降级，离线可用

### 1.3 版本演进

| 版本 | 架构 | L1 | L2 | 创新点 |
|------|------|-----|-----|--------|
| V3.0 | Hall+IMU | CNN+Attn 21-dim | ST-GCN 21节点 | 双层推理 |
| V3.1 | Flex+IMU | CNN+Attn 11-dim | ST-GCN 21节点 | 低维高效 |
| V4.0 | 双手Flex+IMU | 双流CrossAttn 28-dim | ST-GCN+CTC 42节点 | ESP-NOW同步 |
| **V5.0** | **三级热切换** | **Gated Bi-CrossAttn** | **ST-GCN→MS-TCN→CTC** | **热切换+门控融合** |


---

## 2. 架构决策记录(ADR)

### ADR-D1: 放弃霍尔传感器，全面转向Flex+IMU

- **状态**: 已决定(V5.0)
- **背景**: V3霍尔方案存在不可解决的磁干扰问题
- **决策**: 5×SpectraFlex 2.2"弯曲传感器 + BNO085(GRV 6轴模式，禁用磁力计) + 2×ADS1115
- **依据**:
  - 全球SLR手套研究普遍采用Flex+IMU方案，文献验证准确率可达85-99%
  - Flex传感器对温度/机械疲劳敏感但可校准补偿，磁干扰是物理不可消除的
  - BNO085在GRV 6轴模式下完全不受磁干扰影响
- **后果**: 特征维度从21(Hall×15+IMU×6)降至11(Flex×5+IMU×6)，但数据质量显著提升

### ADR-D2: 双MCU架构(每手独立ESP32-S3)

- **状态**: 已决定(V5.0)
- **背景**: 双手同步需要亚毫秒级精度
- **决策**: 每手套独立ESP32-S3，通过接收器广播SYNC_TICK同步
- **依据**: ESP-NOW广播延迟1-2ms，免配对，已验证可靠
- **后果**: 成本翻倍(3×ESP32-S3)，但同步精度和模块化程度大幅提升

### ADR-D3: BNO085使用GRV 6轴模式(禁用磁力计)

- **状态**: 已决定(V5.0)
- **背景**: 室内磁干扰不可控，且Flex传感器无磁性
- **决策**: SH2_GAME_ROTATION_VECTOR模式，仅使用加速度计+陀螺仪
- **依据**: BNO085官方支持IMU-only模式，融合率400Hz
- **后果**: 航向会缓慢漂移，但手语识别中绝对航向不重要，相对姿态变化才是关键

### ADR-D4: 2×ADS1115每手(共4个)

- **状态**: 已决定(V5.0)
- **背景**: ESP32-S3内置ADC精度不足(12位)，且多路复用延迟高
- **决策**: 每手2片ADS1115(16位, 860SPS)，0x48(Ch0-2→Flex0-2), 0x49(Ch0-1→Flex3-4)
- **后果**: I2C总线负载增加，需严格100kHz限速

### ADR-D5: 28-dim特征向量含相对特征

- **状态**: 已决定(V5.0)
- **背景**: 双手手语核心信息是空间关系
- **决策**: 28-dim = 左手11 + 右手11 + 相对6(间距3+方位2+角速度1)
- **后果**: 需要接收器计算相对特征，增加~1ms延迟

### ADR-D6: Unity XR Hands + OpenXR

- **状态**: 已决定(V5.0)
- **背景**: React Three Fiber适合MVP，Unity适合生产级
- **决策**: Unity 2022.3 LTS + XR Hands 1.7 + OpenXR标准26关节
- **后果**: 5DoF→26关节需要线性耦合映射(MCP直接, PIP=0.67×MCP, DIP=0.5×MCP)

### ADR-D7: 双手ST-GCN 42节点图结构

- **状态**: 已决定(V5.0)
- **背景**: 单手21节点→双手42节点，需定义跨手边
- **决策**: 42节点(21×2) + 66边(40内+20内+6跨)
- **后果**: 图卷积计算量增加，但PC GPU可承受

### ADR-D11: Gated Bidirectional CrossAttention

- **状态**: 已决定(V5.0)
- **背景**: V4.0普通CrossAttention在单手主导手势时，空闲手噪声干扰严重
- **决策**: 采用Gated Bi-CrossAttention，每维度独立门控
- **门控公式**: `gate = σ(W·[X_self; Attn_cross] + b)`, `output = gate⊙Attn_cross + (1-gate)⊙X_self`
- **后果**: 参数仅增加2×d_model(128个)，空闲手时门控自动关闭，主导手不受干扰
- **论文支撑**: DGCA(WWW 2025) +2.27% over SOTA

### ADR-D12: MS-TCN替代单阶段时序卷积

- **状态**: 已决定(V5.0)
- **背景**: V4.0 L2使用单阶段时序卷积，手语边界过分割严重
- **决策**: 采用MS-TCN(4-stage)多阶段精炼，每阶段纠正上阶段过分割
- **后果**: 计算量约4×单阶段，但PC GPU可承受(~5ms)
- **论文支撑**: Renz et al., ICASSP 2021, MS-TCN在手语分割上mF1=68.68

### ADR-D13: 三级热切换架构

- **状态**: 已决定(V5.0)
- **背景**: 纯PC依赖断线即失效，纯边缘无法运行复杂模型
- **决策**: 三级推理——Tier1(手套CNN)→Tier2(接收器Gated CrossAttn)→Tier3(PC全链路)
- **后果**: 接收器需运行Tier2模型(~170KB int8)，需PSRAM支持
- **过渡策略**: 双模融合，5帧线性渐变(~50ms切换)

### ADR-D14: BiLSTM+CTC作为L2备选

- **状态**: 已决定(V5.0)
- **背景**: 部分部署场景无GPU，ST-GCN+MS-TCN推理慢
- **决策**: BiLSTM+CTC作为无GPU场景的L2备选方案
- **后果**: 精度略低(~5%)，但CPU推理80ms可接受
- **限制**: LSTM在TFLite-Micro中量化困难，不适合边缘部署


---

## 3. 系统架构总览

### 3.1 三级推理架构

```
+-------------------------------------------------------------+
|                 EchoGlove V5.0 三级推理架构                    |
+--------------+--------------+-------------------------------+
|  Tier 1      |  Tier 2      |       Tier 3                  |
|  手套端       |  接收器端     |       PC端                     |
|              |              |                               |
|  CNN+Attn    |  Gated Bi-   |  L1: Gated Bi-CrossAttn       |
|  11-dim      |  CrossAttn   |     + Full MS-TCN             |
|  单手46类     |  + MS-TCN    |  L2: ST-GCN → MS-TCN → CTC   |
|  ~148KB      |  2-stage     |     + Beam Search             |
|  ~30ms       |  28-dim      |  ~3-5MB (GPU)                 |
|  ~80%精度    |  ~170KB      |  ~15ms (GPU)                  |
|  离线可用     |  ~50ms       |  ~92%精度                     |
|              |  ~87%精度     |  连续手语                      |
|              |  需双手数据   |  完整功能                      |
+--------------+--------------+-------------------------------+
|  始终运行     |  PC离线时激活 |  PC在线时主力                   |
|  作为fallback │  双手edge    │  最高精度                      │
+--------------+--------------+-------------------------------+
```

### 3.2 数据流

```
传感器(5×Flex + BNO085) ─I2C 100kHz→ MCU采样(11-dim, 100Hz)
    → Kalman滤波 → 归一化
    ┌→ Tier1: CNN+Attention → 单手手势(始终运行)
    │
    └→ Protobuf编码(+hand_id) → ESP-NOW发送
        → 接收器汇聚(按tick_id配对)
        ┌→ Tier2: Gated Bi-CrossAttn + MS-TCN 2-stage → 双手手势(PC离线时)
        │
        └→ USB/WiFi → PC Relay
            → Tier3 L1: Gated Bi-CrossAttn + MS-TCN 4-stage
            → Tier3 L2: ST-GCN(42节点) → MS-TCN(4-stage) → CTC
            → Confidence Router(三级结果融合) → NLP → TTS
            → WebSocket → React3D / Unity XR Hands
```

### 3.3 热切换时序

```
正常模式(Tier3):
  手套: 采样+Tier1推理(~30ms) + ESP-NOW发送
  接收器: 汇聚+转发到PC
  PC: Tier3全链路推理(~15ms GPU)
  输出: Tier3结果(α=1.0)

PC断开过渡(50ms):
  帧1-2: α=0.6 Tier2 + 0.4 Tier3(缓存)
  帧3-4: α=0.2 Tier2 + 0.8(缓存+外推)
  帧5+:  α=1.0 Tier2(完全接管)

PC恢复过渡(50ms):
  帧1-2: α=0.4 Tier2 + 0.6 Tier3
  帧3-4: α=0.2 Tier2 + 0.8 Tier3
  帧5+:  α=1.0 Tier3(完全接管)
```

---

## 4. Phase 1: 单手固件稳定化 + Tier1模型部署

### 4.1 关键模块

| 模块 | 文件 | 功能 |
|------|------|------|
| ADS1115Manager | `lib/Sensors/ADS1115Manager.h` | 双ADC读取5路Flex |
| FlexManager | `lib/Sensors/FlexManager.h` | 5点校准+滑动平均+温漂补偿 |
| IMUManager | `lib/Sensors/IMUManager.h` | BNO085 GRV 6轴 |
| SensorManager | `lib/Sensors/SensorManager.h` | 统一管理，输出11-dim |

### 4.2 V5.0新增: Tier1模型部署

```cpp
// 文件: glove_firmware/lib/Inference/Tier1Model.h
class Tier1Model {
public:
    void begin();  // 加载TFLite模型到PSRAM
    Tier1Result infer(const float features[11]);

private:
    // TFLite-Micro int8量化模型
    // 大小: ~148KB Flash, ~50KB PSRAM Arena
    // 推理: ~30ms @ 240MHz
    tflite::MicroInterpreter* _interpreter;
    float* _input;
    float* _output;
};

struct Tier1Result {
    uint32_t gesture_id;
    float confidence;
    uint32_t inference_time_ms;
};
```

---

## 5. Phase 2: 单手数据采集与模型训练

### 5.1 训练流水线

```
原始CSV → Kalman滤波 → 归一化 → 滑动窗口(30帧)
    → PyTorch训练CNN+Attention(11-dim, 46类)
    → 验证精度>85%
    → 导出ONNX → TFLite → int8量化
    → 部署到ESP32-S3 (Tier1模型)
```

### 5.2 TFLite导出脚本

```python
# tools/export_tier1.py
import torch
import onnx
from onnx_tf.backend import prepare
import tensorflow as tf

def export_tflite(model, input_shape=(1, 30, 11), output_path='tier1_model.tflite'):
    # PyTorch → ONNX
    dummy = torch.randn(*input_shape)
    torch.onnx.export(model, dummy, 'tier1.onnx', opset_version=13)

    # ONNX → TensorFlow → TFLite
    onnx_model = onnx.load('tier1.onnx')
    tf_rep = prepare(onnx_model)
    tf_rep.export_graph('tier1_tf')

    converter = tf.lite.TFLiteConverter.from_saved_model('tier1_tf')
    converter.optimizations = [tf.lite.Optimize.DEFAULT]
    converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
    tflite_model = converter.convert()

    with open(output_path, 'wb') as f:
        f.write(tflite_model)
    print(f"TFLite int8 model saved: {len(tflite_model)/1024:.1f} KB")
```

---

## 6. Phase 3: Unity XR可视化

关键点:
- Unity XR Hands 1.7, 26关节OpenXR标准
- 5DoF→26关节: MCP直接, PIP=0.67×MCP, DIP=0.5×MCP
- Phase2升级MANO参数化

---

## 7. Phase 4: 双手系统升级

### 7.1 ESP-NOW通信（与V4.0相同）

### 7.2 Gated Bi-CrossAttention（V5.0升级）

详见 [10.1节](#101-l1-gated-bidirectional-crossattention)

---

## 8. Phase 5: 三级热切换架构

### 8.1 接收器Tier2模型部署

```cpp
// 文件: receiver_firmware/lib/Tier2Model.h
class Tier2Model {
public:
    void begin();
    Tier2Result infer(const float left[11], const float right[11],
                      const float relative[6]);

private:
    // Gated Bi-CrossAttention + Reduced MS-TCN (2-stage)
    // 大小: ~170KB Flash, ~60KB PSRAM Arena
    // 推理: ~50ms @ 240MHz (PSRAM 80MHz模式)
    tflite::MicroInterpreter* _interpreter;
};

struct Tier2Result {
    uint32_t gesture_id;
    float confidence;
    uint32_t inference_time_ms;
    bool valid;  // 是否有足够数据推理
};
```

### 8.2 接收器主循环

```cpp
// receiver_firmware/main.cpp
void loop() {
    // 1. 广播SYNC_TICK
    broadcastSyncTick();

    // 2. 等待双手数据(超时5tick = 50ms)
    if (waitForBothHands(5)) {
        // 3. 计算相对特征
        computeRelativeFeatures(left, right, relative);

        // 4. Tier2推理
        Tier2Result result = tier2.infer(left.features, right.features, relative);

        // 5. 转发到PC(带Tier2结果)
        forwardToPC(left, right, relative, result);
    } else {
        // 超时，转发可用数据(单手)
        forwardAvailable();
    }
}
```


### 8.3 PC Relay三级融合

```python
# glove_relay/src/tier_router.py
class TierRouter:
    """V5.0三级推理路由与融合"""

    def __init__(self):
        self.tier1_left = None    # 手套Tier1结果(左手)
        self.tier1_right = None   # 手套Tier1结果(右手)
        self.tier2_result = None  # 接收器Tier2结果
        self.tier3_result = None  # PC Tier3结果
        self.active_tier = TierLevel.TIER3
        self.blend_alpha = 1.0    # 1.0=完全使用当前active_tier
        self.blend_frames = 5     # 过渡帧数

    def update(self, dual_data: DualGloveData):
        # 从Protobuf提取各级结果
        if dual_data.left.HasField('tier1_result'):
            self.tier1_left = dual_data.left.tier1_result
        if dual_data.right.HasField('tier1_result'):
            self.tier1_right = dual_data.right.tier1_result
        if dual_data.HasField('inference_tier'):
            self.tier2_result = dual_data.inference_tier.tier2_result

        # Tier3: 本地推理
        if self._pc_available:
            self.tier3_result = self._run_tier3(dual_data)

        # 确定active tier
        self._update_active_tier()

        # 融合输出
        return self._blend_results()

    def _update_active_tier(self):
        if self._pc_available:
            new_tier = TierLevel.TIER3
        elif self.tier2_result and self.tier2_result.valid:
            new_tier = TierLevel.TIER2
        else:
            new_tier = TierLevel.TIER1

        if new_tier != self.active_tier:
            self.active_tier = new_tier
            self.blend_alpha = 0.0  # 开始过渡

    def _blend_results(self):
        # 线性渐变融合
        if self.blend_alpha < 1.0:
            self.blend_alpha = min(1.0, self.blend_alpha + 1.0/self.blend_frames)

        # 获取当前级和前一级结果
        current = self._get_tier_result(self.active_tier)
        previous = self._get_tier_result(self._previous_tier)

        # 温度校准后融合
        calibrated_current = self._calibrate_temperature(current)
        calibrated_previous = self._calibrate_temperature(previous)

        blended = {}
        for cls_id in range(len(calibrated_current)):
            blended[cls_id] = (
                self.blend_alpha * calibrated_current[cls_id] +
                (1 - self.blend_alpha) * calibrated_previous[cls_id]
            )
        return blended

    def _calibrate_temperature(self, probs: dict) -> dict:
        """温度校准: 不同Tier模型的softmax分布对齐"""
        tier = self.active_tier
        tau = self._temperature[tier]  # 验证集KL散度最小化得到
        return {k: v**tau for k, v in probs.items()}
```

### 8.4 WebSocket消息格式V5.0

```json
{
    "timestamp": 1234567890,
    "tier": "TIER3",
    "blend_alpha": 1.0,
    "left_hand": {
        "flex": [45.2, 30.1, 60.5, 15.3, 22.8],
        "imu": [0.12, -0.34, 1.57, 0.01, -0.02, 0.05],
        "tier1_gesture": 5,
        "tier1_confidence": 0.82
    },
    "right_hand": {
        "flex": [10.5, 85.2, 5.0, 88.1, 3.2],
        "imu": [0.15, -0.30, 1.60, -0.02, 0.01, 0.03],
        "tier1_gesture": 12,
        "tier1_confidence": 0.91
    },
    "relative": [0.15, -0.05, 0.30, 0.98, 0.12, 0.05],
    "inference": {
        "gesture_id": 23,
        "confidence": 0.94,
        "active_tier": "TIER3",
        "tier2_gesture": 23,
        "tier2_confidence": 0.88,
        "tier3_gesture": 23,
        "tier3_confidence": 0.94
    },
    "l2_result": {
        "words": ["你", "好"],
        "ctc_confidence": [0.95, 0.91]
    },
    "nlp_text": "你好",
    "tts_active": true
}
```

---

## 9. Phase 6: 进阶优化

### 9.1 MANO参数化模型(替换线性耦合)
### 9.2 多模态视觉融合(MediaPipe补全)
### 9.3 BiLSTM+CTC备选实现(无GPU场景)

---

## 10. 核心模型架构深度设计

### 10.1 L1: Gated Bidirectional CrossAttention

#### 10.1.1 架构概述

```
左手 11-dim → Conv1D(11→64, k=3) → 左流特征 (T×64)
右手 11-dim → Conv1D(11→64, k=3) → 右流特征 (T×64)

左流自注意力: SelfAttn(Q_L, K_L, V_L) → self_L
右流自注意力: SelfAttn(Q_R, K_R, V_R) → self_R

左→右交叉: cross_L2R = MultiHeadAttn(Q=self_L, K=V=self_R)
右→左交叉: cross_R2L = MultiHeadAttn(Q=self_R, K=V=self_L)

门控:
  gate_L = σ(W_g · [self_L; cross_L2R] + b_g)     ← V5.0关键创新
  gated_L = gate_L ⊙ cross_L2R + (1-gate_L) ⊙ self_L

  gate_R = σ(W_g · [self_R; cross_R2L] + b_g)
  gated_R = gate_R ⊙ cross_R2L + (1-gate_R) ⊙ self_R

融合:
  relative(6-dim) → FC(6→64) → rel_feat
  concat = [gated_L; gated_R; rel_feat] → Pool → FC → num_classes
```

#### 10.1.2 门控机制详解

**问题**: 双手手语中，约40%的手势是单手主导的(如"我"=右手食指指自己，左手不动)。V4.0的普通CrossAttention会让空闲手的噪声干扰主导手。

**解决方案**: 每维度独立门控，自动学习何时使用跨手信息。

```python
class GatedCrossAttention(nn.Module):
    def __init__(self, d_model=64, num_heads=4, dropout=0.1):
        super().__init__()
        self.cross_attn = nn.MultiheadAttention(d_model, num_heads,
                                                 dropout=dropout, batch_first=True)
        self.gate_proj = nn.Linear(d_model * 2, d_model)
        self.norm = nn.LayerNorm(d_model)

    def forward(self, x_self, x_cross):
        # x_self: (B, T, d), x_cross: (B, T, d)
        # Cross-attention: self queries cross
        attn_out, _ = self.cross_attn(
            query=x_self, key=x_cross, value=x_cross
        )  # (B, T, d)

        # 门控: 学习每个维度是否使用cross信息
        gate = torch.sigmoid(
            self.gate_proj(torch.cat([x_self, attn_out], dim=-1))
        )  # (B, T, d)

        # 门控融合
        output = gate * attn_out + (1 - gate) * x_self
        return self.norm(output + x_self)  # residual + norm
```

**参数增量**: 仅 `gate_proj = Linear(128→64) = 8,256参数`，微不足道。

**效果**:
- 单手主导手势: gate→0, 退化为自注意力，空闲手不影响
- 双手协同手势: gate→1, 充分利用跨手信息
- 动态适应: 不同手势不同门控模式，无需手动规则

#### 10.1.3 三级L1差异

| 特性 | Tier1(手套) | Tier2(接收器) | Tier3(PC) |
|------|------------|--------------|-----------|
| 输入 | 11-dim单手 | 28-dim双手 | 28-dim双手 |
| 架构 | CNN+SE-Attn | CNN+Gated Bi-CrossAttn | CNN+Gated Bi-CrossAttn |
| 时序 | 无(单帧) | MS-TCN 2-stage | MS-TCN 4-stage |
| 参数 | ~148KB int8 | ~170KB int8 | ~500KB FP32 |
| 精度 | ~80% | ~87% | ~92% |


### 10.2 L2: ST-GCN → MS-TCN → CTC 三阶段

#### 10.2.1 阶段1: ST-GCN空间图卷积

```python
class DualHandSTGCN(nn.Module):
    """42节点双手图 + 6跨手边"""

    def __init__(self, input_dim=28, hidden_dim=64, num_nodes=42):
        super().__init__()
        # 28-dim → 42节点×2坐标 的投影
        self.skeleton_proj = nn.Linear(input_dim, num_nodes * 2)

        # 图邻接矩阵 (66条边)
        self.adj = self._build_dual_adjacency()

        # 3个ST-Conv块
        self.st_blocks = nn.ModuleList([
            STConvBlock(hidden_dim, self.adj) for _ in range(3)
        ])

        # 时序池化 → 每帧64维特征
        self.temporal_pool = nn.AdaptiveAvgPool1d(1)

    def _build_dual_adjacency(self):
        # 42节点, 66边 (40左+40右+6跨手)
        edges = [
            # 左手 (20条)
            (0,1),(1,2),(2,3),(3,4), (0,5),(5,6),(6,7),(7,8),
            (0,9),(9,10),(10,11),(11,12), (0,13),(13,14),(14,15),(15,16),
            (0,17),(17,18),(18,19),(19,20),
            # 右手 (20条, 偏移21)
            (21,22),(22,23),(23,24),(24,25), (21,26),(26,27),(27,28),(28,29),
            (21,30),(30,31),(31,32),(32,33), (21,34),(34,35),(35,36),(36,37),
            (21,38),(38,39),(39,40),(40,41),
            # 跨手 (6条)
            (0,21), (4,25), (8,29), (12,33), (16,37), (20,41)
        ]
        # 构建归一化邻接矩阵...
        return adj

    def forward(self, x):  # (B, T, 28)
        # 投影到图空间
        x = self.skeleton_proj(x)  # (B, T, 84)
        x = x.view(x.size(0), x.size(1), 42, 2)  # (B, T, 42, 2)

        # 图卷积
        for block in self.st_blocks:
            x = block(x)  # (B, T, 42, 64)

        # 节点聚合 → 每帧特征
        x = x.mean(dim=2)  # (B, T, 64)
        return x
```

#### 10.2.2 阶段2: MS-TCN多阶段时序精炼

```python
class SingleStageTCN(nn.Module):
    """单阶段TCN: 10层膨胀因果卷积"""

    def __init__(self, in_dim, hidden_dim=64, num_classes=47, num_layers=10):
        super().__init__()
        self.conv = nn.ModuleList()
        self.bn = nn.ModuleList()

        for l in range(num_layers):
            dilation = 2 ** l
            self.conv.append(nn.Conv1d(
                hidden_dim if l > 0 else in_dim,
                hidden_dim,
                kernel_size=3,
                padding=dilation,
                dilation=dilation
            ))
            self.bn.append(nn.BatchNorm1d(hidden_dim))

        self.output_conv = nn.Conv1d(hidden_dim, num_classes, kernel_size=1)

    def forward(self, x):  # (B, T, C_in)
        x = x.permute(0, 2, 1)  # (B, C, T)
        for conv, bn in zip(self.conv, self.bn):
            residual = x
            x = torch.relu(bn(conv(x)))
            x = x + residual[:, :, -x.size(2):]  # residual连接
        return self.output_conv(x).permute(0, 2, 1)  # (B, T, num_classes)


class MSTCN(nn.Module):
    """多阶段TCN: 4阶段迭代精炼"""

    def __init__(self, in_dim=64, hidden_dim=64, num_classes=47,
                 num_stages=4, num_layers=10):
        super().__init__()
        self.stages = nn.ModuleList()

        # Stage 1: 特征提取(从空间特征)
        self.stages.append(SingleStageTCN(in_dim, hidden_dim, num_classes, num_layers))

        # Stage 2-4: 精炼(从上阶段预测+空间特征)
        for _ in range(num_stages - 1):
            self.stages.append(SingleStageTCN(
                num_classes + in_dim, hidden_dim, num_classes, num_layers
            ))

        self.smoothing_lambda = 0.15  # 过分割惩罚系数

    def forward(self, spatial_features):  # (B, T, 64)
        predictions = []
        x = spatial_features

        for i, stage in enumerate(self.stages):
            if i == 0:
                pred = stage(x)
            else:
                # 拼接上阶段预测 + 原始空间特征
                stage_input = torch.cat([pred, spatial_features], dim=-1)
                pred = stage(stage_input)
            predictions.append(pred)

        return predictions  # [pred_1, pred_2, pred_3, pred_4]

    def compute_loss(self, predictions, targets, lengths):
        """分类损失 + 平滑损失"""
        total_loss = 0
        for pred in predictions:
            # 分类损失(每帧CE)
            cls_loss = F.cross_entropy(pred.view(-1, pred.size(-1)),
                                        targets.view(-1),
                                        reduction='none')
            # 平滑损失(惩罚相邻帧预测突变)
            diff = pred[:, 1:, :] - pred[:, :-1, :]
            smooth_loss = torch.mean(diff ** 2)

            total_loss += cls_loss.mean() + self.smoothing_lambda * smooth_loss

        # 后期阶段权重更大
        weights = [1.0, 0.5, 0.25, 0.125][:len(predictions)]
        total_loss = sum(w * l for w, l in zip(weights, [total_loss]))

        return total_loss / len(predictions)
```

#### 10.2.3 阶段3: CTC解码

```python
class CTCDecoder(nn.Module):
    """CTC连续手语序列解码"""

    def __init__(self, num_classes=47, blank_idx=0, beam_width=10):
        super().__init__()
        self.blank_idx = blank_idx
        self.beam_width = beam_width
        self.ctc_loss = nn.CTCLoss(blank=blank_idx, zero_infinity=True)

    def forward_loss(self, logits, targets, input_lengths, target_lengths):
        """训练时CTC损失"""
        log_probs = F.log_softmax(logits, dim=-1)
        return self.ctc_loss(log_probs, targets, input_lengths, target_lengths)

    def decode(self, logits):
        """推理时Beam Search解码"""
        log_probs = F.log_softmax(logits, dim=-1)
        # (T, B, C) → Beam Search → 词序列
        # 使用简单CTC greedy decode作为基础
        best_path = log_probs.argmax(dim=-1)  # (T, B)

        # 去重+去blank
        results = []
        for b in range(best_path.size(1)):
            path = best_path[:, b].tolist()
            merged = []
            prev = None
            for p in path:
                if p != self.blank_idx and p != prev:
                    merged.append(p)
                prev = p
            results.append(merged)
        return results
```

### 10.3 BiLSTM+CTC备选方案

```python
class BiLSTM_CTC(nn.Module):
    """无GPU场景的L2备选方案"""

    def __init__(self, input_dim=28, hidden_dim=128, num_layers=2,
                 num_classes=47):
        super().__init__()
        self.lstm = nn.LSTM(input_dim, hidden_dim, num_layers,
                            bidirectional=True, batch_first=True,
                            dropout=0.3)
        self.fc = nn.Linear(hidden_dim * 2, num_classes)
        self.ctc = CTCDecoder(num_classes)

    def forward(self, x):  # (B, T, 28)
        x, _ = self.lstm(x)
        logits = self.fc(x)  # (B, T, num_classes)
        return logits

    # 优势: 实现简单，CPU推理~80ms
    # 劣势: 无空间图建模，精度低~5%，LSTM量化困难
```

### 10.4 模型参数总览

| 模型 | 参数量 | Int8大小 | 推理延迟 | 精度(预估) |
|------|--------|---------|----------|-----------|
| Tier1 CNN+Attn (11-dim) | ~148K | ~148KB | 30ms(ESP32) | ~80% |
| Tier2 Gated Bi-CrossAttn+MS-TCN2s (28-dim) | ~170K | ~170KB | 50ms(ESP32+PSRAM) | ~87% |
| Tier3 L1 Full (28-dim) | ~500K | ~500KB | 15ms(GPU) | ~92% |
| Tier3 L2 ST-GCN+MS-TCN+CTC | ~3M | ~3MB | 15ms(GPU) | ~88%(连续) |
| 备选 L2 BiLSTM+CTC | ~1M | ~1MB | 80ms(CPU) | ~83%(连续) |


---

## 11. 全链路迁移影响矩阵

### 11.1 V3/V4 → V5.0 变更

| 层 | 变更项 | 影响度 | 具体变更 |
|----|--------|--------|----------|
| 硬件 | 放弃霍尔传感器 | 🔴 | 移除TMAG5273×5+N52磁珠+TCA9548A |
| 硬件 | 新增Flex传感器 | 🔴 | 5×SpectraFlex 2.2"每手+分压电阻 |
| 硬件 | 新增ADS1115 | 🔴 | 2×每手，共4片，I2C 100kHz |
| 固件 | 移除Hall驱动 | 🔴 | 删除TMAG5273+TCA9548A代码 |
| 固件 | 新增Flex驱动 | 🔴 | ADS1115Manager+FlexManager |
| 固件 | BNO085模式变更 | 🟡 | 9轴→GRV 6轴(禁用磁力计) |
| 固件 | Tier1模型部署 | 🔴 | 新增TFLite-Micro推理模块(~148KB) |
| 固件 | Tier1结果嵌入 | 🟡 | Protobuf消息增加tier1_result字段 |
| 接收器 | Tier2模型部署 | 🔴 | 新增Gated Bi-CrossAttn+MS-TCN2s推理 |
| 接收器 | 状态LED | 🟢 | 新增Tier级别指示灯 |
| Proto | InferenceTier消息 | 🟡 | 新增tier字段+Tier1Result+Tier2Result |
| Relay | TierRouter | 🔴 | 新增三级融合+双模过渡+温度校准 |
| L1 | Gated Bi-CrossAttn | 🟡 | 门控层替代简单CrossAttn |
| L2 | MS-TCN | 🔴 | 新增4阶段TCN精炼模块 |
| L2 | ST-GCN→MS-TCN→CTC | 🔴 | 三阶段流水线重构 |
| L2备选 | BiLSTM+CTC | 🟡 | 新增备选L2实现 |
| Web | Tier状态显示 | 🟢 | 新增当前Tier级别指示器 |
| Unity | Tier切换通知 | 🟢 | 新增Tier级别UI |
| NLP | 不受影响 | 🟢 | — |
| TTS | 不受影响 | 🟢 | — |

---

## 12. 已知风险与缓解方案

### 12.1 V5.0新增风险

| 风险 | 概率 | 影响 | 缓解方案 |
|------|------|------|----------|
| 接收器PSRAM不足 | 低 | Tier2无法运行 | 降低MS-TCN至1-stage，模型约100KB |
| Tier2推理超50ms | 中 | 帧率下降 | 降频至50Hz或简化模型 |
| 热切换时输出跳变 | 中 | 用户体验差 | 延长blend帧至10帧(100ms) |
| 温度校准不准 | 中 | Tier间概率分布不对齐 | 验证集上重标定，增加校准样本 |
| MS-TCN过分割 | 中 | 手语边界不准 | 调高smoothing_lambda(0.15→0.3) |
| Tier1/Tier2结果冲突 | 低 | 置信度倒挂 | 信任更高级别，忽略低级别 |
| Flex传感器疲劳漂移 | 中 | 长期精度下降 | 每30秒自动零点校准+温漂补偿 |
| BNO085 GRV漂移 | 低 | 航向缓慢漂移 | 手语识别不依赖绝对航向，可接受 |

### 12.2 模型训练风险

| 风险 | 缓解方案 |
|------|----------|
| Gated CrossAttn门控退化为全0/全1 | 添加门控正则化: loss += λ·|gate-0.5|² |
| MS-TCN后期stage梯度消失 | 渐进训练: 先训Stage1→冻结→训Stage2→... |
| CTC blank类过多 | 调整blank权重+语言模型约束 |
| ST-GCN→MS-TCN接口不对齐 | 统一中间特征维度=64 |
| Flex传感器个体差异大 | 每用户独立校准+数据增强 |

---

## 附录A: 全球参考项目对比

| 项目 | 传感器 | 模型 | 精度 | 年份 |
|------|--------|------|------|------|
| EchoGlove V5.0 (本项) | Flex×5+IMU×1 | Gated Bi-CrossAttn+MS-TCN+CTC | ~92% | 2026 |
| PSL Smart Glove | Flex×5+MPU9250 | LSTM | ~94% | 2025 |
| Lenguantec LSM | Flex+IMU(ESP32-C3) | ML Classifier | 97% | 2026 |
| UltraGlove | MEMS-Ultrasonic | Deep Network | — | 2023 |
| iManus Rehab | Flex×5+IMU×1 | — | — | 2022 |
| DGCA(WWW2025) | 文本+图像 | Gated CrossAttn | +2.27% | 2025 |
| MS-TCN Sign Seg | 视频(I3D特征) | MS-TCN 4-stage | mF1=68.68 | 2021 |
| DSLNet | 骨架42节点 | Dual-STGCN | — | 2025 |

---

## 附录B: 关键决策点清单（供用户审阅）

| # | 决策点 | 当前选择 | 备选方案 | 风险 |
|---|--------|----------|----------|------|
| 1 | 双手架构 | 双MCU(3×ESP32-S3) | 单MCU+I2C Mux | 成本+3 |
| 2 | BNO085模式 | GRV 6轴(无磁力计) | 9轴(有磁力计) | 磁干扰 |
| 3 | Flex传感器 | SpectraFlex 2.2" | 国产替代¥5/个 | 精度-10% |
| 4 | 接收器MCU | ESP32-S3 N16R8 | ESP32-S3 N8R8 | PSRAM不足 |
| 5 | 热切换过渡 | 5帧线性渐变(50ms) | 10帧(100ms) | 感知延迟 |
| 6 | MS-TCN stage | PC:4-stage, 接收器:2-stage | 统一4-stage | 接收器性能 |
| 7 | CTC解码 | Greedy + Beam Search(k=10) | 纯Greedy | 精度-3% |
| 8 | Unity渲染 | XR Hands 1.7 + OpenXR | 自定义骨骼 | 兼容性问题 |
