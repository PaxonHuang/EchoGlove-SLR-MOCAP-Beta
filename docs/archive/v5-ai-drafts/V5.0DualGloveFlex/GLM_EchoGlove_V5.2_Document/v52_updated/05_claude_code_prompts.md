# EchoGlove V5.2 — Claude Code 逐步提示词

> **文档版本**: v5.2-final
> **生成日期**: 2026-06-08
> **适用对象**: Claude Code / Cursor / 其他 AI 编码助手
> **前置条件**: 已阅读 04_SOP-SPEC-PLAN_V5.2.md 全部内容

---

## Phase A: ESP32-P4 基站基础搭建

### A1: 双芯片工程搭建

```
你正在为 EchoGlove V5.2 双手数据手套系统搭建 ESP32-P4 基站。
硬件：ESP32-P4-Function-EV-Board v1.5.2 (含 ESP32-C6-MINI-1 协处理器)

请创建以下 ESP-IDF 工程：

1. `/p4_base_station/` — P4 主芯片工程
   - CMakeLists.txt: idf_component_register, target esp32p4
   - sdkconfig: 启用 MIPI-DSI, LVGL, UART, PSRAM
   - main/main.c: UART 接收 C6 数据 + LVGL 初始化

2. `/c6_relay/` — C6 协处理器工程
   - CMakeLists.txt: target esp32c6
   - sdkconfig: 启用 ESP-NOW, UART, BLE
   - main/main.c: ESP-NOW 接收 + UART 转发

3. 两工程共享：
   - `/shared/glove_protocol.h` — 数据包格式定义 (magic + hand_id + 11-dim features + timestamp + checksum)
   - `/shared/protobuf/` — Protobuf V5.2 定义文件

C6→P4 UART 参数: 波特率 2Mbps, TX=GPIO43, RX=GPIO44, 8N1
ESP-NOW 通道: 1, 左右手套 MAC 预置
```

### A2: LVGL 7 寸屏 UI

```
基于 Phase A1 的 P4 工程，添加 LVGL 7寸 MIPI-DSI (1024x600) 触摸屏 UI：

显示内容：
1. 顶部状态栏：C6连接状态 | 双手套电量 | Tier 级别 | FPS
2. 左半区：左手手势名称 + 置信度条 + 5指Flex弯曲度柱状图
3. 右半区：右手手势名称 + 置信度条 + 5指Flex弯曲度柱状图
4. 中央区：UWB 双手距离数值 + 距离可视化条
5. 底部：最近10条识别结果滚动日志

使用 LVGL v8.3+，MIPI-DSI 驱动由 ESP-IDF esp_lcd 组件提供。
刷新率目标：30fps。
数据更新频率：10Hz (来自 UART 解析器)。
```

### A3: 端到端集成测试

```
编写集成测试脚本 (Python)，验证完整数据通路：

1. 模拟手套端：通过 ESP-NOW 发送 11-dim 测试数据
2. 验证 C6 接收：C6 UART 输出与输入一致
3. 验证 P4 解析：Protobuf 解码正确，无丢帧
4. 验证屏幕显示：LVGL UI 更新率 >10Hz
5. 延迟测试：端到端延迟 <10ms (手套→C6→P4→屏幕)

测试工具：ESP-IDF unity 测试框架 + Python pytest
```

---

## Phase B: Tier2 模型迁移至 P4

### B1: 模型训练与导出

```
训练 EchoGlove V5.2 Tier2 模型 (Gated Bi-CrossAttn + MS-TCN 2-stage)：

输入：29-dim 特征向量 = left(11) + right(11) + relative(7)
     relative = delta_pos(3) + uwb_dist(1) + delta_orient(2) + delta_angular_vel(1)
输出：46 类手势分类 + 置信度

训练参数：
- 优化器: AdamW, lr=1e-3, weight_decay=1e-4
- 调度器: CosineAnnealingWarmRestarts
- Batch size: 128
- 序列长度: T=30 帧 (300ms@100Hz)
- 数据增强: 随机时间偏移 ±5 帧, 高斯噪声 σ=0.01

导出：
1. PyTorch → ONNX → ESP-DL 格式
2. 混合量化: gate 层 FP16, 其余 INT8
3. 目标模型大小: ~120KB

请生成完整训练脚本 train_v52.py，包含：
- Dataset 类 (加载 29-dim npy 数据)
- GatedBiCrossAttnV52 模型定义
- 训练循环 + 验证
- ONNX 导出 + 量化
- ESP-DL 格式转换脚本
```

### B2: ESP-DL P4 部署

```
将训练好的 Tier2 模型部署到 ESP32-P4：

1. 模型文件: tier2_v52.bin (存储在 PSRAM)
2. 推理 API:
   - p4_inference_init(): 加载模型到 PSRAM
   - p4_inference_run(float input[29], float output[46]): 推理
   - p4_inference_get_latency(): 获取最近推理延迟

3. 推理任务 (FreeRTOS):
   - 优先级: tskIDLE_PRIORITY + 5
   - 核心: Core 1 (独占)
   - 输入: 来自 UART 解析器的 29-dim 特征
   - 输出: 手势 ID + 置信度 → LVGL UI + USB→PC

4. 性能基准:
   - 目标: 推理延迟 <40ms
   - 内存: 模型 ~120KB, Arena ~32KB
   - CPU 占用: <80% (单核)

请生成完整的 p4_inference.h 和 p4_inference.cpp
```

---

## Phase C: UWB DW3000 集成

### C1: DW3000 SPI 驱动

```
为 ESP32-S3 实现 DW3000 (DWM3000) UWB 模块的 SPI 驱动：

引脚分配：
- MOSI: GPIO10
- MISO: GPIO11
- SCLK: GPIO12
- CS:   GPIO13
- IRQ:  GPIO14

SPI 参数：
- 模式: SPI Mode 0 (CPOL=0, CPHA=0)
- 时钟: 8MHz (DW3000 最高支持 20MHz)
- CS 空闲高电平

请实现 UWBManager 类：
- init(mosi, miso, sclk, cs, irq, is_initiator)
- twr_range(): 发起 TWR 测距，返回距离(cm)
- get_distance(): 获取最近一次测距值
- is_valid(): 距离数据是否在有效期内 (<200ms)

TWR 流程 (Single-Sided)：
1. Initiator 发送 Poll 消息
2. Responder 接收后发送 Response 消息
3. Initiator 计算往返时间 → 距离

左手套 = Initiator (主动发起 TWR)
右手套 = Responder (被动响应)

TWR 频率: 10-20Hz (FreeRTOS 定时器触发)
```

### C2: EKF 融合实现

```
实现 EKF (Extended Kalman Filter) 融合 UWB 距离 + IMU 位置估计：

状态向量: x = [px, py, pz, vx, vy, vz] (6-dim)
观测向量: z = [uwb_distance] (1-dim, 标量距离)

预测步 (IMU 驱动):
  x_pred = F * x + B * u
  F = [I, dt*I; 0, I]
  B = [0.5*dt^2*I; dt*I]
  u = [ax, ay, az] (IMU 加速度, 来自 BNO085)

更新步 (UWB 观测):
  h(x) = sqrt(px^2 + py^2 + pz^2)  (假设右手在原点)
  H = [px/r, py/r, pz/r, 0, 0, 0]
  K = P*H' / (H*P*H' + R)
  x = x + K*(z - h(x))
  P = (I - K*H)*P

过程噪声 Q: position 0.01, velocity 0.1
观测噪声 R: 25.0 (5cm 标准差)

输出: 100Hz 连续 3D 位置差估计 (UWB 10-20Hz 更新期间由预测插值)

请实现 EKFFusion 类，包含 predict() 和 update_uwb() 方法
```

### C3: 29 维特征向量组装

```
更新手套固件的传感器任务，组装 V5.2 29 维特征向量：

void assemble_features_v52(
    const float flex[5],          // 弯曲传感器
    const float quat[4],          // BNO085 四元数
    const float gyro[3],          // BNO085 陀螺仪
    const float accel[3],         // BNO085 加速度计
    const float ekf_pos[3],       // EKF 位置差估计
    const float uwb_dist,         // UWB TWR 距离
    const float other_quat[4],    // 对侧 BNO085 四元数 (ESP-NOW 接收)
    float output[29]              // 输出 29 维特征向量
) {
    // dim 0-4: Flex (5)
    memcpy(output, flex, 5*sizeof(float));
    // dim 5-7: Gyroscope (3)
    memcpy(output+5, gyro, 3*sizeof(float));
    // dim 8-10: Accelerometer (3)
    memcpy(output+8, accel, 3*sizeof(float));
    // Right hand features (11)
    // ... (received from other glove via ESP-NOW)
    // dim 22-24: Delta position from EKF (3)
    memcpy(output+22, ekf_pos, 3*sizeof(float));
    // dim 25: UWB distance (1) — V5.2 NEW!
    output[25] = uwb_dist;
    // dim 26-27: Delta orientation (2) = azimuth + elevation
    // dim 28: Delta angular velocity (1)
    output[28] = gyro_diff;  // |gyro_L - gyro_R|
}
```

---

## Phase D: 29 维特征 + 模型重训

### D1: 数据采集流水线

```
创建 V5.2 双手手语数据采集流水线，包含 UWB 距离标签：

采集格式 (每帧):
- left_features: [5 flex, 3 gyro, 3 accel] = 11-dim
- right_features: [5 flex, 3 gyro, 3 accel] = 11-dim
- uwb_distance: float (cm) — NEW in V5.2
- delta_position: [3] float (EKF 估计)
- delta_orientation: [2] float
- delta_angular_vel: float
- label: int (0-45, 手势类别)
- timestamp: uint64 (us)

采集流程：
1. 采集者佩戴双手套，面向 P4 基站
2. 按屏幕提示执行指定手势 (46类)
3. 每手势重复 10 次，每次持续 3-5 秒
4. P4 基站自动记录 29-dim 数据至 MicroSD 卡
5. 采集完毕后导出为 .npy 格式

请生成采集固件代码 + Python 数据处理脚本
```

### D2: 模型重训与评估

```
使用 V5.2 29-dim 数据重新训练 Gated Bi-CrossAttn + MS-TCN 2-stage 模型：

关键对比实验：
1. V5.1 (28-dim, 无UWB) vs V5.2 (29-dim, 含UWB)
2. 消融实验：移除 uwb_distance 特征，观察精度下降
3. 仅依赖双手协同手势子集 (约20类)，UWB 贡献度

预期结果：
- V5.1 28-dim: 46类 ~85% (Tier2)
- V5.2 29-dim: 46类 ~92% (Tier2, P4 更大模型)
- 消融 (移除UWB): ~88% (UWB 贡献约 4%)
- 双手协同手势子集: UWB 贡献可达 7-10%

请生成完整实验脚本和结果可视化
```

---

## Phase E: MIPI-CSI 摄像头视觉补充

### E1: 摄像头+PPA 流水线

```
在 ESP32-P4 上实现 MIPI-CSI 摄像头采集 + PPA 硬件预处理流水线：

1. 摄像头: OV2710, 2MP, MIPI-CSI 2-lane
2. 采集分辨率: 320x240 RGB565 @15fps
3. PPA 预处理:
   - RGB565 → RGB888 色彩转换
   - 缩放至 224x224 (MobileNetV3 输入)
   - 均值归一化: (pixel/255 - 0.5) / 0.5
4. 推理: MobileNetV3-Small (INT8, ~2MB)
5. 输出: 视觉手势类别 + 置信度

融合策略 (后期融合):
- 传感器 Tier2 结果: weight = 0.7
- 视觉模型结果: weight = 0.3
- 加权融合: final_score = 0.7*sensor_score + 0.3*vision_score

请生成 CameraManager 和融合逻辑代码
```

---

## Phase F: 音频 TTS + 集成测试

### F1: ES8311 音频播放

```
在 ESP32-P4 上实现 ES8311 I2S 音频播放：

1. I2S 配置:
   - 采样率: 16000 Hz
   - 位宽: 16-bit
   - 通道: 单声道
   - MCLK: GPIO13, BCLK: GPIO12, WS: GPIO14, DOUT: GPIO11

2. TTS 策略 (离线):
   - 46 类手势名称预录 PCM 数据 (每条 <3 秒)
   - 存储在 MicroSD 卡: /tts/{gesture_id}.pcm
   - 识别到手势后从 SD 读取并播放

3. 播放流程:
   识别结果 → 查找 PCM 文件 → I2S DMA 播放
   目标延迟: 从识别到语音输出 <500ms

请生成 AudioManager 代码和 PCM 文件生成脚本
```

### F2: 全系统联调

```
执行 EchoGlove V5.2 全系统联调测试：

测试矩阵:
| 测试项 | Tier1 | Tier2(P4) | Tier3(PC) | 热切换 |
|--------|-------|-----------|-----------|--------|
| 延迟   | <10ms | <40ms     | <50ms     | <100ms |
| 精度   | >80%  | >90%      | >95%      | N/A    |
| UWB测距 | N/A  | <10cm     | <5cm      | N/A    |
| 丢包恢复 | N/A  | <50ms     | <100ms    | N/A    |
| 显示FPS | N/A  | >25fps    | N/A       | N/A    |
| TTS延迟 | N/A  | <500ms    | <200ms    | N/A    |

竞赛 Demo 场景:
1. 佩戴双手套 → 基站自动识别
2. 执行"你好"手势 → 屏幕显示+语音播报
3. 执行"谢谢"(右手触左手) → UWB距离缩小→识别
4. 断开 PC → Tier2 接管 → 屏幕继续显示
5. 断开基站 → Tier1 接管 → 手套 LED 反馈

请生成自动化测试脚本和 Demo 演示脚本
```

---

> **文档结束** — EchoGlove V5.2 Claude Code 逐步提示词
