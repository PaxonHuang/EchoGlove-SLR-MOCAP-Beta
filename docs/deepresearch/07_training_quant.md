# 07 · 训练 + INT8 量化（数据管道 → checkpoint → 板载部署）

> **Date**: 2026-08-11 ｜ **依据**: `glove_relay/scripts/train_tier1.py`、`glove_firmware/scripts/export_model.py`、生产设计 §7 ｜ **真实性**: 管线 ✅ / 真实权重 🔬 / 校准 🔬

---

## 1. 全景：从数据到板载模型

```
采集 (Hand Token v1 帧)
    serial / UDP / USB-CDC 三源 ──► data_collector ──► CSV / npz
        │
        ▼
训练 train_tier1.py (11 维) ──► checkpoint .pt ──► export_model.py
                                                    │
                           PyTorch → ONNX → TF SavedModel → TFLite INT8 → C 头文件
                                                    │
                                                    ▼
                                     P4 / S3 板载 TFLite Micro 推理
```

**关键事实**：
- **数据资产几乎为空**：`synthetic_dataset.npz`（21 维伪造）+ 30 个 `zero_*.csv`。synthetic 数据**只能验证管线形状，不能当训练集**（生产设计 §7 真实性分级）。
- **无任何 checkpoint**（B2，随机权重）：46 类精度前提 = 数据量 + 训练。
- **UDP 采集未实现**（B8）：`data_collector` 现状仅 serial；UDP 模式待实现。

---

## 2. 训练管线

`train_tier1.py`（已是 11 维契约）：加载数据 → 训练 Tier1CNN → 存 `.pt`。

$$
\mathcal{L} = \text{CrossEntropy}(\hat{y}, y), \qquad
\hat{y} = \text{softmax}(f_\theta(\mathbf{x}))
$$

**数据组织**（生产设计 §7）：以 **Hand Token v1 帧**为单位落盘（CSV + 可选 npz），带 `timestamp_us` 与 device_id 供双手对齐。训练目标 46 类 Top-1 >90%（L1）/ >95%（L2）。

---

## 3. INT8 量化（真实导出代码）

`export_model.py` 管线（以 L2 GatedBiCrossAttention 为例）：

1. **ONNX**：`torch.onnx.export`，双输入 `left`/`right`（`(1,11)` 各），opset 13，batch 动态轴。
2. **TF SavedModel**：`onnx_tf.backend.prepare` 转换。
3. **TFLite INT8**：
   ```python
   converter.optimizations = [tf.lite.Optimize.DEFAULT]
   def representative_dataset():
       for _ in range(100):
           yield [np.random.randn(1,11).astype(np.float32),
                  np.random.randn(1,11).astype(np.float32)]   # ⚠️ 伪造校准数据
   converter.representative_dataset = representative_dataset
   converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
   ```
4. **C 头文件**：`model_data.h`（`alignas(16)` 字节数组 + `model_data_len`）。

### 3.1 量化数学

INT8 仿射量化（per-tensor 对称/非对称）：

$$
q = \text{round}\!\left(\frac{r}{s}\right) + z, \qquad
r \approx s\,(q - z), \qquad s = \frac{r_{\max} - r_{\min}}{255}
$$

**量化误差界**：

$$
|r - \hat{r}| \le \frac{s}{2}, \qquad
\text{SNR} \approx \frac{\sigma_r^2}{(s^2/12)} = \frac{12\,\sigma_r^2}{s^2}
$$

### 3.2 ⚠️ 校准问题（关键真实性缺口）

`representative_dataset` 是 **`np.random.randn` 高斯噪声**，不是真实手部特征分布：

$$
s_{\text{calib}} = \frac{\max|\text{randn}|}{127} \ll s_{\text{real}}
$$

→ 真实特征动态范围远超校准范围 → 量化**截断 + 分辨率失配**，INT8 精度无意义。**必须用真实采集数据（非 synthetic）做代表性校准集**——这是训练/量化路线图的🔬项。

### 3.3 FP32/INT8 一致性（已知问题）

V6 审计确认 **FP32/INT8 量化不一致**（属 B 级 bug 清单）：导出与板上 TFLite 使用的权重/校准来源需统一为真实 checkpoint 后重新验证 golden-frame。

---

## 4. 端到端验收标准（生产设计 §7）

| 阶段 | 内容 | 状态 |
|------|------|------|
| 数据采集 | Hand Token v1 帧落盘（serial+UDP+USB-CDC） | serial ✅ / UDP 🔬(B8) |
| 训练 | `train_tier1.py` 11 维 → `.pt` | 脚本 ✅ / 权重 🔬 |
| 校准量化 | 真实代表性集 → INT8 | 🔬 |
| 导出 | ONNX → TFLite → C 头 | ✅（随机权重） |
| 板载验证 | P4/S3 golden-frame | 🔬 |

---

## 5. 真实性总表

| 能力 | 标注 | 说明 |
|------|------|------|
| 训练脚本 / 导出脚本结构 | ✅ | 代码存在、形状正确 |
| ONNX→TFLite INT8 转换链 | ✅ | 可运行（随机权重） |
| 真实数据集 + checkpoint | 🔬 | 资产近空（synthetic 仅验形状） |
| 真实校准集量化精度 | 🔬 | `np.random.randn` 伪造校准无效 |
| FP32/INT8 golden-frame 一致 | 🔬 | 审计确认不一致，待真实权重后重验 |

---

*本文档导出管线为真实代码；所有精度相关条目标 🔬（无真实数据/权重）。*
