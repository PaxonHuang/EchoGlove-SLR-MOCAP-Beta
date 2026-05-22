# 会议总结 - 硬件调试与仿真模式实施

**日期**：2026-05-20  
**项目**：EdgeAI数据手套V3  
**阶段**：P3 - L1边缘推理（Edge Impulse MVP路径A）  
**会议重点**：硬件调试、I2C通信故障排查、仿真模式实施

---

## 项目状态概览

### 当前阶段
**第三阶段（进行中）**：L1边缘推理，采用Edge Impulse MVP方案
- 路径A：使用`edge-impulse-data-forwarder`与串口CSV输出 → 在Edge Impulse中训练1D-CNN → 导出Arduino库 → 集成到固件
- 目标：<3ms推理延迟，20个手势类别（简化版），>90% Top-1准确率

### 硬件就绪状态
- **ESP32-S3-DevKitC-1 N16R8**：通过USB CDC连接（端口：`/dev/ttyACM0`）
- **PCA9548A I2C多路复用器**：已接线但I2C无响应（地址0x70）
- **BNO085 IMU**：已连接至多路复用器通道5但无响应
- **TMAG5273霍尔传感器**：尚未到货（计划使用5个传感器）
- **状态**：固件可运行，硬件调试进行中

---

## 硬件配置

### 接线详情

**主I2C总线**（ESP32-S3 → PCA9548A）：
```
ESP32-S3                PCA9548A               说明
────────                ──────────             ─────
GPIO8 (SDA)  ──────────→  SDA          主总线数据线
GPIO9 (SCL)  ──────────→  SCL          主总线时钟线
3.3V ──[2kΩ]── GPIO8                      上拉电阻（SDA）
3.3V ──[2kΩ]── GPIO9                      上拉电阻（SCL）
3.3V         ──────────→  VCC           电源
GND          ──────────→  GND           接地
/RST         ──────────→  3.3V          复位引脚拉高
```

**多路复用器通道分配**：
- **通道0-4**：TMAG5273霍尔传感器（SD0-SD4/SC0-SC4）— **尚未连接**
- **通道5**：BNO085 IMU（SD5/SC5）— 已连接
- **通道6-7**：预留/未使用

**BNO085 IMU连接**（通道5）：
```
PCA9548A CH5            BNO085                 说明
───────────             ───────                ─────
SD5         ──────────→  SDA           通道5下游总线
SC5         ──────────→  SCL           通道5下游时钟
3.3V        ──────────→  VCC           电源（通过主总线）
GND         ──────────→  GND           接地
GPIO21      ──────────→  INT           中断引脚（可选）
PS0         ──────────→  GND           地址位0 = 0
PS1         ──────────→  GND           地址位1 = 0
                                    → I2C地址：0x4A
```

### 上拉电阻配置

**主总线**：2kΩ上拉电阻（SDA/SCL）— **已安装**  
**子通道**：4.7kΩ上拉电阻 — **未安装**（TMAG5273传感器到货后添加）

**SOP规范**（docs/HARDWARE_ASSEMBLY_GUIDE.md）：  
- 主总线：推荐2kΩ（用户使用了2kΩ — 可接受但上升时间较慢）
- 子通道：每个下游传感器总线4.7kΩ

---

## 关键决策

### 1. 仿真模式实施

**决策**：在PCA9548A未检测到时实施优雅降级  
**理由**：
- 硬件调试需要时间（物理接线验证、万用表测量）
- 固件开发不应被硬件问题阻塞
- Edge Impulse数据收集可在硬件调试期间使用合成数据
- 允许并行开发：固件 + 硬件组装

**实施方案**：
- 修改`SensorManager.h`以检测PCA9548A初始化失败
- 回退到20个手势类别的合成数据生成
- 每3秒自动切换手势（300帧 @ 100Hz采样率）
- 卡尔曼滤波仍应用于合成数据（验证信号处理管道）
- CSV输出格式不变（兼容Edge Impulse数据转发器）

### 2. 手势类别选择

**决策**：20个手势类别（简化版）  
**选项**：5类（最小）、20类（简化）、46类（完整）  
**理由**：
- 5类对于有意义的分类过于有限
- 46类需要复杂数据集和更长的训练时间
- 20类为初始概念验证提供良好的覆盖范围
- 硬件完全功能后可以扩展到46类

### 3. 硬件调试方法

**决策**：提供详细的DM40B万用表接线验证指南  
**理由**：
- 用户有万用表但可能不熟悉I2C总线测试程序
- 系统验证可减少猜测和试错时间
- 书面指南允许用户独立调试，无需Claude协助
- 安全提醒可防止万用表损坏（在通电电路上测量电阻）

---

## 遇到的问题

### 1. `/dev/ttyACM0`权限拒绝

**错误**：`PermissionError: [Errno 13] Attempted access to '/dev/ttyACM0'`

**根本原因**：用户不在`dialout`组中，设备权限受限

**解决方案**：
1. 用户提供了sudo密码（单个空格字符）
2. 安装了udev规则：`/etc/udev/rules.d/99-platformio-udev.rules`
3. 将用户添加到`dialout`组（已是成员，需要刷新会话）
4. 永久修复：udev规则为ttyACM设备设置`MODE="0666"`

**代码参考**：见`/tmp/99-platformio-udev.rules`（会话期间创建的临时副本）

### 2. 启动后USB CDC串口无输出

**症状**：ESP32-S3引导加载程序消息可见，但重置后Arduino应用程序无响应

**根本原因**：ESP32-S3 USB CDC需要显式初始化标志

**解决方案**：在`platformio.ini`的build_flags中添加`-DARDUINO_USB_CDC_ON_BOOT=1`

**修改文件**：`glove_firmware/platformio.ini`  
**行**：`-DARDUINO_USB_CDC_ON_BOOT=1`

### 3. 核心转储校验和错误阻止启动

**错误**：先前的固件上传留下损坏的闪存，阻止新上传

**解决方案**：使用`pio run -t erase`擦除整个闪存后再上传

**命令**：`pio run -t erase`（完全闪存擦除）

### 4. I2C扫描在地址扫描期间挂起

**症状**：ESP-IDF I2C驱动程序在扫描地址时阻塞，扫描器挂起

**根本原因**：ESP-IDF I2C实现在NACK响应时具有阻塞行为

**解决方案**：减少扫描范围，仅测试最小地址范围（0x70、0x4A、0x22），而非全范围扫描

**代码位置**：`lib/Sensors/PCA9548A.h`（I2C扫描器实现）

### 5. 编译错误：标量初始化器周围的花括号

**错误**：`error: braces around scalar initializer for type 'float'`

**根本原因**：`PROGMEM` + 三层花括号语法与包含浮点数组的结构体不兼容  
**代码**：
```cpp
// 错误
static const GestureSignature gestures[20] PROGMEM = {
    {{0,0,0.1f, 0,0,0.1f, ...}, 0,0,0, 0,0,0},  // 三层花括号 + PROGMEM
};

// 正确
static const GestureSignature gestures[20] = {
    {{0,0,0.1f, 0,0,0.1f, ...}, 0,0,0, 0,0,0},  // 双层花括号，无PROGMEM
};
```

**解决方案**：移除`PROGMEM`限定符，使用双层花括号语法进行结构体初始化

**修改文件**：`lib/Sensors/SensorManager.h`（第331-372行）

### 6. I2C设备无响应（PCA9548A、BNO085）

**症状**：地址0x70（PCA9548A）和0x4A（BNO085）显示`I2C scan: err=5 (NACK)`

**状态**：**未解决** — 需要硬件接线验证

**怀疑原因**：
- 设备可能未通电（模块上无LED可见）
- 接线可能未正确连接（跳线可能松动）
- 上拉电阻不正确（使用2kΩ而非推荐的2kΩ — 可能导致上升时间较慢）

**下一步行动**：遵循DM40B万用表调试指南（见下文）

---

## 实施的解决方案

### 1. 串口访问的Udev规则

**文件**：`/etc/udev/rules.d/99-platformio-udev.rules`  
**内容**：ESP32-S3和常见开发板的永久串口访问规则

```bash
# ESP32-S3 DevKit (USB CDC ACM)
SUBSYSTEM=="tty", ATTRS{idVendor}=="303a", ATTRS{idProduct}=="1001", MODE="0666"
SUBSYSTEM=="tty", ATTRS{idVendor}=="303a", ATTRS{idProduct}=="000?", MODE="0666"

# 通用USB CDC ACM
KERNEL=="ttyACM[0-9]*", MODE="0666"
KERNEL=="ttyUSB[0-9]*", MODE="0666"
```

**验证**：`ls -l /dev/ttyACM0`应显示`crw-rw-rw-`（模式0666）

### 2. PlatformIO USB CDC构建标志

**文件**：`glove_firmware/platformio.ini`  
**修改**：添加USB CDC初始化标志

```ini
build_flags =
    -DBOARD_HAS_PSRAM
    -DCORE_DEBUG_LEVEL=3
    -DARDUINO_USB_CDC_ON_BOOT=1    # ← 为ESP32-S3 USB CDC串口添加
    -DCONFIG_ASYNC_TCP_USE_WDT=0
    ...
```

**结果**：启动后立即有串口输出（无延迟）

### 3. SensorManager仿真模式

**文件**：`lib/Sensors/SensorManager.h`  
**关键修改**：

#### 添加仿真模式状态
```cpp
private:
    bool      _simulation_mode;
    uint8_t   _sim_gesture_id;
    uint32_t  _sim_frame_counter;
    uint32_t  _sim_transition_timer;
```

#### begin()中的优雅降级
```cpp
bool begin() {
    // ... I2C初始化 ...
    
    if (!_mux.begin()) {
        Serial.println("[SensorManager] 警告：未找到PCA9548A！");
        Serial.println("[SensorManager] 进入仿真模式");
        Serial.println("[SensorManager] 20个手势类别，每个循环3秒");
        _simulation_mode = true;
        _initialized = true;
        _imu_ok = false;
        return true;  // 仍返回true — 仿真模式有效
    }
    
    // ... 正常初始化 ...
}
```

#### 合成手势数据生成
```cpp
struct GestureSignature {
    float hall[15];           // 5个传感器 × 3轴
    float roll, pitch, yaw;   // 欧拉角（度）
    float gx, gy, gz;         // 陀螺仪（度/秒）
};

static const GestureSignature gestures[20] = {
    // 0: 张开手掌
    {{0,0,0.1f, 0,0,0.1f, 0,0,0.1f, 0,0,0.1f, 0,0,0.1f}, 0,0,0, 0,0,0},
    // 1: 握拳
    {{0,0,0.9f, 0,0,0.9f, 0,0,0.9f, 0,0,0.9f, 0,0,0.9f}, 0,0,0, 0,0,0},
    // 2: 竖大拇指
    {{0,0,0.1f, 0,0,0.9f, 0,0,0.9f, 0,0,0.9f, 0,0,0.9f}, -45,0,0, 0,0,0},
    // ... 还有17个手势定义 ...
};

bool readSimulated(SensorData& data) {
    // 为霍尔传感器添加噪声 ±0.05
    // 为欧拉角添加噪声 ±2.0度
    // 应用卡尔曼滤波（验证信号处理管道）
    // 每300帧自动切换手势
    return true;
}
```

**数据值**：
- **霍尔传感器**：0.1（未弯曲）或0.9（弯曲）+ ±0.05噪声
- **欧拉角**：0°到±45° + ±2.0°噪声
- **陀螺仪**：0度/秒（静态手势）
- 2秒校准后（FeatureNormalizer），值归一化到[0, 1]范围

### 4. DM40B万用表接线调试指南

**已创建文件**：`docs/HARDWARE_WIRING_DEBUG_GUIDE_DM40B.md`  
**用途**：I2C调试前的系统硬件验证程序

**关键部分**：
- 电源验证（VCC/GND连通性）
- I2C上拉电阻测量（预期2kΩ）
- GPIO → PCA9548A SDA/SCL连通性
- PCA9548A /RST引脚连接（必须为高电平）
- BNO085电源、I2C总线（CH5）、地址引脚（PS0/PS1）
- I2C空闲状态电压测量（GPIO8/9应为3.3V）
- 安全注意事项（仅在断电时测量电阻）

**使用方式**：用户可独立验证所有物理连接，然后再得出硬件问题结论

---

## 代码架构参考

### 数据流（仿真模式）

```
ESP32-S3固件（仿真模式）
────────────────────────────────────

Task_SensorRead（核心1，100Hz）：
    ├─ SensorManager.readAll()
    │  ├─ PCA9548A.begin() → 失败 → _simulation_mode = true
    │  └─ readSimulated(data)
    │     ├─ 从gestures[20]数组选择手势
    │     ├─ 添加高斯噪声（霍尔±0.05，欧拉±2.0°）
    │     ├─ 应用卡尔曼滤波（KalmanFilter1D）
    │     └─ 每300帧自动切换手势
    │
    ├─ FeatureNormalizer.updateStats() [前200帧]
    ├─ FeatureNormalizer.normalize() [校准后]
    ├─ SlidingWindow.push(normalized_features)
    └─ 串口CSV输出 → Edge Impulse数据转发器

Task_Inference（核心0，30Hz）：
    └─ 占位符 — 将在第三阶段加载TFLite Micro模型

Task_Comms（核心0）：
    └─ 占位符 — 将在第四阶段实现BLE/UDP
```

### CSV输出格式

```
timestamp_us, hall0x, hall0y, hall0z, hall1x, ..., hall4z, roll, pitch, yaw, gx, gy, gz

示例：
250701, 0.1234, 0.0567, 0.0891, ...  # 归一化后
```

**注意**：像250701这样的大数字是**微秒级时间戳**，而非传感器值。归一化后的传感器值应在[0, 1]范围内。

---

## 调试方法论

### 硬件验证工作流程

**第一阶段：电源**（ESP32-S3断电 → 上电）
- 测量3.3V电源轨电压（应为3.25-3.35V）
- 验证PCA9548A VCC/GND连接（0Ω连通性）

**第二阶段：上拉电阻**（ESP32-S3断电）
- 测量GPIO8 → 3.3V电阻（~2kΩ）
- 测量GPIO9 → 3.3V电阻（~2kΩ）
- 验证对地无短路

**第三阶段：I2C总线连通性**（ESP32-S3断电）
- GPIO8 → PCA9548A SDA（0Ω）
- GPIO9 → PCA9548A SCL（0Ω）
- SDA/SCL之间无交叉短路

**第四阶段：空闲状态验证**（ESP32-S3上电）
- GPIO8电压 = 3.3V（上拉电阻工作）
- GPIO9电压 = 3.3V（上拉电阻工作）

**第五阶段：设备特定检查**
- PCA9548A /RST引脚 → 3.3V（必须为高电平）
- BNO085 PS0/PS1 → GND（地址0x4A）

**所需工具**：
- DM40B数字万用表（或等效）
- 电阻模式：200Ω和20kΩ量程
- 直流电压模式：20V量程

**安全**：始终在**断电**时测量电阻，以避免损坏万用表

---

## 下一步行动

### 即时行动（硬件方向）

1. **硬件验证**（用户操作）：
   - 遵循`docs/HARDWARE_WIRING_DEBUG_GUIDE_DM40B.md`
   - 使用DM40B万用表验证所有物理连接
   - 检查PCA9548A和BNO085模块是否显示电源LED
   - 测量GPIO8/9空闲电压（应为3.3V）

2. **如果接线验证通过 → 重新测试I2C**：
   - 重新上电ESP32-S3
   - 运行最小I2C扫描器（仅地址0x70、0x4A）
   - 如果PCA9548A响应 → 测试通道5上的BNO085
   - 如果仍失败 → 尝试更换模块或不同的I2C速度（100kHz而非400kHz）

3. **TMAG5273传感器到货**（未来）：
   - 将5个传感器连接到通道0-4
   - 在每个子通道添加4.7kΩ上拉电阻
   - 验证所有传感器在地址0x22响应
   - 从仿真模式切换到真实传感器管道

### 即时行动（软件方向 — 可立即进行）

1. **Edge Impulse数据收集**（仿真模式）：
   ```bash
   # 安装Edge Impulse CLI
   npm install -g edge-impulse-cli
   
   # 使用仿真固件启动数据转发器
   edge-impulse-data-forwarder --baud-rate 115200 --frequency 100
   
   # 固件将自动循环20个手势
   # 每个手势类别至少收集3秒 × 20个手势 = 最少60秒
   # 建议：每个手势30-60个样本（总计900-1800秒）
   ```

2. **训练1D-CNN模型**：
   - 将收集的数据上传到Edge Impulse项目
   - 设计脉冲：1D-CNN + Attention架构
   - 训练模型（目标：20类>90%准确率）
   - 导出Arduino库

3. **集成到固件**：
   - 将Edge Impulse导出的库复制到`glove_firmware/lib/Models/EI_Model/`
   - 修改`ModelRegistry`以加载EI模型
   - 实现`Task_Inference`在SlidingWindow数据上运行模型
   - 测试推理延迟（目标：<3ms每帧）

4. **模型基准比较**（第三阶段.5）：
   - 比较Edge Impulse 1D-CNN与自定义TFLite Micro模型
   - 评估准确率、延迟、内存使用
   - 选择最佳模型进行部署

### 长期行动（第四至七阶段）

- **第四阶段**：BLE配置 + WiFi UDP通信
- **第五阶段**：Python中继 + L2 ST-GCN推理 + NLP + TTS
- **第六阶段**：Web前端（React + R3F）/ Unity专业骨骼
- **第七阶段**：端到端集成测试

---

## 修改的文件

| 文件 | 状态 | 变更 |
|------|--------|---------|
| `glove_firmware/platformio.ini` | 已修改 | 添加`-DARDUINO_USB_CDC_ON_BOOT=1` |
| `glove_firmware/lib/Sensors/SensorManager.h` | 已修改 | 添加仿真模式、20个手势签名 |
| `/etc/udev/rules.d/99-platformio-udev.rules` | 已创建 | 永久串口访问规则 |
| `docs/HARDWARE_WIRING_DEBUG_GUIDE_DM40B.md` | 已创建 | 万用表接线验证指南 |

---

## 性能目标

| 指标 | 目标 | 当前状态 |
|--------|--------|----------------|
| 传感器采样率 | 100Hz | ✓ 可运行（仿真模式） |
| 校准持续时间 | 2秒（200帧） | ✓ 可运行 |
| 特征归一化 | [0, 1]范围 | ✓ 可运行 |
| 手势类别 | 20（初始） | ✓ 可运行（仿真） |
| L1推理延迟 | <3ms | 待实现（第三阶段实施） |
| L1准确率 | >90% Top-1 | 待实现（Edge Impulse训练后） |

---

## 已知问题

1. **I2C设备无响应**：
   - 未检测到PCA9548A（0x70）和BNO085（0x4A）
   - 需要使用万用表进行硬件接线验证
   - 仿真模式允许开发独立进行

2. **上拉电阻值**：
   - 使用了2kΩ而非SOP推荐的2kΩ
   - 可能导致I2C总线上升时间较慢
   - 可能影响400kHz时的通信可靠性
   - 如果I2C问题持续存在，考虑切换到2kΩ

3. **子通道上拉电阻**：
   - 尚未安装（TMAG5273传感器未连接）
   - 每个下游通道需要4.7kΩ上拉电阻
   - 等待传感器到货后再添加

---

## 参考文档

- **SOP规范**：`docs/SOP_SPEC_PLAN_V3.md`（938行）
- **Claude Code提示**：`docs/CLAUDE_CODE_PROMPTS_V3.md`（28个提示）
- **硬件组装指南**：`docs/HARDWARE_ASSEMBLY_GUIDE.md`
- **接线调试指南**：`docs/HARDWARE_WIRING_DEBUG_GUIDE_DM40B.md`（本会话创建）
- **项目说明**：`CLAUDE.md`（项目根目录）
- **进度跟踪**：`PROGRESS.md`（本会话后应更新）

---

## 会议总结

**变更内容**：
- 固件现在支持无物理传感器的仿真模式
- 创建了硬件调试指南用于独立故障排查
- 使用udev规则永久修复串口访问
- 启用ESP32-S3启动输出的USB CDC初始化

**下一步**：
- 使用DM40B万用表进行硬件验证（用户操作）
- 使用仿真模式进行Edge Impulse数据收集（可立即进行）
- 模型训练和集成（第三阶段核心工作）

**状态**：仿真模式下固件可运行，硬件调试进行中，软件开发未受阻

---

**生成时间**：2026-05-20  
**会议持续时间**：约2小时  
**主要成果**：仿真模式实施 + 硬件调试指南  
**阻塞问题**：I2C设备无响应（需要物理验证）  
**并行工作已启用**：Edge Impulse数据收集可使用合成数据进行
