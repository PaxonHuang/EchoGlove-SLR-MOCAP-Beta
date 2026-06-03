# EchoGlove V5.0 — 模型训练与部署文档

> **版本**: V5.0 | **日期**: 2026-06-03
>
> **V3 用户注意**: V5 使用柔性传感器（非霍尔），特征维度从 21 变为 11/28。V3 版本见 `docs/archive/v3/`。

---

## Notebooks

| #  | 文件                                           | 内容                                                                                 | 大小   |
| -- | ---------------------------------------------- | ------------------------------------------------------------------------------------ | ------ |
| 01 | `01_data_collection_signal_processing.ipynb` | 数据采集、卡尔曼滤波、Min-Max 归一化、滑动窗口、合成数据生成、CSV 导出              | ~44 KB |
| 02 | `02_l1_model_training.ipynb`                 | Tier1 CNN+SE 模型架构、训练、QAT 量化、INT8 导出、C 头文件生成                      | ~45 KB |
| 03 | `03_l2_stgcn_deployment.ipynb`               | Tier2 CrossAttn + Tier3 ST-GCN、图卷积、伪骨架投影、置信度路由、Relay 部署集成       | ~40 KB |

## 数据流

```
01_data_collection → 02_l1_training → 03_l2_stgcn → ESP32 部署 + Relay 部署
```

## V5 核心公式索引

| 公式               | 位置     | 说明                                           |
| ------------------ | -------- | ---------------------------------------------- |
| 卡尔曼滤波         | NB01 §3 | $K = P/(P+R)$, $x = x + K(z-x)$            |
| Min-Max 归一化     | NB01 §4 | $\hat{x} = (x - min)/(max - min)$            |
| 1D-CNN + SE Block  | NB02 §2 | Conv1d → BN → ReLU → SE → GAP → FC     |
| 膨胀卷积感受野     | NB02 §3 | $RF = 1 + (k-1) \times d$                    |
| 图卷积             | NB03 §2 | $y = \hat{A} x W + b$                        |
| 时空图卷积         | NB03 §3 | Spatial GCN → Temporal Conv + Residual        |
| 伪骨架投影         | NB03 §4 | Linear(11 → 24) + LayerNorm                   |
| 置信度路由         | NB03 §6 | $p > 0.85$ → Tier1, else → Tier2/Tier3     |

## 运行环境

```bash
# 需要的 Python 包
pip install numpy matplotlib torch

# 运行
cd docs/notebooks
jupyter notebook
```

## V5 项目常量

| 常量                     | 值    | 说明                                    |
| ------------------------ | ----- | --------------------------------------- |
| `NUM_FLEX_SENSORS`       | 5     | 每手柔性传感器数                        |
| `SINGLE_HAND_FEATURES`   | 11    | 单手特征 (5 Flex + 3 Euler + 3 Gyro)   |
| `DUAL_HAND_FEATURES`     | 28    | 双手特征 (L11 + R11 + Relative6)       |
| `WINDOW_SIZE`            | 30    | 滑动窗口帧数 (300ms @ 100Hz)           |
| `NUM_CLASSES`            | 46    | 手势类别数 (MVP)                        |
| `SENSOR_RATE_HZ`         | 100   | 采样率                                  |
| Kalman Q                 | 0.001 | 过程噪声                                |
| Kalman R                 | 0.01  | 测量噪声                                |
| L1 threshold             | 0.85  | 置信度路由阈值                          |

## V5 数据格式 (CSV)

```csv
tick_id,timestamp,hand_id,flex_0,flex_1,flex_2,flex_3,flex_4,euler_0,euler_1,euler_2,gyro_0,gyro_1,gyro_2,l1_gesture_id,l1_confidence,status
```

| 列 | 类型 | 说明 |
|----|------|------|
| tick_id | uint32 | 帧序号 |
| timestamp | uint64 | Unix 时间戳 (ms) |
| hand_id | 0/1 | 0=左手, 1=右手 |
| flex_0..4 | float | 柔性传感器值 (0.0~1.0, 已校准) |
| euler_0..2 | float | 欧拉角 (度) |
| gyro_0..2 | float | 陀螺仪 (度/秒) |
| l1_gesture_id | int | 手势 ID (0~45) |
| l1_confidence | float | 置信度 (0.0~1.0) |
| status | str | 状态字符串 |

## V3 → V5 迁移说明

| 项目 | V3 | V5 |
|------|----|----|
| 传感器 | TMAG5273 霍尔 × 15 | SpectraFlex 柔性 × 5 |
| 特征维度 | 21 (15 hall + 3 euler + 3 gyro) | 11 (5 flex + 3 euler + 3 gyro) |
| CSV 列名 | hall_x0, hall_y0, ... | flex_0, flex_1, ... |
| 通信 | BLE + UDP | ESP-NOW |
| 手套数量 | 单手 | 双手 |

> **注意**: V3 CSV 数据与 V5 不兼容，需要重新采集。
