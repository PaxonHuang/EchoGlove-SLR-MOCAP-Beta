# Claude Code Prompt 工程手册 V3.0

## Edge AI 数据手套项目 — 分阶段可执行提示词文档

| 项目信息 | 详情                                                             |
| -------- | ---------------------------------------------------------------- |
| 配套文档 | SOP SPEC PLAN V3.0                                               |
| MCU      | ESP32-S3-DevKitC-1 N16R8（双核 240MHz，8MB Flash，8MB PSRAM）    |
| AI 框架  | Edge Impulse / TFLite Micro / PyTorch / FastAPI                  |
| 渲染框架 | React 18 + Vite + R3F（MVP）/ Unity 2022 LTS + ms-MANO（L3 Pro） |
| 中继服务 | FastAPI + WebSockets（Python Relay）                             |
| 日期     | 2026-04-24                                                       |
| 版本     | Version 3.0                                                      |

---

> **V3.0 核心变更说明**
>
> - **完全移除** Tauri / Rust / cargo 等所有 Rust 相关代码和引用
> - **新增** React 18 + Vite + R3F 纯 Web 前端（P0.3 / P6.1 / P6.2）
> - **新增** Python Relay（FastAPI + WebSockets）作为统一数据枢纽（P0.2 / P5.1）
> - **新增** L1 模型池 + BaseModel 统一接口 + YAML 热切换（P3.3 / P3.4）
> - **新增** MS-TCN 浅层实验模型（P3.3）
> - **新增** 模型 Benchmark 自动化框架（P3.5B）
> - **新增** L1→L2 置信度驱动路由（P5.4）
> - **更新** ST-GCN 为真正的时空图卷积实现（P5.2）
> - **新增** PWA 配置（P6.2）
> - **新增** Unity XR Hands + ms-MANO 简要 Prompt（P6.3）
> - BLE 仅用于配网，前端通过 WiFi → Python Relay → WebSocket 获取数据

---

## 目录

| 章节         | 内容                            | Prompt 编号  |
| ------------ | ------------------------------- | ------------ |
| **1**  | 项目初始化与环境配置            | P0.1 — P0.3 |
| **2**  | Phase 1：HAL 与驱动层           | P1.1 — P1.4 |
| **3**  | Phase 2：信号处理               | P2.1 — P2.2 |
| **4**  | Phase 3：L1 边缘推理            | P3.1 — P3.5 |
| **5**  | Phase 3.5：模型 Benchmark       | P3.5B        |
| **6**  | Phase 4：通信协议               | P4.1 — P4.3 |
| **7**  | Phase 5：Python Relay + L2 推理 | P5.1 — P5.4 |
| **8**  | Phase 6：渲染层                 | P6.1 — P6.3 |
| **9**  | Phase 7：集成测试               | P7.1         |
| **10** | Bug 修复专用                    | P8.1 — P8.2 |

---

## 1 项目初始化与环境配置

本节包含项目初始化阶段的所有提示词，涵盖 PlatformIO 嵌入式项目、Python Relay 中继服务项目和 React 前端项目的创建与配置。在开始任何硬件驱动或算法开发之前，必须先完成本节的所有任务，确保三个子系统（边缘端、中继端、渲染端）的开发环境正确搭建。V3 新增 P0.3 初始化 React 前端项目，取代了 V2 的 Tauri 桌面端初始化。

---

### P0.1 初始化 PlatformIO 项目

**💡 上下文说明：** ESP32-S3-DevKitC-1 N16R8 版本需要正确配置 PSRAM 和内存模式，否则无法运行 TensorFlow Lite Micro 等内存密集型任务。本 Prompt 确保项目骨架和编译环境一次性正确建立。注意本项目不使用任何 Rust 或 Tauri 工具链。

**📝 Prompt文本（可直接复制粘贴到 Claude Code）：**

```
请初始化一个 PlatformIO 项目，目标平台为 ESP32-S3-DevKitC-1 (N16R8, 8MB Flash, 8MB PSRAM)。

要求：

1. 在 platformio.ini 中配置:
   - platform = espressif32 @ ^6.5.0
   - board = esp32-s3-devkitc-1-n8r8 (注意: 我们的实际板子是 N16R8 版本，8MB Flash + 8MB PSRAM)
   - framework = arduino
   - board_build.psram = enable (启用外部 PSRAM)
   - board_build.arduino.memory_type = qio_opi (PSRAM 模式)
   - monitor_speed = 115200
   - build_flags =
     -DBOARD_HAS_PSRAM
     -DCORE_DEBUG_LEVEL=3

2. 创建以下目录结构:
   - src/          (主源码: main.cpp)
   - lib/Sensors/  (传感器驱动: TCA9548A, TMAG5273, BNO085)
   - lib/Filters/  (滤波算法: KalmanFilter1D)
   - lib/Comms/    (通信模块: BLEManager, UDPTransmitter, Protobuf)
   - lib/Models/   (AI 模型: TFLiteInference, BaseModel 接口)
   - include/      (头文件)
   - test/         (测试)

3. 在每个 lib/ 子目录中放置一个空的 .h 占位文件，确保目录结构被 Git 追踪。

4. 验证编译通过，无错误。pio run 应输出 0 errors。

重要: 本项目是纯 C/C++ 嵌入式项目，不涉及任何 Rust、Tauri 或 cargo 工具链。
```

**🎯 预期输出：**

- platformio.ini 配置文件，包含了完整的 ESP32-S3 N16R8 PSRAM 配置
- 完整的目录结构，所有 lib/ 子目录已创建
- 编译输出 0 errors, 0 warnings（或仅有已知可忽略的 warning）

**✅ 验收标准：**

- [ ] platformio.ini 包含 `board_build.psram = enable`
- [ ] platformio.ini 包含 `-DBOARD_HAS_PSRAM` 编译标志
- [ ] 目录结构包含 `lib/Sensors/`, `lib/Filters/`, `lib/Comms/`, `lib/Models/`, `include/`, `test/`
- [ ] `pio run` 编译通过（0 errors）
- [ ] 项目中不存在任何 `.rs`、`Cargo.toml`、`tauri.conf.json` 文件

---

### P0.2 初始化 Python Relay 项目（V3 重写）

**💡 上下文说明：** Python Relay 是 V3 架构的核心枢纽，替代了 V2 中分散的多个服务。它承担四个职责：UDP 数据接收与 Protobuf 解析（端口 8888）、WebSocket JSON 推送（端口 8765）、L2 ST-GCN 推理、NLP 语法纠错与 TTS 语音合成。本 Prompt 建立规范的目录结构和依赖管理，并预置模型热切换的 YAML 配置文件。

**📝 Prompt文本（可直接复制粘贴到 Claude Code）：**

```
请初始化 Python Relay 中继服务项目，项目根目录为 glove_relay/。

要求：

1. 创建项目目录结构:
   glove_relay/
   ├── src/
   │   ├── __init__.py
   │   ├── main.py              (FastAPI 入口)
   │   ├── udp_server.py         (asyncio UDP 服务器, 端口 8888)
   │   ├── ws_server.py          (WebSocket 服务器, 端口 8765)
   │   ├── protobuf_parser.py    (Protobuf → JSON 转换)
   │   ├── models/
   │   │   ├── __init__.py
   │   │   ├── base_model.py     (BaseModel 统一接口)
   │   │   ├── stgcn_model.py    (L2 ST-GCN 模型)
   │   │   └── model_registry.py (模型注册表)
   │   ├── nlp/
   │   │   ├── __init__.py
   │   │   └── grammar_corrector.py (CSL→普通话语法纠错)
   │   ├── tts/
   │   │   ├── __init__.py
   │   │   └── tts_engine.py     (edge-tts 异步集成)
   │   └── utils/
   │       ├── __init__.py
   │       ├── config.py         (全局配置加载)
   │       └── logger.py         (日志工具)
   ├── data/
   │   └── gesture_labels.json   (46 类手势标签)
   ├── configs/
   │   ├── model_config.yaml     (L1/L2 模型配置, 支持热切换)
   │   └── relay_config.yaml     (中继服务器配置: 端口, CORS, etc.)
   ├── scripts/
   │   ├── data_collector.py     (数据采集工具)
   │   └── run_benchmark.py      (模型 Benchmark 脚本)
   ├── proto/
   │   └── glove_data.proto      (Protobuf 定义)
   ├── tests/
   │   └── __init__.py
   ├── requirements.txt
   ├── pyproject.toml
   └── README.md

2. 创建 requirements.txt:
   fastapi>=0.104.0
   uvicorn[standard]>=0.24.0
   websockets>=11.0
   torch>=2.0.0
   torchvision>=0.15.0
   protobuf>=4.24.0
   edge-tts>=6.1.0
   numpy>=1.24.0
   pydantic>=2.5.0
   pyyaml>=6.0.1
   scikit-learn>=1.3.0
   matplotlib>=3.7.0

3. 创建 configs/model_config.yaml:
   # L1/L2 模型配置文件 (支持热切换)
   active_l1_model: "cnn_attention_v2"
   active_l2_model: "stgcn_v1"
   l1_models:
     - name: "cnn_attention_v2"
       class_path: "src.models.cnn_attention.L1EdgeModel"
       weights_path: "data/models/l1_cnn_attention_v2_int8.tflite"
       params: 34000
       num_classes: 46
     - name: "ms_tcn_v1"
       class_path: "src.models.ms_tcn.MSTCNModel"
       weights_path: "data/models/l1_ms_tcn_v1_int8.tflite"
       params: 12000
       num_classes: 20
   l2_models:
     - name: "stgcn_v1"
       class_path: "src.models.stgcn_model.STGCNModel"
       weights_path: "data/models/l2_stgcn_v1.pt"
       params: 280000
       num_classes: 46

4. 创建 configs/relay_config.yaml:
   udp:
     host: "0.0.0.0"
     port: 8888
   websocket:
     host: "0.0.0.0"
     port: 8765
   cors:
     origins: ["http://localhost:5173", "http://localhost:3000"]
     allow_methods: ["*"]
     allow_headers: ["*"]
   inference:
     l1_confidence_threshold: 0.85
     l2_enabled: true
     debounce_frames: 5
     gesture_silence_ms: 100

5. 创建 pyproject.toml 配置 Python 3.9+ 兼容性:
   [project]
   name = "glove-relay"
   version = "3.0.0"
   requires-python = ">=3.9"
   description = "Edge AI Data Glove - Python Relay Server"

6. 创建 src/main.py 骨架:
   from fastapi import FastAPI
   from fastapi.middleware.cors import CORSMiddleware
   import uvicorn

   app = FastAPI(title="Glove Relay", version="3.0.0")
   # CORS 配置
   # UDP 服务器启动
   # WebSocket 端点
   # 生命周期管理 (startup/shutdown)

重要: 本项目不使用任何 Tauri、Rust 或 cargo 工具链。前端是纯 React Web 应用。
```

**🎯 预期输出：**

- 完整的项目目录结构，所有 `__init__.py` 文件就位
- requirements.txt 包含 FastAPI、WebSockets、PyTorch、edge-tts 等所有核心依赖
- model_config.yaml 预置了 1D-CNN+Attention 和 MS-TCN 两个 L1 模型配置
- relay_config.yaml 预置了 UDP 端口 8888、WebSocket 端口 8765、CORS 等配置
- src/main.py FastAPI 应用骨架，包含 CORS 配置和生命周期管理
- pyproject.toml 指定 Python 3.9+

**✅ 验收标准：**

- [ ] 所有目录存在且包含 `__init__.py`
- [ ] `pip install -r requirements.txt` 可成功安装所有依赖
- [ ] `python -c "import torch; print(torch.__version__)"` 正常输出
- [ ] `python -c "import fastapi; print(fastapi.__version__)"` 正常输出
- [ ] `python -c "import edge_tts"` 正常导入
- [ ] model_config.yaml 包含 `active_l1_model` 和 `l1_models` 配置块
- [ ] relay_config.yaml 包含 UDP 端口 8888 和 WebSocket 端口 8765
- [ ] 项目中不存在任何 `.rs`、`Cargo.toml`、`tauri.conf.json` 文件

---

### P0.3 初始化 React 前端项目（V3 新增）

**💡 上下文说明：** V3 移除了 Tauri 桌面端架构，改为纯 Web 方案。React 18 + Vite 提供极速的开发体验和构建速度，R3F（React Three Fiber）将 Three.js 3D 渲染能力封装为 React 组件，Zustand 轻量状态管理库负责管理 WebSocket 数据流，TailwindCSS 提供响应式原子化 CSS。本 Prompt 初始化完整的前端项目骨架，包括 WebSocket 客户端 Hook 和 PWA 配置。

**📝 Prompt文本（可直接复制粘贴到 Claude Code）：**

```
请初始化 React 前端项目，项目根目录为 glove_web/。

要求：

1. 使用 Vite 创建 React 18 + TypeScript 项目:
   npm create vite@latest glove_web -- --template react-ts
   cd glove_web && npm install

2. 安装核心依赖:
   npm install three @react-three/fiber @react-three/drei zustand
   npm install -D tailwindcss @tailwindcss/vite

3. 创建以下目录结构:
   glove_web/
   ├── public/
   │   ├── manifest.json        (PWA 配置)
   │   ├── icons/               (192x192, 512x512 图标占位)
   │   └── favicon.svg
   ├── src/
   │   ├── main.tsx             (入口, 挂载 App)
   │   ├── App.tsx              (根组件: 路由 + 布局)
   │   ├── index.css            (TailwindCSS 入口)
   │   ├── components/
   │   │   ├── Hand3D/
   │   │   │   ├── HandSkeleton.tsx    (21 关键点手部骨架)
   │   │   │   ├── FingerBone.tsx      (单根手指骨骼)
   │   │   │   └── HandCanvas.tsx      (R3F Canvas 容器)
   │   │   ├── Dashboard/
   │   │   │   ├── GestureResult.tsx   (手势识别结果面板)
   │   │   │   ├── SensorDataPanel.tsx (传感器数据实时面板)
   │   │   │   └── StatsPanel.tsx      (性能统计面板)
   │   │   ├── Settings/
   │   │   │   └── SettingsSidebar.tsx (设置侧边栏)
   │   │   └── Layout/
   │   │       ├── Header.tsx
   │   │       └── MobileNav.tsx
   │   ├── stores/
   │   │   ├── useSensorStore.ts  (Zustand: 传感器数据)
   │   │   ├── useGestureStore.ts (Zustand: 手势结果)
   │   │   └── useSettingsStore.ts(Zustand: 用户设置)
   │   ├── hooks/
   │   │   ├── useWebSocket.ts    (WebSocket 客户端 Hook)
   │   │   └── useHandAnimation.ts (手部动画驱动 Hook)
   │   ├── utils/
   │   │   ├── quaternion.ts      (四元数工具函数)
   │   │   └── constants.ts       (常量定义: 骨骼连接, 颜色, etc.)
   │   └── types/
   │       └── index.ts           (TypeScript 类型定义)
   ├── tailwind.config.js
   ├── tsconfig.json
   ├── vite.config.ts
   └── package.json

4. 实现 useWebSocket Hook (src/hooks/useWebSocket.ts):
   - 连接地址: ws://<RELAY_HOST>:8765
   - 自动重连: 指数退避, 最大重试 10 次, 初始延迟 1s
   - 消息解析: JSON.parse() 处理 incoming WebSocket 消息
   - 消息格式:
     {
       "timestamp": 1713950000000,
       "hall": [15 floats],        // 5 sensors × 3 axes (mT)
       "imu": [6 floats],          // 3 euler (deg) + 3 gyro (dps)
       "l1_gesture_id": 5,
       "l1_confidence": 0.92,
       "l2_gesture_id": null,      // 仅当 L1 置信度不足时
       "l2_confidence": null,
       "nlp_text": null,           // NLP 纠错后的文本
       "status": "STREAMING"       // STREAMING / MODEL_SWITCHING / ERROR
     }
   - 连接状态: 'connecting' | 'connected' | 'disconnected' | 'error'
   - 将接收到的数据更新到 Zustand store
   - 连接成功时 console.log('[WS] Connected to relay')
   - 断线时 console.warn('[WS] Disconnected, reconnecting...')

5. 实现 Zustand store (src/stores/useSensorStore.ts):
   import { create } from 'zustand';
   interface SensorState {
     hall: number[];
     imu: number[];
     timestamp: number;
     isStreaming: boolean;
     updateFromRelay: (data: any) => void;
   }
   export const useSensorStore = create<SensorState>((set) => ({
     hall: new Array(15).fill(0),
     imu: new Array(6).fill(0),
     timestamp: 0,
     isStreaming: false,
     updateFromRelay: (data) => set({
       hall: data.hall,
       imu: data.imu,
       timestamp: data.timestamp,
       isStreaming: true,
     }),
   }));

6. 配置 TailwindCSS (vite.config.ts):
   import tailwindcss from '@tailwindcss/vite';
   export default defineConfig({
     plugins: [react(), tailwindcss()],
   });

7. 配置响应式断点:
   - 移动端: < 768px (3D 视口堆叠在上方, 结果面板在下方)
   - 平板端: 768px - 1024px (左右分栏)
   - 桌面端: > 1024px (三栏布局: 3D + 结果 + 设置)

8. 创建 PWA manifest.json (public/manifest.json):
   {
     "name": "Edge AI 手语手套",
     "short_name": "GloveAI",
     "start_url": "/",
     "display": "standalone",
     "background_color": "#0f172a",
     "theme_color": "#3b82f6",
     "icons": [
       { "src": "/icons/icon-192.png", "sizes": "192x192", "type": "image/png" },
       { "src": "/icons/icon-512.png", "sizes": "512x512", "type": "image/png" }
     ]
   }

9. 在 index.html 中添加:
   <meta name="theme-color" content="#3b82f6">
   <link rel="manifest" href="/manifest.json">
   <meta name="apple-mobile-web-app-capable" content="yes">
   <meta name="apple-mobile-web-app-status-bar-style" content="black-translucent">

重要: 本项目是纯 Web 前端，不使用 Tauri、Rust、Electron 或任何桌面端框架。
WebSocket 连接到 Python Relay (ws://localhost:8765)，不直接连接 ESP32。
绝对不使用 Web Bluetooth API。
```

**🎯 预期输出：**

- 完整的 React 18 + Vite + TypeScript 项目结构
- useWebSocket Hook 实现自动重连和 JSON 消息解析
- Zustand store (useSensorStore, useGestureStore) 正确管理 WebSocket 数据流
- TailwindCSS 配置完成，支持响应式断点
- PWA manifest.json 和 meta 标签就位
- TypeScript 类型定义覆盖 WebSocket 消息格式

**✅ 验收标准：**

- [ ] `npm install` 成功安装所有依赖（无错误）
- [ ] `npm run dev` 成功启动开发服务器 (http://localhost:5173)
- [ ] `npm run build` 成功构建生产版本
- [ ] useWebSocket Hook 支持 auto-reconnect（指数退避）
- [ ] Zustand store 可被组件订阅并响应更新
- [ ] TailwindCSS 类在组件中可用
- [ ] 项目中不存在任何 `.rs`、`Cargo.toml`、`tauri.conf.json` 文件
- [ ] 项目中不存在 `navigator.bluetooth` 或 Web Bluetooth API 引用

---

## 2 Phase 1：HAL 与驱动层

本节覆盖所有底层硬件驱动开发，包括 I2C 多路复用器、3D 霍尔传感器、9 轴 IMU 以及 FreeRTOS 双核任务调度。这些驱动是整个系统的数据基石，任何 bug 都会传播到上层。特别注意 P1.4 中记录的已知 FreeRTOS 参数顺序 Bug。

> **⚠️ 已知 Bug 警告：FreeRTOS xTaskCreatePinnedToCore 参数顺序**
>
> 原始代码中将函数指针参数和句柄参数搞反了。错误写法：第一个参数传入了句柄变量 `TaskSensorReadHandle` 而不是函数指针 `Task_SensorRead`。此 Bug 会导致编译通过但运行时崩溃（`Guru Meditation Error: CacheAccessError`）。P8.1 提供了完整修复 Prompt。

---

### P1.1 TCA9548A I2C 多路复用器驱动

**💡 上下文说明：** 数据手套使用 5 个 TMAG5273 霍尔传感器，它们共享同一 I2C 总线（地址相同），必须通过 TCA9548A 多路复用器切换通道来逐一访问。如果多路复用器驱动存在 I2C 时序问题，将导致所有传感器数据采集失败。

**📝 Prompt文本（可直接复制粘贴到 Claude Code）：**

```
请实现 TCA9548A I2C 多路复用器驱动，文件路径: lib/Sensors/TCA9548A.h

要求：

1. 类名: TCA9548A

2. 构造函数: TCA9548A(TwoWire *wire, uint8_t addr = 0x70)

3. 核心方法:
   - bool begin() - 初始化, 验证 I2C 通信
   - bool selectChannel(uint8_t ch) - 选择通道 0-7
   - bool disableAll() - 禁用所有通道

4. I2C 速率: 400kHz (Wire.setClock(400000))

5. 错误处理: 每次 I2C 操作后检查返回值

6. 在 .cpp 中实现所有方法

7. 在 test/ 目录创建单元测试 test_tca9548a.cpp

已知约束:
- TCA9548A 地址为 0x70
- 需要在每次 TMAG5273 读取前调用 selectChannel()
- 通道切换不是互斥的: 必须先 disableAll() 再 selectChannel()
- 切换后等待 1ms 总线稳定时间
- 主总线上拉电阻 2.2kΩ, 子通道 4.7kΩ
```

**🎯 预期输出：**

- lib/Sensors/TCA9548A.h - 类声明
- lib/Sensors/TCA9548A.cpp - 方法实现
- test/test_tca9548a.cpp - 单元测试框架

**✅ 验收标准：**

- [ ] begin() 返回 true，I2C 通信验证通过
- [ ] selectChannel(0-7) 均返回 true
- [ ] disableAll() 正确关闭所有通道
- [ ] I2C 速率设置为 400kHz
- [ ] 每次 I2C 操作后有错误检查
- [ ] 通道切换包含 disableAll() → selectChannel() 两步操作
- [ ] 通道切换后包含 1ms 延迟

---

### P1.2 TMAG5273 3D 霍尔传感器驱动

**💡 上下文说明：** TMAG5273A1 是核心位置传感器，每根手指安装 1 个（共 5 个），通过检测 N52 磁铁的磁场变化来计算关节弯曲角度。12-bit 分辨率、±40mT 量程，需要 32 次平均来降低噪声。传感器通过 TCA9548A 通道 0-4 接入。

**📝 Prompt文本（可直接复制粘贴到 Claude Code）：**

```
请实现 TMAG5273A1 3D 霍尔传感器驱动，文件路径: lib/Sensors/TMG5273.h

要求：

1. 类名: TMAG5273

2. 构造函数: TMAG5273(TwoWire *wire, uint8_t mux_channel, TCA9548A *mux)

3. 核心方法:
   - bool begin() - 初始化传感器
     - 设置 CONV_MODE 为触发模式 (0x01)
     - 设置 CONFIG 寄存器: 感应范围 ±40mT
     - 设置 AVG 为 32 次平均 (降低噪声)
   - bool readXYZ(float &x, float &y, float &z) - 读取 12-bit 原始数据并转换为 mT
   - bool triggerConversion() - 触发一次 ADC 转换

4. 寄存器定义:
   - DEVICE_ID = 0x00 (期望值 0x11)
   - X_MSB = 0x01, X_LSB = 0x02
   - Y_MSB = 0x03, Y_LSB = 0x04
   - Z_MSB = 0x05, Z_LSB = 0x06
   - CONV_MODE = 0x0D
   - CONFIG = 0x0E

5. 数据转换: 12-bit signed, LSB = 0.048mT

6. 关键: 每次读取前必须通过 TCA9548A 选择正确通道

已知约束:
- 5 个传感器分别连接 TCA9548A 通道 0-4
- 磁铁为 N52 4x2mm 圆盘, 安装在近节指骨
- 传感器安装在 MCP 关节背侧
```

**🎯 预期输出：**

- lib/Sensors/TMG5273.h - 类声明，含寄存器常量
- lib/Sensors/TMG5273.cpp - 完整驱动实现
- readXYZ() 正确进行 12-bit signed → mT 转换
- 每次读取前调用 mux->selectChannel()

**✅ 验收标准：**

- [ ] begin() 通过 DEVICE_ID 验证（期望值 0x11）
- [ ] CONV_MODE 设为触发模式，AVG 设为 32 次平均
- [ ] readXYZ() 返回 3 轴磁场值（mT）
- [ ] 5 个实例分别对应通道 0-4 且互不干扰
- [ ] 12-bit signed 数据正确转换（负值处理）

---

### P1.3 BNO085 9 轴 IMU 驱动

**💡 上下文说明：** BNO085 提供手部整体姿态信息（四元数 + 陀螺仪），安装在手背中心。使用 Hillcrest Labs 的 SH-2 传感器融合算法，输出 Game Rotation Vector（不受磁场干扰，适合室内使用）。必须正确实现 SH-2 协议才能获取数据。

**📝 Prompt文本（可直接复制粘贴到 Claude Code）：**

```
请实现 BNO085 9 轴 IMU 驱动，文件路径: lib/Sensors/BNO085.h

要求：

1. 类名: BNO085

2. 构造函数: BNO085(TwoWire *wire, uint8_t addr = 0x4A)

3. 核心方法:
   - bool begin() - 初始化
     - 软复位
     - 启用 SH2_GAME_ROTATION_VECTOR 报告 (100Hz)
     - 启用 SH2_GYROSCOPE_CALIBRATED 报告 (100Hz)
   - bool getQuaternion(float &w, float &x, float &y, float &z)
   - bool getGyro(float &x, float &y, float &z)
   - bool getEuler(float &roll, float &pitch, float &yaw)

4. SH-2 协议实现:
   - 报告头解析 (SHTP Header: 4 bytes)
   - 产品 ID 查询
   - 特征报告使能

5. 四元数到欧拉角转换 (内置方法)

已知约束:
- BNO085 安装在手背中心, 必须与手掌平面平行
- 使用硬件传感器融合, 输出 Game Rotation Vector (无磁场干扰)
- I2C 地址: 0x4A (SDO 接 GND)
- 必须配备 0.1uF + 10uF 去耦电容
- INT 引脚连接 GPIO 21, 用于数据就绪中断
```

**🎯 预期输出：**

- lib/Sensors/BNO085.h - 类声明
- lib/Sensors/BNO085.cpp - SH-2 协议实现
- getQuaternion() 返回归一化四元数 (w,x,y,z)
- getEuler() 返回 (roll, pitch, yaw) 单位为度

**✅ 验收标准：**

- [ ] begin() 成功软复位并启用 Game Rotation Vector 报告
- [ ] SH-2 报告头解析正确（4字节头 + 可变长度 payload）
- [ ] getQuaternion() 返回值满足 w²+x²+y²+z² ≈ 1.0
- [ ] getEuler() 输出范围: roll[-180,180], pitch[-90,90], yaw[-180,180]
- [ ] 数据输出频率稳定在 100Hz

---

### P1.4 FreeRTOS 双核任务调度（含已知 Bug 修复）

**💡 上下文说明：** ESP32-S3 拥有双核（Core 0: Protocol CPU, Core 1: Application CPU）。本 Prompt 利用 FreeRTOS 将传感器采集（100Hz 实时性要求）放在 Core 1，推理和通信放在 Core 0，实现真正的并行处理。⚠️ 原始代码存在 xTaskCreatePinnedToCore 参数顺序 Bug，必须修复。

> **⚠️ 关键 Bug 修复：xTaskCreatePinnedToCore 参数顺序**
>
> 错误写法: `xTaskCreatePinnedToCore(TaskSensorReadHandle, "SensorRead", 4096, NULL, 3, &TaskSensorReadHandle, 1)`
>
> 正确写法: `xTaskCreatePinnedToCore(Task_SensorRead, "SensorRead", 4096, NULL, 3, &TaskSensorReadHandle, 1)`
>
> 参数顺序: `(函数指针, 任务名, 栈大小, 参数, 优先级, 句柄指针, 核心编号)`
>
> 错误后果: 编译通过但运行时 `Guru Meditation Error: Core 0 panic'ed (CacheAccessError)`

**📝 Prompt文本（可直接复制粘贴到 Claude Code）：**

```
请实现基于 FreeRTOS 的双核任务调度框架，文件路径: src/main.cpp

⚠️ 已知 Bug 修复: 原始代码 xTaskCreatePinnedToCore 参数顺序错误!
错误写法: xTaskCreatePinnedToCore(TaskSensorReadHandle, "SensorRead", 4096, NULL, 3, &TaskSensorReadHandle, 1)
正确写法: xTaskCreatePinnedToCore(Task_SensorRead, "SensorRead", 4096, NULL, 3, &TaskSensorReadHandle, 1)
参数顺序: (函数指针, 任务名, 栈大小, 参数, 优先级, 句柄指针, 核心编号)

要求：

1. 定义三个 FreeRTOS 任务:

   - Task_SensorRead (Core 1, 优先级 3, 栈 4096):
     100Hz 采样循环, 读取所有传感器数据
     使用 vTaskDelayUntil 确保精确 100Hz
     采样顺序: BNO085 → TCA9548A ch0-4 (5路 TMAG5273)

   - Task_Inference (Core 0, 优先级 2, 栈 8192):
     从队列接收传感器数据
     填充 30 帧滑动窗口
     执行 L1 推理 (TFLite Micro)
     支持模型热切换 (BaseModel 接口)

   - Task_Comms (Core 0, 优先级 1, 栈 8192):
     BLE/WiFi 通信
     Protobuf 序列化与 UDP 发送 (100Hz, 目标端口 8888)

2. FreeRTOS 队列:
   - inferenceQueue: 传感器数据 → 推理任务 (深度 10)
   - dataQueue: 推理结果 → 通信任务 (深度 10)

3. 数据结构:
   - FullDataPacket: timestamp + SensorData(hall_xyz[15] + euler[3] + gyro[3]) + flex[5]
   - InferenceResult: FullDataPacket + gesture_id + confidence

4. 在 setup() 中创建任务和队列。

5. 添加编译期静态断言验证函数指针:
   static_assert(std::is_function_v<decltype(Task_SensorRead)>,
                 "Task_SensorRead must be a function pointer");

6. 重要: 不要使用任何 Rust 或 Tauri 相关代码。
```

**🎯 预期输出：**

- src/main.cpp - 包含正确的任务创建代码（Bug 已修复）
- 数据结构 FullDataPacket 和 InferenceResult 定义
- 三个 FreeRTOS 任务函数实现
- 两个 FreeRTOS 队列创建和使用
- 编译期静态断言验证

**✅ 验收标准：**

- [ ] xTaskCreatePinnedToCore 第 1 个参数为函数指针 `Task_SensorRead`（非句柄变量）
- [ ] Task_SensorRead 使用 vTaskDelayUntil 实现 100Hz 精确定时
- [ ] inferenceQueue 和 dataQueue 深度为 10
- [ ] 编译通过，无错误
- [ ] 存在 static_assert 验证函数指针
- [ ] （详细修复 Prompt 见 P8.1）

---

## 3 Phase 2：信号处理

本节包含原始传感器信号的滤波处理和数据采集标注工具。卡尔曼滤波器用于降低霍尔传感器的噪声（12-bit 分辨率 + 磁场干扰），数据采集工具用于构建训练数据集，是后续所有 AI 模型训练的基础。V3 更新了数据采集工具，支持通过 Python Relay 的 UDP 端口接收数据。

---

### P2.1 1D 卡尔曼滤波器

**💡 上下文说明：** TMAG5273 霍尔传感器虽然设置了 32 次平均，但磁场信号仍受环境干扰（电机、金属等）。卡尔曼滤波器在已知过程噪声和测量噪声模型的前提下，能提供最优估计。15 路霍尔信号各需要一个独立滤波器实例。

**📝 Prompt文本（可直接复制粘贴到 Claude Code）：**

```
请实现 1D 卡尔曼滤波器类，文件路径: lib/Filters/KalmanFilter1D.h

要求：

1. 类名: KalmanFilter1D

2. 构造函数: KalmanFilter1D(float process_noise = 0.01, float measurement_noise = 0.1)

3. 核心方法:
   - float update(float measurement) - 输入测量值, 返回滤波后估计值
   - void reset() - 重置滤波器状态

4. 内部状态: 估计值 x, 估计误差 P, 卡尔曼增益 K

5. 对 15 路霍尔信号各实例化一个 KalmanFilter1D

关键参数调优指导:
- process_noise (Q): 传感器固有噪声水平, 起始建议 0.01
- measurement_noise (R): 测量不确定性, 起始建议 0.1
- 如果滤波器响应太慢, 增大 Q 或减小 R
- 如果输出抖动太大, 减小 Q 或增大 R

实现要求:
- 头文件-only 实现 (header-only)
- 使用 float 类型 (嵌入式端效率优先)
- 首次调用 update() 时使用测量值初始化 x (不使用零先验)
```

**🎯 预期输出：**

- lib/Filters/KalmanFilter1D.h - 模板类实现（头文件-only）
- 包含完整的状态预测和更新方程
- 构造函数支持自定义 Q 和 R 参数

**✅ 验收标准：**

- [ ] update() 返回滤波后的估计值
- [ ] reset() 能将内部状态清零
- [ ] 首次调用 update() 时正确初始化（不使用零先验）
- [ ] 15 个独立实例可并行工作，互不干扰
- [ ] Q=0.01, R=0.1 默认参数下输出平滑

---

### P2.2 数据采集与标注工具

**💡 上下文说明：** AI 模型训练需要大量标注数据。本工具支持通过 BLE 串口或 UDP（连接 Python Relay）两种方式实时接收传感器数据，录制手势标签，自动保存为 NPY 格式（兼容 Edge Impulse CSV 格式）。每个手势建议录制 100-200 个样本以确保模型泛化能力。

**📝 Prompt文本（可直接复制粘贴到 Claude Code）：**

```
请实现数据采集 Python 脚本，文件路径: glove_relay/scripts/data_collector.py

要求：

1. 支持三种数据源:
   - BLE 串口 (pyserial, /dev/ttyUSB0 或 COM口)
   - UDP socket (连接 Python Relay 的 UDP 端口 8888, 接收 Protobuf)
   - CSV 文件回放 (用于离线调试)

2. 功能:
   - 实时显示传感器数据波形 (使用 matplotlib 或 rich)
   - 录制指定手势标签的数据
   - 自动保存为 NPY 格式: shape=(N, 30, 21) (样本数, 窗口长度, 特征数)
   - 每个手势建议录制 100-200 个样本
   - 滑动窗口自动切分: 30 帧一窗, 步长 1

3. 手势标签管理:
   - 预定义 46 类手势标签列表 (中国手语 CSL)
   - 支持自定义标签
   - 按标签自动组织文件目录: data/recordings/<label_name>/

4. 数据增强 (可选):
   - 随机时移 ±5 帧
   - 高斯噪声 σ=0.01
   - 时间遮蔽 (随机零化 10% 帧)

5. 使用 edge-impulse-data-forwarder 兼容的 CSV 格式作为备选输出

6. 注意: UDP 数据源接收的是 Protobuf 格式, 需要 glove_data_pb2 解析
```

**🎯 预期输出：**

- glove_relay/scripts/data_collector.py - 完整的数据采集脚本
- 46 类预定义手势标签
- 实时波形显示界面
- NPY 和 CSV 双格式输出
- 数据增强管道

**✅ 验收标准：**

- [ ] BLE 串口和 UDP 两种数据源可切换
- [ ] 录制的 NPY 文件 shape = (N, 30, 21)
- [ ] CSV 输出兼容 edge-impulse-data-forwarder
- [ ] 数据增强功能可开关
- [ ] 标签管理支持新增/删除/列表

---

## 4 Phase 3：L1 边缘推理

L1 边缘推理是数据手套的实时手势识别层，运行在 ESP32-S3 上。V3 新增了模型池（Model Pool）概念，包含两个候选模型：1D-CNN+Attention（主力）和 MS-TCN（实验性），通过统一的 BaseModel 接口管理，支持 YAML 配置文件驱动的运行时热切换。

> **ℹ️ V3 双路线策略 + 模型池**
>
> Edge Impulse 路线（P3.1）适合快速验证 feasibility，2-3 天内可得到可用的 L1 模型。PyTorch 路线（P3.2）提供更高的定制化能力和量化精度控制。模型池（P3.4）允许在运行时通过 YAML 配置文件切换模型，无需重启设备。建议先走 EI 路线验证数据质量，再切换到 PyTorch 路线优化精度，最后通过 Benchmark（P3.5B）对比选择最优模型。

---

### P3.1 Edge Impulse 数据采集与训练（MVP 路线）

**💡 上下文说明：** Edge Impulse 提供端到端的边缘 AI 开发平台，从数据采集、模型训练到 C++ 库导出一气呵成。对于 MVP 阶段，这是最快的路线：无需手动实现训练管道，直接在浏览器中完成所有工作。输出为 Arduino Library，PlatformIO 可直接引用。

**📝 Prompt文本（可直接复制粘贴到 Claude Code）：**

```
请编写 Edge Impulse 数据采集和模型训练指南:

一、数据采集:

1. 安装 edge-impulse-data-forwarder:
   pip install edge-impulse-data-forwarder

2. 在 ESP32 固件中实现串口 CSV 输出:
   - 波特率: 115200
   - 格式: "hall_1,hall_2,...,hall_15,euler_r,euler_p,euler_y,gyro_x,gyro_y,gyro_z\n"
   - 频率: 100Hz
   - 每行一个时间步

3. 运行 forwarder 连接串口, 创建项目

4. 在 EI Studio 中为每个手势类别录制数据 (每类 ≥100 样本)

二、模型训练 (EI Studio):

1. 创建 Impulse:
   - 输入: 21 个传感器特征
   - 窗口大小: 3000ms (30帧 × 100ms)
   - 窗口增量: 100ms

2. 处理块:
   - Raw Data (直接使用原始数据)
   - 或添加 Filter (低通滤波预处理)

3. 学习块:
   - Classification (Keras)
   - 建议: 1D CNN + Attention
   - 训练周期: 100-200 epochs
   - 学习率: 0.001 (Adam)

4. 启用 EON Tuner 优化

三、部署导出:

1. 导出为 Arduino Library

2. 解压到 PlatformIO 项目的 lib/ 目录

3. 在 main.cpp 中:
   #include <edge_impulse_inferencing.h>
   实现 signal_t callback 和 run_classifier() 调用

四、集成到 BaseModel 接口:
   创建 EIModel 类, 继承 BaseModel, 封装 EI 推理调用
```

**🎯 预期输出：**

- 完整的 Edge Impulse 使用指南文档
- ESP32 端串口 CSV 输出代码片段
- EI Studio 配置参数清单
- C++ 集成代码模板
- BaseModel 接口适配示例

**✅ 验收标准：**

- [ ] EI 项目中每类手势 ≥100 样本
- [ ] Impulse 窗口大小 3000ms / 增量 100ms
- [ ] 模型训练准确率 >90%（验证集）
- [ ] Arduino Library 导出成功并集成到 PlatformIO
- [ ] ESP32 端推理延迟 <5ms

---

### P3.2 PyTorch 1D-CNN+Attention 模型训练

**💡 上下文说明：** 当 Edge Impulse 的模型精度不能满足需求时，使用自定义 PyTorch 模型可以获得更高的精度和更灵活的架构控制。1D-CNN 提取局部时序特征，Attention 机制聚焦关键时间步。训练完成后通过 QAT（量化感知训练）转为 INT8 格式以适配 ESP32-S3 的计算资源。本 Prompt 训练的模型将注册到模型池中（P3.4），通过 YAML 配置进行热切换。

**📝 Prompt文本（可直接复制粘贴到 Claude Code）：**

```
请实现 PyTorch 1D-CNN+Attention 模型训练脚本，文件路径: glove_relay/src/models/l1_cnn_attention.py

要求：

1. 模型架构 L1EdgeModel (继承 BaseModel):
   - 输入: (Batch, 21, 30) - 21 特征 × 30 帧 (注意: 特征在 dim=1)
   - Block1: Conv1d(21→32, kernel=5, padding=2) + BatchNorm1d + ReLU + MaxPool(2)
   - Block2: Conv1d(32→64, kernel=3, padding=1) + BatchNorm1d + ReLU + MaxPool(2)
   - Block3: Conv1d(64→128, kernel=3, padding=1) + BatchNorm1d + ReLU
   - TemporalAttention(128):
     * W_h: Linear(128, 32)
     * W_e: Linear(128, 32)
     * v: Linear(32, 1)
     * score = v(tanh(W_h(h) + W_e(e_mean)))
     * alpha = softmax(score)
     * context = sum(alpha * h)
   - FC: Linear(128, num_classes)

   模型参数量约 34K:
   | 层 | 输出通道 | 参数量 |
   |----|---------|-------|
   | Conv1d(21,32,k=5) | 32 | 3,392 |
   | Conv1d(32,64,k=3) | 64 | 6,208 |
   | Conv1d(64,128,k=3) | 128 | 24,704 |
   | TemporalAttention | 128 | 4,192 |
   | FC(128,46) | 46 | 5,934 |
   | **合计** | | **~34K** |

2. 训练配置:
   - 优化器: AdamW(lr=1e-3, weight_decay=1e-4)
   - 调度器: CosineAnnealingLR(T_max=200)
   - 损失: CrossEntropyLoss(label_smoothing=0.1)
   - Batch Size: 64
   - Epochs: 200 (早停 patience=20)
   - 数据增强: 随机时移, 高斯噪声, Mixup(alpha=0.2)

3. INT8 量化导出:
   - 使用 torch.quantization.prepare_qat
   - QAT 训练 3-5 个 epoch
   - 转换为 TFLite INT8
   - 使用 repr_dataset 校准 (100-500 样本)
   - 验证量化后精度下降 <2%

4. 生成 model_data.h:
   - xxd -i model_quant.tflite > model_data.h
   - 验证文件大小 <100KB

5. 训练脚本接受命令行参数:
   --data_dir, --epochs, --batch_size, --lr, --num_classes, --output_dir
```

**🎯 预期输出：**

- glove_relay/src/models/l1_cnn_attention.py - 完整训练脚本
- L1EdgeModel 类定义（含 TemporalAttention）
- 训练循环（含早停、学习率调度、数据增强）
- QAT 量化管道
- TFLite INT8 导出脚本
- model_data.h 生成命令

**✅ 验收标准：**

- [ ] 训练集准确率 >98%，验证集准确率 >92%
- [ ] INT8 量化后准确率 >90%（精度下降 <2%）
- [ ] model_quant.tflite 文件 <100KB
- [ ] model_data.h 正确生成并可被 ESP32 编译
- [ ] 在 ESP32-S3 上推理延迟 <3ms

---

### P3.3 MS-TCN 浅层实验模型（V3 新增）

**💡 上下文说明：** MS-TCN（Multi-Stage Temporal Convolutional Network）是一种基于膨胀卷积（Dilated Convolution）的时序模型，相比 1D-CNN+Attention 更轻量，参数量更少（约 12K vs 34K）。作为实验性模型加入 L1 模型池，通过 Benchmark（P3.5B）与 1D-CNN+Attention 进行对比。浅层设计（仅 3 个 dilated conv 层）确保在 ESP32-S3 上的推理延迟同样满足 <3ms 要求。

**📝 Prompt文本（可直接复制粘贴到 Claude Code）：**

```
请实现浅层 MS-TCN 模型训练脚本，文件路径: glove_relay/src/models/l1_ms_tcn.py

要求：

1. 模型架构 MSTCNModel (继承 BaseModel):
   - 输入: (Batch, 21, 30) - 21 特征 × 30 帧
   - Stage 1: DilatedConv1d(21→32, kernel=3, dilation=1, padding=1) + ReLU + Dropout(0.1)
   - Stage 2: DilatedConv1d(32→64, kernel=3, dilation=2, padding=2) + ReLU + Dropout(0.1)
   - Stage 3: DilatedConv1d(64→64, kernel=3, dilation=4, padding=4) + ReLU + Dropout(0.1)
   - 每个阶段包含残差连接 (Residual Connection):
     * 如果输入通道 != 输出通道, 使用 1x1 Conv1d 调整维度
     * output = F.relu(residual + conv(x))
   - Global Average Pooling: AdaptiveAvgPool1d(1)
   - FC: Linear(64, num_classes)

   模型参数量约 12K:
   | 层 | 参数量 |
   |----|-------|
   | DilatedConv(21,32,d=1) + 1x1 | ~1.4K |
   | DilatedConv(32,64,d=2) + 1x1 | ~5.4K |
   | DilatedConv(64,64,d=4) | ~12.4K (含残差) |
   | FC(64,46) | ~3K |
   | **合计** | **~12K** |

2. 训练配置 (与 P3.2 保持一致以便对比):
   - 优化器: AdamW(lr=1e-3, weight_decay=1e-4)
   - 调度器: CosineAnnealingLR(T_max=200)
   - 损失: CrossEntropyLoss(label_smoothing=0.1)
   - Batch Size: 64
   - Epochs: 200 (早停 patience=20)

3. INT8 量化导出 (与 P3.2 相同管道):
   - QAT 3-5 epochs
   - TFLite INT8
   - 验证量化后精度下降 <2%

4. 目标:
   - 推理延迟 <2ms (比 1D-CNN+Attention 更快)
   - 验证集准确率 >85% (可能低于 1D-CNN+Attention)
   - 模型大小 <30KB (比 1D-CNN+Attention 更小)

5. 训练脚本接受命令行参数:
   --data_dir, --epochs, --batch_size, --lr, --num_classes, --output_dir
```

**🎯 预期输出：**

- glove_relay/src/models/l1_ms_tcn.py - 完整训练脚本
- MSTCNModel 类定义（含残差连接）
- 训练循环（与 P3.2 共享训练逻辑）
- QAT 量化管道
- TFLite INT8 导出

**✅ 验收标准：**

- [ ] 模型参数量约 12K（±2K）
- [ ] 3 个 dilated conv 层，dilation=[1, 2, 4]
- [ ] 每层包含残差连接
- [ ] 训练集准确率 >95%，验证集准确率 >85%
- [ ] INT8 量化后模型 <30KB
- [ ] 推理延迟 <2ms（ESP32-S3）

---

### P3.4 BaseModel 统一接口 + 模型热切换（V3 新增）

**💡 上下文说明：** 模型池（Model Pool）架构允许在 ESP32-S3 上运行时切换不同的 L1 模型，无需重新编译固件。所有模型通过统一的 BaseModel 抽象接口注册和管理。Python 端和 C++ 端各有一套 BaseModel 实现：Python 端用于训练和 Benchmark，C++ 端用于嵌入式部署。YAML 配置文件指定当前激活的模型，切换时自动释放旧模型资源并加载新模型。

**📝 Prompt文本（可直接复制粘贴到 Claude Code）：**

```
请实现 BaseModel 统一接口和模型注册表，包含 Python 端和 C++ 端。

一、Python 端 (glove_relay/src/models/base_model.py):

1. 抽象基类 BaseModel:
   from abc import ABC, abstractmethod

   class BaseModel(ABC):
       @abstractmethod
       def forward(self, x: torch.Tensor) -> torch.Tensor:
           """前向传播, 输入 (B, 21, 30), 输出 (B, num_classes)"""
           pass

       @abstractmethod
       def predict(self, x: torch.Tensor) -> tuple[int, float]:
           """推理, 返回 (gesture_id, confidence)"""
           pass

       @abstractmethod
       def get_config(self) -> dict:
           """返回模型配置: name, num_params, num_classes, input_shape"""
           pass

       @abstractmethod
       def get_model_info(self) -> dict:
           """返回模型详细信息: FLOPs, latency_ms, model_size_kb"""
           pass

2. 模型注册表 (glove_relay/src/models/model_registry.py):
   _registry: dict[str, type[BaseModel]] = {}

   def register_model(name: str):
       """装饰器: 注册模型类"""
       def decorator(cls):
           _registry[name] = cls
           return cls
       return decorator

   def create_model(config_path: str) -> BaseModel:
       """工厂函数: 从 YAML 配置创建模型实例"""
       # 加载 model_config.yaml
       # 根据 active_l1_model 查找 class_path
       # 动态导入并实例化
       # 加载权重
       pass

   def list_models() -> list[str]:
       """返回所有已注册的模型名称"""
       return list(_registry.keys())

3. 运行时切换:
   def switch_model(config_path: str, new_model_name: str) -> BaseModel:
       """切换激活的模型"""
       # 保存当前模型状态
       # 释放旧模型 GPU 内存
       # 加载新模型
       # 更新 YAML 配置
       pass

4. 在 l1_cnn_attention.py 中使用装饰器注册:
   @register_model("cnn_attention_v2")
   class L1EdgeModel(BaseModel):
       ...

5. 在 l1_ms_tcn.py 中使用装饰器注册:
   @register_model("ms_tcn_v1")
   class MSTCNModel(BaseModel):
       ...

二、C++ 端 (lib/Models/BaseModel.h):

class BaseModel {
public:
    virtual bool init(const uint8_t* model_data, size_t model_size) = 0;
    virtual int preprocess(float* input, const SensorFrame* frames, int num_frames) = 0;
    virtual int infer(const float* input, float* output) = 0;
    virtual int postprocess(const float* output, GestureResult* result) = 0;
    virtual ModelInfo get_model_info() const = 0;
    virtual void cleanup() = 0;
    virtual ~BaseModel() = default;
};

// 模型注册表
class ModelRegistry {
public:
    static ModelRegistry& instance();
    void registerModel(const char* name, BaseModel* model);
    BaseModel* getActiveModel();
    bool switchModel(const char* name);
private:
    std::map<std::string, BaseModel*> models_;
    BaseModel* active_model_ = nullptr;
    std::string active_name_;
};

三、YAML 配置文件 (configs/model_config.yaml):
   active_l1_model: "cnn_attention_v2"
   l1_models:
     - name: "cnn_attention_v2"
       class_path: "src.models.l1_cnn_attention.L1EdgeModel"
       weights_path: "data/models/l1_cnn_attention_v2_int8.tflite"
       params: 34000
       num_classes: 46
     - name: "ms_tcn_v1"
       class_path: "src.models.l1_ms_tcn.MSTCNModel"
       weights_path: "data/models/l1_ms_tcn_v1_int8.tflite"
       params: 12000
       num_classes: 20
```

**🎯 预期输出：**

- Python 端 BaseModel 抽象基类 + 模型注册表 + 工厂函数
- C++ 端 BaseModel 虚基类 + ModelRegistry 单例
- YAML 配置文件驱动模型选择
- L1EdgeModel 和 MSTCNModel 已注册到注册表
- 运行时切换功能实现

**✅ 验收标准：**

- [ ] Python 端 `create_model("configs/model_config.yaml")` 返回正确的模型实例
- [ ] `switch_model("ms_tcn_v1")` 可在不重启的情况下切换模型
- [ ] 切换后旧模型 GPU 内存已释放
- [ ] C++ 端 ModelRegistry::switchModel() 正确清理旧模型 Arena
- [ ] YAML 配置更新后自动触发切换
- [ ] 两个模型均可独立训练和推理

---

### P3.5 TFLite Micro 集成（C++ 端）

**💡 上下文说明：** 将 PyTorch 导出的 TFLite INT8 模型集成到 ESP32-S3 固件中。TFLite Micro 是 Google 为微控制器设计的轻量推理引擎，支持 ESP32-S3 的 AI 向量指令加速。关键挑战是内存管理——Arena 必须分配到 PSRAM 以容纳 80KB 的运行时内存。V3 中 TFLiteInference 类需要实现 BaseModel 接口以支持热切换。

**📝 Prompt文本（可直接复制粘贴到 Claude Code）：**

```
请实现 TFLite Micro 推理集成代码，文件路径: src/TFLiteInference.h

要求：

1. 类 TFLiteInference 继承 BaseModel (lib/Models/BaseModel.h):
   class TFLiteInference : public BaseModel {
       // 实现 BaseModel 接口的所有方法
   };

2. 初始化 (init):
   - 加载 model_data.h 中的模型数据
   - 创建 AllOpsResolver
   - 创建 Interpreter (arena_size=80KB, 使用 PSRAM)

3. 推理方法 (infer):
   - int8_t runInference(float* input_window, int window_size, float* confidence)
   - 输入: 30×21 = 630 个 float 特征
   - 输出: gesture_id 和 confidence

4. 性能优化:
   - 使用 ESP32-S3 的 AI 向量指令
   - 将推理 Arena 分配到 PSRAM (CONFIG_SPIRAM, mallocPSRAM)
   - 栈大小 ≥8192 字节

5. 集成到 FreeRTOS Task_Inference:
   - 从队列获取数据
   - 填充滑动窗口
   - 通过 ModelRegistry 获取当前激活模型
   - 调用 runInference
   - 结果入队

6. 模型热切换支持 (cleanup):
   - 释放 Interpreter 和 Arena
   - 重置内部状态
   - 准备加载新模型

关键约束:
- 使用 #include "tensorflow/lite/micro/all_ops_resolver.h"
- 使用 #include "tensorflow/lite/micro/micro_interpreter.h"
- Arena 大小建议 80KB (含模型 38KB + 运行时)
- 使用 ESP-IDF 的 heap_caps_malloc(arena_size, MALLOC_CAP_SPIRAM) 分配 PSRAM
```

**🎯 预期输出：**

- src/TFLiteInference.h - 类声明
- src/TFLiteInference.cpp - TFLite Micro 集成实现
- Arena 分配到 PSRAM 的代码
- 与 FreeRTOS Task_Inference 的集成代码
- BaseModel 接口的完整实现

**✅ 验收标准：**

- [ ] Arena 成功分配到 PSRAM（使用 heap_caps_malloc + MALLOC_CAP_SPIRAM）
- [ ] runInference() 输入 630 个 float，输出 gesture_id 和 confidence
- [ ] 推理延迟 <3ms（使用 ESP32-S3 AI 指令）
- [ ] 编译后固件大小增量 <200KB
- [ ] 与 Task_Inference 正确集成，队列数据流无阻塞
- [ ] cleanup() 正确释放所有资源
- [ ] 项目中不存在任何 Rust 相关代码

---

## 5 Phase 3.5：模型 Benchmark（V3 新增）

本节包含统一的模型评估框架，在 L1 模型部署之前对所有候选模型进行系统性的对比评估。评估指标包括准确率、FLOPs、推理延迟、参数量和模型大小。Benchmark 结果用于数据驱动的模型选择决策。

---

### P3.5B 模型 Benchmark 自动化脚本（V3 新增）

**💡 上下文说明：** L1 模型池中有多个候选模型（1D-CNN+Attention、MS-TCN 等），需要客观的数据驱动的标准来决定最终部署哪个模型。仅看 Top-1 准确率是不够的——嵌入式部署还需要考虑计算开销、内存占用和推理延迟。本 Prompt 实现自动化 Benchmark 脚本，支持 PyTorch 和 TFLite 两种模型格式，输出 CSV 表格和 Markdown 报告。

**📝 Prompt文本（可直接复制粘贴到 Claude Code）：**

```
请实现模型 Benchmark 自动化脚本，文件路径: glove_relay/scripts/run_benchmark.py

要求：

1. 加载模型:
   - 从 configs/model_config.yaml 读取所有注册模型
   - 支持 PyTorch (.pt) 和 TFLite (.tflite) 两种格式
   - 自动加载对应权重

2. 评估指标:
   - Top-1 准确率 (%)
   - Top-5 准确率 (%)
   - FLOPs (使用 thop 或 fvcore)
   - 推理延迟 (ms): 在目标设备上运行 100 次, 取 mean ± std
   - 参数量 (parameters count)
   - 模型大小 (KB): 文件体积
   - 内存占用峰值 (MB): GPU 显存或 RAM

3. 评估协议:
   - 5 折分层交叉验证 (5-fold Stratified Cross-Validation)
   - 每折使用相同的数据划分
   - 报告 mean ± std

4. 延迟测试:
   - PyTorch: torch.cuda.Event 或 time.perf_counter
   - TFLite: Interpreter.invoke() 计时
   - Warmup: 10 次 (不计入统计)
   - 测试: 100 次
   - 报告: mean ± std (ms)

5. 输出:
   - CSV 表格: benchmark_results_<timestamp>.csv
   - Markdown 报告: benchmark_report.md
   - 雷达图: radar_chart.png (准确率 vs 延迟 vs 参数量 vs 模型大小)
   - 使用 matplotlib 绘制雷达图和柱状图

6. Markdown 报告格式:
   # L1 Model Benchmark Report
   | Model | Top-1 (%) | Top-5 (%) | FLOPs (M) | Latency (ms) | Params (K) | Size (KB) |
   |-------|----------|----------|-----------|-------------|-----------|----------|
   | cnn_attention_v2 | 92.3±0.5 | 99.1±0.2 | 12.5 | 2.8±0.3 | 34 | 38 |
   | ms_tcn_v1 | 86.7±0.8 | 97.2±0.4 | 4.2 | 1.5±0.1 | 12 | 18 |
   **推荐模型: cnn_attention_v2 (帕累托最优)**

7. 命令行接口:
   python scripts/run_benchmark.py \
     --config configs/model_config.yaml \
     --data_dir data/recordings/ \
     --output_dir benchmark_results/ \
     --folds 5 \
     --device cuda

8. 帕累托最优分析:
   - 在准确率和延迟之间找到帕累托前沿
   - 标记帕累托最优模型
   - 给出推荐理由
```

**🎯 预期输出：**

- glove_relay/scripts/run_benchmark.py - 完整的 Benchmark 脚本
- CSV 表格输出
- Markdown 报告
- 雷达图可视化
- 帕累托最优分析

**✅ 验收标准：**

- [ ] 可加载 model_config.yaml 中的所有模型
- [ ] 支持 PyTorch 和 TFLite 两种模型格式
- [ ] 5 折分层交叉验证正确实现
- [ ] 推理延迟测试包含 warmup 阶段
- [ ] 输出 CSV、Markdown 报告和雷达图
- [ ] 帕累托最优模型被正确标记
- [ ] 命令行参数完整可用

---

## 6 Phase 4：通信协议

通信层负责将 ESP32 的传感器数据和推理结果传输到 Python Relay 中继服务器。V3 架构中，BLE 仅用于 WiFi 配网（配网完成后退化为低速备用通道），高频数据传输使用 WiFi UDP（端口 8888），前端通过 WebSocket 连接 Python Relay（端口 8765）获取 JSON 格式数据。

---

### P4.1 Protobuf 定义与 Nanopb 生成

**💡 上下文说明：** Protobuf 提供高效的二进制序列化，相比 JSON 减少约 60% 的传输数据量。ESP32 端使用 Nanopb（极简 C 实现），Python Relay 端使用标准 protobuf 库。统一的 .proto 定义确保两端数据格式一致，避免手工解析错误。前端（React）不直接处理 Protobuf——Python Relay 负责转换为 JSON。

**📝 Prompt文本（可直接复制粘贴到 Claude Code）：**

```
请实现 Protobuf 数据帧定义和 Nanopb 生成:

1. 编辑 glove_data.proto (glove_relay/proto/glove_data.proto):

   syntax = "proto3";

   message GloveData {
     uint32 timestamp = 1;           // 毫秒时间戳
     repeated float hall_features = 2;   // 15 路霍尔 (mT)
     repeated float imu_features = 3;    // 6 路 IMU (3 欧拉角 ° + 3 陀螺仪 dps)
     repeated float flex_features = 4;   // 5 路柔性 (预留)
     uint32 l1_gesture_id = 5;
     uint32 l1_confidence_x100 = 6;     // 置信度 *100 (0-10000)
   }

2. 安装 Nanopb:
   pip install protobuf nanopb
   生成 C 代码: nanopb/generator/protoc --nanopb_out=. glove_data.proto

3. 在 ESP32 端集成 (lib/Comms/):
   #include "glove_data.pb.h"
   使用 pb_encode 序列化
   使用 pb_ostream_from_buffer 创建输出流
   拷贝生成的 glove_data.pb.h 和 glove_data.pb.c 到 lib/Comms/

4. 在 Python Relay 端:
   protoc --python_out=. glove_data.proto
   将 glove_data_pb2.py 放入 glove_relay/src/ 下

5. Python Relay 端解析函数:
   from glove_data_pb2 import GloveData
   def parse_protobuf(data: bytes) -> dict:
       msg = GloveData()
       msg.ParseFromString(data)
       return {
           "timestamp": msg.timestamp,
           "hall": list(msg.hall_features),
           "imu": list(msg.imu_features),
           "l1_gesture_id": msg.l1_gesture_id,
           "l1_confidence": msg.l1_confidence_x100 / 100.0,
       }
```

**🎯 预期输出：**

- glove_relay/proto/glove_data.proto - Protobuf 消息定义
- lib/Comms/glove_data.pb.h / glove_data.pb.c - Nanopb 生成的 C 代码
- glove_relay/glove_data_pb2.py - Python protobuf 代码
- ESP32 端序列化示例代码
- Python Relay 端反序列化 + JSON 转换示例代码

**✅ 验收标准：**

- [ ] GloveData 消息包含所有 6 个字段
- [ ] Nanopb 生成代码编译通过（ESP32 端）
- [ ] Python Relay 端可正确解析 ESP32 发送的 Protobuf 数据
- [ ] 单帧数据大小 <200 字节
- [ ] 序列化/反序列化往返测试通过（round-trip）
- [ ] Python 端 parse_protobuf() 返回标准 Python dict

---

### P4.2 BLE 5.0 GATT 服务实现

**💡 上下文说明：** BLE 在 V3 架构中的角色已简化为 WiFi 配网通道和低速备用通道。GATT 服务提供两个主要功能：Config Characteristic 用于接收 WiFi 凭证（SSID/Password），Sensor Data Characteristic 用于低速数据推送（20Hz，WiFi 不可用时的降级通道）。BLE MTU 协商可提升吞吐量至 512 字节/帧。

**📝 Prompt文本（可直接复制粘贴到 Claude Code）：**

```
请实现 BLE 5.0 GATT 服务，文件路径: lib/Comms/BLEManager.h

要求：

1. 类名: BLEManager

2. GATT 服务定义:
   - Custom Service UUID: 0x181A
   - Sensor Data Characteristic (Notify): UUID 自定义
     - 数据格式: Protobuf 序列化的 GloveData
     - 最大 MTU: 512 字节 (需 MTU 协商)
   - Config Characteristic (Read/Write): UUID 自定义
     - 用于配网 (WiFi SSID/Password 下发)

3. 配网流程:
   - 手机 APP 通过 Config Characteristic 写入 WiFi 凭证
   - 格式: "SSID:PASSWORD\n"
   - ESP32 收到后调用 WiFi.begin()
   - 连接成功后通过 Notify 回复 "OK"
   - 成功配网后清除 BLE 中的 WiFi 凭证缓存

4. 数据传输:
   - L1 识别结果通过 Notify 发送 (每次手势变化时)
   - 传感器原始数据每 5 帧发送一次 (20Hz BLE 降级)
   - WiFi 连接正常时 BLE 仅维持心跳 (每 10s 一次)

5. 安全:
   - 配网数据仅 BLE 初始连接时有效
   - WiFi 凭证存储到 NVS 分区 (非 BLE 缓存)
   - 成功配网后 BLE 中的 WiFi 凭证立即清除

重要: BLE 不用于高频实时数据传输 (100Hz)。高频数据使用 WiFi UDP (P4.3)。
```

**🎯 预期输出：**

- lib/Comms/BLEManager.h - 类声明
- lib/Comms/BLEManager.cpp - GATT 服务实现
- BLE 服务器回调注册
- MTU 协商逻辑
- WiFi 配网流程代码

**✅ 验收标准：**

- [ ] BLE 服务成功广播和连接
- [ ] MTU 协商成功（≥247 字节，目标 512）
- [ ] Config Characteristic 写入 WiFi 凭证后成功连接
- [ ] 配网凭证存储到 NVS，不在 BLE 回调中持久化
- [ ] Sensor Data Notify 正常发送 Protobuf 数据
- [ ] WiFi 连接正常时 BLE 降级为心跳模式

---

### P4.3 WiFi UDP 发送实现

**💡 上下文说明：** WiFi UDP 是 V3 架构中 ESP32 → Python Relay 的高频数据传输通道。目标端口为 8888，发送频率 100Hz，每帧数据使用 Nanopb 序列化的 GloveData Protobuf 消息。UDP 允许丢包的设计理念与实时渲染的容错特性高度契合——偶尔丢一帧数据不会影响用户体验。

**📝 Prompt文本（可直接复制粘贴到 Claude Code）：**

```
请实现 WiFi UDP 发送功能，文件路径: lib/Comms/UDPTransmitter.h

要求：

1. 类名: UDPTransmitter

2. 核心方法:
   - bool begin(const char* ssid, const char* password, const char* relay_ip, uint16_t port = 8888)
     - 连接 WiFi (使用存储在 NVS 中的凭证)
     - 创建 UDP socket
   - bool sendGloveData(const GloveData& data)
     - 使用 Nanopb 序列化 GloveData
     - 通过 UDP 发送到 relay_ip:8888
     - 返回发送是否成功
   - bool isConnected()
     - 检查 WiFi 连接状态
   - void setRelayIP(const char* ip)
     - 动态修改目标 IP (通过 BLE 或 HTTP 配置)

3. WiFi 连接管理:
   - 自动重连: WiFi 断线后每 5s 尝试重连
   - 连接超时: 10s
   - 事件回调: WiFi.onEvent() 处理连接/断连事件

4. 性能要求:
   - 发送频率: 100Hz (每 10ms 一帧)
   - 单帧大小: <200 字节
   - 发送延迟: <1ms (局域网)

5. 集成到 Task_Comms:
   - 从 dataQueue 获取推理结果
   - 序列化为 Protobuf
   - UDP 发送
   - 统计丢包率 (可选)

已知约束:
- 目标端口: 8888 (Python Relay UDP 服务器)
- 数据格式: Protobuf (Nanopb 序列化)
- 允许丢包, 不需要 ACK 确认
- WiFi IP: 192.168.x.x 局域网
```

**🎯 预期输出：**

- lib/Comms/UDPTransmitter.h - 类声明
- lib/Comms/UDPTransmitter.cpp - WiFi + UDP 实现
- WiFi 自动重连逻辑
- 与 Task_Comms 的集成代码

**✅ 验收标准：**

- [ ] WiFi 连接稳定（局域网环境）
- [ ] UDP 发送频率稳定在 100Hz
- [ ] 单帧 Protobuf 数据 <200 字节
- [ ] 断线后自动重连
- [ ] 动态修改目标 IP 功能正常
- [ ] 与 Python Relay 的 UDP 服务器对接成功

---

## 7 Phase 5：Python Relay + L2 推理

Python Relay 是 V3 架构的核心枢纽，替代了 V2 中分散的多个服务。它承担四个职责：UDP 数据接收与 Protobuf 解析（端口 8888）、WebSocket JSON 推送（端口 8765）、L2 ST-GCN 推理、NLP 语法纠错与 TTS 语音合成。核心设计思想是「前端只接收 JSON，不理解 Protobuf」。

> **⚠️ 严重 Bug：原始 ST-GCN 实现为假实现**
>
> 原论文 l2_inference.py 中的 STGCNModel 仅使用 `nn.Linear(42*30, 46)`——这只是一个单层全连接层，没有任何图卷积结构！它丢失了所有空间（手部骨骼拓扑）和时间（动作序列）信息。P5.2 提供了完整替换方案。P8.2 提供了修复 Prompt。

---

### P5.1 FastAPI + WebSocket 中继服务器（V3 新增）

**💡 上下文说明：** FastAPI + WebSockets 是 Python Relay 的核心，负责接收 ESP32 的 UDP Protobuf 数据、解析为 JSON、并通过 WebSocket 推送到前端。这是 V3 架构中最重要的新增组件——它解耦了嵌入式端和前端，使前端开发者只需处理 JSON，无需理解 Protobuf 协议。

**📝 Prompt文本（可直接复制粘贴到 Claude Code）：**

```
请实现 FastAPI + WebSocket 中继服务器，文件路径: glove_relay/src/main.py

要求：

1. FastAPI 应用:
   from fastapi import FastAPI, WebSocket, WebSocketDisconnect
   from fastapi.middleware.cors import CORSMiddleware

   app = FastAPI(title="Glove Relay", version="3.0.0")

2. CORS 配置 (开发环境):
   app.add_middleware(
       CORSMiddleware,
       allow_origins=["http://localhost:5173", "http://localhost:3000"],
       allow_methods=["*"],
       allow_headers=["*"],
   )

3. asyncio UDP 服务器 (端口 8888):
   - 使用 asyncio.DatagramProtocol 创建 UDP 接收器
   - 绑定 0.0.0.0:8888
   - 收到数据后使用 glove_data_pb2 解析 Protobuf
   - 转换为 JSON 格式
   - 缓冲最近 30 帧 (用于 L2 推理的滑动窗口)

4. WebSocket 端点 (端口 8765):
   @app.websocket("/ws")
   async def websocket_endpoint(websocket: WebSocket):
       await websocket.accept()
       connected_clients.add(websocket)
       try:
           while True:
               # 接收客户端控制消息 (可选)
               data = await websocket.receive_text()
               # 处理: 暂停/恢复/切换模型等命令
       except WebSocketDisconnect:
           connected_clients.remove(websocket)

5. JSON 推送格式:
   {
       "timestamp": 1713950000000,       // 毫秒时间戳
       "hall": [1.23, -0.45, ..., 0.78], // 15 floats (mT)
       "imu": [12.5, -3.2, 178.9, ...],  // 6 floats (deg + dps)
       "l1_gesture_id": 5,                // L1 手势 ID
       "l1_confidence": 0.92,             // L1 置信度
       "l2_gesture_id": null,             // L2 手势 ID (仅 L1 低置信度时)
       "l2_confidence": null,             // L2 置信度
       "nlp_text": null,                  // NLP 纠错后文本
       "tts_url": null,                   // TTS 音频 URL (可选)
       "status": "STREAMING"              // STREAMING / MODEL_SWITCHING / ERROR
   }

6. 广播机制:
   - 维护已连接的 WebSocket 客户端集合
   - 收到 UDP 数据后, 解析并广播给所有客户端
   - 广播频率与 UDP 接收频率同步 (100Hz)

7. L2 推理集成 (异步):
   - 当 l1_confidence <= 0.85 时, 触发 L2 推理
   - L2 推理在后台线程中执行 (不阻塞 WebSocket 推送)
   - 推理完成后将 l2_gesture_id 和 l2_confidence 追加到后续消息中

8. 错误处理:
   - UDP 包丢失: 静默丢弃, 打印警告日志
   - Protobuf 解析错误: 跳过该包, 记录错误
   - WebSocket 断线: 自动从客户端集合中移除
   - L2 推理超时: 超过 100ms 降级为 L1 结果

9. 生命周期管理:
   @app.on_event("startup")
   async def startup():
       # 启动 UDP 服务器
       # 加载 L2 ST-GCN 模型
       # 初始化 TTS 引擎

   @app.on_event("shutdown")
   async def shutdown():
       # 停止 UDP 服务器
       # 释放 GPU 内存
       # 关闭所有 WebSocket 连接

10. 启动命令:
    uvicorn src.main:app --host 0.0.0.0 --port 8000 --reload
```

**🎯 预期输出：**

- glove_relay/src/main.py - 完整的 FastAPI 应用
- glove_relay/src/udp_server.py - asyncio UDP 服务器
- glove_relay/src/ws_server.py - WebSocket 管理器
- glove_relay/src/protobuf_parser.py - Protobuf → JSON 转换
- CORS 配置
- 错误处理和日志

**✅ 验收标准：**

- [ ] `uvicorn src.main:app --reload` 成功启动
- [ ] UDP 端口 8888 成功接收 ESP32 数据
- [ ] Protobuf 解析正确，JSON 格式符合规范
- [ ] WebSocket 端口 8765 支持多客户端同时连接（≥3）
- [ ] 100Hz UDP 输入 → <5ms 转换延迟 → WebSocket 广播
- [ ] 客户端断线后自动清理
- [ ] Swagger UI 可访问 (http://localhost:8000/docs)
- [ ] 项目中不存在任何 Rust 代码

---

### P5.2 自研 ST-GCN 模型实现（V3 重写）

**💡 上下文说明：** L2 推理使用自研的时空图卷积网络（ST-GCN），基于 MS-GCN3 原始论文。关键创新点是伪骨骼映射——将 21 维传感器特征映射为 21×2D 骨骼关键点坐标，使标准图卷积操作可以应用于传感器数据。模型包含 4 个 ST-Conv Block（空间卷积 + 时间卷积 + 残差连接），最后通过注意力池化生成 46 类手势的分类 logits。

> **⚠️ 严重警告：不要使用原始代码中的假 ST-GCN！**
>
> 原代码 `nn.Linear(42*30, 46)` 是单层全连接，没有任何图卷积结构。必须完整实现以下所有组件。

**📝 Prompt文本（可直接复制粘贴到 Claude Code）：**

```
请实现完整的 ST-GCN 模型（非假实现），文件路径: glove_relay/src/models/stgcn_model.py

要求：

1. 伪骨骼映射层 (Pseudo-Skeleton Mapping):
   class PseudoSkeletonMapping(nn.Module):
       def __init__(self, input_dim=21, num_joints=21, coord_dim=2):
           self.mapping = nn.Linear(input_dim, num_joints * coord_dim)
       def forward(self, x):
           # 输入: (B, 21, 30) → 输出: (B, T, V, C) = (B, 30, 21, 2)
           x = x.permute(0, 2, 1)  # (B, 30, 21)
           mapped = self.mapping(x)  # (B, 30, 42)
           return mapped.view(B, 30, 21, 2)

2. 手部邻接矩阵:
   定义 5 分支树状结构的手部骨骼拓扑:
   - 0: WRIST (根节点)
   - 1-4: THUMB (CMC, MCP, IP, TIP)
   - 5-8: INDEX (MCP, PIP, DIP, TIP)
   - 9-12: MIDDLE (MCP, PIP, DIP, TIP)
   - 13-16: RING (MCP, PIP, DIP, TIP)
   - 17-20: PINKY (MCP, PIP, DIP, TIP)

   邻接矩阵 A (21×21), 3 种邻接矩阵:
   - A_self: 自连接 (对角线)
   - A_forward: 父→子方向
   - A_backward: 子→父方向
   归一化: D^(-1/2) * A * D^(-1/2)

3. 空间图卷积 (Spatial Graph Convolution):
   class SpatialConv(nn.Module):
       def __init__(self, in_channels, out_channels, adj_matrix):
           # 3 种邻接矩阵分别卷积后求和
           self.conv_self = nn.Conv2d(in_ch, out_ch, 1)
           self.conv_forward = nn.Conv2d(in_ch, out_ch, 1)
           self.conv_backward = nn.Conv2d(in_ch, out_ch, 1)
       def forward(self, x):
           # x: (B, C, T, V) → (B, C_out, T, V)

4. 时间卷积 (Temporal Convolution):
   class TemporalConv(nn.Module):
       def __init__(self, channels, dilations=[1,2,1,2]):
           # TCN 结构: 多层 dilated conv
       def forward(self, x):
           # x: (B, C, T, V) → (B, C, T', V)

5. ST-Conv Block:
   class STConvBlock(nn.Module):
       def __init__(self, channels, adj_matrix, dilation):
           self.spatial = SpatialConv(channels, channels, adj_matrix)
           self.temporal = TemporalConv(channels, [dilation])
           self.norm = nn.LayerNorm([channels, T, V])
       def forward(self, x):
           return F.relu(self.norm(self.temporal(self.spatial(x))) + x)

6. 注意力池化:
   class AttentionPooling(nn.Module):
       def __init__(self, channels):
           self.attention = nn.Sequential(
               nn.AdaptiveAvgPool1d(1),
               nn.Flatten(),
               nn.Linear(channels, channels),
               nn.Sigmoid()
           )
       def forward(self, x):
           # x: (B, C, T, V) → (B, C)

7. 完整模型架构:
   class STGCNModel(BaseModel):
       def __init__(self, num_classes=46):
           self.pseudo_skeleton = PseudoSkeletonMapping(21, 21, 2)
           self.adj = build_hand_adjacency()  # (21, 21)
           self.st_blocks = nn.ModuleList([
               STConvBlock(64, self.adj, 1),
               STConvBlock(64, self.adj, 2),
               STConvBlock(128, self.adj, 1),
               STConvBlock(128, self.adj, 2),
           ])
           self.attention_pool = AttentionPooling(128)
           self.fc = nn.Linear(128, num_classes)
           self.input_proj = nn.Conv2d(2, 64, 1)  # 2D coord → 64 channels

8. 训练配置:
   - 优化器: AdamW(lr=1e-3, weight_decay=1e-4)
   - 调度器: CosineAnnealingLR(T_max=300)
   - 损失: CrossEntropyLoss(label_smoothing=0.1)
   - Batch Size: 32 (模型较大, 显存限制)
   - Epochs: 300 (早停 patience=30)
   - 数据增强: 时序 Mixup, 随机裁剪

   验收标准:
   - 训练集准确率 >99%
   - 验证集准确率 >95% (46 类 CSL)
   - Top-5 准确率 >99%
   - 推理延迟 <20ms (GPU)
```

**🎯 预期输出：**

- glove_relay/src/models/stgcn_model.py - 完整 ST-GCN 实现
- 伪骨骼映射层
- 手部骨骼邻接矩阵
- 空间图卷积 + 时间卷积
- ST-Conv Block (含残差连接和 LayerNorm)
- 注意力池化层
- 完整训练脚本

**✅ 验收标准：**

- [ ] 模型包含 PseudoSkeletonMapping 层（Linear 21→42 + reshape）
- [ ] 模型包含 ≥2 个 ST-Conv Block
- [ ] 空间图卷积基于邻接矩阵实现（不是 nn.Linear 假实现）
- [ ] 时间卷积使用膨胀卷积（dilation ≥1）
- [ ] 每个块包含残差连接
- [ ] 验证集 Top-1 准确率 >95%（46 类）
- [ ] 模型参数量约 280K（±50K）
- [ ] **绝对不使用** `nn.Linear(42*30, 46)` 这种假实现

---

### P5.3 NLP 语法纠错 + TTS（V3 更新）

**💡 上下文说明：** 中国手语（CSL）与普通话的语法存在显著差异——CSL 主要采用 SOV（主语-宾语-动词）语序，而普通话采用 SVO（主语-动词-宾语）。此外 CSL 是话题优先语言，存在量词省略、体标记缺失等现象。NLP 模块负责将 L2 的手势 ID 序列翻译为语法正确的普通话句子，再通过 edge-tts 合成语音。

**📝 Prompt文本（可直接复制粘贴到 Claude Code）：**

```
请实现 NLP 语法纠错和 TTS 语音合成模块。

一、NLP 语法纠错 (glove_relay/src/nlp/grammar_corrector.py):

1. 规则引擎实现 CSL → 普通话语法转换:

   规则列表 (至少 10 条):
   a. SOV → SVO: "我 饭 吃" → "我 吃饭"
   b. 话题优先处理: "饭 我 吃" → "我 吃饭"
   c. 量词插入: "一 人" → "一个人"
   d. 疑问句: "你 去 吗" → "你去吗" (移除多余空格)
   e. 否定句: "不 去" → "不去"
   f. 时态标记: 动词+了 表示完成 → 保持
   g. 方位词: "学校 在" → "在学校"
   h. 比较句: "他 高 我" → "他比我高"
   i. 被动句: "信 收到" → "收到信了"
   j. 连词添加: 多手势序列用逗号和句号连接

2. 接口:
   class GrammarCorrector:
       def __init__(self):
           self.gesture_labels = load_gesture_labels()  # 46 类标签
           self.rules = load_grammar_rules()

       def correct(self, gesture_ids: list[int]) -> str:
           """将手势 ID 序列转换为语法正确的普通话句子"""
           # 1. ID → 词汇映射
           # 2. 应用语法规则
           # 3. 添加标点符号
           # 4. 返回完整句子
           pass

3. 示例输入输出:
   输入: [5, 12, 23]  →  词汇: ["我", "学校", "去"]
   输出: "我去学校。"

   输入: [5, 23, 12]  →  词汇: ["我", "去", "学校"]
   输出: "我去学校。"

   输入: [12, 5, 23]  →  词汇: ["学校", "我", "去"]
   输出: "我去学校。" (话题优先 → SVO)

4. 未来升级路径:
   - 使用 distilbert-base-chinese 轻量级 Transformer
   - 训练 CSL→普通话平行语料
   - Beam Search 解码

二、TTS 语音合成 (glove_relay/src/tts/tts_engine.py):

1. 使用 edge-tts 异步集成:
   import edge_tts

   class TTSEngine:
       def __init__(self):
           self.voice = "zh-CN-XiaoxiaoNeural"
           self.rate = "+0%"  # 语速
           self.volume = "+0%"  # 音量

       async def synthesize(self, text: str) -> bytes:
           """合成语音, 返回 MP3 音频数据"""
           communicate = edge_tts.Communicate(text, self.voice)
           audio_data = b""
           async for chunk in communicate.stream():
               if chunk["type"] == "audio":
                   audio_data += chunk["data"]
           return audio_data

       async def synthesize_stream(self, text: str, websocket: WebSocket):
           """流式合成并播放 (边合成边发送)"""
           communicate = edge_tts.Communicate(text, self.voice)
           async for chunk in communicate.stream():
               if chunk["type"] == "audio":
                   await websocket.send_bytes(chunk["data"])

2. 性能要求:
   - TTS 首字延迟 <500ms
   - 支持流式输出 (边合成边发送)
   - 语音自然度 MOS ≥4.0

3. 缓存策略:
   - 对已合成的句子进行缓存 (LRU, 最多 100 条)
   - 相同文本直接返回缓存, 不重复合成
```

**🎯 预期输出：**

- glove_relay/src/nlp/grammar_corrector.py - NLP 语法纠错模块
- glove_relay/src/tts/tts_engine.py - edge-tts 集成模块
- 10+ 条 CSL 语法规则
- 46 类手势标签映射表
- TTS 流式合成实现
- 音频缓存策略

**✅ 验收标准：**

- [ ] NLP 纠错率 >85%（人工评估 50 句测试集）
- [ ] SOV→SVO 转换正确
- [ ] 话题优先结构正确处理
- [ ] edge-tts 合成成功，声音可播放
- [ ] TTS 首字延迟 <500ms
- [ ] 流式合成功能正常
- [ ] LRU 缓存命中时延迟 <10ms

---

### P5.4 L1→L2 置信度驱动路由（V3 新增）

**💡 上下文说明：** 双层推理架构的协同逻辑是系统的核心创新点。L1 边缘模型运行在 ESP32-S3 上，推理延迟 <3ms，但准确率有限（90%）。L2 ST-GCN 运行在 Python Relay 上，准确率高（95%+）但推理延迟较大（~20ms）。置信度驱动的智能路由机制根据 L1 的输出置信度决定是否触发 L2 推理，在准确率和延迟之间取得最优平衡。

**📝 Prompt文本（可直接复制粘贴到 Claude Code）：**

```
请实现 L1→L2 置信度驱动路由模块，文件路径: glove_relay/src/router.py

要求：

1. 路由配置:
   CONFIDENCE_THRESHOLD = 0.85    # L1 置信度阈值
   DEBOUNCE_FRAMES = 5            # 去抖: 连续 N 帧相同手势才确认
   GESTURE_SILENCE_MS = 100       # 不同手势之间的静默间隔
   L2_TIMEOUT_MS = 100            # L2 推理超时
   UNKNOWN_LABEL = "UNKNOWN"      # 低置信度输出

2. 路由逻辑:
   class ConfidenceRouter:
       def __init__(self, l1_threshold=0.85, debounce=5):
           self.l1_threshold = l1_threshold
           self.debounce = debounce
           self.recent_gestures = deque(maxlen=debounce)
           self.frame_buffer = deque(maxlen=30)  # 30 帧滑动窗口

       def route(self, sensor_data: dict) -> RouteResult:
           """
           输入: 从 UDP 解析的最新一帧传感器数据
           输出: RouteResult(gesture_id, confidence, source, nlp_text)
           """
           # Step 1: 将当前帧加入 frame_buffer
           # Step 2: 检查 L1 置信度
           if l1_confidence > self.l1_threshold:
               # L1 高置信度 → 直接输出
               result = self._debounce(l1_gesture_id, l1_confidence, source="L1")
           else:
               # L1 低置信度 → 触发 L2
               if len(self.frame_buffer) >= 30:
                   l2_result = await self._invoke_l2(self.frame_buffer)
                   if l2_result.confidence > self.l1_threshold:
                       result = self._debounce(l2_result.id, l2_result.conf, source="L2")
                   else:
                       result = RouteResult(gesture_id=-1, confidence=0, source="FALLBACK",
                                           nlp_text=UNKNOWN_LABEL)

           # Step 3: NLP 纠错
           if result.gesture_id >= 0:
               result.nlp_text = self.grammar_corrector.correct([result.gesture_id])

           return result

3. 去抖逻辑:
   def _debounce(self, gesture_id, confidence, source):
       self.recent_gestures.append(gesture_id)
       if len(self.recent_gestures) < self.debounce:
           return RouteResult(gesture_id=-1, ...)  # 未确认
       if all(g == gesture_id for g in self.recent_gestures):
           return RouteResult(gesture_id, confidence, source)  # 确认
       return RouteResult(gesture_id=-1, ...)  # 抖动中

4. 手势转换静默:
   def _check_silence(self, new_gesture_id, last_confirmed_id):
       if new_gesture_id != last_confirmed_id:
           # 强制 100ms 静默期, 避免手势闪烁
           time_since_last = now() - self.last_confirm_time
           if time_since_last < self.gesture_silence_ms:
               return False  # 在静默期内, 不确认新手势
       return True

5. L2 异步调用:
   async def _invoke_l2(self, frame_buffer):
       try:
           result = await asyncio.wait_for(
               self.l2_model.predict(frame_buffer),
               timeout=self.l2_timeout_ms / 1000.0
           )
           return result
       except asyncio.TimeoutError:
           logger.warning("L2 inference timeout, falling back to L1")
           return None

6. 性能目标:
   - L1 高置信度路径: <5ms 端到端延迟 (含 NLP + TTS)
   - L2 路径: <100ms 端到端延迟 (含 L2 推理 + NLP + TTS)
   - L2 触发率: <20% (大多数手势 L1 即可识别)

7. 集成到 main.py:
   - 在 UDP 数据接收回调中调用 router.route()
   - 根据路由结果构建 JSON 消息
   - 通过 WebSocket 广播
```

**🎯 预期输出：**

- glove_relay/src/router.py - 置信度驱动路由模块
- ConfidenceRouter 类
- 去抖逻辑
- 手势转换静默机制
- L2 异步调用 + 超时处理
- 与 main.py 的集成

**✅ 验收标准：**

- [ ] L1 置信度 >0.85 时直接输出，不触发 L2
- [ ] L1 置信度 ≤0.85 时自动触发 L2 推理
- [ ] 去抖: 连续 5 帧相同手势才确认
- [ ] 手势转换间隔 ≥100ms
- [ ] L2 超时 (>100ms) 降级为 L1 结果或 UNKNOWN
- [ ] L1 高置信度路径端到端延迟 <100ms（含 TTS）
- [ ] L2 触发率 <20%

---

## 8 Phase 6：渲染层

渲染层采用两阶段渐进式演进策略。第一阶段（L1/L2 MVP）使用 React 18 + Vite + R3F 纯 Web 方案，实现响应式的浏览器端 3D 手部骨架可视化。第二阶段（L3 Professional）升级至 Unity 2022 LTS + XR Hands + ms-MANO，实现高逼真度手部渲染。两个阶段的骨骼定义和关节旋转接口保持一致。

> **⚠️ 重要: V3 渲染架构变更**
>
> V2 使用 Tauri + Rust 桌面端架构。V3 完全移除 Tauri/Rust，改为纯 Web 方案（React + Vite + R3F）。前端通过 WebSocket 连接 Python Relay 获取数据，不直接连接 ESP32。绝对不使用 Web Bluetooth API。

---

### P6.1 React + R3F 3D 手部渲染 MVP（V3 新增）

**💡 上下文说明：** React Three Fiber (R3F) 将 Three.js 的 3D 渲染能力封装为 React 声明式组件，极大地提升了开发效率。本 Prompt 实现 21 关键点手部骨架的 3D 可视化，通过 WebSocket 实时更新骨骼旋转角度。关键性能要求：60fps 在中端设备上流畅运行。per-frame 数据更新必须使用 useRef 而非 useState，避免 React 重新渲染导致帧率下降。

**📝 Prompt文本（可直接复制粘贴到 Claude Code）：**

```
请实现 React + R3F 3D 手部渲染 MVP，项目路径: glove_web/

要求：

1. HandCanvas 组件 (src/components/Hand3D/HandCanvas.tsx):
   import { Canvas } from '@react-three/fiber'
   import { OrbitControls } from '@react-three/drei'

   function HandCanvas() {
     return (
       <Canvas camera={{ position: [0, 0, 15], fov: 50 }}>
         <ambientLight intensity={0.5} />
         <pointLight position={[10, 10, 10]} />
         <HandSkeleton />
         <OrbitControls enableZoom={true} />
         <gridHelper args={[20, 20]} />
       </Canvas>
     )
   }

2. HandSkeleton 组件 (src/components/Hand3D/HandSkeleton.tsx):
   - 21 个球体 (SphereGeometry) 表示关键点
   - 20 根骨骼 (CylinderGeometry 或 Line) 连接关键点
   - 使用 useFrame hook 在每帧更新骨骼旋转

3. 骨骼连接定义 (src/utils/constants.ts):
   HAND_CONNECTIONS = [
     [0, 1], [1, 2], [2, 3], [3, 4],     // THUMB
     [0, 5], [5, 6], [6, 7], [7, 8],     // INDEX
     [0, 9], [9, 10], [10, 11], [11, 12], // MIDDLE
     [0, 13], [13, 14], [14, 15], [15, 16], // RING
     [0, 17], [17, 18], [18, 19], [19, 20], // PINKY
     [5, 9], [9, 13], [13, 17],            // PALM
   ]

4. 动画驱动 (src/hooks/useHandAnimation.ts):
   import { useFrame } from '@react-three/fiber'
   import { useRef } from 'react'
   import { useSensorStore } from '../stores/useSensorStore'

   function useHandAnimation() {
     const jointRefs = useRef<(THREE.Mesh | null)[]>([])
     const hall = useSensorStore((s) => s.hall)  // 15 floats
     const imu = useSensorStore((s) => s.imu)    // 6 floats

     useFrame(() => {
       // ⚠️ 关键: 使用 useRef 更新, 不要用 useState!
       // 将 15 路霍尔数据映射到 5 根手指的弯曲角度
       // 将 6 路 IMU 数据映射到手腕的整体旋转
       for (let finger = 0; finger < 5; finger++) {
         const bendAngle = hall[finger * 3]  // 使用 X 轴数据
         // 更新对应关节的旋转
         if (jointRefs.current[finger * 4 + 1]) {
           jointRefs.current[finger * 4 + 1].rotation.x = bendAngle
         }
       }
       // 手腕旋转 (四元数)
       // euler → quaternion → wrist joint rotation
     })

     return jointRefs
   }

5. 布局 (src/App.tsx):
   - 桌面端: 左侧 3D 视口 + 右侧结果面板
   - 移动端: 3D 视口在上 + 结果面板在下
   - 使用 TailwindCSS flex/grid 布局

6. GestureResult 组件 (src/components/Dashboard/GestureResult.tsx):
   - 显示当前识别的手势名称 (中文)
   - 显示 L1/L2 置信度
   - 显示 NLP 纠错后的文本
   - 显示 WebSocket 连接状态

7. 性能优化:
   - React.memo 包裹静态组件
   - useFrame 中避免创建新对象
   - 3D 几何体使用 useMemo 缓存
   - 目标: 60fps @ 1080p (中端设备)

8. 关键约束:
   - 绝对不使用 useState 存储 per-frame 数据
   - WebSocket 数据更新通过 Zustand store (异步, 不阻塞渲染)
   - 3D 渲染通过 useFrame (同步, 60fps)
   - 不使用 Web Bluetooth API
   - 不使用 Tauri 或任何桌面端框架
```

**🎯 预期输出：**

- HandCanvas / HandSkeleton / FingerBone 组件
- useHandAnimation hook
- HAND_CONNECTIONS 骨骼定义
- GestureResult 面板
- 响应式布局

**✅ 验收标准：**

- [ ] 3D 手部骨架正确渲染（21 关键点 + 20 骨骼）
- [ ] WebSocket 数据实时驱动骨骼旋转
- [ ] useFrame 中使用 useRef 更新（不使用 useState）
- [ ] 桌面端和移动端布局均正常
- [ ] 60fps @ 1080p（Chrome, 中端设备）
- [ ] 手势识别结果实时显示
- [ ] 项目中不存在 Tauri/Rust 代码

---

### P6.2 PWA 配置（V3 新增）

**💡 上下文说明：** PWA（Progressive Web App）使 Web 应用具备"安装到桌面"、"离线缓存"和"推送通知"等原生应用特性。对于数据手套的演示场景，PWA 模式提供了接近原生应用的启动体验，用户无需每次打开浏览器输入 URL，直接从桌面图标启动即可。

**📝 Prompt文本（可直接复制粘贴到 Claude Code）：**

```
请为 glove_web 项目配置 PWA 支持:

1. 更新 manifest.json (public/manifest.json):
   {
     "name": "Edge AI 手语手套",
     "short_name": "GloveAI",
     "description": "实时中国手语翻译与 3D 手部动画渲染",
     "start_url": "/",
     "display": "standalone",
     "background_color": "#0f172a",
     "theme_color": "#3b82f6",
     "orientation": "any",
     "icons": [
       { "src": "/icons/icon-72.png", "sizes": "72x72", "type": "image/png" },
       { "src": "/icons/icon-96.png", "sizes": "96x96", "type": "image/png" },
       { "src": "/icons/icon-128.png", "sizes": "128x128", "type": "image/png" },
       { "src": "/icons/icon-144.png", "sizes": "144x144", "type": "image/png" },
       { "src": "/icons/icon-152.png", "sizes": "152x152", "type": "image/png" },
       { "src": "/icons/icon-192.png", "sizes": "192x192", "type": "image/png" },
       { "src": "/icons/icon-384.png", "sizes": "384x384", "type": "image/png" },
       { "src": "/icons/icon-512.png", "sizes": "512x512", "type": "image/png" }
     ],
     "categories": ["utilities", "education"]
   }

2. 创建 Service Worker (public/sw.js):
   const CACHE_NAME = 'glove-ai-v3.0';
   const STATIC_ASSETS = [
     '/',
     '/index.html',
     '/manifest.json',
     '/icons/icon-192.png',
     '/icons/icon-512.png',
   ];

   self.addEventListener('install', (event) => {
     event.waitUntil(
       caches.open(CACHE_NAME)
         .then(cache => cache.addAll(STATIC_ASSETS))
     );
   });

   self.addEventListener('fetch', (event) => {
     // Network-first for WebSocket data, Cache-first for static assets
     if (event.request.url.includes('/ws')) {
       // WebSocket: 不缓存, 直接网络请求
       return;
     }
     event.respondWith(
       caches.match(event.request)
         .then(cached => cached || fetch(event.request))
     );
   });

3. 注册 Service Worker (src/main.tsx):
   if ('serviceWorker' in navigator) {
     window.addEventListener('load', () => {
       navigator.serviceWorker.register('/sw.js');
     });
   }

4. 添加 meta 标签 (index.html):
   <meta name="theme-color" content="#3b82f6">
   <link rel="manifest" href="/manifest.json">
   <meta name="apple-mobile-web-app-capable" content="yes">
   <meta name="apple-mobile-web-app-status-bar-style" content="black-translucent">
   <link rel="apple-touch-icon" href="/icons/icon-192.png">
   <meta name="description" content="实时中国手语翻译与 3D 手部动画渲染">

5. 生成图标占位 (使用 SVG 占位):
   - 创建简单的 SVG 图标 (蓝色圆形 + 白色手套轮廓)
   - 不同尺寸使用同一 SVG, 通过 <svg> viewBox 缩放
```

**🎯 预期输出：**

- 完整的 manifest.json
- Service Worker (sw.js)
- Service Worker 注册代码
- HTML meta 标签
- PWA 图标

**✅ 验收标准：**

- [ ] Chrome DevTools → Application → Manifest 正确显示
- [ ] "Add to Home Screen" 可用
- [ ] 安装后从桌面图标启动正常
- [ ] 静态资源离线可访问
- [ ] WebSocket 连接不受 Service Worker 影响
- [ ] Apple 移动设备支持 "Add to Home Screen"

---

### P6.3 Unity XR Hands + ms-MANO（V3 新增简要）

**💡 上下文说明：** Unity 阶段是渲染层的最终形态（L3 Professional），提供照片级的高逼真度手部渲染。本 Prompt 为简要框架，详细实现将在后续阶段展开。Unity 通过 WebSocket 连接 Python Relay 获取传感器数据（与 React 前端使用相同的数据格式），确保两个渲染端的行为一致。

**📝 Prompt文本（可直接复制粘贴到 Claude Code）：**

```
请创建 Unity 2022 LTS 项目框架，用于高逼真度手部渲染。

要求：

1. Unity 2022 LTS 项目初始化:
   - 模板: 3D (URP)
   - 目标平台: Windows Standalone (论文演示) / Android (移动端)
   - 安装 XR Plugin Management
   - 安装 XR Hands Package (com.unity.xr.hands)

2. 自定义 XRHandSubsystem 替换:
   - 默认 XRHands 使用摄像头追踪手部
   - 本项目需要通过 WebSocket 接收传感器数据
   - 创建 CustomHandDataProvider:
     * 继承 XRHandSubsystem.Provider
     * 从 WebSocket 接收 JSON 数据
     * 将 21 个传感器值映射到 21 个手部关节
     * 更新 XRHandJoint 数据

3. ms-MANO 模型集成:
   - 下载 ms-MANO 预训练模型
   - 输入: 48 维 pose_parameters
   - 输出: 778 个顶点的 3D 手部网格
   - 映射关系: 21 关键点 → 48 维 pose_parameters
     * 3 维: 全局旋转 (wrist quaternion)
     * 45 维: 15 个关节 × 3 DOF (finger joints)
   - 使用 SkinnedMeshRenderer 渲染

4. WebSocket 客户端 (C#):
   - 使用 NativeWebSocket 插件
   - 连接地址: ws://<RELAY_IP>:8765/ws
   - 消息格式: 与 React 前端相同的 JSON 格式
   - 自动重连

5. 骨骼定义一致性:
   - Unity 端的 21 个关节编号和旋转 DOF 必须与 React 端一致
   - 两端共享 src/utils/constants.ts 中的 HAND_CONNECTIONS 定义

6. 验收:
   - Unity 场景中显示高逼真度手部网格
   - WebSocket 数据驱动手部动画
   - 渲染效果达到论文发表质量

注意: 此为简要框架 Prompt, 详细实现将在 L3 阶段展开。
```

**🎯 预期输出：**

- Unity 2022 LTS 项目骨架
- CustomHandDataProvider C# 脚本
- ms-MANO 模型导入脚本
- NativeWebSocket 客户端脚本
- 骨骼映射文档

**✅ 验收标准：**

- [ ] Unity 项目编译通过
- [ ] XR Hands Package 安装成功
- [ ] CustomHandDataProvider 替换默认追踪
- [ ] WebSocket 连接到 Python Relay 成功
- [ ] 21 个关节数据正确映射
- [ ] 骨骼定义与 React 端一致

---

## 9 Phase 7：集成测试

---

### P7.1 系统集成测试

**💡 上下文说明：** 集成测试验证三个子系统（ESP32 边缘端、Python Relay、React 前端）协同工作的正确性。测试覆盖完整的数据流链路：传感器采集 → L1 推理 → UDP 发送 → Relay 解析 → WebSocket 推送 → 前端渲染。V3 架构下测试重点包括 Python Relay 的 Protobuf→JSON 转换正确性和 WebSocket 多客户端广播能力。

**📝 Prompt文本（可直接复制粘贴到 Claude Code）：**

```
请编写系统集成测试方案，文件路径: glove_relay/tests/test_integration.py

要求：

1. 测试环境准备:
   - ESP32 固件运行在开发板上 (或使用模拟器)
   - Python Relay 运行在本机
   - React 前端运行在本机 (npm run dev)
   - 网络环境: 同一 WiFi 局域网

2. 测试用例:

   a. UDP 通信测试:
      - ESP32 以 100Hz 发送 Protobuf 数据
      - Python Relay 接收并解析
      - 验证: 丢包率 <1%, 延迟 <5ms

   b. Protobuf → JSON 转换测试:
      - 发送已知的 Protobuf 二进制数据
      - 验证 Relay 输出的 JSON 字段正确
      - 验证: hall 数组长度=15, imu 数组长度=6

   c. WebSocket 广播测试:
      - 连接 3 个 WebSocket 客户端
      - 发送 UDP 数据
      - 验证: 所有 3 个客户端均收到相同 JSON 消息
      - 验证: 广播延迟 <10ms

   d. L1→L2 路由测试:
      - 模拟 L1 高置信度 (>0.85): 不触发 L2
      - 模拟 L1 低置信度 (≤0.85): 触发 L2
      - 验证: 路由逻辑正确, L2 延迟 <100ms

   e. 端到端延迟测试:
      - 从传感器采集到前端渲染的完整链路
      - 验证: 端到端延迟 <200ms (含 L2 + NLP + TTS)
      - 验证: L1-only 路径延迟 <100ms

   f. 去抖测试:
      - 模拟手势快速切换
      - 验证: 5 帧去抖生效, 无手势闪烁

   g. 模型热切换测试:
      - 修改 model_config.yaml 的 active_l1_model
      - 验证: Relay 自动切换模型, 不重启服务
      - 验证: 前端收到 "MODEL_SWITCHING" 状态消息

   h. 断线恢复测试:
      - 断开 WiFi → 重新连接
      - 验证: ESP32 自动重连, UDP 恢复发送
      - 断开 WebSocket → 重新连接
      - 验证: 前端自动重连, 数据恢复推送

3. 自动化测试脚本:
   - 使用 pytest 编写测试用例
   - 使用 pytest-asyncio 测试异步代码
   - 生成测试报告 (HTML)

4. 手动测试清单:
   □ ESP32 串口输出 CSV 格式正确
   □ BLE 配网成功
   □ WiFi UDP 发送正常
   □ Relay Swagger UI 可访问
   □ 前端页面加载正常
   □ 3D 手部骨架动画流畅
   □ 手势识别结果显示正确
   □ TTS 语音播放正常
```

**🎯 预期输出：**

- glove_relay/tests/test_integration.py - 自动化测试脚本
- 测试用例文档
- 手动测试清单
- pytest 配置

**✅ 验收标准：**

- [ ] UDP 丢包率 <1%（局域网环境）
- [ ] Protobuf → JSON 转换零错误
- [ ] WebSocket 广播到 3+ 客户端正常
- [ ] 端到端延迟 <200ms
- [ ] L1→L2 路由逻辑正确
- [ ] 去抖功能正常
- [ ] 断线恢复正常
- [ ] 所有 pytest 用例通过

---

## 10 Bug 修复专用

---

### P8.1 FreeRTOS Bug 修复

**💡 上下文说明：** 这是 V2 中发现并修复的关键 Bug。FreeRTOS `xTaskCreatePinnedToCore()` 的参数顺序错误会导致编译通过但运行时崩溃。此 Prompt 提供完整的诊断和修复步骤。

> **⚠️ Bug 详情**
>
> **症状:** ESP32 启动后立即崩溃，串口输出 `Guru Meditation Error: Core 0 panic'ed (CacheAccessError)`
>
> **原因:** `xTaskCreatePinnedToCore()` 的第 1 个参数传入了任务句柄变量（`TaskSensorReadHandle`，类型为 `TaskHandle_t`，本质是 `void**`），而非函数指针（`Task_SensorRead`，类型为 `TaskFunction_t`，本质是 `void (*)(void*)`）。FreeRTOS 将一个指针值当作函数入口地址执行，触发非法内存访问。
>
> **修复:** 交换第 1 和第 6 个参数的位置。

**📝 Prompt文本（可直接复制粘贴到 Claude Code）：**

```
请诊断并修复以下 FreeRTOS Bug:

错误信息:
  Guru Meditation Error: Core 0 panic'ed (CacheAccessError). 
  Core 0 register dump:
  PC      : 0x400d1f5a  
  EXCVADDR: 0x00000000  

错误代码 (src/main.cpp):
  TaskHandle_t TaskSensorReadHandle = NULL;
  TaskHandle_t TaskInferenceHandle = NULL;
  TaskHandle_t TaskCommsHandle = NULL;

  void setup() {
    // ... 传感器初始化 ...
  
    // ❌ 错误: 第一个参数传入了句柄变量!
    xTaskCreatePinnedToCore(
      TaskSensorReadHandle,    // ❌ 这是 TaskHandle_t, 不是函数指针!
      "SensorRead",
      4096,
      NULL,
      3,
      &TaskSensorReadHandle,   // ❌ 这才是句柄的地址!
      1
    );
    // ... 类似错误在 Task_Inference 和 Task_Comms 中也存在 ...
  }

修复步骤:

1. 将 xTaskCreatePinnedToCore 的参数顺序修正为:
   xTaskCreatePinnedToCore(
     Task_SensorRead,          // ✅ 函数指针 (TaskFunction_t)
     "SensorRead",             // 任务名称
     4096,                     // 栈大小
     NULL,                     // 参数
     3,                        // 优先级
     &TaskSensorReadHandle,    // ✅ 句柄指针 (TaskHandle_t*)
     1                         // 核心编号
   );

2. 参数顺序记忆法:
   (谁做什么, 叫什么, 多大空间, 给什么参数, 多重要, 句柄放哪, 跑在哪个核)

3. 添加编译期静态断言防止再次出错:
   #include <type_traits>
   static_assert(
     std::is_function_v<std::remove_pointer_t<decltype(Task_SensorRead)>>,
     "First argument to xTaskCreatePinnedToCore must be a function pointer!"
   );

4. 对所有三个任务创建进行相同修复:
   - Task_SensorRead → Core 1, 优先级 3
   - Task_Inference → Core 0, 优先级 2
   - Task_Comms → Core 0, 优先级 1
```

**🎯 预期输出：**

- 修复后的 src/main.cpp
- 编译期静态断言
- 三个任务创建代码全部正确

**✅ 验收标准：**

- [ ] 编译通过，无警告
- [ ] ESP32 启动后不再崩溃
- [ ] Task_SensorRead 在 Core 1 以 100Hz 运行
- [ ] static_assert 通过编译

---

### P8.2 ST-GCN 假实现修复

**💡 上下文说明：** 原论文 `l2_inference.py` 中的 `STGCNModel` 仅使用 `nn.Linear(42*30, 46)`，这只是一个单层全连接层，没有任何图卷积结构。必须替换为 P5.2 中描述的完整 ST-GCN 实现。

> **⚠️ 假实现代码（必须完全替换）**
>
> ```python
> # ❌ 假实现 - 必须删除!
> class STGCNModel(nn.Module):
>     def __init__(self):
>         super().__init__()
>         self.fc = nn.Linear(42*30, 46)  # 仅一层全连接!
>     def forward(self, x):
>         return self.fc(x.flatten(1))
> ```
>
> 问题: 输入直接 flatten 后接全连接，丢失了:
>
> 1. 所有空间信息（手指间的骨骼拓扑关系）
> 2. 所有时序信息（手势动作的动态变化）
> 3. 伪骨骼映射（传感器特征 → 2D 关键点坐标）

**📝 Prompt文本（可直接复制粘贴到 Claude Code）：**

```
请检查并修复 ST-GCN 假实现 Bug:

1. 检查是否存在以下模式:
   - 仅使用 nn.Linear 进行分类 (无图卷积)
   - 输入直接 flatten 后接全连接
   - 模型参数量极少 (<1K)

2. 如果发现假实现, 用 P5.2 的完整 ST-GCN 替换:
   - 确认 PseudoSkeletonMapping 层存在
   - 确认空间图卷积基于邻接矩阵实现
   - 确认时间卷积使用膨胀卷积
   - 确认至少 2 个 ST-Conv Block
   - 确认包含残差连接
   - 确认包含注意力池化

3. 验证修复:
   - 对比修复前后模型参数量 (假实现: <1K, 真实现: ~280K)
   - 运行 Benchmark (P3.5B) 验证准确率
   - 检查模型 FLOPs (假实现: <0.01M, 真实现: ~50M)

4. 确保替换后:
   - 训练脚本能正常运行
   - 模型能正常导出
   - Python Relay 能正常加载和推理
```

**🎯 预期输出：**

- 诊断结果（假实现/真实现）
- 修复后的模型代码（如果需要）
- 验证报告

**✅ 验收标准：**

- [ ] 模型包含 PseudoSkeletonMapping 层
- [ ] 模型包含空间图卷积（基于邻接矩阵）
- [ ] 模型参数量 >100K
- [ ] 不存在 `nn.Linear(42*30, 46)` 这种假实现
- [ ] Benchmark 验证 Top-1 >95%

---

## 附录

### A. 快速参考：V3 架构决策摘要

| 编号 | 决策        | 选型                                                   |
| ---- | ----------- | ------------------------------------------------------ |
| D1   | 前端渲染    | React 18 + Vite + R3F + Zustand + TailwindCSS (纯 Web) |
| D2   | Python 中继 | FastAPI + WebSockets (统一: relay + L2 + TTS + NLP)    |
| D3   | 专业渲染    | Unity 2022 LTS + XR Hands + ms-MANO (L3 Pro)           |
| D4   | 部署模式    | 响应式 Web App + 可选 PWA                              |
| D5   | BLE 用途    | 仅用于 WiFi 配网; 前端通过 WiFi → Python Relay        |
| D6   | L1 模型池   | 1D-CNN+Attention (主力) + MS-TCN (实验)                |
| D7   | 模型接口    | BaseModel 统一接口 + YAML 配置热切换                   |
| D8   | 评估框架    | Top-1/5 + FLOPs + latency + params                     |

### B. 端口分配表

| 端口     | 协议      | 用途                             | 方向 |
| -------- | --------- | -------------------------------- | ---- |
| 8888     | UDP       | ESP32 → Python Relay (Protobuf) | 入站 |
| 8765     | WebSocket | Python Relay → 前端 (JSON)      | 出站 |
| 8000     | HTTP      | FastAPI Swagger UI / API         | 入站 |
| 115200   | Serial    | ESP32 调试串口                   | 本地 |
| BLE GATT | BLE       | WiFi 配网 / 低速备用             | 入站 |

### C. 数据格式对照表

| 阶段           | 数据格式          | 说明                 |
| -------------- | ----------------- | -------------------- |
| ESP32 → Relay | Protobuf (Nanopb) | 二进制, <200 字节/帧 |
| Relay 内部     | Python dict       | 中间处理格式         |
| Relay → 前端  | JSON (WebSocket)  | 文本, ~500 字节/帧   |
| 前端渲染       | Three.js objects  | 3D 骨骼旋转矩阵      |

### D. 文件结构总览

```
Edge-AI-Data-Glove/
├── firmware/                    # PlatformIO 项目 (ESP32-S3)
│   ├── platformio.ini
│   ├── src/
│   │   ├── main.cpp             # FreeRTOS 任务调度
│   │   └── TFLiteInference.h    # TFLite Micro 推理
│   ├── lib/
│   │   ├── Sensors/             # TCA9548A, TMAG5273, BNO085
│   │   ├── Filters/             # KalmanFilter1D
│   │   ├── Comms/               # BLE, UDP, Protobuf
│   │   └── Models/              # BaseModel, ModelRegistry
│   └── test/
│
├── glove_relay/                 # Python Relay 中继服务
│   ├── src/
│   │   ├── main.py              # FastAPI 入口
│   │   ├── udp_server.py        # asyncio UDP (端口 8888)
│   │   ├── ws_server.py         # WebSocket (端口 8765)
│   │   ├── router.py            # L1→L2 置信度路由
│   │   ├── protobuf_parser.py   # Protobuf → JSON
│   │   ├── models/              # ST-GCN, BaseModel, Registry
│   │   ├── nlp/                 # CSL→普通话语法纠错
│   │   └── tts/                 # edge-tts 语音合成
│   ├── configs/
│   │   ├── model_config.yaml    # 模型热切换配置
│   │   └── relay_config.yaml    # 中继服务器配置
│   ├── scripts/
│   │   ├── data_collector.py    # 数据采集
│   │   └── run_benchmark.py     # 模型 Benchmark
│   ├── proto/
│   │   └── glove_data.proto     # Protobuf 定义
│   └── tests/
│
├── glove_web/                   # React 前端
│   ├── src/
│   │   ├── components/
│   │   │   ├── Hand3D/          # R3F 3D 手部骨架
│   │   │   ├── Dashboard/       # 结果面板
│   │   │   └── Settings/        # 设置
│   │   ├── stores/              # Zustand 状态管理
│   │   ├── hooks/               # useWebSocket, useHandAnimation
│   │   └── utils/               # 骨骼定义, 常量
│   └── public/
│       ├── manifest.json        # PWA 配置
│       └── sw.js                # Service Worker
│
├── glove_unity/                 # Unity L3 Pro (后续阶段)
│
├── CLAUDE_CODE_PROMPTS_V3.md    # 本文档
└── SOP_SPEC_PLAN_V3.md          # 配套规范文档
```

---

> **文档结束**
>
> 本文档共包含 **28 个 Prompt**（P0.1-P0.3, P1.1-P1.4, P2.1-P2.2, P3.1-P3.5, P3.5B, P4.1-P4.3, P5.1-P5.4, P6.1-P6.3, P7.1, P8.1-P8.2），覆盖 Edge AI 数据手套项目的全部开发阶段。每个 Prompt 均可直接复制粘贴到 Claude Code 执行，包含上下文说明、详细需求、预期输出和验收标准。
>
> **Version 3.0 | 2026-04-24 | 配套文档: SOP SPEC PLAN V3.0**
