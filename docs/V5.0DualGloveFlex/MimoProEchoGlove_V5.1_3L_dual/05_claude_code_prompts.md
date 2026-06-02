# EchoGlove V5.1 — Claude Code 分阶段AI编程提示词

> 面向 Claude Code 的V5.1完整分阶段提示词  
> 三级热切换 + Gated Bi-CrossAttention + 分层ST-GCN + MS-TCN→CTC  
> 分支: `Flex2.2in-BNO085-ADS1115-ESP32S3N16R8-ArduinoFramework`  
> 日期: 2026-06

---

## 使用说明

1. 每个 Phase 对应一次 Claude Code 会话
2. 复制对应Phase提示词到 Claude Code
3. 每Phase完成后 `git commit -am "Phase N: 描述"`，再进入下一Phase
4. 所有路径相对于项目根目录 `EchoGlove-SLR-MOCAP-Beta/`
5. **严格按顺序执行**，每个Phase的验收标准必须通过才能进入下一Phase

---

## Phase 1: 单手固件稳定化 + 霍尔传感器移除

```
我正在将EchoGlove项目从霍尔传感器方案完全迁移到弯曲传感器方案。

## 项目背景
- 仓库: https://github.com/PaxonHuang/EchoGlove-SLR-MOCAP-Beta/tree/Flex2.2in-BNO085-ADS1115-ESP32S3N16R8-ArduinoFramework
- ESP32-S3 N16R8, Arduino Framework, PlatformIO
- 原方案(已废弃): 5×TMAG5273 + 5×N52磁铁 + TCA9548A + BNO085
- 废弃原因: 5颗指尖N52磁珠对指根TMAG5273和BNO085磁力计造成严重磁场交叉干扰，无法采集有效数据
- 新方案: 5×SpectraFlex 2.2"弯曲传感器 + 2×ADS1115(16-bit ADC) + BNO085(GRV 6轴模式)

## 本次任务

### 1.1 移除旧传感器代码（彻底清理）
- 删除所有TCA9548A相关代码（头文件、驱动、引用）
- 删除所有TMAG5273相关代码（头文件、驱动、引用）
- 删除所有霍尔传感器校准代码
- 删除data_structures.h中的hall_xyz[15]字段
- 删除Protobuf/UDP/BLE中的hall相关序列化代码

### 1.2 新增ADS1115Manager
创建 lib/Sensors/ADS1115Manager.h 和 .cpp:
```cpp
#pragma once
#include <Adafruit_ADS1X15.h>

class ADS1115Manager {
public:
    bool begin();                           // 初始化两片ADS1115
    bool readAll(float out[5]);             // 读取5路Flex原始电压
    bool readChannel(uint8_t ch, float &v); // 读取单通道
    
private:
    Adafruit_ADS1115 adc1;  // ADDR→GND = 0x48, AIN0-2 → Flex 0-2
    Adafruit_ADS1115 adc2;  // ADDR→VDD = 0x49, AIN0-1 → Flex 3-4
    
    // 连续模式配置
    // 增益: ±2.048V (ADS1115_GAIN_TWO)
    // 数据速率: 860 SPS (最高)
    // 模式: 连续转换
};
```

### 1.3 新增FlexManager
创建 lib/Sensors/FlexManager.h 和 .cpp:
```cpp
#pragma once
#include "ADS1115Manager.h"

class FlexManager {
public:
    bool begin(ADS1115Manager &adc);
    void calibrate();                       // 5点分段线性校准
    void update();                          // 读取+滤波+校准
    void getAngles(float out[5]);           // 输出0~90°弯曲角度
    void autoZeroCalibrate();               // 每30秒自动零点校准
    
private:
    ADS1115Manager *_adc;
    float raw_voltages[5];
    float angles[5];
    
    // 校准参数
    float calib_points[5][5];               // 5传感器×5校准点(voltage→angle)
    
    // 滑动平均滤波(窗口8)
    static const int WINDOW_SIZE = 8;
    float windows[5][WINDOW_SIZE];
    int window_idx;
    
    // 温漂补偿
    float zero_offsets[5];                  // 零点偏移量
    unsigned long last_zero_check;
    static const unsigned long ZERO_CHECK_INTERVAL = 30000; // 30秒
    
    // 人体生理约束
    static const float MIN_ANGLE = 0.0f;
    static const float MAX_ANGLE = 90.0f;
};
```

### 1.4 修改IMUManager
- BNO085使用SH2_GAME_ROTATION_VECTOR (GRV 6轴模式)
- 不启用磁力计（避免磁干扰）
- 输出: euler[3] + gyro[3] = 6维

### 1.5 修改SensorManager
```cpp
class SensorManager {
public:
    bool begin();
    void readAll(SensorData &data);         // 填充11-dim数据
    
private:
    ADS1115Manager flexADC;
    FlexManager flex;
    IMUManager imu;
};

struct SensorData {
    float flex[5];                          // 5个Flex弯曲角度(0~90°)
    float euler[3];                         // BNO085欧拉角
    float gyro[3];                          // BNO085角速度
    uint32_t timestamp;
    
    void toFeatureArray(float out[11]) {
        // 输出顺序: [flex0-4, euler0-2, gyro0-2]
        for (int i = 0; i < 5; i++) out[i] = flex[i];
        for (int i = 0; i < 3; i++) out[5+i] = euler[i];
        for (int i = 0; i < 3; i++) out[8+i] = gyro[i];
    }
    
    static const int FEATURE_COUNT = 11;
};
```

### 1.6 修改I2C配置
在setup()中:
```cpp
Wire.setClock(100000);  // 强制100kHz
// 三设备地址: 0x48(ADS1115#1), 0x49(ADS1115#2), 0x4B(BNO085)
// 无地址冲突
```

### 1.7 修改通信协议
- BLE CSV: 移除Hall值, 新增5个Flex角度, buf扩大到512字节
- Protobuf: flex_features填5个float, 移除hall_features
- UDP binary: 移除60B Hall, 新增20B Flex

### 1.8 修改Kalman滤波器
- 通道数: 11 (5 Flex + 6 IMU)
- Flex通道R值: 0.02-0.05 (弯曲传感器噪声较小)
- IMU通道R值: 0.01-0.03

### 1.9 修改SlidingWindow和InferencePipeline
- SlidingWindow: 30×11=330 (输入维度从21改为11)
- InferencePipeline: input_dim=11

### 1.10 Tier1模型部署框架(预留)
创建 lib/Inference/Tier1Model.h:
```cpp
#pragma once
#include <TensorFlowLite_ESP32.h>
#include "tensorflow/lite/micro/all_ops_resolver.h"
#include "tensorflow/lite/micro/micro_interpreter.h"

class Tier1Model {
public:
    bool begin();                           // 加载TFLite模型到PSRAM
    bool infer(const float features[11], Tier1Result &result);
    
private:
    static const int TENSOR_ARENA_SIZE = 50 * 1022;  // 50KB in PSRAM
    uint8_t tensor_arena[TENSOR_ARENA_SIZE] __attribute__((aligned(16)));
    tflite::MicroInterpreter *_interpreter;
    TfLiteTensor *_input;
    TfLiteTensor *_output;
};

struct Tier1Result {
    uint32_t gesture_id;
    float confidence;
    uint32_t inference_time_ms;
};
```

## 验收标准
- [ ] `pio run` 编译通过，无错误
- [ ] 串口输出11维数据，100Hz稳定
- [ ] Flex角度范围0~90°，无跳变(标准差<2°)
- [ ] I2C扫描确认3个设备地址(0x48, 0x49, 0x4B)
- [ ] Tier1Model框架可编译(模型文件可先空占位)
- [ ] 无任何TMAG5273/TCA9548A残留代码
```

---

## Phase 2: 单手数据采集 + L1模型训练 + Tier1导出

```
我已完成EchoGlove V5.1单手固件(Phase 1)，现在采集数据集、训练L1模型、导出Tier1 TFLite模型。

## 本次任务

### 2.1 数据采集工具
创建 tools/collect_dataset.py:
- pyserial读取ESP32串口(11-dim: 5 Flex + 6 IMU)
- 按手势标签采集，每标签10次×5秒(100Hz=500帧/次)
- 数据增强: 时间拉伸(0.8-1.2×), 高斯噪声(σ=0.01)
- 保存格式: CSV(每行11个float) + JSON元数据(手势ID、执行者、时间戳、手势名称)
- 命名规范: dataset/raw/{gesture_id}_{gesture_name}/{sample_id}.csv

### 2.2 数据预处理
创建 tools/preprocess_dataset.py:
- CSV → Kalman滤波 → Z-score归一化(保存mean和std) → 滑动窗口(30帧, 步长10)
- 训练/验证/测试 = 7:1:2 (按执行者划分，防止数据泄漏)
- 保存为PyTorch .pt格式: dataset/processed/{train,val,test}.pt
- 每个.pt包含: features(B,30,11), labels(B,), lengths(B,)

### 2.3 L1模型定义
修改 glove_relay/src/models/l1_cnn_attention.py:
```python
class L1CNNAttention(nn.Module):
    """V5.1 Tier1: CNN + SE-Attention 单手模型"""
    def __init__(self, input_dim=11, num_classes=46):
        super().__init__()
        self.features = nn.Sequential(
            nn.Conv1d(input_dim, 32, kernel_size=3, padding=1),
            nn.BatchNorm1d(32), nn.ReLU(),
            nn.Conv1d(32, 64, kernel_size=3, padding=1),
            nn.BatchNorm1d(64), nn.ReLU(),
        )
        self.se = nn.Sequential(
            nn.AdaptiveAvgPool1d(1),
            nn.Flatten(),
            nn.Linear(64, 16), nn.ReLU(),
            nn.Linear(16, 64), nn.Sigmoid(),
        )
        self.classifier = nn.Sequential(
            nn.AdaptiveAvgPool1d(1), nn.Flatten(),
            nn.Linear(64, 128), nn.ReLU(), nn.Dropout(0.3),
            nn.Linear(128, num_classes),
        )
    
    def forward(self, x):  # (B, 30, 11)
        x = x.permute(0, 2, 1)  # (B, 11, 30)
        feat = self.features(x)  # (B, 64, 30)
        w = self.se(feat).unsqueeze(-1)  # (B, 64, 1)
        feat = feat * w  # SE attention
        return self.classifier(feat)  # (B, 46)
```

### 2.4 训练脚本
创建 tools/train_l1.py:
- LR=1e-3, Adam, CosineAnnealingLR, Epoch=100, EarlyStop(patience=10)
- 保存最佳模型: models/l1_best.pth
- 日志: TensorBoard
- 评估: Top-1, Top-5, 混淆矩阵

### 2.5 Tier1 TFLite导出
创建 tools/export_tier1.py:
```python
# PyTorch → ONNX(opset=13) → TensorFlow → TFLite int8
# 量化: 代表性数据集(训练集随机1000个样本)
# 输出: models/tier1_model.tflite (预期~148KB)
# 转C数组: xxd -i tier1_model.tflite > tier1_model_data.cc
# 部署: glove_firmware/lib/Inference/tier1_model_data.cc
```

### 2.6 Relay层适配
- protobuf_parser.py: 解析flex_features(5) 替代hall_features(15)
- confidence_router.py: 特征组装 flex+imu = 11-dim
- udp_server.py: 滑动窗口 30×11

### 2.7 前端适配
- useSensorStore.ts: hall→flex, 数据结构更新
- useHandAnimation.ts: Flex→关节角度映射
  - MCP=flex[i], PIP=0.67×MCP, DIP=0.5×MCP
  - 拇指特殊: CMC_x=Flex0×0.8, CMC_y=Flex0×0.3, MCP=Flex0×0.9, IP=Flex0×0.7
- types/index.ts: SensorMessage类型更新

## 验收标准
- [ ] 采集至少20类手势，每类10个样本
- [ ] L1验证精度>85% Top-1
- [ ] tier1_model.tflite < 200KB
- [ ] ESP32-S3 Tier1推理<50ms(串口输出推理时间)
- [ ] 前端3D手部正确显示Flex弯曲角度
```

---

## Phase 3: React Three Fiber MVP + Unity XR Hands

```
我已完成V5.1单手固件+Tier1模型(Phase 1-2)，现在实现3D手部可视化。

## 本次任务

### 3.1 React Three Fiber前端(优先)
创建 glove_web/ 目录:
- React 18 + Vite + TailwindCSS
- R3F (React Three Fiber) 3D渲染
- Zustand状态管理
- WebSocket实时数据(useWebSocket hook, 自动重连)
- PWA manifest

关键组件:
- HandSkeleton.tsx: 26关节3D手部骨骼
- SensorPanel.tsx: 实时传感器数值显示
- GestureDisplay.tsx: 当前识别手势显示
- ConnectionStatus.tsx: 连接状态指示

### 3.2 5DoF→26关节映射算法
创建 glove_web/src/utils/handMapping.ts:
```typescript
interface JointAngles {
    // 每个关节的旋转欧拉角(x,y,z) 单位:度
    wrist: { x: number; y: number; z: number };
    thumb: { cmc_x: number; cmc_y: number; mcp: number; ip: number };
    index: { mcp: number; pip: number; dip: number };
    middle: { mcp: number; pip: number; dip: number };
    ring: { mcp: number; pip: number; dip: number };
    pinky: { mcp: number; pip: number; dip: number };
}

function mapFlexToJoints(flex: number[], quaternion: number[]): JointAngles {
    // flex[0]=拇指, flex[1]=食指, ..., flex[4]=小指
    // quaternion: BNO085 GRV [w, x, y, z]
    
    // 拇指(双自由度)
    const thumb = {
        cmc_x: flex[0] * 0.8,
        cmc_y: flex[0] * 0.3,
        mcp: flex[0] * 0.9,
        ip: flex[0] * 0.7,
    };
    
    // 其他四指(单自由度，线性比例)
    const finger = (f: number) => ({
        mcp: f,
        pip: f * 0.67,
        dip: f * 0.5,
    });
    
    // 腕部: BNO085四元数 → Unity/Three.js坐标系
    // 注意: BNO085=[w,x,y,z] → Three.js=[x,y,z,w]
    const wrist = quaternionToEuler(quaternion);
    
    return { wrist, thumb, index: finger(flex[1]), middle: finger(flex[2]),
             ring: finger(flex[3]), pinky: finger(flex[4]) };
}
```

### 3.3 WebSocket消息格式
```json
{
    "type": "sensor_data",
    "timestamp": 1234567890,
    "hand": "left",
    "flex": [45.2, 30.1, 60.5, 15.3, 22.8],
    "imu": [0.12, -0.34, 1.57, 0.01, -0.02, 0.05],
    "gesture": { "id": 5, "name": "你好", "confidence": 0.82 }
}
```

### 3.4 Unity XR Hands集成(并行)
创建 unity/ 目录:
- Unity 2022.3 LTS + XR Hands 1.7 + OpenXR
- Assets/Scripts/EchoGloveDriver.cs: WebSocket数据接收
- Assets/Scripts/HandPoseMapper.cs: 5DoF→26关节映射
- Assets/Scripts/DualHandManager.cs: 双手管理(预留接口)

### 3.5 串口桥接
修改 glove_relay/src/websocket_server.py:
- 接收UDP数据 → JSON → WebSocket广播
- 支持多客户端同时连接
- 100Hz推送

## 验收标准
- [ ] React前端: 虚拟手实时跟随物理手套, 延迟<100ms
- [ ] 26关节运动自然, 无穿模, 角度约束0~90°
- [ ] WebSocket断线自动重连
- [ ] Unity: 基本手部驱动可运行(可选)
- [ ] 两个前端可同时连接同一个Relay
```

---

## Phase 4: 双手系统 + Gated Bi-CrossAttention + ESP-NOW

```
我已完成V5.1单手系统(Phase 1-3)，现在升级为双手系统。

## 架构决策(已确认)
- 双MCU架构: 2×ESP32-S3(每手独立) + 1×ESP32-S3(接收器)
- 通信: ESP-NOW广播(手套→接收器, 1-2ms延迟)
- 同步: 接收器广播SYNC_TICK, 按tick_id配对双手数据
- L1融合: Gated Bidirectional CrossAttention (门控解决空闲手问题)
- 特征: 28-dim = 左11 + 右11 + 相对6(间距3+方位2+角速度1)
- ST-GCN: Tier1/2用12节点(指尖+腕), Tier3用42节点(全骨架)

## 本次任务

### 4.1 ESP-NOW通信
创建 glove_firmware/lib/Communication/ESPNOWManager.h:
```cpp
#pragma once
#include <esp_now.h>
#include <WiFi.h>

struct GlovePacket {
    uint32_t tick_id;           // 接收器分配的同步ID
    uint32_t timestamp;         // 本地时间戳
    uint8_t hand_id;            // 0=left, 1=right
    float flex[5];              // Flex角度
    float euler[3];             // IMU欧拉角
    float gyro[3];              // IMU角速度
    uint8_t l1_gesture_id;      // Tier1手势ID
    float l1_confidence;        // Tier1置信度
    uint8_t checksum;           // 校验和
} __attribute__((packed));      // 69字节

class ESPNOWManager {
public:
    bool begin(const uint8_t *receiver_mac);
    bool send(const GlovePacket &pkt);
    void onReceive(void (*callback)(const uint8_t *data, int len));
    
private:
    uint8_t _receiver_mac[6];
    esp_now_peer_info_t _peer;
};
```

### 4.2 接收器固件
创建 receiver_firmware/ 目录:
```cpp
// receiver_firmware/main.cpp
// ESP32-S3 N16R8 接收器
// 功能:
// 1. 广播SYNC_TICK(每10ms)
// 2. 接收左右手套数据
// 3. 按tick_id配对
// 4. 计算相对特征
// 5. USB串口转发到PC

void setup() {
    Serial.begin(115200);
    initESPNOW();  // 接收模式
    initLED();     // WS2812B状态指示
}

void loop() {
    broadcastSyncTick();
    
    if (waitForBothHands(50)) {  // 50ms超时
        RelativeFeatures rel = computeRelative(left, right);
        sendToPC(left, right, rel);
        setLEDColor(COLOR_GREEN);  // 双手数据就绪
    } else {
        sendAvailable();  // 转发可用数据
        setLEDColor(COLOR_YELLOW);  // 单手数据
    }
}
```

### 4.3 Protobuf V5.1
更新 glove_data.proto:
```protobuf
syntax = "proto3";

message GloveData {
    uint32 timestamp = 1;
    uint32 hand_id = 2;
    repeated float flex_features = 3;   // 5个
    repeated float imu_features = 4;    // 6个
    uint32 l1_gesture_id = 5;
    float l1_confidence = 6;
    uint32 tick_id = 7;
    Tier1Result tier1_result = 8;
}

message Tier1Result {
    uint32 gesture_id = 1;
    float confidence = 2;
    uint32 inference_time_ms = 3;
}

message RelativeFeatures {
    float delta_pos_x = 1;
    float delta_pos_y = 2;
    float delta_pos_z = 3;
    float delta_orient_w = 4;
    float delta_orient_diff = 5;
    float delta_angular_vel = 6;
}

message SyncInfo {
    uint32 tick_id = 1;
    uint32 left_recv_delay_us = 2;
    uint32 right_recv_delay_us = 3;
}

message DualGloveData {
    uint32 timestamp = 1;
    GloveData left = 2;
    GloveData right = 3;
    RelativeFeatures relative = 4;
    SyncInfo sync = 5;
    InferenceTier inference = 6;
}

message InferenceTier {
    TierLevel active_tier = 1;
    float blend_alpha = 2;
    Tier1Result tier1_left = 3;
    Tier1Result tier1_right = 4;
    Tier2Result tier2_result = 5;
    Tier3Result tier3_result = 6;
}

enum TierLevel {
    TIER1_GLOVE_ONLY = 0;
    TIER2_RECEIVER = 1;
    TIER3_PC_FULL = 2;
}

message Tier2Result {
    uint32 gesture_id = 1;
    float confidence = 2;
    uint32 inference_time_ms = 3;
    bool valid = 4;
}

message Tier3Result {
    uint32 gesture_id = 1;
    float confidence = 2;
    repeated string l2_words = 3;
    repeated float l2_confidences = 4;
}
```

### 4.4 Gated Bi-CrossAttention模型
创建 glove_relay/src/models/l1_gated_cross_attention.py:
```python
import torch
import torch.nn as nn

class GatedCrossAttention(nn.Module):
    """门控交叉注意力: 解决双手手语中的空闲手噪声问题
    
    核心思想: 当一只手静止时，门控值→0，自动退化为自注意力，
    避免空闲手噪声干扰主导手的特征。
    """
    def __init__(self, d_model=64, num_heads=4, dropout=0.1):
        super().__init__()
        self.cross_attn = nn.MultiheadAttention(
            d_model, num_heads, dropout=dropout, batch_first=True
        )
        self.gate_proj = nn.Linear(d_model * 2, d_model)
        self.norm = nn.LayerNorm(d_model)
        self.dropout = nn.Dropout(dropout)

    def forward(self, x_self, x_cross):
        """
        x_self: (B, T, d) - 查询手特征
        x_cross: (B, T, d) - 键值手特征
        """
        attn_out, _ = self.cross_attn(
            query=x_self, key=x_cross, value=x_cross
        )
        # 门控: 学习每个维度是否使用cross信息
        gate = torch.sigmoid(
            self.gate_proj(torch.cat([x_self, attn_out], dim=-1))
        )
        # 门控融合
        output = gate * attn_out + (1 - gate) * x_self
        return self.norm(self.dropout(output) + x_self)


class SingleHandStream(nn.Module):
    """单手特征提取流"""
    def __init__(self, input_dim=11, d_model=64):
        super().__init__()
        self.encoder = nn.Sequential(
            nn.Conv1d(input_dim, 32, 3, padding=1),
            nn.BatchNorm1d(32), nn.ReLU(),
            nn.Conv1d(32, d_model, 3, padding=1),
            nn.BatchNorm1d(d_model), nn.ReLU(),
        )
        self.pool = nn.AdaptiveAvgPool1d(1)

    def forward(self, x):  # (B, T, input_dim)
        x = x.permute(0, 2, 1)
        feat = self.encoder(x)  # (B, d_model, T)
        return self.pool(feat).squeeze(-1)  # (B, d_model)


class L1GatedBiCrossAttn(nn.Module):
    """V5.1 L1: Gated Bidirectional CrossAttention 双手融合模型
    
    输入: 左手窗口(30,11) + 右手窗口(30,11) + 相对特征(6,)
    输出: 46类手势概率
    
    架构:
    1. 左右手各自CNN编码 → 64-dim
    2. 自注意力增强
    3. 门控双向交叉注意力
    4. 相对特征FC
    5. 融合分类
    """
    def __init__(self, input_dim=11, num_classes=46, d_model=64):
        super().__init__()
        self.left_encoder = SingleHandStream(input_dim, d_model)
        self.right_encoder = SingleHandStream(input_dim, d_model)
        
        self.left_self_attn = nn.MultiheadAttention(d_model, 4, batch_first=True)
        self.right_self_attn = nn.MultiheadAttention(d_model, 4, batch_first=True)
        
        self.gated_cross_l2r = GatedCrossAttention(d_model)
        self.gated_cross_r2l = GatedCrossAttention(d_model)
        
        self.rel_fc = nn.Sequential(
            nn.Linear(6, d_model),
            nn.ReLU(),
        )
        
        self.classifier = nn.Sequential(
            nn.Linear(d_model * 3, 128),
            nn.ReLU(),
            nn.Dropout(0.3),
            nn.Linear(128, num_classes),
        )

    def forward(self, left_window, right_window, relative):
        """
        left_window: (B, 30, 11)
        right_window: (B, 30, 11)
        relative: (B, 6)
        """
        left_feat = self.left_encoder(left_window).unsqueeze(1)   # (B, 1, 64)
        right_feat = self.right_encoder(right_window).unsqueeze(1) # (B, 1, 64)
        
        # 自注意力
        left_self, _ = self.left_self_attn(left_feat, left_feat, left_feat)
        right_self, _ = self.right_self_attn(right_feat, right_feat, right_feat)
        
        # 门控双向交叉注意力
        left_enhanced = self.gated_cross_l2r(left_self, right_self)  # (B, 1, 64)
        right_enhanced = self.gated_cross_r2l(right_self, left_self) # (B, 1, 64)
        
        # 相对特征
        rel_feat = self.rel_fc(relative).unsqueeze(1)  # (B, 1, 64)
        
        # 融合
        fused = torch.cat([
            left_enhanced.squeeze(1),
            right_enhanced.squeeze(1),
            rel_feat.squeeze(1)
        ], dim=-1)  # (B, 192)
        
        return self.classifier(fused)  # (B, 46)
    
    def get_gate_values(self, left_window, right_window):
        """获取门控值用于可视化和调试"""
        # 返回门控值，用于分析模型是否正确学习了空闲手抑制
        pass
```

### 4.5 相对特征计算
创建 glove_relay/src/utils/relative_features.py:
```python
import numpy as np

def compute_relative_features(left_imu: np.ndarray, right_imu: np.ndarray) -> np.ndarray:
    """计算双手相对特征(6维)
    
    left_imu: [euler_x, euler_y, euler_z, gyro_x, gyro_y, gyro_z]
    right_imu: 同上
    
    返回: [delta_pos_x, delta_pos_y, delta_pos_z, 
           delta_orient_w, delta_orient_diff, delta_angular_vel]
    
    注: delta_pos需要从IMU积分或外部位置传感器获取，
    初期可用BNO085的线性加速度积分(有漂移，但短期可用)
    """
    # 相对方位
    delta_orient = left_imu[:3] - right_imu[:3]  # 欧拉角差
    delta_orient_w = np.linalg.norm(delta_orient)  # 方位差模长
    delta_orient_diff = delta_orient[2]  # yaw差(水平面旋转)
    
    # 相对角速度
    delta_angular_vel = np.linalg.norm(left_imu[3:6] - right_imu[3:6])
    
    # 相对位置(简化: 用方位差近似，真实位置需要积分或外部传感器)
    delta_pos = delta_orient * 0.1  # 粗略近似，后续用真实位置替换
    
    return np.array([
        delta_pos[0], delta_pos[1], delta_pos[2],
        delta_orient_w, delta_orient_diff, delta_angular_vel
    ])
```

### 4.6 双手数据集采集
更新 tools/collect_dataset.py:
- 支持双手同步采集(通过接收器汇聚)
- 双手协同手势标签
- 命名: dataset/raw/{gesture_id}_{gesture_name}/{sample_id}_dual.csv

### 4.7 前端双手支持
- React: DualHandCanvas.tsx - 两个HandSkeleton并排
- Unity: DualHandManager.cs - 双手驱动
- useSensorStore.ts: leftHand + rightHand + relative

## 验收标准
- [ ] ESP-NOW双手数据传输稳定，延迟<5ms
- [ ] 接收器正确配对双手数据(tick_id匹配)
- [ ] Gated Bi-CrossAttn双手手势验证精度>87%
- [ ] 门控可视化: 单手手势时门控值→0(空闲手被抑制)
- [ ] 双手3D渲染同步，无明显时差
- [ ] 相对特征6维计算正确
```

---

## Phase 5: 三级热切换架构

```
我已完成V5.1双手系统(Phase 1-4)，现在实现三级热切换。

## 架构
- Tier1(手套): CNN+Attn 11-dim, ~148KB, ~30ms, ~80% (始终运行)
- Tier2(接收器): Gated Bi-CrossAttn only 28-dim, ~80KB, ~30ms, ~85% (PC离线时激活)
- Tier3(PC): Gated Bi-CrossAttn+MS-TCN4s + ST-GCN42→MS-TCN4s→CTC, ~3MB, ~15ms(GPU), ~92%
- 热切换: 5帧线性渐变(~50ms), 温度校准对齐

## 本次任务

### 5.1 Tier2模型(精简版，仅Gated Bi-CrossAttn)
创建 glove_relay/src/models/tier2_model.py:
```python
class Tier2Model(nn.Module):
    """接收器端精简模型: 仅Gated Bi-CrossAttn, 无MS-TCN
    
    大小: ~80KB int8
    推理: ~30ms @ ESP32-S3 240MHz + PSRAM
    输入: 左手11-dim + 右手11-dim + 相对6-dim
    输出: 46类手势概率
    
    与Tier3 L1的区别: 无MS-TCN时序精炼，直接池化分类
    """
    def __init__(self, input_dim=11, num_classes=46, d_model=64):
        super().__init__()
        # 复用L1GatedBiCrossAttn的结构，但去掉MS-TCN
        self.left_encoder = SingleHandStream(input_dim, d_model)
        self.right_encoder = SingleHandStream(input_dim, d_model)
        self.gated_cross_l2r = GatedCrossAttention(d_model)
        self.gated_cross_r2l = GatedCrossAttention(d_model)
        self.rel_fc = nn.Linear(6, d_model)
        self.classifier = nn.Sequential(
            nn.Linear(d_model * 3, 128),
            nn.ReLU(),
            nn.Dropout(0.3),
            nn.Linear(128, num_classes),
        )
    
    def forward(self, left, right, relative):
        # 与L1GatedBiCrossAttn相同，但无自注意力(节省计算)
        left_feat = self.left_encoder(left).unsqueeze(1)
        right_feat = self.right_encoder(right).unsqueeze(1)
        left_enhanced = self.gated_cross_l2r(left_feat, right_feat)
        right_enhanced = self.gated_cross_r2l(right_feat, left_feat)
        rel_feat = torch.relu(self.rel_fc(relative)).unsqueeze(1)
        fused = torch.cat([
            left_enhanced.squeeze(1),
            right_enhanced.squeeze(1),
            rel_feat.squeeze(1)
        ], dim=-1)
        return self.classifier(fused)
```

### 5.2 Tier2 TFLite导出
创建 tools/export_tier2.py:
```python
# 导出Tier2模型为TFLite int8
# 混合量化: gate_proj层FP16, 其余int8
# 输出: models/tier2_model.tflite (预期~80KB)
# 转C数组 → receiver_firmware/lib/tier2_model_data.cc
```

### 5.3 接收器Tier2推理
创建 receiver_firmware/lib/Tier2Inference.h:
```cpp
#pragma once
#include <TensorFlowLite_ESP32.h>

class Tier2Inference {
public:
    bool begin();  // 加载模型到PSRAM
    bool infer(const float left[11], const float right[11], 
               const float relative[6], Tier2Result &result);
    
private:
    static const int ARENA_SIZE = 60 * 1024;  // 60KB PSRAM
    uint8_t arena[ARENA_SIZE];
    tflite::MicroInterpreter *_interpreter;
};
```

### 5.4 接收器完整主循环
更新 receiver_firmware/main.cpp:
```cpp
Tier2Inference tier2;
ESPNOWReceiver receiver;
USBSerialBridge bridge;

void setup() {
    Serial.begin(115200);
    receiver.begin();      // ESP-NOW接收
    tier2.begin();         // Tier2模型加载
    bridge.begin();        // USB串口桥接
    initLED();             // 状态LED
}

void loop() {
    // 1. 广播SYNC_TICK
    receiver.broadcastSyncTick();
    
    // 2. 等待双手数据(50ms超时)
    if (receiver.waitForBothHands(50)) {
        // 3. 计算相对特征
        float relative[6];
        computeRelative(receiver.left, receiver.right, relative);
        
        // 4. Tier2推理
        Tier2Result result;
        tier2.infer(receiver.left.features, receiver.right.features, 
                    relative, result);
        
        // 5. 转发到PC(含Tier2结果)
        bridge.sendDualData(receiver.left, receiver.right, relative, result);
        
        // 6. LED指示
        setLED(COLOR_GREEN);  // 双手+Tier2就绪
    } else {
        bridge.sendAvailable(receiver.getAvailable());
        setLED(COLOR_YELLOW);  // 单手数据
    }
}
```

### 5.5 PC Relay TierRouter
创建 glove_relay/src/tier_router.py:
```python
class TierRouter:
    """V5.1 三级推理路由与融合"""
    
    def __init__(self):
        self.tier1_left = None
        self.tier1_right = None
        self.tier2_result = None
        self.tier3_result = None
        self.active_tier = TierLevel.TIER3
        self.blend_alpha = 1.0
        self.blend_frames = 5
        self.previous_tier = TierLevel.TIER3
        
        # 温度校准参数(验证集上确定)
        self.temperature = {
            TierLevel.TIER1: 1.0,
            TierLevel.TIER2: 1.0,
            TierLevel.TIER3: 1.0,
        }
    
    def update(self, dual_data: DualGloveData) -> dict:
        # 更新各级结果
        self._extract_tier_results(dual_data)
        
        # 确定active tier
        new_tier = self._determine_active_tier()
        if new_tier != self.active_tier:
            self.previous_tier = self.active_tier
            self.active_tier = new_tier
            self.blend_alpha = 0.0  # 开始过渡
        
        # 融合输出
        return self._blend_results()
    
    def _determine_active_tier(self) -> TierLevel:
        if self._pc_available():
            return TierLevel.TIER3
        elif self.tier2_result and self.tier2_result.get('valid', False):
            return TierLevel.TIER2
        else:
            return TierLevel.TIER1
    
    def _blend_results(self) -> dict:
        if self.blend_alpha < 1.0:
            self.blend_alpha = min(1.0, self.blend_alpha + 1.0/self.blend_frames)
        
        current = self._get_probs(self.active_tier)
        previous = self._get_probs(self.previous_tier)
        
        # 温度校准后融合
        calibrated_cur = self._temperature_calibrate(current, self.active_tier)
        calibrated_prev = self._temperature_calibrate(previous, self.previous_tier)
        
        blended = {}
        for cls_id in calibrated_cur:
            blended[cls_id] = (
                self.blend_alpha * calibrated_cur[cls_id] +
                (1 - self.blend_alpha) * calibrated_prev.get(cls_id, 0.0)
            )
        return blended
    
    def _temperature_calibrate(self, probs: dict, tier: TierLevel) -> dict:
        """温度校准: 对齐不同Tier的softmax分布"""
        tau = self.temperature[tier]
        if tau == 1.0:
            return probs
        recalibrated = {k: v**(1/tau) for k, v in probs.items()}
        total = sum(recalibrated.values())
        return {k: v/total for k, v in recalibrated.items()}
```

### 5.6 Tier1 TFLite部署到手套ESP32
更新 glove_firmware/lib/Inference/Tier1Model.h:
- 加载tier1_model_data.cc
- infer()方法: 输入11-dim, 输出Tier1Result
- 推理频率: 可降至50Hz(每2帧推理一次)节省电量

### 5.7 前端Tier级别显示
- React: TierIndicator.tsx - 绿/黄/红 + 推理延迟数字
- Unity: TierUI.cs - HUD显示当前Tier级别

### 5.8 MS-TCN模块(Tier3 L2时序精炼)
创建 glove_relay/src/models/ms_tcn.py:
```python
class SingleStageTCN(nn.Module):
    """单阶段TCN: 10层膨胀因果卷积"""
    def __init__(self, in_dim, hidden_dim=64, num_classes=47, num_layers=10):
        super().__init__()
        self.conv = nn.ModuleList()
        self.bn = nn.ModuleList()
        for l in range(num_layers):
            dilation = 2 ** l
            self.conv.append(nn.Conv1d(
                hidden_dim if l > 0 else in_dim,
                hidden_dim, kernel_size=3,
                padding=dilation, dilation=dilation
            ))
            self.bn.append(nn.BatchNorm1d(hidden_dim))
        self.output_conv = nn.Conv1d(hidden_dim, num_classes, 1)

    def forward(self, x):  # (B, T, C_in)
        x = x.permute(0, 2, 1)
        for conv, bn in zip(self.conv, self.bn):
            residual = x
            x = torch.relu(bn(conv(x)))
            x = x + residual[:, :, -x.size(2):]
        return self.output_conv(x).permute(0, 2, 1)


class MSTCN(nn.Module):
    """多阶段TCN: 4阶段迭代精炼"""
    def __init__(self, in_dim=64, hidden_dim=64, num_classes=47,
                 num_stages=4, num_layers=10):
        super().__init__()
        self.stages = nn.ModuleList()
        self.stages.append(SingleStageTCN(in_dim, hidden_dim, num_classes, num_layers))
        for _ in range(num_stages - 1):
            self.stages.append(SingleStageTCN(
                num_classes + in_dim, hidden_dim, num_classes, num_layers
            ))
        self.smoothing_lambda = 0.15

    def forward(self, spatial_features):  # (B, T, 64)
        predictions = []
        pred = None
        for i, stage in enumerate(self.stages):
            if i == 0:
                pred = stage(spatial_features)
            else:
                stage_input = torch.cat([pred, spatial_features], dim=-1)
                pred = stage(stage_input)
            predictions.append(pred)
        return predictions

    def compute_loss(self, predictions, targets, lengths):
        total_loss = 0
        for pred in predictions:
            cls_loss = F.cross_entropy(pred.view(-1, pred.size(-1)), targets.view(-1))
            diff = pred[:, 1:, :] - pred[:, :-1, :]
            smooth_loss = torch.mean(diff ** 2)
            total_loss += cls_loss + self.smoothing_lambda * smooth_loss
        return total_loss / len(predictions)
```

### 5.9 ST-GCN模块(分层12/42节点)
创建 glove_relay/src/models/stgcn.py:
```python
class DualHandSTGCN(nn.Module):
    """分层ST-GCN: Tier1/2用12节点, Tier3用42节点"""
    
    def __init__(self, input_dim=28, hidden_dim=64, num_nodes=42):
        super().__init__()
        self.num_nodes = num_nodes
        self.proj = nn.Linear(input_dim, num_nodes * 2)
        
        # 根据节点数选择图结构
        if num_nodes == 12:
            self.adj = self._build_12node_adj()  # 22边
        else:
            self.adj = self._build_42node_adj()  # 66边
        
        self.st_blocks = nn.ModuleList([
            STConvBlock(hidden_dim, self.adj) for _ in range(3)
        ])
        self.pool = nn.AdaptiveAvgPool1d(1)
    
    def _build_12node_adj(self):
        """12节点: 每手5指尖+1腕, 22边(10内+6跨+6自环)"""
        edges = [
            # 左手内部(5边): 腕→各指尖
            (0,1),(0,2),(0,3),(0,4),(0,5),
            # 右手内部(5边): 腕→各指尖
            (6,7),(6,8),(6,9),(6,10),(6,11),
            # 跨手(6边): 对应指尖连接
            (1,7),(2,8),(3,9),(4,10),(5,11),(0,6),
            # 自环(6边)
            (0,0),(1,1),(2,2),(3,3),(4,4),(5,5),
        ]
        return self._edges_to_adj(12, edges)
    
    def _build_42node_adj(self):
        """42节点: 每手21关节, 66边(40内+6跨+20自环)"""
        # 左手21节点(0-20), 右手21节点(21-41)
        edges = []
        # 左手内部20边(标准手部骨架)
        hand_edges = [(0,1),(1,2),(2,3),(3,4),(0,5),(5,6),(6,7),(7,8),
                      (0,9),(9,10),(10,11),(11,12),(0,13),(13,14),(14,15),(15,16),
                      (0,17),(17,18),(18,19),(19,20)]
        edges.extend(hand_edges)  # 左手
        edges.extend([(a+21,b+21) for a,b in hand_edges])  # 右手
        # 跨手6边
        edges.extend([(0,21),(4,25),(8,29),(12,33),(16,37),(20,41)])
        # 自环20边(每手10个关键节点)
        for i in [0,1,5,9,13,17,21,22,26,30,34,38]:
            edges.append((i,i))
        return self._edges_to_adj(42, edges)
    
    def forward(self, x):  # (B, T, 28)
        x = self.proj(x).view(x.size(0), x.size(1), self.num_nodes, 2)
        for block in self.st_blocks:
            x = block(x)
        x = x.mean(dim=2)  # 节点聚合 → (B, T, 64)
        return x
```

### 5.10 L2完整流水线
创建 glove_relay/src/models/l2_stgcn_mstcn_ctc.py:
```python
class L2Pipeline(nn.Module):
    """V5.1 L2: ST-GCN → MS-TCN → CTC"""
    def __init__(self, num_nodes=42, num_classes=47):
        super().__init__()
        self.stgcn = DualHandSTGCN(input_dim=28, hidden_dim=64, num_nodes=num_nodes)
        self.mstcn = MSTCN(in_dim=64, hidden_dim=64, num_classes=num_classes)
        self.ctc = CTCDecoder(num_classes=num_classes, blank_idx=0, beam_width=10)
    
    def forward(self, x):  # (B, T, 28)
        spatial_feat = self.stgcn(x)          # (B, T, 64)
        predictions = self.mstcn(spatial_feat) # [pred_1..pred_4]
        return predictions[-1]                 # 最终阶段预测 (B, T, 47)
    
    def decode(self, logits):
        return self.ctc.decode(logits)
```

### 5.11 CTC解码器
```python
class CTCDecoder(nn.Module):
    def __init__(self, num_classes=47, blank_idx=0, beam_width=10):
        super().__init__()
        self.blank_idx = blank_idx
        self.beam_width = beam_width
    
    def decode(self, logits):  # (B, T, C)
        log_probs = F.log_softmax(logits, dim=-1)
        best_path = log_probs.argmax(dim=-1)  # (B, T)
        results = []
        for b in range(best_path.size(0)):
            path = best_path[b].tolist()
            merged = []
            prev = None
            for p in path:
                if p != self.blank_idx and p != prev:
                    merged.append(p)
                prev = p
            results.append(merged)
        return results
```

## 验收标准
- [ ] Tier3→Tier2切换: <100ms过渡, 无输出跳变
- [ ] Tier2→Tier1切换: <50ms过渡
- [ ] 接收器Tier2推理稳定<40ms
- [ ] Tier1 TFLite在ESP32-S3上运行正常
- [ ] 温度校准后Tier间概率分布KL散度<0.1
- [ ] 热切换100次无异常
- [ ] LED正确指示当前Tier级别
```

---

## Phase 6: 进阶优化 + 系统集成

```
我已完成V5.1三级热切换系统(Phase 1-5)，现在进行进阶优化和系统集成测试。

## 本次任务

### 6.1 数据集扩展到60+类双手协同手势
- 采集双手协同手势(如"你好"、"谢谢"、"我爱你"等)
- 重新训练所有模型(Tier1, Tier2, Tier3 L1, L2)
- 验证扩展后精度

### 6.2 MANO参数化模型(替换线性耦合)
- 集成manopth: 5个Flex → MANO β(10维) → 26关节+网格
- Unity端ms-mano-unity插件
- 目标: 角度误差<3°

### 6.3 多模态视觉融合(MediaPipe补全)
- MediaPipe手部42关键点
- 扩展Kalman: Flex(5)+IMU(6)+Vision(42)=53-dim
- Flex异常时视觉自动补全

### 6.4 BiLSTM+CTC备选(L2无GPU场景)
创建 glove_relay/src/models/l2_bilstm_ctc.py:
```python
class BiLSTM_CTC(nn.Module):
    """无GPU场景的L2备选方案"""
    def __init__(self, input_dim=28, hidden_dim=128, num_layers=2, num_classes=47):
        super().__init__()
        self.lstm = nn.LSTM(input_dim, hidden_dim, num_layers,
                            bidirectional=True, batch_first=True, dropout=0.3)
        self.fc = nn.Linear(hidden_dim * 2, num_classes)
    
    def forward(self, x):  # (B, T, 28)
        x, _ = self.lstm(x)
        return self.fc(x)  # (B, T, 47)
```

### 6.5 低功耗优化
- 空闲降采样: 100Hz → 20Hz(无手势检测时)
- Flex传感器间歇供电(通过MOSFET控制)
- 电池电压监测+低电提醒(GPIO4 ADC)

### 6.6 NLP语法修正
创建 glove_relay/src/nlp/grammar_corrector.py:
- CSL(中国手语) → 普通话语法转换
- 基于规则+小型语言模型

### 6.7 TTS语音合成
- edge-tts集成
- 支持中文语音输出

### 6.8 系统集成测试
- 端到端延迟测试: 目标<100ms
- 2小时稳定性测试
- 多环境测试(WiFi密集/户外)
- 三级热切换压力测试(100次切换)
- 双手46类手势精度测试
- 电池续航测试(目标>2小时)

## 验收标准
- [ ] 60+类双手手势精度>80%
- [ ] MANO映射角度误差<3°
- [ ] 2小时连续运行无崩溃
- [ ] 端到端延迟<100ms
- [ ] 三级切换100次无异常
- [ ] 电池续航>2小时
- [ ] NLP语法修正功能可用
- [ ] TTS语音输出正常
```

---

## 快速参考: V5.1各Phase关键文件变更

| Phase | 新增文件 | 修改文件 |
|-------|----------|----------|
| **1** | ADS1115Manager.h/.cpp, FlexManager.h/.cpp, Tier1Model.h | SensorManager.h, data_structures.h, IMUManager.h, BLEManager.h, UDPTransmitter.h, KalmanFilter.h, glove_data.proto |
| **2** | collect_dataset.py, preprocess_dataset.py, train_l1.py, export_tier1.py, tier1_model_data.cc | l1_cnn_attention.py, protobuf_parser.py, confidence_router.py, useSensorStore.ts, types/index.ts |
| **3** | HandSkeleton.tsx, SensorPanel.tsx, handMapping.ts, EchoGloveDriver.cs, HandPoseMapper.cs | WebSocket server, useHandAnimation.ts |
| **4** | ESPNOWManager.h, receiver_firmware/*, l1_gated_cross_attention.py, relative_features.py, DualHandCanvas.tsx | glove_data.proto, feature_fusion.py, useSensorStore.ts |
| **5** | tier2_model.py, export_tier2.py, Tier2Inference.h, tier_router.py, ms_tcn.py, stgcn.py, l2_stgcn_mstcn_ctc.py, TierIndicator.tsx | receiver_firmware/main.cpp, glove_data.proto |
| **6** | l2_bilstm_ctc.py, grammar_corrector.py, mediapipe_fusion.py, mano_regressor.py | InferencePipeline.h, HandPoseMapper.cs |

---

## 避坑清单

| 坑 | Phase | 解决方案 |
|----|-------|----------|
| Flex传感器温漂 | 1 | 自动零点校准每30秒 |
| I2C总线冲突 | 1 | 强制100kHz + 地址检查 |
| Gated CrossAttn门控退化 | 4 | 门控正则化: loss += λ·|gate-0.5|² |
| MS-TCN梯度消失 | 5 | 渐进训练: Stage1→冻结→Stage2→... |
| 接收器PSRAM不足 | 5 | Tier2已精简到~80KB |
| 热切换输出跳变 | 5 | 延长blend至10帧+温度校准 |
| CTC blank类过多 | 5 | 调blank权重+语言模型约束 |
| ESP-NOW丢包 | 4 | 插值补帧(tick_id不连续时) |
| int8量化门控精度下降 | 5 | 门控层FP16混合量化 |
| BNO085四元数顺序 | 3 | BNO085=[w,x,y,z] → Unity=[x,y,z,w] |
| LSTM量化TFLite不支持 | 6 | BiLSTM仅PC端运行 |
| PSRAM看门狗超时 | 5 | task watchdog超时>1s |

---

## Git提交规范

每个Phase完成后:
```bash
git add -A
git commit -m "Phase N: 简要描述

- 变更项1
- 变更项2
- 验收标准: 通过/部分通过

Co-Authored-By: Claude <noreply@anthropic.com>"
```
