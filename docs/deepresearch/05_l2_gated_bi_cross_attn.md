# 05 · L2 双手门控双向交叉注意力（GatedBiCrossAttention，22→46 类）

> **Date**: 2026-08-11 ｜ **依据**: `glove_relay/src/models/tier2_cross_attn.py` + `model_config.yaml`（`gated_cross_attn_v1`）｜ **真实性**: 结构 ✅ / 权重 🔬 / INT8 导出 🔬

---

## 1. 角色：L2 基站双手融合分类

L2 消费**双手拼接特征** `(batch, 22) = [left(11) ‖ right(11)]`（或 `(batch, T, 22)` 时间窗口），用**双向交叉注意力**建模左右手关系，输出 46 类。作为 L1 低置信度时的 fallback（`hot_switch.auto_fallback`）。

```
 left(11) ──► embed ──► ┐
                        ├──► GatedBiCrossAttention ──► (B, 46) ──► 46 类
 right(11) ──► embed ──► ┘    双手关系建模
```

---

## 2. 网络结构（真实代码）

```python
self.embed       = nn.Linear(11, d_model)              # (B,11) → (B,32)
self.cross_attn  = nn.MultiheadAttention(d_model, num_heads=4, batch_first=True)
self.gate_proj   = nn.Linear(d_model*2, d_model)       # 门控投影
self.classifier  = nn.Sequential(
    nn.Linear(d_model*2, d_model), nn.ReLU(),          # 拼接后分类
    nn.Linear(d_model, num_classes),
)
```

**前向**（`forward(left, right)`）：

$$
e_l = \text{Embed}(l) \in \mathbb{R}^{B\times D},\quad e_r = \text{Embed}(r) \in \mathbb{R}^{B\times D},\quad D=32
$$

$$
\text{attn}_l = \text{MHA}(Q=e_l,\ K=e_r,\ V=e_r),\qquad
\text{attn}_r = \text{MHA}(Q=e_r,\ K=e_l,\ V=e_l)
$$

**门控融合**（残差式，每手独立门）：

$$
g_l = \sigma\!\Big(W_g\,[e_l \,\|\, \text{attn}_l]\Big) \in \mathbb{R}^{B\times D}
\qquad
\text{fused}_l = g_l \odot \text{attn}_l + (1-g_l) \odot e_l
$$

**分类头**：$\hat{y} = W_2\, \text{ReLU}\big(W_1\,[\text{fused}_l \,\|\, \text{fused}_r]\big) \in \mathbb{R}^{B\times 46}$。

**时间窗口**：3D 输入 `(B, T, 22)` 先按时间均值 `(B,22)`（与 L1 同策略），再进注意力。

---

## 3. 网络层架构图（张量维度）

```
 left (B,11) ───────────────────── right (B,11)
      │  Embed(11→32)                    │  Embed(11→32)
      ▼                                  ▼
   e_l (B,32) ─┐                     e_r (B,32) ─┐
   unsqueeze(1)│ (B,1,32)         unsqueeze(1)  │ (B,1,32)
               ▼                                ▼
   ┌─────────────────────────────────────────────────────┐
   │  MultiheadAttention(D=32, heads=4, batch_first)     │
   │                                                      │
   │  attn_l = MHA(Q=e_l, K=e_r, V=e_r)  (left queries    │
   │         ← right keys/values)                         │
   │  attn_r = MHA(Q=e_r, K=e_l, V=e_l)  (right queries   │
   │         ← left keys/values)                          │
   └─────────────────────────────────────────────────────┘
      │ (B,1,32) squeeze ─┐        │ (B,1,32) squeeze ─┐
      ▼                    │        ▼                    │
   attn_l (B,32)           │     attn_r (B,32)           │
      │                    │        │                    │
      ├─ cat[e_l‖attn_l]─► gate_proj(64→32) ─► σ ─► g_l  │
      │  fused_l = g_l⊙attn_l + (1-g_l)⊙e_l              │
      ├──────────────────────────────────────────────────┤
      │  cat[fused_l ‖ fused_r] (B,64)                    │
      ▼                                                  ▼
   Linear(64→32) ─► ReLU ─► Linear(32→46)          (B,46) logits
                                              ──► softmax ──► (class, conf)

参数统计：11·32·2 + 4·32·(32·32·4投影)·2 + 64·32 + 64·32 + 32·46 ≈ 40K
```

---

## 4. 数学要点

- **双向性**：左右手互为 Q 与 K/V，不对称地捕捉「谁跟谁关联」——双手手语/交互手势（如双手配合）的关键模式。
- **门控（Gating）**：$g \in [0,1]$ 控制「注意力输出 vs 自身嵌入」的软切换——抑制弱关联、保留强信号，缓解交叉注意力在小数据上的过拟合。
- **尺度问题**：`MultiheadAttention` 自带 $\text{softmax}(QK^\top/\sqrt{d_k})$；$d_k = D/\text{heads} = 8$。

---

## 5. 部署与导出

- **Registry**：`active_l2_model: gated_cross_attn_v1`（B3 修复），`CrossAttnAdapter` 包装（`adapters.py`）统一 `BaseModel` 契约；输入 `(batch, 22)`，内部按 `input_dim=11` 拆分左右手。
- **INT8 导出**：`glove_firmware/scripts/export_model.py` 走 PyTorch→ONNX（opset 13，双输入 `left`/`right`）→ TF SavedModel → **TFLite INT8** → C 头文件（`model_data.h`）。⚠️ 校准集为 `np.random.randn` 伪造数据——**量化精度无意义**，需真实数据校准（`07`）。
- **现状**：权重随机 🔬；P4 板上 Tier2 部署属生产路径。

---

## 6. 真实性

| 能力 | 标注 | 说明 |
|------|------|------|
| GatedBiCrossAttention 结构 | ✅ | 真实代码 + adapter 包好（B1）+ registry/warmup/L2 predict 验证 |
| 22 维双手契约 / 46 类 | ✅ | `model_config.yaml` + `adapters.py`（B7 `udp_server._run_l2` 双手窗口） |
| 真实权重 + 精度 | 🔬 | 无 checkpoint（B2） |
| INT8 板载部署 | 🔬 | 导出管线 ✅（随机权重），校准需真实数据 |

---

*本文档网络结构为真实代码；门控融合公式与 `tier2_cross_attn.py` 一致。*
