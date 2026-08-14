# 04 · L1 边缘 CNN：Tier1CNN + SE-Attention（11→46 类单手分类）

> **Date**: 2026-08-11 ｜ **依据**: `glove_relay/src/models/tier1_cnn.py` + `configs/model_config.yaml`（`cnn_attention_v2`）｜ **真实性**: 结构 ✅ / 权重 🔬 / S3 部署 🟡

---

## 1. 角色：L1 边缘轻量分类

L1 消费**单只手 11 维特征**（5 flex + 3 euler + 3 gyro），输出 46 类手势。设计目标：边缘/中继侧毫秒级推理、<3ms 延迟、~35K 参数（`model_config.yaml` metadata）。

```
 (batch, 11) ──► L1 Tier1CNN ──► (batch, 46)  logits ──► softmax ──► (class, conf)
   单手特征          边缘/relay          46 类手势            Top-1 + 置信度
```

> ⚠️ 现状：`active_l1_model: cnn_attention_v2`（registry 加载 ✅，B1/B3 修复后 warmup/predict 验证通过）；**权重为随机初始化**（B2）——46 类精度需真实数据训练（`07_training_quant.md`）。S3 板上推理（`Task_Inference`）此前从不运行，属已确认 bug 清单，S3 部署待生产路径落地 🟡。

---

## 2. 网络结构（真实代码）

`Tier1CNN` 是带 SE 注意力块的 MLP 主干：

```python
nn.Sequential(
    nn.Linear(input_dim, hidden),      # (B,11) → (B,64)
    nn.ReLU(),
    nn.BatchNorm1d(hidden),            # (B,64)
    SEBlock(hidden),                   # (B,64)
    nn.Linear(hidden, hidden),         # (B,64) → (B,64)
    nn.ReLU(),
    nn.BatchNorm1d(hidden),
    SEBlock(hidden),
    nn.Linear(hidden, num_classes),    # (B,64) → (B,46)
)
```

**SEBlock**（Squeeze-and-Excitation，`reduction=4`）：

$$
\mathbf{z} = \text{Linear}_{c \to c/4}(\mathbf{x}) \xrightarrow{\text{ReLU}} \text{Linear}_{c/4 \to c}(\cdot) \xrightarrow{\sigma}
$$

$$
\text{SE}(\mathbf{x}) = \mathbf{x} \odot \sigma\!\Big(W_2\, \text{ReLU}\big(W_1\, \mathbf{x}\big)\Big), \qquad W_1 \in \mathbb{R}^{c/4 \times c},\ W_2 \in \mathbb{R}^{c \times c/4}
$$

（注意：此实现的 SE 直接作用于特征向量本身而非全局池化通道权重——对单帧特征做逐通道门控重标定，等价于「通道注意力」作用于当前样本。）

**时间窗口**：`forward` 对 3D 输入 `(B, T, 11)` 先取时间均值再走主干：

$$
\mathbf{x} = \frac{1}{T}\sum_{t=1}^{T} \mathbf{x}_t
$$

——与 `udp_server._run_l2` 的 `(T,22)` 窗口配套：L1 用均值池化压缩时间，L2 交叉注意力在时间轴均值（`tier2_cross_attn.py` 同策略）。

---

## 3. 网络层架构图（张量维度）

```
输入  (B, 11)         5 flex + 3 euler + 3 gyro（单手，V6 11 维契约）
        │
        ▼ Linear 11→64
      (B, 64) ──► ReLU ──► BatchNorm1d
        │
        ▼ SEBlock(64, r=4)
        │   z = W2·ReLU(W1·x);   SE(x) = x ⊙ σ(z)
      (B, 64)
        │
        ▼ Linear 64→64 ──► ReLU ──► BatchNorm1d
        │
        ▼ SEBlock(64, r=4)
      (B, 64)
        │
        ▼ Linear 64→46
      (B, 46)  logits
        │
        ▼ softmax ──► Top-1 (class_idx, confidence)
      (class, conf)            → 低置信度时 fallback 到 L2（hot-switch auto_fallback）

参数统计：11·64 + 64·(64/4)·2·2 + 64·64 + 64·46 ≈ 35K（config metadata 0.42 MFLOPs, 1.2ms @ i5）
```

---

## 4. 数学要点

- **BatchNorm1d**（推理折叠）：$\hat{x}_i = \gamma\,\dfrac{x_i - \mu}{\sqrt{\sigma^2 + \epsilon}} + \beta$，推理期折叠为仿射。
- **Softmax 置信度**：$p_k = \dfrac{e^{z_k}}{\sum_j e^{z_j}}$，`predict` 返回 Top-1 类 + 置信度（`adapters.py::_argmax_confidence`）。
- **热切换**：`BaseModel` 接口 + registry 动态加载；`hot_switch.auto_fallback=true` 失败时回退上一模型。

---

## 5. 部署路径与现状

| 层 | 目标 | 现状 |
|----|------|------|
| relay/边缘 CPU | cnn_attention_v2（INT8 可选） | ✅ registry 加载；权重随机 🔬 |
| S3 板上 L1（<3ms） | 微型 TFLite 部署 | 🟡 `Task_Inference` 现不运行（bug 清单）；依赖 M2+ 数据 |

**导出链**（L2 的 INT8 导出见 `07_training_quant.md`；L1 同类：PyTorch→ONNX→TFLite INT8→C 数组，`glove_firmware/scripts/export_model.py` 模式）。

---

## 6. 真实性

| 能力 | 标注 | 说明 |
|------|------|------|
| Tier1CNN + SE 结构 | ✅ | 代码 + registry 加载/warmup/predict 验证（B1/B3 修复后 8/8） |
| 11 维输入 / 46 类输出契约 | ✅ | `model_config.yaml` + `adapters.py` |
| 真实权重 + >90% Top-1 | 🔬 | 无 checkpoint（B2），需采集+训练 |
| S3 板上 <3ms 部署 | 🟡 | `Task_Inference` 未运行，生产路径待排 |

---

*本文档网络结构为真实代码；性能数字来自 config metadata（估计值），权重精度 🔬 待训练。*
