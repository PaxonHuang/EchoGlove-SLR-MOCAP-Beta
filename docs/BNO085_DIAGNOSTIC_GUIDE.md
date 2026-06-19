# GY-BNO085 模块验证诊断指南

> 适用平台：ESP32-S3-DevKitC-1 N16R8 + Adafruit GY-BNO085
> 框架：PlatformIO Arduino
> 目的：验证全新 BNO085 模块是否正常工作（独立测试，不依赖 BLE/TFLite）

---

## 1. 硬件接线

### 1.1 GY-BNO085 模块引脚说明

GY-BNO085 模块基于 Bosch BNO080/BNO085 RVC，通过 I²C 与主控通信。模块上通常有以下引脚：

| 模块引脚 | 功能 | 连接目标 | 说明 |
|----------|------|----------|------|
| **VCC** | 电源 | **3V3** | 必须 3.3V，**禁止接 5V**（会烧毁模块） |
| **GND** | 地线 | **GND** | 必须与 ESP32-S3 共地 |
| **SDA** | I²C 数据 | **GPIO 8** | 需 4.7kΩ 上拉到 3V3 |
| **SCL** | I²C 时钟 | **GPIO 9** | 需 4.7kΩ 上拉到 3V3 |
| **INT** | 中断输出 | **GPIO 1** | 下降沿触发（可选，不接也行） |
| **RST** | 复位 | **3V3** | 直接硬连到 3V3，不接 GPIO |
| **PS0** | 协议选择 0 | **3V3** | PS0=3V3 → I²C 模式 |
| **PS1** | 协议选择 1 | **GND** | PS1=GND → I²C 模式 |
| **ADO** | I²C 地址选择 | **3V3** | ADO=3V3 → 地址 0x4B |
| **CS** | SPI 片选 | **悬空** | I²C 模式下不用 |

### 1.2 I²C 地址配置

| PS0 | ADO | I²C 地址 |
|-----|-----|----------|
| 3V3 | GND | 0x4A（默认） |
| 3V3 | 3V3 | **0x4B**（本项目使用） |

### 1.3 接线图（ASCII）

```
ESP32-S3-DevKitC-1 N16R8          GY-BNO085 模块
┌─────────────────────┐           ┌─────────────────┐
│                     │           │                 │
│  3V3  ────────────────────────── VCC             │
│                     │           │                 │
│  GND  ────────────────────────── GND             │
│                     │           │                 │
│  GPIO8 (SDA) ──┬──────────────── SDA             │
│                │ 4.7kΩ          │                 │
│                └─── 3V3         │                 │
│                     │           │                 │
│  GPIO9 (SCL) ──┬──────────────── SCL             │
│                │ 4.7kΩ          │                 │
│                └─── 3V3         │                 │
│                     │           │                 │
│  GPIO1  ───────────────────────── INT             │
│                     │           │                 │
│  3V3  ────────────────────────── RST             │
│  3V3  ────────────────────────── PS0             │
│  GND  ────────────────────────── PS1             │
│  3V3  ────────────────────────── ADO             │
│                     │           │                 │
│                     │           CS (悬空)         │
└─────────────────────┘           └─────────────────┘
```

### 1.4 关键注意事项

1. **电压**：BNO085 工作电压 2.4V~3.6V，必须接 3.3V。接 5V 会永久损坏。
2. **上拉电阻**：SDA/SCL 各需 4.7kΩ 上拉到 3V3。部分 GY-BNO085 模块自带上拉，此时可省略。
3. **PS0/PS1**：必须正确配置为 I²C 模式（PS0=3V3, PS1=GND）。接错会进入 SPI/UART 模式。
4. **RST**：直接接 3V3（硬连），不要接 GPIO。Adafruit 库构造函数传 `-1` 表示无 GPIO 复位。
5. **GPIO8/9 冲突**：ESP32-S3 的 GPIO8/9 是默认 I²C 引脚，**但 N8 版本（8MB Flash）的 GPIO8/9 被 PSRAM 占用**。本项目使用 N16R8（16MB Flash + 8MB Octal PSRAM），GPIO8/9 可用。详见 [[s3-n8-gpio8-9-i2c-bug]]。
6. **INT 引脚**：可选。Adafruit 库支持有/无 INT 两种模式。接 GPIO1 可获得更低延迟的数据就绪通知。

---

## 2. PlatformIO 构建配置

### 2.1 为什么不需要 BLE/TFLite

诊断固件 `main.cpp` 只依赖：
- `Wire.h`（Arduino 内置）
- `Adafruit_BNO08x.h`（I²C IMU 驱动）
- `Adafruit_BusIO.h`（I²C/SPI 抽象层，Adafruit_BNO08x 的依赖）

以下库对诊断**完全不需要**，编译它们只会浪费时间：
- `NimBLE-Arduino`（BLE 协议栈，~2MB 编译产物）
- `TensorFlowLite_ESP32`（TFLite Micro，~5MB 编译产物）
- `Nanopb`（Protobuf，诊断不用）
- `ArduinoJson`（JSON 解析，诊断不用）

### 2.2 使用专用诊断环境

`platformio.ini` 中已配置专用环境 `[env:bno085-diag]`：

```ini
[env:bno085-diag]
extends = env:esp32-s3-devkitc-1-n16r8
lib_deps =
    adafruit/Adafruit BNO08x @ ^1.2.3
    adafruit/Adafruit BusIO @ ^1.14.3
build_src_filter =
    +<main.cpp>
    -<../lib/Sensors/>
    -<../lib/Models/>
    -<../lib/Comms/>
    -<../lib/Filters/>
```

关键配置：
- `lib_deps`：只包含 Adafruit BNO08x + BusIO，排除 BLE/TFLite
- `build_src_filter`：只编译 `main.cpp`，排除 lib/ 下的生产代码
- `extends`：继承 N16R8 板级配置（PSRAM、Flash、USB CDC）

### 2.3 N16R8 板级配置

PlatformIO 没有 `esp32-s3-devkitc-1-n16r8` 官方板型，使用 `esp32-s3-devkitc-1` + 手动配置：

```ini
board = esp32-s3-devkitc-1
board_build.partitions = default_16MB.csv   ; 16MB Flash 分区表
board_build.psram      = enable             ; 启用 8MB PSRAM
board_build.f_flash    = 80000000L          ; 80MHz Flash 时钟
board_build.flash_mode = qio                ; Quad I/O Flash 模式
board_build.arduino.memory_type = qio_qspi  ; PSRAM QIO 模式
```

---

## 3. 烧录步骤

### 3.1 连接 USB

ESP32-S3-DevKitC-1 N16R8 有两个 USB 口：
- **USB-UART**（左侧）：连接 UART0，用于 boot ROM 下载和串口监视
- **USB-CDC**（右侧）：连接原生 USB，用于 USB CDC 串口

**烧录使用 USB-UART 口（左侧）**。

### 3.2 确认 COM 口

```powershell
# Windows
pio device list
# 或
powershell "Get-WMIObject Win32_SerialPort | Select-Object DeviceID, Description"
```

```bash
# Linux
ls /dev/ttyUSB*
# 或
ls /dev/ttyACM*
```

### 3.3 构建 + 烧录

```bash
cd glove_firmware

# 构建诊断固件（仅编译 BNO08x 依赖，速度快）
pio run -e bno085-diag

# 烧录（指定 COM 口）
pio run -e bno085-diag -t upload --upload-port COM3
```

如果烧录失败，手动进入下载模式：
1. 按住 **BOOT** 按钮
2. 按一下 **RESET** 按钮
3. 松开 **BOOT**
4. 重新执行烧录命令

### 3.4 串口监视

```bash
pio device monitor -e bno085-diag -b 115200
```

或使用 PlatformIO 的一体化命令（构建+烧录+监视）：

```bash
pio run -e bno085-diag -t upload -t monitor
```

---

## 4. 诊断输出解读

### 4.1 正常输出示例

```
########################################
# EchoGlove V5.2 - BNO085 Diagnostic    #
# ESP32-S3-N16R8 + GY-BNO085 (isolated) #
########################################

========================================
  PHASE 0 - ELECTRICAL SANITY
========================================
SDA=GPIO8  SCL=GPIO9  INT=GPIO1  RST=HARDWIRED  ADDR=0x4B  I2C=100000 Hz
Manual checks before code test:
  [ ] BNO085 VCC between 3.20 and 3.35 V (loaded)
  [ ] SDA pull-up 4.7k to 3V3
  [ ] SCL pull-up 4.7k to 3V3
  [ ] PS0=3V3, PS1=GND, ADO=3V3, RST=3V3
  [ ] ESP32-S3 GND == BNO085 GND

========================================
  PHASE 1 - I2C SCAN @ 100 kHz
========================================
  [ACK ] 0x4B              ← BNO085 应答，地址正确
  -> 1 device(s) found @ 100 kHz

========================================
  PHASE 1 - I2C SCAN @ 50 kHz
========================================
  [ACK ] 0x4B
  -> 1 device(s) found @ 50 kHz

========================================
  PHASE 1 - I2C SCAN @ 10 kHz
========================================
  [ACK ] 0x4B
  -> 1 device(s) found @ 10 kHz

========================================
  PHASE 1.5 - RAW READ @ 0x4B
========================================
  [RX] 6 bytes: 01 00 00 00 00 00    ← SHTP 握手包

========================================
  PHASE 2 - begin_I2C()
========================================
  [BNO085] begin_I2C OK               ← 驱动初始化成功
  [BNO085] enabled ROTATION_VECTOR @ 100 Hz
  [BNO085] enabled ACCELEROMETER @ 50 Hz

========================================
  PHASE 3 - STREAM (10 s)
========================================
  [ROT ] i=12 j=-45 k=987 r=1 st=3 t=1234
  [ACC ] x=0.12 y=-0.34 z=9.81 st=3 t=1235
  ...
  -> 987 events in 10000 ms (98.7 Hz)

========================================
  DIAGNOSTIC COMPLETE
========================================
```

### 4.2 故障排查

| 现象 | 原因 | 解决方案 |
|------|------|----------|
| PHASE 1 扫描无设备 | I²C 接线错误 | 检查 SDA/SCL 是否接对 GPIO，上拉是否存在 |
| PHASE 1 扫描到 0x4A 而非 0x4B | ADO 引脚接错 | ADO 应接 3V3（当前 0x4A = ADO 接 GND） |
| PHASE 1 扫描到多个地址 | 有其他 I²C 设备 | 断开其他设备，只保留 BNO085 |
| PHASE 1.5 RAW READ 无数据 | BNO085 未启动 | 检查 VCC 是否 3.3V，RST 是否接 3V3 |
| PHASE 2 begin_I2C FAILED | 驱动初始化失败 | 检查 PS0=3V3, PS1=GND（I²C 模式） |
| PHASE 2 enableReport FAILED | 传感器不响应 | 尝试断电重启，检查模块是否损坏 |
| PHASE 3 事件数为 0 | 传感器未产生数据 | 检查 INT 引脚连接，或设 INT=-1（轮询模式） |
| PHASE 3 status=0 | 传感器未校准 | 正常现象，移动模块几秒后 status 会变为 3 |
| 编译错误：找不到库 | 依赖未安装 | 执行 `pio run -e bno085-diag` 会自动下载 |
| 烧录失败：串口不可用 | USB 口选错 | 使用 USB-UART 口（左侧），非 USB-CDC |

### 4.3 传感器状态码

`ev.status` 字段含义：
- `0`：未校准（uncalibrated）
- `1`：低精度（low accuracy）
- `2`：中精度（medium accuracy）
- `3`：高精度（fully calibrated）

旋转矢量（ROTATION_VECTOR）需要磁力计校准才能达到 status=3。加速度计（ACCELEROMETER）通常开机就是 status=3。

---

## 5. 已知问题

### 5.1 main.cpp print_pin_map() Bug（已修复）

原代码第 57 行在 RST 为硬连时错误地打印 `BNO085_INT_PIN` 而非 `BNO085_RST_PIN`。已修复。

### 5.2 GPIO8/9 与 N8 板冲突

ESP32-S3 **N8**（8MB Flash）版本的 GPIO8/9 被 Octal PSRAM 占用，**不能用作 I²C**。本项目使用 **N16R8**（16MB Flash + 8MB Quad PSRAM），GPIO8/9 可正常使用。

如果使用 N8 板，需改用其他 GPIO（如 GPIO1/2）作为 I²C。详见 memory: [[s3-n8-gpio8-9-i2c-bug]]。

### 5.3 SensorManager 中 BNO085 被禁用

生产代码 `SensorManager.h` 中 BNO085 初始化被跳过（`_bno_ok = false`），这是有意为之——调试 ADS1115 时临时禁用。验证 BNO085 正常后，需在 `SensorManager::begin()` 中重新启用。
