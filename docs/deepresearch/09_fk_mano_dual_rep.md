# 09 · 双表示层：FK → MANO / Robot Action（Hand Token 分叉）

> **Date**: 2026-08-11 ｜ **依据**: `EgoGlove/firmware/shared/hand_skeleton.{h,c}`（FK21 已实现 + host 验证）+ `hand_token.h`（v2 skeleton 契约）+ V7 D3 ｜ **真实性**: canonical-20 + FK21 ✅ / MANO mesh 层 🌌 / Robot Action 🌌

---

## 1. 双表示层（V7 D3）

同一硬件传感器流归一化为 **Hand Token**（统一中间表示），再分叉为两个下游表示：

```
                           ┌────────────────────────── Hand Token v1 (79B) ──┐
 传感器 (flex/IMU) ──► 特征 ──► 同一中间表示 ─────────────────────────────► 分叉
                           │      v1: quat[4] 手掌姿态                      │
                           │      v2: skeleton TLV (canonical-20 + FK)     │
                           └──────────────────────────────────────────────┘
                                        │
                    ┌───────────────────┴───────────────────┐
                    ▼                                       ▼
         MANO Layer（数字人侧）                   Robot Action Layer（机器人侧）
         FK21 → 21 点 / MANO 网格               关节角 → 遥操作/机械手指令
```

- **v1 角色**：只带 `quat[4]`（手掌/腕姿态）+ flex 关节角 → 双手 46 类 SLR 下游 + 简单骨架重建。
- **v2 演进**（D10/D11）：加 canonical-20 旋转关节 + FK 派生 21 点 + capability-flagged TLV → 支撑 MANO/OpenXR/Robot 精确重建。

---

## 2. Canonical-20 骨架（真实代码）✅

`hand_skeleton_t`（v2）：20 个关节四元数 + 25 个偏移向量（20 关节 + 5 指尖偏移）+ rest model id + revision。

$$
\text{skeleton} = \big\{ \underbrace{q_{0..19}}_{\text{关节局部旋转}},\, \underbrace{o_{0..24}}_{\text{骨长偏移}},\, \underbrace{\text{model\_id},\, \text{revision}}_{\text{rest 模型对齐}} \big\}
$$

**父子链**（`hand_skeleton_parent[20]`，根腕 0 = −1）：

```
   0 腕 (root)
   ├─ 1 → 2 → 3           拇指（3 关节）
   ├─ 4 → 5 → 6 → 7       食指
   ├─ 8 → 9 → 10 → 11     中指
   ├─ 12 → 13 → 14 → 15   无名指
   └─ 16 → 17 → 18 → 19   小指
```

**rest 模型对齐**（`model_id`）：

| id | 名称 | 用途 |
|----|------|------|
| 0 | `CANONICAL_HUMAN` | 人类手 canonical 中性姿态 |
| 1 | `MANO_ALIGNED` | 对齐 MANO 网格 rest pose（数字人） |
| 2 | `OPENXR_ALIGNED` | 对齐 OpenXR 手部跟踪（XR） |

---

## 3. 前向运动学 FK21（真实代码）✅

**核心递推**（`hand_skeleton_fk21`）：

$$
q^{\text{global}}_j = q^{\text{global}}_{\text{parent}(j)} \otimes q^{\text{local}}_j
$$

$$
p_j = p_{\text{parent}(j)} + R\!\big(q^{\text{global}}_{\text{parent}(j)}\big)\, o_j, \qquad p_{\text{root}} = 0 \;\;(\text{约束 } o_0 = 0)
$$

其中 $R(q)$ 用**双叉积旋转**（`quat_rotate`）：

$$
R(q)\,v = v + 2\,q_0\,(u \times v) + 2\,u \times (u \times v), \qquad u = (q_1,q_2,q_3)
$$

（比通用旋转矩阵省 3×3 矩阵乘法，适合嵌入式。）

**输出映射 MediaPipe 21 点**：
- 关节点：`canonical_for_mediapipe[i]` 将 20 关节映射到 21 landmark 中的 16 个。
- **指尖**（landmark 4/8/12/16/20）：由末节关节（distal）全局姿态旋转指尖偏移向量 $o_{20+f}$ 得到：

$$
p_{\text{tip},f} = p_{\text{distal},f} + R\!\big(q^{\text{global}}_{\text{distal},f}\big)\, o_{20+f}, \qquad f = 0..4
$$

**校验**：`model_id`/`revision`/`offsets[0]` 合法性 + 四元数归一化（`normalize_quat` 防 NaN）——FK 无奇异。

---

## 4. 网络层/数据流架构图（张量维度）

```
 Hand Token v2 skeleton TLV (TLV_SKELETON_QUAT20)
        │
        ▼  parse → hand_skeleton_t
      q[20][4]  +  o[25][3]  +  model_id / revision
        │
        ▼  hand_skeleton_fk21 (parent 链递推, quat_mul + quat_rotate)
      global_q[20][4]  global_p[20][3]
        │  canonical_for_mediapipe 映射 + 指尖外推
        ▼
      landmarks[21][3]   (MediaPipe 兼容 21 点)
        │
        ├──► MANO Layer     (mesh 蒙皮: θ → MANO PCA → 网格)  🌌
        ├──► OpenXR Layer   (XR 手部跟踪)                      🌌
        └──► Robot Action   (关节角 → 遥操作指令)               🌌
```

---

## 5. 与 v1 的关系

| 层 | v1 (79B) | v2 (skeleton) |
|----|----------|---------------|
| 手掌/腕姿态 | `quat[4]` (f16) ✅ | + 可选 `QUAT_WLAST` |
| 关节级 | 仅 flex[5]（归一化角）| canonical-20 旋转四元数 |
| 21 点重建 | 需下游 FK（骨架简化）| FK21 直接产出 |
| 表示精度 | 手掌级 | 关节级 |
| 兼容性 | 永久兼容 | v1 API 不变（`parse_compatible`） |

---

## 6. 现状与真实性

| 能力 | 标注 | 说明 |
|------|------|------|
| canonical-20 结构 + parent 链 | ✅ | `hand_skeleton.h/.c`（host 已验证） |
| FK21（quat 链递推 + 指尖外推 + 输入校验） | ✅ | `hand_skeleton_fk21` 返回 `HAND_SKELETON_OK` |
| rest 模型对齐（CANONICAL/MANO/OPENXR） | ✅ 契约 | `model_id` 枚举 |
| MANO 网格蒙皮 / PCA 姿态空间 | 🌌 | 不在仓库；属数字人侧（V7 D3） |
| Robot Action 层（遥操作指令） | 🌌 | V7 D3，机器人侧 |
| v2 上链（405B skeleton 帧） | 🌌 | v2 协议 🌌；v1 永久兼容 |

---

*本文档 canonical-20 结构、FK 递推与指尖外推为真实代码（`hand_skeleton.c`，host 验证）；MANO 蒙皮 / Robot Action 层属 V7 roadmap 🌌。*
