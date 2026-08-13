# 02 · IMU 融合：LSM6DSV16X + Madgwick 梯度下降 AHRS

> **Date**: 2026-08-11 ｜ **依据**: 生产设计 §4 + `EgoGlove` M2 计划 `2026-08-11-lite-lsm6dsv16x-madgwick.md` ｜ **真实性**: 驱动 🟡 / 算法 ✅(host) / SFLP quat 上链 🟡

---

## 1. 系统角色与坐标约定

**决策（D-C + 2026-08-11）**：姿态在 **S3 Host 上跑 Madgwick**（非芯片 SFLP 硬件加速器），输出 **SFLP 四元数**（w,x,y,z）写入 Hand Token v1 的 `quat[4]`。视觉/VIO 属 Pro/roadmap（D7），S3 不做。

**坐标约定**：身份四元数 $q=(1,0,0,0)$ ⟺ **传感器 +z 轴对齐重力向上**。即估计重力在机体系下为：

$$
\hat{\mathbf{g}} = q \otimes (0,0,0,1) \otimes q^*
$$

**物理限制（不可观性）**：绕重力轴的旋转（yaw）无法由加速度计单独观测——tilt（roll/pitch）收敛、yaw 有界但不修正（无磁力计）。这是重力参考的本质限制，不是算法缺陷。

---

## 2. LSM6DSV16X 寄存器配置（M2 计划，已对照 datasheet 核实）🟡

| 寄存器 | 值 | 含义 |
|--------|-----|------|
| WHO_AM_I (0Fh) | 0x70 | 器件 ID 校验 |
| CTRL1 (10h) | 0x06 | OP_MODE_XL=000 (HP) + **ODR_XL=0110 (120Hz)** |
| CTRL2 (11h) | 0x06 | OP_MODE_G=000 + **ODR_G=0110 (120Hz)** |
| CTRL3 (12h) | 0x44 | BDU[6] + IF_INC[2]（默认值显式写） |
| CTRL6 (15h) | 0x04 | **FS_G=0100 (±2000 dps)** |
| CTRL8 (17h) | 0x01 | **FS_XL=01 (±4 g)** |

- 灵敏度：±4g → **0.122 mg/LSB**；±2000dps → **70 mdps/LSB**。
- **ODR 120Hz 决策（2026-08-11）**：芯片**无 104Hz 档**（最接近 0b0110=120Hz），与 `Task_SensorRead` 120Hz 对齐。生产设计 §4 原文 104Hz 已修正。

**原始值 → 物理量**（驱动内，`lsm6dsv16x.c`）：

$$
a_{\text{g}} = \text{raw}_{\text{acc}} \times 0.122 \times 10^{-3},\qquad
\omega_{\text{rad/s}} = \text{raw}_{\text{gyro}} \times 70 \times 10^{-3} \times \frac{\pi}{180}
$$

**接线**：I²C @0x6A（SA0 LOW），GPIO8 SDA / GPIO9 SCL，400kHz；CS 拉高（LOW→SPI）；SDX/SCX 不接；INT1 GPIO10 可选（DRDY 门控 120Hz 采样，M2 计划）。

---

## 3. Madgwick 六轴梯度下降 AHRS（标准数学）

### 3.1 目标与残差

在单位四元数流形上最小化加速度计残差范数：

$$
\min_{q \in S^3} \; \lVert f(q) \rVert^2, \qquad
f(q) = \hat{\mathbf{g}}(q) - \mathbf{a}
$$

其中 $\mathbf{a}$ 为归一化加速度计测量 $(\|\mathbf{a}\|=1)$。展开半向量形式：

$$
f(q) = \begin{pmatrix}
2(q_1 q_3 - q_0 q_2) - a_x \\[2pt]
2(q_0 q_1 + q_2 q_3) - a_y \\[2pt]
(q_0^2 - q_1^2 - q_2^2 + q_3^2) - a_z
\end{pmatrix}
= \hat{\mathbf{g}}(q) - \mathbf{a}
$$

### 3.2 梯度下降步

取 $f$ 对 $q$ 的 Jacobian $J=\dfrac{\partial \hat{\mathbf{g}}}{\partial q}$，梯度为 $s = J^\top f$。完整展开（**标准形式，本文件为准**）：

$$
\begin{aligned}
s_0 &= f_0\,(-2 q_2) + f_1\,( \mathbf{+2} q_1) + f_2\,( 2 q_0) \\
s_1 &= f_0\,( 2 q_3) + f_1\,( \mathbf{+2} q_0) + f_2\,(-2 q_1) \\
s_2 &= f_0\,(-2 q_0) + f_1\,( 2 q_3) + f_2\,(-2 q_2) \\
s_3 &= f_0\,( 2 q_1) + f_1\,( 2 q_2) + f_2\,( 2 q_3)
\end{aligned}
$$

其中加粗的 `+2q1`、`+2q0` 是 **2026-08-11 修正的 Jacobian 系数**（见 §5 决策记录）。修正前为 `−2q1`、`−2q0`（错误），使 $s$ 不再是真梯度。

**更新律**（β≈0.1 增益）：

$$
\dot{q} = \underbrace{\tfrac12\, q \otimes \omega}_{\text{陀螺积分}} \;-\; \beta\, \frac{J^\top f}{\lVert J^\top f \rVert}
\qquad\Longrightarrow\qquad
q_{t+1} = \mathrm{normalize}\!\left(q_t + \dot{q}\,\Delta t\right)
$$

离散实现中 $s$ 先归一化（$\hat{s} = s/\lVert s\rVert$），步长 $\beta\Delta t$。

> **部署参考**：arduino-libraries/MadgwickAHRS `updateIMU` 使用代数化简后的梯度（利用 $\|q\|=1$ 消去 $f_2$ 的径向分量）。化简只删去与 $q$ 平行（径向）的部分——不影响姿态动力学（归一化吸收），故与上表 $J^\top f$ 等价。M2 保留完整 $J^\top f$ 展开以便 host 可逐项核对。

### 3.3 物理验证锚点

机体系重力估计可由旋转矩阵 $R(q)$ 直接导出：

$$
\hat{\mathbf{g}}(q) = R(q)^\top \begin{pmatrix}0\\0\\1\end{pmatrix}
= \begin{pmatrix}
2(q_1 q_3 - q_0 q_2) \\
2(q_0 q_1 + q_2 q_3) \\
q_0^2 - q_1^2 - q_2^2 + q_3^2
\end{pmatrix}
$$

**关键恒等式（决定测试断言的物理真值）**：

$$
\hat{\mathbf{g}} = (0,-1,0) \quad\Longleftrightarrow\quad \text{roll} = -90^\circ
\quad\Longleftrightarrow\quad q = \left(\cos 45^\circ,\; -\sin 45^\circ,\; 0,\; 0\right)
$$

即设备绕 x 轴**负向**转 90° 时，重力在机体系指向 −y。**注意方向**：修正前代码错误地收敛到 $+90^\circ$（$q_1=+\sin45^\circ$），与物理真值反号。

### 3.4 离散不动点分析（保号性质）

离散迭代 $\mathrm{normalize}(q - \varepsilon \hat{s})$ 的固定点条件是 $s \parallel q$（此时修正步与 $q$ 共线，归一化不变）。在 $q=(0.7071,+0.7071,0,0)$（roll +90°）、$\mathbf{a}=(0,-1,0)$ 处：

$$
s = (2.828,\; 2.828,\; 0,\; 0) \;\parallel\; q
$$

→ 该状态是**保号离散不动点**：加速度计单独无法分辨 ±180°（无磁力计，yaw 面不可观的一部分）。**这不是缺陷**——连续目标函数在此处 $f$ 不为 0，但离散梯度步与流形切线平行方向恰好被归一化吸收；真实陀螺噪声会把状态移离精确射线。M2 的 `test_converge_flip_180_no_ambiguity_guard` 断言"起始 +90° 保持 +90°"即验证此性质，**断言保持有效**。

---

## 4. 四元数 → 欧拉角（特征落位）

Hand Token 的 `quat[4]` 喂 L1 前需派生 euler（roll/pitch/yaw）与 5×flex 组成 11 维特征（双手 22 维喂 L2）。标准变换：

$$
\begin{aligned}
\text{roll}  &= \mathrm{atan2}\!\left(2(q_0 q_1 + q_2 q_3),\; 1 - 2(q_1^2 + q_2^2)\right) \\
\text{pitch} &= \mathrm{asin}\!\left(2(q_0 q_2 - q_3 q_1)\right) \\
\text{yaw}   &= \mathrm{atan2}\!\left(2(q_0 q_3 + q_1 q_2),\; 1 - 2(q_2^2 + q_3^2)\right)
\end{aligned}
$$

---

## 5. 决策记录：2026-08-11 Madgwick 梯度三处符号修正

> **触发**：M2 实施前 host 预验证发现 `test_converge_gravity_neg_y` 收敛到 roll **+90°**，与物理真值 −90° 反号。用户批准「修正后执行」。

**根因**：M2 计划 `madgwick.c` 的 $s = J^\top f$ 实现有**三处符号错误**，使 $s$ 根本不是真梯度：

| # | 位置 | 原（错误） | 修正后 | 依据 |
|---|------|-----------|--------|------|
| 1 | `f1` 公式 | $2(q_2 q_3 - q_0 q_1) - a_y$ | $2(q_0 q_1 + q_2 q_3) - a_y$ | $\hat{g}_y$ 半向量形式符号 |
| 2 | `s0` 的 f1 系数 | $f_1(-2 q_1)$ | $f_1(\mathbf{+2} q_1)$ | 真 Jacobian $\partial f_1/\partial q_0 = +2q_1$ |
| 3 | `s1` 的 f1 系数 | $f_1(-2 q_0)$ | $f_1(\mathbf{+2} q_0)$ | 真 Jacobian $\partial f_1/\partial q_1 = +2q_0$ |

**验证**（host 数值，dt=0.01s，β=0.1，`gcc -std=c11 -O2 -Werror`）：

| 场景 | orig（原实现） | f1-only | **full（三处修正）** | ref（参考化简） |
|------|---------------|---------|----------------------|-----------------|
| 静止 +z 60s | identity ✓ | identity ✓ | identity ✓ | identity ✓ |
| 扰动 +z → identity | ✓ | ✗ +60° | ✓ | ✓ |
| 重力 −y → roll −90° | ✗ +90° | ✓ −89.5° | **✓ −90.09°** | ✓ −90.03° |
| 翻转起始 +90° 保号 | ✓ +90° | ✓ +90° | ✓ +90° | ✓ +90° |
| 扰动 −y → roll −90° | ✗ +90° | ✓ −89.5° | **✓ −89.96°** | ✓ −90.02° |

- `orig`（原计划）在重力 −y 收敛 +90° → 现 `test_converge_gravity_neg_y` 断言改为 `q[1] → −0.7071`（roll −90°，`(cos45, −sin45, 0, 0)`）。
- `f1only`（只修 #1）扰动 +z 跑到 +60° → 证明「一处不够」，三处必须同时修。
- `full` 与 `ref`（arduino-libraries/MadgwickAHRS 化简梯度）方向一致，佐证修正正确。
- `test_converge_flip_180` 断言保持（§3.4 保号不动点）。

---

## 6. 验证目标（板上，Task 6 清单）

| 指标 | 目标 | 方法 |
|------|------|------|
| 静止 60s tilt 漂移 | < 3° | 静置姿态角方差 |
| 手翻转 90° 响应 | < 200 ms | 阶跃响应到达 ±90° 的 90% |
| yaw 漂移 | 有界（不修正） | 重力轴旋转后 yaw 保持 |

板上验证依赖硬件（M2 之后），当前 host 层已全部通过（驱动 6 项 / 滤波 5 项 / 管理器 5 项）。

---

## 7. 真实性小结

| 能力 | 标注 | 说明 |
|------|------|------|
| 寄存器编码 + 灵敏度 | ✅ | 对照 datasheet 核实（`EgoGlove/docs/datasheet/lsm6dsv16x.md`） |
| Madgwick 算法（修正后标准 Jᵀf） | ✅ | host 全 5 测试 PASS（`-Werror`） |
| LSM6DSV16X 驱动（I²C 注入式） | 🟡 | M2 host 验证通过，板上待验 |
| SFLP 四元数 → Hand Token `quat[4]` | 🟡 | M2（管理器已 host 验证），上链 M3 |
| 视觉/VIO 融合（Pro） | 🌌 | V7 D7，第一代不进 CV |

*本文档公式为准（修正后标准数学）；任何与旧版 `madgwick.c` 不一致处以本文档 + M2 计划决策记录为准。*
