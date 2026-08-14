# 06 · L3 视觉侧 ST-GCN（时空图卷积，世界态）

> **Date**: 2026-08-11 ｜ **依据**: `glove_relay/src/models/stgcn_model.py` + `model_config.yaml`（`stgcn_v1` catalog）｜ **真实性**: 结构 ✅ / 权重 🔬 / 视觉集成 🌌

---

## 1. 角色：L3 视觉侧世界态分类

ST-GCN（Spatial-Temporal Graph Convolutional Network）在**骨架图**上建模手部关节时空动态。V7 架构中属**视觉侧（世界态）**模型：视觉摄像头观测提供世界坐标系关节轨迹，与手套（手态）互补（D1 视觉主导 + 可穿戴增强）。

**当前定位**（`model_config.yaml` catalog 注释原文）：**"ST-GCN is the visual-side model, NOT active in the V6 glove relay"**——保留 catalog 供参考，无 adapter，不可加载。

```
视觉世界态 ──► 21 关键点轨迹 (B,T,N,C) ──► L3 ST-GCN ──► (B, 46)
                                            12 节点 (Tier1/2) / 42 节点 (Tier3)
```

---

## 2. 图构建（真实代码）

两个拓扑：**12 节点**（单手 Tier1/2）与 **42 节点**（双手 Tier3）。

**12 节点**（`build_12node_edges`）：1 腕（0）+ 5 指尖（1–5）+ 1 掌中（6）+ 5 掌缘（7–11），星型 + 环形边：

```
  手背腕 0 ──► 指尖 1..5        掌中 6 ──► 掌缘 7..11
  1..5 ──► 6..10 (指尖→掌缘)     自环 0..6
```

**42 节点**（`build_42node_edges`）：每只手 21 节点（腕 0 + 5 指 × 4 关节链），双手对称。关键边：

- **指链**：每指 `prev → node` 4 级链（腕 → 指根 → … → 指尖）。
- **双手指尖桥**：左手 5 指尖 ↔ 右手 5 指尖（`left_tips=[4,8,12,16,20]` ↔ `right_tips=[25,29,33,37,41]`）——**双手协作手势的关键拓扑**。
- **腕腕桥**：`(0, 21)`。
- **全节点自环**（42 条）→ 归一化后保留自身特征。

```
 左手 (0..20)                     右手 (21..41)
  0 腕 ──────────────────────────── 21 腕  (腕腕桥)
  │┌┬┬┬┐                           │┌┬┬┬┐
  1 5 9 13 17 指尖                 22 26 30 34 38 指尖
  │ │ │ │ │  (每指 4 关节链)          │ │ │ │ │
  · · · · ·                         · · · · ·
 4 8 12 16 20 指尖                25 29 33 37 41 指尖
   │└┴┴┴┘                           │└┴┴┴┘
  └────────── 指尖桥 (5 条) ──────────┘
```

---

## 3. 图卷积（真实代码）

**邻接矩阵归一化**：建图 → 邻接 $A$（含自环）→ **行归一化**（row-normalized，非对称）：

$$
\tilde{A}_{ij} = \frac{A_{ij}}{\sum_k A_{ik}},\qquad
X' = \tilde{A} X,\qquad
Y = X' W + b
$$

> ⚠️ 设计注记：此处用**行归一化**（`deg = adj.sum(dim=1, keepdim=True).clamp(min=1)`），非对称；常见 ST-GCN 用对称归一化 $D^{-1/2}AD^{-1/2}$。行归一化对入度（聚合邻居）语义更直接，但非对称 → 有向传播。属既有实现事实，标 🟡 待评审。

**时空模块**（`STGCNModel`）：

```python
self.spatial  = nn.Sequential(GraphConv(N→H), ReLU, GraphConv(H→H), ReLU)   # (B,T,N,C) → (B,T,N,H)
self.temporal = nn.Sequential(Conv1d(H,H,k=3,pad=1), ReLU, Conv1d(H,H,k=3,pad=1), ReLU)  # (B·N, H, T)
self.classifier = nn.Sequential(Linear(H*N→128), ReLU, Linear(128→46))
```

**前向**：

$$
X \in \mathbb{R}^{B\times T\times N\times C} \xrightarrow{\text{spatial}} \mathbb{R}^{B\times T\times N\times H}
\xrightarrow{\text{permute}} \mathbb{R}^{(B\cdot N)\times H\times T}
\xrightarrow{\text{temporal (Conv1d)}} \mathbb{R}^{(B\cdot N)\times H}
\xrightarrow{\text{mean over }T} \mathbb{R}^{B\times N\times H}
\xrightarrow{\text{flatten}} \mathbb{R}^{B\times NH}
\xrightarrow{\text{cls}} \mathbb{R}^{B\times 46}
$$

---

## 4. 网络层架构图（张量维度）

```
输入骨架轨迹   (B, T, N, C)        N=12 或 42（节点）；C=in_features；T=时间窗（config num_frames=30）
        │
        ▼  spatial: GraphConv ×2（行归一化邻接 + Linear）＋ ReLU
      (B, T, N, H=64)
        │  permute (B,N,H,T) → reshape (B·N, H, T)
        ▼  temporal: Conv1d(H,H,k3,pad1) ×2 ＋ ReLU  （跨时间建模）
      (B·N, H, T) ──► mean over T ──► (B·N, H) ──► reshape (B, N·H)
        │
        ▼  classifier: Linear(N·H→128) → ReLU → Linear(128→46)
      (B, 46)  logits ──► softmax ──► (class, conf)

config (stgcn_v1): ~280K params, 3.80 MFLOPs, 8.5ms (i5)  [catalog metadata]
```

---

## 5. 数学要点

- **图卷积聚合**：$h_i' = \sigma\!\Big(W \sum_{j \in \mathcal{N}(i)} \tfrac{1}{\deg(i)} h_j + b\Big)$ —— 邻居平均 + 线性映射。
- **时间卷积**：$H^{(\ell+1)} = \sigma\!\big(W_t * H^{(\ell)} + b\big)$，核宽 3 捕获相邻帧变化。
- **时空组合**：先空间（帧内关节关系）后时间（帧间演化）——与经典 ST-GCN 两分支耦合（空间-时间注意力）相比更简单。

---

## 6. 现状与真实性

| 能力 | 标注 | 说明 |
|------|------|------|
| 12/42 节点图 + 图卷积结构 | ✅ | 真实代码（`stgcn_model.py`） |
| registry catalog 保留（不可加载） | ✅ | `model_config.yaml` 注释明确视觉侧 |
| 真实权重 / 视觉世界态接入 | 🔬 / 🌌 | V7 D1 视觉主导，第一代硬件不进 CV |
| 对称归一化评审 | 🟡 | 现行行归一化，标待评审项 |

---

*本文档图拓扑与卷积为真实代码；视觉世界态集成属 V7 roadmap。*
