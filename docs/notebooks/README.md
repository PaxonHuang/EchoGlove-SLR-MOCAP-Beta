# EchoGlove 模型训练与部署文档

Phase 3-5 的完整模型数据算法文档，以 Jupyter Notebook 格式提供，分块可运行。

## Notebooks

| # | 文件 | 内容 | 大小 |
|---|------|------|------|
| 01 | `01_data_collection_signal_processing.ipynb` | 数据采集、卡尔曼滤波、归一化、滑动窗口、合成数据生成 | ~44 KB |
| 02 | `02_l1_model_training.ipynb` | L1 CNN+Attention、MS-TCN 模型架构、训练、QAT 导出 | ~45 KB |
| 03 | `03_l2_stgcn_deployment.ipynb` | L2 ST-GCN 图卷积模型、置信度路由、Relay 部署、延迟分析 | ~40 KB |

## 数据流

```
01_data_collection → 02_l1_training → 03_l2_stgcn → ESP32部署 + Relay部署
```

## 核心公式索引

| 公式 | 位置 | 说明 |
|------|------|------|
| 卡尔曼滤波 | NB01 §3 | $K = P/(P+R)$, $x = x + K(z-x)$ |
| Min-Max 归一化 | NB01 §4 | $\hat{x} = (x - min)/(max - min)$ |
| 1D-CNN + Attention | NB02 §2 | Conv1d → BN → ReLU → GAP → Attention → FC |
| 膨胀卷积感受野 | NB02 §3 | $RF = 1 + (k-1) \times d$ |
| 图卷积 | NB03 §2 | $y = \hat{A} x W + b$ |
| 时空图卷积 | NB03 §3 | Spatial GCN → Temporal Conv + Residual |
| 伪骨架投影 | NB03 §4 | Linear(21 → 42) + LayerNorm |
| 置信度路由 | NB03 §6 | $p > 0.85$ → L1, else → L2 |

## 运行环境

```bash
# 需要的 Python 包
pip install numpy matplotlib torch

# 运行
cd docs/notebooks
jupyter notebook
```

## 项目常量

| 常量 | 值 | 说明 |
|------|-----|------|
| `FEATURE_COUNT` | 21 | 每帧特征 (15 Hall + 3 Euler + 3 Gyro) |
| `WINDOW_SIZE` | 30 | 滑动窗口帧数 (300ms @ 100Hz) |
| `NUM_CLASSES` | 46 | 手势类别数 (CSL) |
| `SENSOR_RATE_HZ` | 100 | 采样率 |
| Kalman Q | 0.001 | 过程噪声 |
| Kalman R | 0.01 | 测量噪声 |
| L1 threshold | 0.85 | 置信度路由阈值 |
