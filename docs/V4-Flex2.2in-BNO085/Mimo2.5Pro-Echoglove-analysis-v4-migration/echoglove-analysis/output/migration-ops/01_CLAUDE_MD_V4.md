# CLAUDE.md — EchoGlove V4 项目记忆

> **使用方式：** 将本文件复制到项目根目录 `echoglove/CLAUDE.md`，覆盖或合并原有内容。  
> Claude Code 每次启动时会自动读取此文件作为项目上下文。  

---

## 项目概述

EchoGlove 是一个开源智能数据手套，用于中国手语 (CSL) 识别和动作捕捉。

**当前版本：V4.0**（从 V3.0 Hall+磁铁架构迁移至 Flex 传感器架构）

## 架构总览

```
传感器层: 5× Flex + BNO085 6DOF → 11维特征向量
    ↓
L1 推理: 1D-CNN+Attention (28K params, INT8, <10ms) → 边缘手势识别
    ↓
通信: Protobuf V4 over UDP → Python Relay
    ↓
L2 推理: ST-GCN (280K params) → 云端精确识别（置信度不足时触发）
    ↓
输出: CSL NLP 语法纠正 → TTS 双向翻译 + R3F/Unity 3D 渲染
```

## 硬件配置 (V4)

| 设备 | I2C 地址 | 用途 |
|---|---|---|
| ADS1115 #1 | 0x48 | Thumb/Index/Middle/Ring ADC |
| ADS1115 #2 | 0x49 | Pinky ADC + 3 spare |
| BNO085 | 0x4B | 6DOF IMU (Game Rotation Vector) |

- **I2C:** GPIO8=SDA, GPIO9=SCL, 400kHz
- **BNO085:** RST→GPIO10, INT→GPIO11
- **校准按钮:** GPIO13 (active low)
- **状态 LED:** GPIO12 (WS2812B)
- **Flex 电源控制:** GPIO47 (MOSFET)

## 特征向量 (V4)

每帧 11 维：
| 维度 | 内容 | 范围 |
|---|---|---|
| 0-4 | 5 指 flex % | 0-100% |
| 5-7 | Euler 角 (roll/pitch/yaw) | ±180° |
| 8-10 | 陀螺仪 (gx/gy/gz) | ±2000°/s |

- 采样率: 100Hz
- 滑动窗口: 30 帧 × 11 维 = 330 维 → L1 输入

## 项目结构

```
echoglove/
├── glove_firmware/           # ESP32-S3 固件 (ESP-IDF 5.x, PlatformIO)
│   ├── components/
│   │   ├── ads1115/          # 16-bit ADC 驱动 (V4 新增)
│   │   ├── bno085/           # IMU 驱动 (V4: 6DOF 模式)
│   │   ├── flex_manager/     # Flex 传感器管理 (V4 新增)
│   │   ├── sensor_manager/   # 传感器采样协调 (V4 重写)
│   │   ├── kalman_filter/    # 卡尔曼滤波 (V4: 11通道)
│   │   ├── sliding_window/   # 滑动窗口 (V4: 30×11)
│   │   ├── l1_inference/     # L1 边缘推理
│   │   ├── protobuf_encoder/ # Protobuf 编码
│   │   └── udp_sender/       # UDP 发送
│   ├── main/                 # 主程序入口
│   ├── proto/                # Protobuf 定义
│   ├── models/               # ML 模型文件
│   ├── platformio.ini        # PlatformIO 配置
│   └── CMakeLists.txt
├── glove_relay/              # Python FastAPI 中继服务器
│   └── glove_relay/
│       ├── main.py           # FastAPI 入口
│       ├── protocol_parser.py # V3/V4 协议解析
│       ├── st_gcn_mapper.py  # 骨架映射
│       ├── l2_inference.py   # L2 ST-GCN 推理
│       ├── nlp_corrector.py  # CSL NLP 纠正
│       ├── tts_engine.py     # TTS 引擎
│       └── websocket_server.py
├── glove_web/                # React + R3F 前端
│   └── src/
│       ├── components/
│       │   ├── HandSkeleton.jsx  # 3D 手部渲染
│       │   └── SensorDisplay.jsx # 传感器显示
│       ├── pages/
│       │   ├── LiveView.jsx      # 实时视图
│       │   └── DatasetCollection.jsx # 数据采集 (V4 新增)
│       └── hooks/
│           └── useWebSocket.js
├── glove_unity/              # Unity ms-MANO 渲染
│   └── Assets/Scripts/
│       ├── ManoMapper.cs     # 传感器→MANO 映射
│       └── WebSocketClient.cs
├── docs/                     # 文档
├── CLAUDE.md                 # 本文件
└── README.md
```

## 关键约束

1. **L1 模型:** <50K 参数, INT8 量化, <10ms 推理 (ESP32-S3)
2. **采样:** 100Hz, 帧时间 <8ms (80% 预算)
3. **I2C:** 所有操作必须 mutex 保护（共享总线）
4. **通信:** Protobuf V4 over UDP (firmware→relay), WebSocket JSON (relay→frontend)
5. **向后兼容:** Relay 必须同时支持 V3 和 V4 协议
6. **FreeRTOS:** Core 0 = 通信+推理, Core 1 = 传感器采样
7. **PlatformIO + ESP-IDF 5.x**

## 编码规范

- **C (固件):** ESP-IDF 风格, `snake_case`, `ESP_LOGI/W/E` 日志
- **Python (Relay):** PEP 8, type hints, dataclass
- **JavaScript (Web):** ES6+, React hooks, functional components
- **C# (Unity):** PascalCase, MonoBehaviour
- **Git:** Conventional Commits (`feat:`, `fix:`, `refactor:`, `docs:`)

## 重要文件说明

| 文件 | 说明 | 修改频率 |
|---|---|---|
| `sensor_manager.c` | 传感器采样主循环，Core 1 任务 | 低 |
| `kalman_filter.c` | 11 通道卡尔曼滤波 | 低 |
| `protocol_parser.py` | V3/V4 协议自动检测 | 低 |
| `st_gcn_mapper.py` | Flex% → 骨架节点映射 | 中 |
| `HandSkeleton.jsx` | 3D 手部渲染，flex→bone 角度 | 中 |
| `sensor_frame.proto` | 通信协议定义 | 低 |

## 数据集

- **格式:** CSV (timestamp, flex0-4, euler0-2, gyro0-2, label, user_id)
- **采集:** Web 工具 (`DatasetCollection.jsx`)
- **目标:** 200 手势 × 50 用户 × 50 次 = 500K 样本
- **增强:** 时间扭曲 (±20%), 噪声注入 (σ=0.01), 旋转 (±5°)

## 常用命令

```bash
# 固件编译
cd glove_firmware && pio run -e esp32s3

# 固件烧录
pio run -e esp32s3 -t upload

# 固件测试
pio test -e esp32s3

# Relay 启动
cd glove_relay && python -m glove_relay.main --port 5000

# 前端开发
cd glove_web && npm run dev

# Protobuf 生成
protoc --nanopb_out=glove_firmware/generated/ glove_firmware/proto/sensor_frame.proto
protoc --python_out=glove_relay/glove_relay/ glove_firmware/proto/sensor_frame.proto
```

## 已知问题 & TODO

- [ ] BNO085 6DOF 模式下 yaw 会漂移（可接受，手语不依赖绝对朝向）
- [ ] Flex 传感器有 ~5% 迟滞（可作为特征利用）
- [ ] 数据集采集是瓶颈，需要并行进行
- [ ] ST-GCN 骨架映射 Scheme C (混合) 需要调优
