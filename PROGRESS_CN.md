以下是 [PROGRESS.md](file:///home/paxon/CodingProjects/EchoGloveProjects/EchoGlove-SLR-MOCAP-Alpha/PROGRESS.md) 和 [platformio.ini](file:///home/paxon/CodingProjects/EchoGloveProjects/EchoGlove-SLR-MOCAP-Alpha/platformio.ini) 文件的中文翻译。

---

# PROGRESS.md — 跨会话状态追踪器

**最后更新**: 2026-05-15

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

| 测试文件                                                                                                                                                     | 覆盖范围                                                                          |
| ------------------------------------------------------------------------------------------------------------------------------------------------------------ | --------------------------------------------------------------------------------- |
| [tests/test_tca9548a.cpp](file:///home/paxon/CodingProjects/EchoGloveProjects/EchoGlove-SLR-MOCAP-Alpha/glove_firmware/tests/test_tca9548a.cpp)                 | TCA9548A 通道选择, disableAll, 探测                                               |
| [tests/test_tmag5273.cpp](file:///home/paxon/CodingProjects/EchoGloveProjects/EchoGlove-SLR-MOCAP-Alpha/glove_firmware/tests/test_tmag5273.cpp)                 | TMAG5273 初始化, readXYZ, 空多路复用器处理                                        |
| [tests/test_euler_conversion.cpp](file:///home/paxon/CodingProjects/EchoGloveProjects/EchoGlove-SLR-MOCAP-Alpha/glove_firmware/tests/test_euler_conversion.cpp) | 四元数转欧拉角 (5 种情况), SlidingWindow (5 种情况), FeatureNormalizer (5 种情况) |

---

## 当前工作

**下一步**: 第三阶段 — L1 边缘推理 (TinyML / TFLite Micro)

### 优先级: 路径 A — Edge Impulse MVP (快速验证)

根据 SOP §6.1，ESP32 CSV 输出已兼容 `edge-impulse-data-forwarder`。步骤如下：

1. 安装 edge-impulse-cli: `npm install -g edge-impulse-cli`
2. 启动数据转发器: `edge-impulse-data-forwarder`
3. 在 Edge Impulse Studio 中收集带标签的手势数据
4. 训练 1D-CNN 分类器 (200 epochs, lr=0.001)
5. 导出为 Arduino 库 → 通过 PlatformIO `lib_deps` 集成

**目标**: 2-3 天内完成 MVP 验证

路径 B (PyTorch → TFLite INT8) 推迟至 Phase 3.5 基准测试阶段。

### 阶段状态汇总

| 阶段 | 名称                     | 状态                |
| ---- | ------------------------ | ------------------- |
| P0   | 项目初始化               | 已完成              |
| P1   | HAL & 驱动               | 已完成              |
| P2   | 信号处理                 | 已完成              |
| P3   | L1 边缘推理              | **← 下一步** |
| P3.5 | 模型基准测试             | 待定                |
| P4   | 通信 (BLE/UDP/Protobuf)  | 已有脚手架          |
| P5   | Python Relay + L2 ST-GCN | 已有脚手架          |
| P6   | Web 渲染 / Unity Pro     | 已有脚手架          |
| P7   | 集成测试                 | 待定                |

---

# platformio.ini 翻译

```ini
; PlatformIO 项目配置文件
;
;   构建选项: 构建标志, 源文件过滤
;   上传选项: 自定义上传端口, 速度及额外标志
;   库选项: 依赖项, 额外库存储
;   高级选项: 额外脚本
;
; 请访问文档了解其他选项和示例
; https://docs.platformio.org/page/projectconf.html

[env:esp32-s3-devkitc-1-n8r8]
platform = espressif32 @ ^6.5.0
board = esp32-s3-devkitc-1
framework = arduino
board_build.psram = enable
board_build.arduino.memory_type = qio_opi
monitor_speed = 115200
build_flags = 
	-DBOARD_HAS_PSRAM
	-DCORE_DEBUG_LEVEL=3
	-std=gnu++17
lib_deps = 
	nanopb/Nanopb @ ^0.4.7
	adafruit/Adafruit BNO08x @ ^1.2.5
	https://github.com/sparkfun/SparkFun_TMAG5273_Arduino_Library.git
	adafruit/Adafruit BusIO @ ^1.14.1
	h2zero/NimBLE-Arduino @ ^1.4.1
	links2004/WebSockets @ ^2.4.1
	Wire
	7semi-solutions/7Semi_TMAG5273@^1.0.0
```
