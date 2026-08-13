# 03 · Hand Token 协议：v1 (79B) 线格式与双表示契约

> **Date**: 2026-08-11 ｜ **依据**: `EgoGlove/firmware/shared/hand_token.h`（已实现 + host 单测）+ 生产设计 §3 ｜ **真实性**: codec ✅ / 上链 🟡 / v2 🌌

---

## 1. 设计动机：统一中间表示（V7 D3）

Hand Token 是**双表示层**的统一中间表示：同一硬件传感器流归一化为 Hand Token，再分叉为 **MANO Layer**（数字人侧）与 **Robot Action Layer**（机器人侧）。同一份协议横切 Lite/Pro 两条产品线（`hand_token.h` 头部注释）。

核心设计属性：
1. **传输无关**：UART / ESP-NOW / BLE / USB-CDC / WiFi-UDP 均可承载。
2. **自描述**：magic + version + device_id 使接收端可识别帧类型。
3. **永久兼容**：v1 固定 79B 永不改动；v2 通过 version 字段 + TLV 扩展（`HAND_TOKEN_VERSION_V2`）。
4. **f16 压缩**：多数浮点字段用 IEEE754 half，腕位姿用 f32 保精度。

```
  传感器流 (Lite/Pro)                     下游消费 (D3 分叉)
  ┌────────────┐   Hand Token v1 (79B)  ┌─────────────────────────┐
  │ flex/IMU/  │ ─────────────────────► │ MANO Layer  (数字人 21 点) │
  │ wrist/...  │                         │ Robot Action (遥操作)      │
  └────────────┘                         │ 46 类 SLR    (下游消费头)    │
        ▲                                └─────────────────────────┘
        └── 厂商手套 = 外部数据源/适配器（D12：Hand Token 是通用中间表示）
```

---

## 2. v1 Canonical 布局（79B 固定帧）✅

偏移常量来自 `hand_token.h` 枚举（`HAND_TOKEN_OFF_*`），小端序：

| 偏移 | 长度 | 字段 | 类型 | Lite 填充 | 语义 |
|------|------|------|------|-----------|------|
| 0 | 2 | magic `"HT"` | u8 | `0x48 0x54` | 区别于 uart_frame 的 `0xAA55` |
| 2 | 1 | version | u8 | `0x01` | v1 永久兼容 |
| 3 | 1 | device_id | u8 | product=Lite | bit7 product / bit6 hand / bits0-5 serial |
| 4 | 4 | timestamp_us | u32 | 单调时钟 | ~71min 回绕，relay 处理 |
| 8 | 10 | flex[5] | f16×5 | 实际值 | 指间关节角，归一化 0..1，拇→小 |
| 18 | 8 | quat[4] | f16×4 | **LSM6DSV16X+Madgwick** | 手掌姿态 (w,x,y,z)，SFLP |
| 26 | 24 | wrist_6dof[6] | f32×6 | 0 | 腕世界/基座位姿 x,y,z,r,p,y |
| 50 | 6 | vel[3] | f16×3 | 0 | 线速度 |
| 56 | 6 | acc[3] | f16×3 | 0 | 线加速度（IMU） |
| 62 | 5 | contact[5] | u8×5 | 0 | 指尖接触布尔 |
| 67 | 10 | force[5] | f16×5 | 0 | 指尖力估计 N |
| 77 | 2 | CRC-16 | u16 LE | — | poly 0xA001, init 0xFFFF |

**合计 79 字节**（= `HAND_TOKEN_FRAME_SIZE`）。

### 2.1 字节级 ASCII 布局

```
0          2  3  4              8              18             26                      50     56     62     67     77  79
├──┬──┬──┬──┬──────────────────┬──────────────┬─────────────────────────────────┬──────┬──────┬──────┬──────┬──────┤
│HT│01│id│timestamp_us (u32)  │flex[5] × f16 │quat[4] × f16 │wrist_6dof[6] × f32 │vel[3]│acc[3]│ct[5] │force[5]│ CRC │
│  │  │  │                     │  0..1        │ (w,x,y,z)     │   x,y,z,r,p,y     │f16×3 │f16×3 │ u8×5 │ f16×5  │ u16 │
└──┴──┴──┴──┴──────────────────┴──────────────┴─────────────────────────────────┴──────┴──────┴──────┴──────┴──────┘
 │  │  └dev_id                │               └── f16 压缩 (Lite 精度足够)          │
 │  └─version                 └── f16 压缩     └── f32 保精度 (腕位姿需外部位姿源)     └── CRC-16/MODBUS
 └─magic "HT"
```

### 2.2 device_id 位域

$$
\underbrace{\text{bit7}}_{\text{product}} \quad
\underbrace{\text{bit6}}_{\text{hand}} \quad
\underbrace{\text{bits0-5}}_{\text{serial (0..63)}}
\qquad \text{Lite}=0,\ \text{Pro}=1;\ \text{Left}=0,\ \text{Right}=1
$$

编解码 API：`hand_token_make_device_id(product, hand, serial)` / `hand_token_split_device_id`。

---

## 3. 浮点压缩：f16（IEEE754 half）✅

多数字段用 half 省带宽；腕位姿用 f32 保精度。转换 API 供交叉实现核对：

$$
\text{val} = \begin{cases}
(-1)^s \cdot 2^{e-15} \cdot (1.m), & e \neq 0 \\[2pt]
(-1)^s \cdot 2^{-14} \cdot (0.m), & e = 0
\end{cases}
\qquad s \in \{0,1\},\ e \in [0,31],\ m \in [0, 2^{10})
$$

- 10 位尾数 → 相对精度 $\approx 2^{-11} \approx 4.9\times10^{-4}$。对归一化 0..1 的 flex 与单位四元数足够。
- 上限 $\pm 65504$；下溢到 subnormal 最小 $2^{-24}$。

---

## 4. CRC-16/MODBUS ✅

与 `shared/uart_frame.h` 同算法（帧尾小端 L,H）：

$$
\text{poly} = 0xA001,\qquad \text{init} = 0xFFFF,\qquad \text{reflected}
$$

覆盖范围：**magic → force 全部 77B**（不含 CRC 自身）。接收端 `hand_token_parse` 校验 magic/version/长度/CRC，任一不符返回 false。

---

## 5. 序列化 / 解析契约 ✅

```c
size_t hand_token_serialize(const hand_token_t *t, uint8_t *buf, size_t buflen); // → 79
bool   hand_token_parse   (const uint8_t *buf, size_t n, hand_token_t *out);     // → true/false
```

- 内存态 `hand_token_t` 用 f32 便于上层；序列化时按 canonical 布局压缩。
- Lite 未采集字段由**生产端置 0**（协议本身不区分产品线语义）。
- 真实性：codec + C/Python wire mirror + golden-frame host 单测 ✅（`firmware/shared/test/`）。

---

## 6. v2 扩展（TLV / capability）🌌

v2 保持 v1 API 不变（永久兼容），通过 version 字段 + capability 位 + TLV 扩展：

| 常量 | 值 | 含义 |
|------|----|------|
| `V2_LITE_FRAME_SIZE` | 82B | v2 Lite（+caps/total_len） |
| `V2_SKELETON_FRAME_SIZE` | 405B | v2 canonical-20 骨架层 |
| `V2_MAX_FRAME_SIZE` | 1024B | TLV 上限 |
| `CAP_HAS_SKELETON / FORCE / VEL_ACC / GLOBAL_WRIST / QUAT_WLAST / HANDEDNESS_AXIS / SKEL_SMALLEST3` | bit0–6 | capability 标志 |
| `TLV_SKELETON_QUAT20 / REST_OFFSETS / REST_MODEL_ID` | 0x01/0x02/0x08 | TLV 类型 |

**v2 演进（D10/D11）**：canonical-20 旋转关节、FK 派生 21 点、capability-flagged TLV。**v1 永久兼容（本次）**；v2 作为后续演进，不阻塞 M2/M3。

---

## 7. 现状与上链缺口

| 能力 | 标注 | 说明 |
|------|------|------|
| v1 codec（serialize/parse/CRC/f16/device_id） | ✅ | host 单测验证（C + Python mirror） |
| relay 侧 Python mirror + `openxr_adapter` | ✅ | `EgoGlove/relay/hand_token.py` + `openxr_adapter.py` |
| S3 Lite 发射 79B（替代 69B GlovePacket） | 🟡 | M3 |
| relay 解析 79B（替代 protobuf） | 🟡 | M3 |
| v2 skeleton 层（canonical-20/FK-21） | 🌌 | V7 roadmap |

---

## 8. 传输承载对照

| 承载 | 载荷上限 | 79B 兼容 | 现状 |
|------|---------|----------|------|
| ESP-NOW | 250B | ✅ 无需改链路 | S3 当前 69B；79B 属 M3 |
| UART 2Mbps | 73B 帧 | 需换帧格式（73B≠79B+CRC） | P4 链路已通（69B payload） |
| BLE / USB-CDC / WiFi-UDP | — | ✅ | 🟡 后续 |

> 注意：UART 路径当前用 `[0xAA 0x55 69B CRC16]`=73B 帧；换 79B Hand Token 后需同步改 P4 接收帧长——属 M3 实施项。

---

*真实性：codec ✅；C/Python mirror ✅；S3/relay 79B 上链 🟡（M3）；v2 skeleton 🌌。*
