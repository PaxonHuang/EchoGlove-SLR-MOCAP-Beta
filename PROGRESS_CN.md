# PROGRESS_CN.md — 跨会话状态追踪器

**最后更新**: 2026-05-28

---

## MCP 插件状态 (更新于 2026-05-15)

| 插件            | 状态           | 备注                                           |
| --------------- | -------------- | ---------------------------------------------- |
| Playwright      | 正常 (WORKING) |                                                |
| Chrome DevTools | 正常 (WORKING) |                                                |
| Context7        | 失败 (FAILING) | 代理路由问题 (`127.0.0.1:15721`)，间歇性出现 |
| GitHub MCP      | 失败 (FAILING) | Token 已配置，但需要重启会话生效               |
| Espressif Docs  | 失败 (FAILING) | 与代理相关，间歇性出现                         |

---

## 会话延续协议

开始新会话时：

1. 首先阅读此文件
2. 检查 MCP 状态表 — 如果最近已验证，则跳过重新测试
3. 从下方的最后一个检查点继续工作
4. 完成任务后更新此文件

---

## Windows → Ubuntu 迁移 (2026-05-15) — 已完成

所有跨平台兼容性问题已解决。在 Ubuntu 24.04 上构建通过。

### 配置清理

| 文件                            | 操作                                                            |
| ------------------------------- | --------------------------------------------------------------- |
| `.claude/settings.json`       | 清除了损坏的 Windows 钩子路径 (`H:/HandSignRecognition/...`)  |
| `.claude/settings.local.json` | 替换为 Ubuntu 原生权限配置 (git, pio, npm, python)              |
| `.gitignore`                  | 添加了 `.claude/settings.local.json` 以支持每操作系统本地配置 |

### 跨平台换行符

`.gitattributes` 强制所有源代码使用 LF 换行符，仅 `.bat/.ps1/.cmd/.vbs/.reg` 文件使用 CRLF。

### 迁移期间修复的 Bug (共 7 个)

| # | 文件                            | 问题                                                                                                                                                  | 修复方案                                   |
| - | ------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------ |
| 1 | `.claude/settings.json`       | 钩子中存在 Windows 绝对路径 `H:/HandSignRecognition/...`                                                                                            | 已清除                                     |
| 2 | `.claude/settings.local.json` | 包含 50+ 条 PowerShell规则及 `C:/Users/QuenchKidney/` 路径                                                                                          | 替换为 Ubuntu 规则                         |
| 3 | `SensorManager.h:44`          | [TMAG5273](file:///home/paxon/CodingProjects/EchoGloveProjects/EchoGlove-SLR-MOCAP-Alpha/glove_firmware/lib/Sensors/TMG5273.h#L27-L310) 没有默认构造函数 | 在成员初始化列表中初始化数组               |
| 4 | `SensorManager.h:214`         | `_imu.begin()` API 变更 (v1.2.5)                                                                                                                    | 更改为 `begin_I2C()`                     |
| 5 | `SensorManager.h:240/274`     | `_sensor_value.type` API 变更                                                                                                                       | 更改为 `.sensorId`                       |
| 6 | `FeatureNormalizer.h:34`      | `FLT_MAX` 未声明                                                                                                                                    | 添加 `#include <cfloat>`                 |
| 7 | `TMG5273.h:45/74/83`          | 类内部使用 `namespace` 无效 (C++)                                                                                                                   | `namespace` 改为 `struct` 并添加 `;` |

### 构建状态

`pio run` → **2 个环境成功** (esp32-s3-devkitc-1-n16r8 + debug), 2026-05-15

---

## 第一阶段 + 第二阶段完成 (2026-05-07)

### 第一阶段: HAL & 驱动层 — 已完成

| 组件                        | 文件                                                                                                                                                 | 状态                                                                    |
| --------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------- |
| TCA9548A I2C 多路复用器驱动 | `lib/Sensors/TCA9548A.h/.cpp`                                                                                                                      | 完成 (disableAll→selectChannel 两步操作，1ms 总线延迟)                 |
| TMAG5273 霍尔传感器驱动     | `lib/Sensors/TMG5273.h/.cpp`                                                                                                                       | 完成 (头文件实现，32次平均，±40mT，Set/Reset 触发)                     |
| BNO085 IMU 集成             | [lib/Sensors/SensorManager.h](file:///home/paxon/CodingProjects/EchoGloveProjects/EchoGlove-SLR-MOCAP-Alpha/glove_firmware/lib/Sensors/SensorManager.h) | 完成 (游戏旋转矢量 + 校准陀螺仪 @ 100Hz)                                |
| SensorManager 统一 HAL      | [lib/Sensors/SensorManager.h](file:///home/paxon/CodingProjects/EchoGloveProjects/EchoGlove-SLR-MOCAP-Alpha/glove_firmware/lib/Sensors/SensorManager.h) | 完成 (I2C 初始化, 多路复用, Hall 数组, IMU, 卡尔曼滤波, 四元数转欧拉角) |
| FlexManager 占位符          | [lib/Sensors/FlexManager.h](file:///home/paxon/CodingProjects/EchoGloveProjects/EchoGlove-SLR-MOCAP-Alpha/glove_firmware/lib/Sensors/FlexManager.h)     | 完成 (V3.0 返回零值, V3.1 将使用 ADC)                                   |
| FreeRTOS 双核任务           | [src/main.cpp](file:///home/paxon/CodingProjects/EchoGloveProjects/EchoGlove-SLR-MOCAP-Alpha/glove_firmware/src/main.cpp)                               | 完成 (static_assert 验证, 正确的参数顺序)                               |

### 第二阶段: 信号处理与数据采集 — 已完成

| 组件               | 文件                                                                                                                                                         | 状态                                                        |
| ------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------ | ----------------------------------------------------------- |
| 一维卡尔曼滤波     | [lib/Filters/KalmanFilter1D.h](file:///home/paxon/CodingProjects/EchoGloveProjects/EchoGlove-SLR-MOCAP-Alpha/glove_firmware/lib/Filters/KalmanFilter1D.h)       | 完成 (21 通道, 首次更新时自动种子初始化)                    |
| 滑动窗口环形缓冲区 | [lib/Filters/SlidingWindow.h](file:///home/paxon/CodingProjects/EchoGloveProjects/EchoGlove-SLR-MOCAP-Alpha/glove_firmware/lib/Filters/SlidingWindow.h)         | 完成 (30×21 浮点数, PSRAM 分配, 单生产者单消费者 SPSC)     |
| 特征归一化器       | [lib/Filters/FeatureNormalizer.h](file:///home/paxon/CodingProjects/EchoGloveProjects/EchoGlove-SLR-MOCAP-Alpha/glove_firmware/lib/Filters/FeatureNormalizer.h) | 完成 (Min-Max [0,1], 2秒校准, 每通道统计)                   |
| 管道集成           | [src/main.cpp](file:///home/paxon/CodingProjects/EchoGloveProjects/EchoGlove-SLR-MOCAP-Alpha/glove_firmware/src/main.cpp)                                       | 完成 (readAll→toFeatureArray→normalize→push→queue→CSV) |
| 串口 CSV 输出      | [src/main.cpp](file:///home/paxon/CodingProjects/EchoGloveProjects/EchoGlove-SLR-MOCAP-Alpha/glove_firmware/src/main.cpp)                                       | 完成 (兼容 Edge Impulse 数据转发器格式)                     |

### 信号处理管道流程

```
SensorManager.readAll()     → SensorData (在 SensorManager 内部进行卡尔曼滤波)
SensorData.toFeatureArray() → float[21] 特征数组
FeatureNormalizer.updateStats() → 在 2秒校准期间更新统计信息
FeatureNormalizer.normalize()  → 特征映射到 [0,1]
SlidingWindow.push()           → 环形缓冲区 (30 帧)
FreeRTOS queue send            → 将 SensorData 发送到 g_data_queue
Serial CSV output              → Edge Impulse 兼容格式输出
```

### 创建的单元测试

| 测试文件                                                                                                                                                     | 覆盖范围                                                                          | 平台   |
| ------------------------------------------------------------------------------------------------------------------------------------------------------------ | --------------------------------------------------------------------------------- | ------ |
| [test/test_tca9548a/test_tca9548a.cpp](file:///home/paxon/CodingProjects/EchoGloveProjects/EchoGlove-SLR-MOCAP-Alpha/glove_firmware/test/test_tca9548a/test_tca9548a.cpp)                 | TCA9548A 通道选择, disableAll, 探测                                               | ESP32  |
| [test/test_tmag5273/test_tmag5273.cpp](file:///home/paxon/CodingProjects/EchoGloveProjects/EchoGlove-SLR-MOCAP-Alpha/glove_firmware/test/test_tmag5273/test_tmag5273.cpp)                 | TMAG5273 初始化, readXYZ, 空多路复用器处理                                        | ESP32  |
| [test/test_euler_conversion/test_euler_conversion.cpp](file:///home/paxon/CodingProjects/EchoGloveProjects/EchoGlove-SLR-MOCAP-Alpha/glove_firmware/test/test_euler_conversion/test_euler_conversion.cpp) | 四元数转欧拉角 (5 种情况), SlidingWindow (5 种情况), FeatureNormalizer (5 种情况) | 原生   |
| [test/test_inference_trigger/test_inference_trigger.cpp](file:///home/paxon/CodingProjects/EchoGloveProjects/EchoGlove-SLR-MOCAP-Alpha/glove_firmware/test/test_inference_trigger/test_inference_trigger.cpp) | InferenceTrigger: 置信度门控, 防抖动, 静默期 (11 项测试)                          | 原生   |
| [test/test_mock_model/test_mock_model.cpp](file:///home/paxon/CodingProjects/EchoGloveProjects/EchoGlove-SLR-MOCAP-Alpha/glove_firmware/test/test_mock_model/test_mock_model.cpp)     | MockModel: 初始化, 预处理, 推理, 后处理, l2_requested (10 项测试)          | 原生   |
| [test/test_inference_pipeline/test_inference_pipeline.cpp](file:///home/paxon/CodingProjects/EchoGloveProjects/EchoGlove-SLR-MOCAP-Alpha/glove_firmware/test/test_inference_pipeline/test_inference_pipeline.cpp) | Pipeline: 窗口→模型→触发器集成 (6 项测试)               | 原生   |

**原生测试总数**: 42 通过 / 2 错误（依赖硬件）/ 44 总计

---

## 硬件调试与仿真模式 (2026-05-20) — 进行中

### 背景

ESP32-S3 通过 USB CDC 连接到 Ubuntu (`/dev/ttyACM0`)。硬件部分接线：
- **PCA9548A 多路复用器** (兼容TCA9548A): 连接在 GPIO8/9，使用 **2kΩ** 上拉电阻 — **I2C 无响应** (err=5 NACK)
- **GY-BNO085 IMU**: 连接在多路复用器 CH5 (SD5→SDA, SC5→SCL)，使用 **5.1kΩ** 子总线上拉电阻 — **无响应** (依赖多路复用器)
- **TMAG5273 霍尔传感器**: 未连接 (传感器尚未到货，使用仿真数据)
- **BNO085 INT**: 连接到 GPIO21

### 已修复的问题 (共 5 个)

| # | 问题 | 修复方案 |
|---|------|---------|
| 1 | `/dev/ttyACM0` 权限拒绝 | 安装 udev 规则 (`/etc/udev/rules.d/99-platformio-udev.rules`)，添加到 `dialout` 组 |
| 2 | ESP32-S3 USB CDC 串口启动后无输出 | 在 `platformio.ini` 中添加 `-DARDUINO_USB_CDC_ON_BOOT=1` |
| 3 | 核心转储校验和错误阻止闪存写入 | 上传前使用 `pio run -t erase` 擦除闪存 |
| 4 | I2C 扫描器挂起 (ESP-IDF 在 NACK 时阻塞) | 减少扫描范围至最小地址测试 (0x70, 0x4B, 0x22) |
| 5 | 编译错误：浮点类型标量初始化器周围的花括号 | 移除 `PROGMEM`，对 GestureSignature 数组使用双层花括号语法 `{{...}, ...}` |

### 硬件配置更新 (2026-05-21)

- **上拉电阻变更**: 主总线 5.1kΩ → **2kΩ** (更接近 SOP 2.2kΩ 规范，400kHz 时上升时间更快)
- **通道修复**: 固件 `MuxChannels::BNO085_IMU` 从 7 更改为 **5** 以匹配硬件接线 (SD5/SC5)
- **Ch5 子总线上拉电阻**: 在 SD5/SC5→3.3V 安装 **5.1kΩ** 用于 GY-BNO085
- **硬件说明**: 多路复用器实际为 **Adafruit TCA9548A 1-to-8 I2C 多路复用器扩展板**（非裸 PCA9548A DIP 芯片）。IMU 为 **7Semi GY-BNO085** 模块。
- **Ch0-4 子总线上拉电阻**: 未安装 (TMAG5273 尚未到货)。主总线 2kΩ 通过 TCA9548A 内部开关传递 — 测试足够。

### 仿真模式实现

当未检测到 TCA9548A 时，SensorManager 回退到合成数据生成：

- **20 个手势类别**，具有不同的特征（张开手掌、握拳、竖大拇指、OK 手势、和平手势、食指指点等）
- 霍尔传感器：0.1（未弯曲）/ 0.9（弯曲）+ ±0.05 噪声
- 欧拉角：0° 到 ±45° + ±2.0° 噪声
- 陀螺仪：0 度/秒（静态手势）
- 对合成数据应用卡尔曼滤波（验证信号处理管道）
- 每 3 秒自动切换手势（300 帧 @ 100Hz）
- CSV 输出不变 — 兼容 Edge Impulse 数据转发器
- 2 秒校准 (FeatureNormalizer) 然后归一化到 [0, 1]

### 已修改的文件

| 文件 | 变更 |
|------|------|
| `platformio.ini` | 添加 `-DARDUINO_USB_CDC_ON_BOOT=1` |
| `lib/Sensors/SensorManager.h` | 添加仿真模式、20 个手势签名、`readSimulated()` 方法 |
| `/etc/udev/rules.d/99-platformio-udev.rules` | 已创建 — 永久串口访问 |
| `docs/HARDWARE_WIRING_DEBUG_GUIDE_DM40B.md` | 已创建 — DM40B 万用表接线验证指南 |
| `docs/SESSION_SUMMARY_2026-05-20_HARDWARE_DEBUG_SIMULATION.md` | 已创建 — 完整会议总结 |

### 硬件调试历史 — 已解决 (2026-05-26)

**通过系统性隔离测试确定了根本原因：**

| 日期 | 发现 |
|------|------|
| 2026-05-22 | 第一块 I2C 多路复用器（裸 PCA9548A DIP-16）损坏 — 内部 SDA↔SCL 短路 (0.82kΩ) |
| 2026-05-24 | 发现实际硬件为 **Adafruit TCA9548A 扩展板**（非裸 PCA9548A）— 修正了引脚假设 |
| 2026-05-25 | 第二块 Adafruit TCA9548A 也失败 — 全部 NACK。通过直连 BNO085 旁路测试确认为 **损坏模块** |
| 2026-05-25 | **BNO085 确认可用**，地址为 **0x4B**（非 0x4A），固件已更新 |
| 2026-05-26 | 第三块 TCA9548A (PW548A TI 芯片) 安装。绕过面包板直连测试：**TCA9548A 在 0x70 确认 ACK** |

**根本原因**: **面包板接触不良**。所有 3 块多路复用器模块可能都是正常的 — 面包板弹簧夹在多次插拔后退化，导致 I2C 信号间歇性断开。物理总线测试（GPIO 翻转 + 上拉恢复）通过，但 I2C 协议失败，因为面包板在信号层面引入了电阻/间歇性问题。

**关键诊断证据：**
- 物理总线测试：SDA/SCL 翻转正常，恢复 0µs → 上拉电阻和 GPIO 完好
- 硬件 I2C：全部 NACK/TIMEOUT → 协议层失败
- 位操作 I2C：全部 NACK → 同样问题，排除 ESP32 外设问题
- **绕过面包板用杜邦线直连：TCA9548A 在 0x70 被找到 ✓** → 确认面包板为根本原因

**当前硬件状态：**
- **Adafruit TCA9548A (PW548A 芯片)**: **正常工作** — 通过杜邦线直连确认
- **BNO085**: 在地址 **0x4B** 正常工作，固件已更新
- **TMAG5273 霍尔传感器**: 尚未安装（等待新面包板 + 子总线上拉电阻）
- **I2C 总线**: 绕过面包板后确认正常工作

**需要操作**: 购买新面包板、新杜邦线，重新组装。用户今晚采购 (2026-05-26)。

---

## 当前工作

**当前**: Phase 5 Python Relay Server 已完成（133/133 测试）。硬件测试暂停 — 用户正在采购新组件（面包板、杜邦线、TCA9548A、TMAG5273 替换件）。明天：Phase 1–4 硬件重新验证。

### Phase 5 完成总结 (2026-05-28)

| 组件 | 测试 | 状态 |
|------|------|------|
| Protobuf 解析器 | 19/19 | 完成 |
| UDP 服务器 | 23/23 | 完成 |
| WebSocket 管理器 | 15/15 | 完成 |
| ST-GCN 模型 | 27/27 | 完成 |
| NLP 语法纠正器 | 15/15 | 完成 |
| TTS 引擎 | 13/13 | 完成 |
| ConfidenceRouter | 11/11 | 完成 |
| 集成测试 | 10/10 | 完成 |
| **总计** | **133/133** | **全部通过** |

新建文件：
- `src/confidence_router.py` — L1→L2 置信度驱动路由（从 UDPServer 提取）
- `tests/test_tts_engine.py` — TTS 引擎测试（通过 sys.modules 注入模拟 edge_tts）
- `tests/test_confidence_router.py` — 路由逻辑测试
- `tests/test_integration.py` — FastAPI 应用生命周期测试

修改文件：
- `src/main.py` — 添加 `/api/status`、`/api/tts/audio` 端点；将 NLP+TTS+ConfidenceRouter 接入 lifespan
- `src/udp_server.py` — 接受可选的 `router: ConfidenceRouter` 参数

### Conda 环境已就绪

| 环境 | Python | PyTorch | 用途 | 中继测试 |
|------|--------|---------|------|----------|
| `pytorch21_env` | 3.9.25 | 2.1.0+cpu | ST-GCN 训练、中继开发 | **133/133 ✓** |
| `tf216` | 3.11.9 | 2.1.0+cpu | TFLite 训练、模型导出 | **133/133 ✓** |

### 阶段状态汇总

| 阶段 | 名称 | 状态 |
|------|------|------|
| P0 | 项目初始化 | 已完成 |
| P1 | HAL & 驱动 | 已完成（代码）— **需用新面包板重新验证硬件** |
| P2 | 信号处理 | 已完成（代码）— **需用新面包板重新验证** |
| P3 | L1 边缘推理 — 管道 + TDD | 已完成（42/44 原生测试） |
| P3.5 | 模型基准测试 | 待定 |
| P4 | 通信 (BLE/UDP/Protobuf) | 已完成（133/133 中继测试） |
| P5 | Python Relay + L2 ST-GCN + NLP + TTS | **已完成**（133/133 测试） |
| P6 | Web 渲染 / Unity Pro | 已有脚手架 |
| P7 | 集成测试 | 待定 |

### 下一步（按顺序）

1. **硬件重新验证**（明天）：新面包板 I2C 扫描 → 传感器验证 → CSV 输出
2. **Edge Impulse 数据收集**（路径 A MVP）：训练 L1 1D-CNN 模型
3. **模型导出**：TFLite → 集成到固件
4. **Phase 6**：React + R3F 前端（WebSocket 消费者 + 3D 手部骨架）
---

## 第三阶段: L1 边缘推理 — TDD 完成 (2026-05-22)

### TDD 红-绿-重构总结

**InferenceTrigger** (交付物 E — SOP §6.6):
- 红: 9 失败, 2 通过（存根返回默认值）
- 绿: 11/11 通过 — 置信度阈值 0.85，5 帧防抖动，100ms 静默期
- 实现: `lib/Inference/InferenceTrigger.h`（97 行，仅头文件）

**MockModel** (管道测试启用器):
- 红: 9 失败, 1 通过（存根返回默认值）
- 绿: 10/10 通过 — 可配置输出，softmax/argmax 后处理，l2_requested 频段
- 实现: `lib/Models/MockModel.h`（仅头文件，无 Arduino/TFLite 依赖）

**InferencePipeline** (管道粘合代码):
- 红: 3 失败, 3 通过（存根返回 false）
- 绿: 6/6 通过 — SlidingWindow → ModelRegistry → InferenceTrigger 流程
- 实现: `lib/Inference/InferencePipeline.h`（仅头文件）

**Task_Inference 接线**:
- ModelRegistry + InferenceTrigger 全局变量在 main.cpp 中实例化
- Task_Inference 现在在活动模型上调用 `runInferencePipeline()`
- 确认手势作为 `InferenceResult` 推送到 `g_inference_queue`
- ESP32 构建：两个环境均通过（常规 + 调试）

### 基础设施修复
- `lib/data_structures.h` → 重定向到 `include/data_structures.h`（单一事实来源）
- `include/data_structures.h` → `#ifdef UNIT_TEST` 存根（Serial, ps_malloc, PROGMEM）
- `platformio.ini` → 添加 `[env:native]` 用于快速 TDD 循环（~1秒 vs ~100秒 ESP32）
- 测试目录重命名为 `test_*` 前缀（PlatformIO 发现要求）
- 所有测试文件中的 Arduino `setup()/loop()` 存根
- TFLiteModel.h → 为新 API 修复 `MicroInterpreter` 构造函数（ErrorReporter 参数）

### 第三阶段交付物状态

| 交付物 | 代码 | 测试 | 备注 |
|-------------|------|--------|-------|
| A. Edge Impulse MVP | 推迟 | — | 需要数据收集 |
| B. 1D-CNN+Attention 训练 | 已编写 | 否 | 需要训练数据 |
| C. MS-TCN 训练 | 已编写 | 否 | 需要训练数据 |
| D. BaseModel + Registry + 热切换 | 已完成 | 框架 | `BaseModel.h`, `ModelRegistry.h` |
| E. 推理触发器 | 已完成 | **11/11** | TDD 完成 |
| F. TFLite Micro 集成 | 已完成 | 构建通过 | 需要 `model_data.h`（训练好的模型） |

### 阻塞依赖

**训练数据** 阻塞：交付物 A/B/C/F 完整验证，第三阶段 3.5 基准测试。
仿真模式提供合成的 20 手势数据用于管道测试。

---

## 第五阶段: Python Relay — TDD 基础设施 (2026-05-22)

### Protobuf 架构同步

固件 `.proto` 确立为单一事实来源。中继的 `glove_data.proto` 被固件的规范版本覆盖（包 `data_glove`，`hall_features` float，`l1_gesture_id`，`l1_confidence`，`l2_requested`，`status` string）。通过 `grpcio-tools` 重新生成 Python `glove_data_pb2.py`。

### TDD 红-绿-重构总结

**protobuf_parser** (19 项测试):
- 绿: 19/19 — 解析有效 protobuf，无效数据处理，往返测试，`build_glove_data_dict` 辅助函数
- Proto3 空字节行为：返回默认值（非错误）

**UDPServer** (23 项测试):
- 绿: 23/23 — 构造，数据报处理，L1→L2 路由，防抖动，静默期，缓冲区累积
- Bug 修复：`_last_gesture_time` 在每个缓冲区追加时设置（过早阻塞静默期内的帧）。移至仅在 L2 实际触发后更新。这是 TDD 发现的真实 bug。

**ST-GCN Model** (27 项测试):
- 绿: 27/27 — 邻接矩阵，GraphConv，TemporalConv，STConvBlock，AttentionPooling，完整模型端到端
- 已验证：输出形状，梯度流，预测 API，配置序列化

**WebSocket ConnectionManager** (15 项测试):
- 绿: 15/15 — 连接/断开生命周期，向所有/JSON/unicode 广播，移除死亡客户端，`close_all` 优雅关闭

**完整中继测试套件**: 84/84 通过，用时 2.43 秒

### 测试文件

| 测试文件 | 测试数 | 覆盖范围 |
|-----------|-------|----------|
| `tests/test_protobuf_parser.py` | 19 | Protobuf 解码，无效数据，往返测试，字典构建器 |
| `tests/test_udp_server.py` | 23 | 服务器初始化，数据报处理，L1→L2 路由，防抖动，静默期 |
| `tests/test_stgcn_model.py` | 27 | 邻接矩阵，GraphConv，TemporalConv，STConvBlock，AttnPool，STGCNModel |
| `tests/test_ws_server.py` | 15 | 连接生命周期，广播，清理 |

### TDD 发现的 Bug

**静默期门控 bug** (`udp_server.py:158`): `_last_gesture_time = now` 位于外部 `if` 块内（在通过防抖的每个低置信度帧上执行）。这导致静默期在**任何**缓冲区追加后阻塞所有后续帧 800ms，而不是仅在 L2 触发后。修复：将 `_last_gesture_time = now` 移至 `if len(buffer) >= window_size` 块内，此处 L2 实际触发。

### 已安装的依赖

`fastapi`, `websockets`, `pyyaml`, `numpy`, `protobuf`, `grpcio-tools`, `pytest`, `pytest-asyncio`, `torch` (CPU)
