# EchoGlove V6.0 — Claude Code 实施提示词集

> **用途**: 将以下提示词逐个粘贴到 Claude Code 中执行，完成 V5→V6 迁移
> **前提**: 已阅读 `04_SOP-SPEC-PLAN_V6.md` 设计规格
> **总周期**: ~17 天 (6 个阶段)

---

## Phase 0: 分支创建与环境准备 (1天)

```
请为 EchoGlove 项目创建 V6 迁移分支并更新项目配置。

## 任务

1. 从当前 `V5-DualGloveFlex` 分支创建新分支 `V6-LSM6DSV16X`

2. 更新 `CLAUDE.md`，在 V5 Constants 表格后追加 V6 变更说明：
   - 新增一行: `| **V6 IMU** | LSM6DSV16X (0x6A) replaces BNO085 (0x4B) |`
   - 在 I2C topology 行改为: `| I2C topology | Flat bus (no MUX): LSM6DSV16X@0x6A + ADS1115@0x48 + ADS1115@0x49 |`
   - 将 "BNO085 Wiring" 小节标题改为 "LSM6DSV16X Wiring"，更新接线表（见 04_SOP-SPEC-PLAN_V6.md 第2.4节）
   - 在 "BNO085 PS0/PS1" Gotcha 行替换为: `**LSM6DSV16X CS**: CS=HIGH for I2C mode (CS=LOW selects SPI). SDO/SA0 latched at power-up.`

3. 更新 `glove_firmware/include/data_structures.h` 第95行的注释：
   ```cpp
   // 改前:
   float quaternion[4];               // BNO085 (w,x,y,z)
   // 改后:
   float quaternion[4];               // IMU quaternion (w,x,y,z) — BNO085 or LSM6DSV16X
   ```

4. 更新 `glove_firmware/lib/Sensors/Sensors.h` 模块版本和描述：
   ```cpp
   #define SENSORS_MODULE_VERSION "6.0.0"
   // 更新注释：BNO085 → LSM6DSV16X
   ```

5. 验证构建：
   ```bash
   cd glove_firmware && pio run
   ```

## 交付物
- 新分支 `V6-LSM6DSV16X` 已创建并切换
- CLAUDE.md 已更新 V6 上下文
- data_structures.h 注释已更新（接口不变）
- `pio run` 构建通过
```

---

## Phase 1: LSM6DSV16X 驱动开发 (3天)

### 1a. 创建 LSM6DSV16XManager 头文件

```
请在 `glove_firmware/lib/Sensors/` 下创建 `LSM6DSV16XManager.h`，实现 LSM6DSV16X IMU 驱动。

## 硬件规格
- I2C 地址: 0x6A (SDO/SA0=GND) 或 0x6B (SDO/SA0=VDD)
- I2C 总线: SDA=GPIO8, SCL=GPIO9, 400kHz（V6: 仅 LSM6DSV16X 独占，ADS1115 已移除见 `07`）
- 电源: VDD + VDDIO = 3.3V
- CS 引脚: 接 3.3V（I2C 模式）

## 寄存器地址（LSM6DSV16X datasheet）
```
WHO_AM_I      = 0x0F  // 固定返回 0x70
CTRL1_XL      = 0x10  // 加速度计控制
CTRL2_G       = 0x11  // 陀螺仪控制
CTRL3_C       = 0x12  // 通用控制
CTRL6_C       = 0x15
CTRL7_G       = 0x16
CTRL8_XL      = 0x17
OUTX_L_G      = 0x22  // 陀螺仪数据起始 (6字节)
OUTX_L_A      = 0x28  // 加速度计数据起始 (6字节)
FUNC_CFG_ACCESS = 0x01  // 嵌入式功能寄存器组访问
EMB_FUNC_EN_A = 0x04  // SFLP 使能
EMB_FUNC_STATUS = 0x52
```

## 类接口设计
```cpp
// 文件: glove_firmware/lib/Sensors/LSM6DSV16XManager.h

#pragma once
#include "data_structures.h"

#ifndef UNIT_TEST
#include <Wire.h>
#endif

#define LSM6DSV16X_ADDR_DEFAULT  0x6A  // SDO=GND
#define LSM6DSV16X_ADDR_ALT      0x6B  // SDO=VDD
#define LSM6DSV16X_WHO_AM_I_VAL  0x70

// 寄存器定义
#define LSM6DSV16X_REG_WHO_AM_I       0x0F
#define LSM6DSV16X_REG_CTRL1_XL       0x10
#define LSM6DSV16X_REG_CTRL2_G        0x11
#define LSM6DSV16X_REG_CTRL3_C        0x12
#define LSM6DSV16X_REG_CTRL6_C        0x15
#define LSM6DSV16X_REG_CTRL7_G        0x16
#define LSM6DSV16X_REG_CTRL8_XL       0x17
#define LSM6DSV16X_REG_OUTX_L_G       0x22  // Gyro: 6 bytes (XL,XH,YL,YH,ZL,ZH)
#define LSM6DSV16X_REG_OUTX_L_A       0x28  // Accel: 6 bytes
#define LSM6DSV16X_REG_FUNC_CFG_ACCESS 0x01
#define LSM6DSV16X_REG_EMB_FUNC_EN_A  0x04

class LSM6DSV16XManager {
public:
    LSM6DSV16XManager();

    // 初始化: I2C 连接 + WHO_AM_I 校验 + SFLP 使能
    bool begin(uint8_t addr = LSM6DSV16X_ADDR_DEFAULT);

    // 读取传感器数据填充 SensorData (quaternion, euler, gyro)
    // 返回 true 表示数据有效
    bool readSensor(SensorData& data);

    // 检查传感器是否就绪
    bool isReady() const;

    // 软件复位
    void reset();

    // 获取芯片温度 (摄氏度)
    float readTemperature();

private:
    uint8_t addr_;
    bool initialized_;
    bool sflp_available_;

    // I2C 底层操作
    uint8_t readRegister(uint8_t reg);
    void writeRegister(uint8_t reg, uint8_t value);
    void readRegisters(uint8_t reg, uint8_t* buf, size_t len);

    // 传感器配置
    bool verifyWhoAmI();
    void configureAccel();      // CTRL1_XL: ODR=104Hz, ±16g
    void configureGyro();       // CTRL2_G: ODR=104Hz, ±2000dps
    bool enableSFLP();          // 嵌入式 SFLP 融合使能

    // 数据转换
    void readGyro(float gyro[3]);           // 原始 → deg/s
    void readAccel(float accel[3]);         // 原始 → g
    bool readSFLPQuaternion(float q[4]);    // SFLP 四元数 (w,x,y,z)
    void quaternionToEuler(const float q[4], float euler[3]);  // 四元数 → 欧拉角 (度)
};
```

## 实现要求

1. `begin()` 流程:
   - 调用 `Wire.begin(I2CPins::SDA, I2CPins::SCL, 400000)` (注意: 400kHz, 不是 100kHz)
   - 调用 `verifyWhoAmI()` 读取 0x0F 寄存器，期望值 0x70
   - 调用 `configureAccel()` + `configureGyro()`
   - 尝试 `enableSFLP()`，如果失败则设置 `sflp_available_=false`（Madgwick 后备）
   - 延时 100ms 等待 SFLP 稳定

2. `readSensor(SensorData& data)` 流程:
   - 调用 `readGyro(data.gyro)` 读取 6 字节原始数据，按灵敏度换算为 deg/s
   - 如果 `sflp_available_`:
     - 调用 `readSFLPQuaternion(data.quaternion)`
   - 调用 `quaternionToEuler(data.quaternion, data.euler)` 换算欧拉角
   - 返回 true

3. 陀螺仪灵敏度换算 (±2000dps):
   - 灵敏度 = 70 mdps/LSB = 0.07 dps/LSB
   - gyro_dps = raw_value * 0.07

4. SFLP 四元数读取:
   - 进入嵌入式寄存器组: `writeRegister(0x01, 0x80)` (FUNC_CFG_ACCESS bit7=1)
   - 读取 8 字节从 EMB_FUNC_OUTPUT (地址参考 datasheet)
   - 退出嵌入式寄存器组: `writeRegister(0x01, 0x00)`
   - 4 个 int16 Q14 格式 → 归一化 float

5. 四元数→欧拉角 (ZYX 约定，与 V5 BNO085 一致):
   ```cpp
   void quaternionToEuler(const float q[4], float euler[3]) {
       float qw = q[0], qx = q[1], qy = q[2], qz = q[3];
       // Roll (X)
       euler[0] = atan2f(2*(qw*qx + qy*qz), 1 - 2*(qx*qx + qy*qy)) * 57.2958f;
       // Pitch (Y)
       float sinp = 2*(qw*qy - qz*qx);
       euler[1] = (fabsf(sinp) >= 1) ? copysignf(90.0f, sinp) * 57.2958f : asinf(sinp) * 57.2958f;
       // Yaw (Z)
       euler[2] = atan2f(2*(qw*qz + qx*qy), 1 - 2*(qy*qy + qz*qz)) * 57.2958f;
   }
   ```
   注意：此公式与 `SensorManager.h` 第230-234行的 BNO085 欧拉角换算完全一致。

6. 需要添加 `#include <cfloat>` 和 `#include <cmath>` 以使用 `atan2f`, `fabsf`, `copysignf`, `asinf`。

## 关键约束
- `SensorData` 结构体接口不变 (11维: flex[5]+euler[3]+gyro[3])
- quaternion 字段格式与 BNO085 一致: `[w, x, y, z]`
- 不要使用任何第三方库，直接通过 Wire 进行 I2C 寄存器操作
- 必须支持 `#ifndef UNIT_TEST` 条件编译（与现有代码风格一致）

## 验证步骤
- 文件创建后执行 `cd glove_firmware && pio run` 确认编译通过
```

### 1b. 创建 LSM6DSV16XManager 实现文件

```
请创建 `glove_firmware/lib/Sensors/LSM6DSV16XManager.cpp`，实现上一步头文件中声明的所有方法。

## 实现要点

### begin() 完整流程
```cpp
bool LSM6DSV16XManager::begin(uint8_t addr) {
    addr_ = addr;
    initialized_ = false;
    sflp_available_ = false;

#ifndef UNIT_TEST
    // I2C 已由 SensorManager 初始化，此处仅设置时钟
    Wire.setClock(400000);

    // WHO_AM_I 校验
    if (!verifyWhoAmI()) {
        Serial.printf("[LSM6DSV16X] WHO_AM_I FAIL at 0x%02X\n", addr_);
        return false;
    }
    Serial.printf("[LSM6DSV16X] WHO_AM_I OK at 0x%02X\n", addr_);

    // 软件复位
    reset();
    delay(50);  // 等待复位完成

    // 配置加速度计和陀螺仪
    configureAccel();
    configureGyro();

    // 尝试使能 SFLP
    sflp_available_ = enableSFLP();
    if (sflp_available_) {
        Serial.println("[LSM6DSV16X] SFLP fusion enabled");
        delay(100);  // SFLP 稳定时间 0.7s，此处先等 100ms
    } else {
        Serial.println("[LSM6DSV16X] SFLP unavailable, using raw gyro+accel");
    }

    initialized_ = true;
#endif
    return true;
}
```

### configureAccel()
```cpp
void LSM6DSV16XManager::configureAccel() {
    // CTRL1_XL = 0x50: ODR=104Hz (0b0101), FS_XL=±16g (00), LPF2_XL_EN=0
    writeRegister(LSM6DSV16X_REG_CTRL1_XL, 0x50);
}
```

### configureGyro()
```cpp
void LSM6DSV16XManager::configureGyro() {
    // CTRL2_G = 0x50: ODR=104Hz (0b0101), FS=±2000dps (00)
    writeRegister(LSM6DSV16X_REG_CTRL2_G, 0x50);
}
```

### enableSFLP() 参考实现
```cpp
bool LSM6DSV16XManager::enableSFLP() {
    // 进入嵌入式功能寄存器组
    writeRegister(LSM6DSV16X_REG_FUNC_CFG_ACCESS, 0x80);
    delay(10);

    // 使能 SFLP (Game Rotation Vector)
    writeRegister(LSM6DSV16X_REG_EMB_FUNC_EN_A, 0x01);
    delay(10);

    // 退出嵌入式功能寄存器组
    writeRegister(LSM6DSV16X_REG_FUNC_CFG_ACCESS, 0x00);
    delay(10);

    // 验证: 重新读取使能状态
    // (简化实现: 假设写入成功即为可用)
    return true;
}
```

### readGyro() 参考实现
```cpp
void LSM6DSV16XManager::readGyro(float gyro[3]) {
    uint8_t buf[6];
    readRegisters(LSM6DSV16X_REG_OUTX_L_G, buf, 6);

    // ±2000dps, 灵敏度 70 mdps/LSB = 0.07 dps/LSB
    const float sensitivity = 0.07f;
    int16_t raw_x = (int16_t)(buf[1] << 8 | buf[0]);
    int16_t raw_y = (int16_t)(buf[3] << 8 | buf[2]);
    int16_t raw_z = (int16_t)(buf[5] << 8 | buf[4]);

    gyro[0] = raw_x * sensitivity;  // deg/s
    gyro[1] = raw_y * sensitivity;
    gyro[2] = raw_z * sensitivity;
}
```

### readSFLPQuaternion() 参考实现
```cpp
bool LSM6DSV16XManager::readSFLPQuaternion(float q[4]) {
    // 进入嵌入式寄存器组
    writeRegister(LSM6DSV16X_REG_FUNC_CFG_ACCESS, 0x80);

    // 读取 SFLP 输出 (8 bytes = 4 x int16 Q14)
    // 注意: 实际地址参考 LSM6DSV16X datasheet 的 EMB_FUNC_OUTPUT 寄存器
    uint8_t buf[8];
    // TODO: 确认正确的 SFLP 输出寄存器地址
    // 读取完成后退出嵌入式寄存器组
    writeRegister(LSM6DSV16X_REG_FUNC_CFG_ACCESS, 0x00);

    // Q14 格式转换: float = raw / 2^14
    const float scale = 1.0f / 16384.0f;  // 1/2^14
    int16_t raw_w = (int16_t)(buf[1] << 8 | buf[0]);
    int16_t raw_x = (int16_t)(buf[3] << 8 | buf[2]);
    int16_t raw_y = (int16_t)(buf[5] << 8 | buf[4]);
    int16_t raw_z = (int16_t)(buf[7] << 8 | buf[6]);

    q[0] = raw_w * scale;  // w
    q[1] = raw_x * scale;  // x
    q[2] = raw_y * scale;  // y
    q[3] = raw_z * scale;  // z

    // 归一化
    float norm = sqrtf(q[0]*q[0] + q[1]*q[1] + q[2]*q[2] + q[3]*q[3]);
    if (norm > 0.001f) {
        q[0] /= norm; q[1] /= norm; q[2] /= norm; q[3] /= norm;
    }

    return true;
}
```

### I2C 底层操作
```cpp
uint8_t LSM6DSV16XManager::readRegister(uint8_t reg) {
    Wire.beginTransmission(addr_);
    Wire.write(reg);
    Wire.endTransmission(false);  // restart
    Wire.requestFrom(addr_, (uint8_t)1);
    return Wire.read();
}

void LSM6DSV16XManager::writeRegister(uint8_t reg, uint8_t value) {
    Wire.beginTransmission(addr_);
    Wire.write(reg);
    Wire.write(value);
    Wire.endTransmission();
}

void LSM6DSV16XManager::readRegisters(uint8_t reg, uint8_t* buf, size_t len) {
    Wire.beginTransmission(addr_);
    Wire.write(reg);
    Wire.endTransmission(false);
    Wire.requestFrom(addr_, (uint8_t)len);
    for (size_t i = 0; i < len && Wire.available(); i++) {
        buf[i] = Wire.read();
    }
}
```

## 注意事项
- SFLP 输出寄存器地址需要查阅 LSM6DSV16X datasheet (AN5763) 确认
- 如果 SFLP 不可用，`readSensor()` 应只返回 gyro 数据，quaternion 设为单位四元数 `[1,0,0,0]`
- Madgwick 滤波器将在 Phase 2 实现，此处先不集成
- 所有 I2C 操作需要 `#ifndef UNIT_TEST` 保护

## 验证步骤
- `cd glove_firmware && pio run` 确认编译通过
```

---

## Phase 2: Madgwick 滤波器实现 (2天)

### 2a. 创建 MadgwickFilter

```
请在 `glove_firmware/lib/Filters/` 下创建 `MadgwickFilter.h`，实现 Madgwick 6 轴传感器融合算法。

## 背景
LSM6DSV16X 的 SFLP 嵌入式融合是首选方案，但 Madgwick 滤波器作为后备：
- 当 SFLP 不可用时（如旧版本芯片）
- 当需要自定义融合参数时
- 纯 6 轴融合（无磁力计），适合手部姿态

## 文件: glove_firmware/lib/Filters/MadgwickFilter.h

```cpp
/* =============================================================================
 * EchoGlove V6 — Madgwick Filter (6-axis sensor fusion fallback)
 * =============================================================================
 * Implementation of Madgwick's IMU fusion algorithm for 6-axis
 * (accelerometer + gyroscope) orientation estimation.
 *
 * Reference: "An efficient orientation filter for inertial and
 *             inertial/magnetic sensor arrays" — S.O.H. Madgwick, 2010
 *
 * Performance: ~0.05ms per update on ESP32-S3 @ 240MHz
 * Output: quaternion (w,x,y,z) and Euler angles (roll,pitch,yaw in degrees)
 * =============================================================================
 */

#pragma once
#include <cmath>

class MadgwickFilter {
public:
    // beta: filter gain (0.01=slow convergence, 1.0=fast/noisy)
    // sampleFreq: expected sample rate in Hz
    MadgwickFilter(float beta = 0.1f, float sampleFreq = 100.0f)
        : q0_(1.0f), q1_(0.0f), q2_(0.0f), q3_(0.0f),
          beta_(beta), sampleFreq_(sampleFreq) {}

    // 更新滤波器
    // gx,gy,gz: 陀螺仪 (rad/s) — 注意输入必须是 rad/s，不是 deg/s
    // ax,ay,az: 加速度计 (g) — 标准化后
    void update(float gx, float gy, float gz,
                float ax, float ay, float az) {
        float recipNorm;
        float s0, s1, s2, s3;
        float qDot1, qDot2, qDot3, qDot4;
        float _2q0, _2q1, _2q2, _2q3;
        float _4q0, _4q1, _4q2;
        float _8q1, _8q2;
        float q0q0, q1q1, q2q2, q3q3;

        // 四元数导数 (陀螺仪)
        qDot1 = 0.5f * (-q1_*gx - q2_*gy - q3_*gz);
        qDot2 = 0.5f * ( q0_*gx + q2_*gz - q3_*gy);
        qDot3 = 0.5f * ( q0_*gy - q1_*gz + q3_*gx);
        qDot4 = 0.5f * ( q0_*gz + q1_*gy - q2_*gx);

        // 如果加速度计数据无效（全零），跳过修正
        if (!((ax == 0.0f) && (ay == 0.0f) && (az == 0.0f))) {
            // 归一化加速度计
            recipNorm = 1.0f / sqrtf(ax*ax + ay*ay + az*az);
            ax *= recipNorm; ay *= recipNorm; az *= recipNorm;

            // 辅助变量
            _2q0 = 2.0f * q0_; _2q1 = 2.0f * q1_;
            _2q2 = 2.0f * q2_; _2q3 = 2.0f * q3_;
            _4q0 = 4.0f * q0_; _4q1 = 4.0f * q1_;
            _4q2 = 4.0f * q2_;
            _8q1 = 8.0f * q1_; _8q2 = 8.0f * q2_;
            q0q0 = q0_*q0_; q1q1 = q1_*q1_;
            q2q2 = q2_*q2_; q3q3 = q3_*q3_;

            // 梯度下降修正
            s0 = _4q0*q2q2 + _2q2*ax + _4q0*q1q1 - _2q1*ay;
            s1 = _4q1*q3q3 - _2q3*ax + 4.0f*q0q0*q1_
                 - _2q0*ay - _4q1 + _8q1*q1q1 + _8q1*q2q2 + _4q1*az;
            s2 = 4.0f*q0q0*q2_ + _2q0*ax + _4q2*q3q3 - _2q3*ay
                 - _4q2 + _8q2*q1q1 + _8q2*q2q2 + _4q2*az;
            s3 = 4.0f*q1q1*q3_ - _2q1*ax + 4.0f*q2q2*q3_ - _2q2*ay;

            // 归一化梯度
            recipNorm = 1.0f / sqrtf(s0*s0 + s1*s1 + s2*s2 + s3*s3);
            s0 *= recipNorm; s1 *= recipNorm;
            s2 *= recipNorm; s3 *= recipNorm;

            // 应用修正
            qDot1 -= beta_ * s0;
            qDot2 -= beta_ * s1;
            qDot3 -= beta_ * s2;
            qDot4 -= beta_ * s3;
        }

        // 积分四元数
        q0_ += qDot1 * (1.0f / sampleFreq_);
        q1_ += qDot2 * (1.0f / sampleFreq_);
        q2_ += qDot3 * (1.0f / sampleFreq_);
        q3_ += qDot4 * (1.0f / sampleFreq_);

        // 归一化
        recipNorm = 1.0f / sqrtf(q0_*q0_ + q1_*q1_ + q2_*q2_ + q3_*q3_);
        q0_ *= recipNorm; q1_ *= recipNorm;
        q2_ *= recipNorm; q3_ *= recipNorm;
    }

    // 获取四元数 (w,x,y,z)
    void getQuaternion(float q[4]) const {
        q[0] = q0_; q[1] = q1_; q[2] = q2_; q[3] = q3_;
    }

    // 获取欧拉角 (度): roll, pitch, yaw
    void getEuler(float euler[3]) const {
        // Roll (X-axis rotation)
        float sinr_cosp = 2.0f * (q0_*q1_ + q2_*q3_);
        float cosr_cosp = 1.0f - 2.0f * (q1_*q1_ + q2_*q2_);
        euler[0] = atan2f(sinr_cosp, cosr_cosp) * 57.2958f;

        // Pitch (Y-axis rotation)
        float sinp = 2.0f * (q0_*q2_ - q3_*q1_);
        if (fabsf(sinp) >= 1.0f)
            euler[1] = copysignf(90.0f, sinp) * 57.2958f;
        else
            euler[1] = asinf(sinp) * 57.2958f;

        // Yaw (Z-axis rotation)
        float siny_cosp = 2.0f * (q0_*q3_ + q1_*q2_);
        float cosy_cosp = 1.0f - 2.0f * (q2_*q2_ + q3_*q3_);
        euler[2] = atan2f(siny_cosp, cosy_cosp) * 57.2958f;
    }

    // 重置滤波器状态
    void reset() {
        q0_ = 1.0f; q1_ = q2_ = q3_ = 0.0f;
    }

    // 更新采样频率
    void setSampleFreq(float freq) { sampleFreq_ = freq; }

    // 更新滤波增益
    void setBeta(float beta) { beta_ = beta; }

private:
    float q0_, q1_, q2_, q3_;  // 四元数状态
    float beta_;                 // 滤波增益
    float sampleFreq_;          // 采样频率 (Hz)
};
```

## 关键约束
- 纯头文件实现（header-only），与项目中其他 Filter 类风格一致
- 输入单位: 陀螺仪 rad/s，加速度计 g（非 m/s²）
- 欧拉角换算公式与 `SensorManager.h` 第230-234行和 `LSM6DSV16XManager` 一致
- 支持 `#ifdef UNIT_TEST` 条件编译
- 不依赖任何外部库

## 验证步骤
- `cd glove_firmware && pio run` 确认编译通过
```

---

## Phase 3: SensorManager 迁移 (2天)

```
请修改 `glove_firmware/lib/Sensors/SensorManager.h`，将 BNO085 替换为 LSM6DSV16X。

## 修改清单

### 1. 头文件引用 (第28行)
```cpp
// 改前:
#include <Adafruit_BNO08x.h>

// 改后:
#include "LSM6DSV16XManager.h"
```

### 2. 私有成员变量 (第188-189行)
```cpp
// 改前:
    Adafruit_BNO08x _bno;
    bool            _bno_ok = false;

// 改后:
    LSM6DSV16XManager _imu;
    bool              _imu_ok = false;
```

### 3. begin() 方法中的 I2C 扫描 (第86-97行)
```cpp
// 改前:
                    if (addr == 0x48) Serial.print("  (ADS1115 #1)");
                    else if (addr == 0x49) Serial.print("  (ADS1115 #2)");
                    else if (addr == 0x4B) Serial.print("  (BNO085)");

// 改后:
                    if (addr == 0x48) Serial.print("  (ADS1115 #1)");
                    else if (addr == 0x49) Serial.print("  (ADS1115 #2)");
                    else if (addr == 0x6A) Serial.print("  (LSM6DSV16X)");
```

### 4. begin() 方法中的 IMU 初始化 (第109-112行)
```cpp
// 改前:
            // ── BNO085 IMU: not initialized in V5 debug build ──
            _bno_ok = false;
            Serial.println("[SensorManager] BNO085: SKIPPED");
            // NOTE: V6 flex path moved to internal ADC1 — see
            //       07_internal_adc_migration.md for InternalADCManager
            //       prompts (replaces V5 ADS1115Manager).

// 改后:
            // ── LSM6DSV16X IMU ──
            _imu_ok = _imu.begin();  // I2C 0x6A, SFLP enable
            Serial.printf("[SensorManager] LSM6DSV16X: %s\n",
                          _imu_ok ? "OK" : "FAIL");
```

### 5. readHardware() 方法 (第196-237行) — 完整替换
```cpp
// 改前 (BNO085 读取逻辑):
    void readHardware(SensorData& data) {
#ifndef UNIT_TEST
        _flex.read(data.flex);

        if (_bno_ok) {
            sh2_SensorValue_t sensor;
            float qw = 1, qx = 0, qy = 0, qz = 0;
            bool got_quat = false;

            while (_bno.getSensorEvent(&sensor)) {
                if (sensor.sensorId == SH2_ROTATION_VECTOR) {
                    qw = sensor.un.rotationVector.real;
                    // ... (省略)
                }
            }
            // ... (省略)
        }
#endif
    }

// 改后 (LSM6DSV16X 读取逻辑):
    void readHardware(SensorData& data) {
#ifndef UNIT_TEST
        // ── Flex sensors (ADS1115) — 不变 ──
        _flex.read(data.flex);

        // ── IMU (LSM6DSV16X) ──
        if (_imu_ok) {
            _imu.readSensor(data);
            // readSensor() 内部已填充:
            //   data.quaternion[0..3]
            //   data.euler[0..2]
            //   data.gyro[0..2]
        }
#endif
    }
```

### 6. 版本注释 (第1-8行)
```cpp
// 改前:
 * EchoGlove V5 — Sensor Manager
 * ...
 *   - IMU: BNO085 (0x4B) via Adafruit BNO08x library

// 改后:
 * EchoGlove V6 — Sensor Manager
 * ...
 *   - IMU: LSM6DSV16X (0x6A) via direct I2C register access + SFLP fusion
```

## 关键约束
- `SensorData` 结构体完全不变（11维输出不变）
- ADS1115 和 FlexManager 代码完全不变
- 仿真模式代码不变
- 手势签名表 (GestureSignature) 不变

## 验证步骤
- `cd glove_firmware && pio run` 确认编译通过（此时 BNO085 库仍存在但不再引用）
- 确认无 `Adafruit_BNO08x` 引用残留: `grep -r "BNO085\|Adafruit_BNO08x\|bno\." glove_firmware/lib/Sensors/SensorManager.h`
```

---

## Phase 4: 构建验证与依赖清理 (1天)

```
请清理 BNO085 相关依赖并验证完整构建。

## 任务

### 1. 修改 platformio.ini — 移除 BNO085 依赖
文件: `glove_firmware/platformio.ini`

```ini
; 改前 (第42-43行):
    ; Adafruit BNO08x — IMU via SH-2 interface (I2C)
    adafruit/Adafruit BNO08x @ ^1.2.3

; 改后:
    ; LSM6DSV16X — direct I2C register access, no external library
    ; (driver in lib/Sensors/LSM6DSV16XManager.h)
```

注意: 保留 `Adafruit BusIO` 依赖（ADS1115Manager 使用 Wire.h，不依赖 Adafruit BusIO，但其他模块可能需要）。
实际上如果 `Adafruit BusIO` 仅被 BNO085 使用，也应移除。检查后再决定：
```bash
grep -r "Adafruit_BusIO\|AdafruitBusIO" glove_firmware/lib/
```
如果没有其他模块引用，一并移除:
```ini
; 移除:
    adafruit/Adafruit BusIO @ ^1.14.3
```

### 2. 移除 bno085-diag 环境
文件: `glove_firmware/platformio.ini`

删除或注释掉 `[env:bno085-diag]` 整个段落 (第64-83行)。

### 3. 新增 lsm6dsv16x-diag 环境
```ini
; =============================================================================
; LSM6DSV16X Diagnostic — minimal build for IMU testing
; Usage: pio run -e lsm6dsv16x-diag -t upload && pio device monitor -e lsm6dsv16x-diag
; =============================================================================
[env:lsm6dsv16x-diag]
platform      = espressif32@^6.5.0
board         = esp32-s3-devkitc-1
framework     = arduino
build_flags =
    -DARDUINO_USB_CDC_ON_BOOT=1
lib_deps =
build_src_filter =
    +<main.cpp>
    -<../lib/Sensors/>
    -<../lib/Models/>
    -<../lib/Comms/>
    -<../lib/Filters/>
monitor_speed = 115200
```

### 4. 创建诊断固件
创建 `glove_firmware/src/main_lsm6dsv16x_diag.cpp`（或修改现有 main.cpp 为诊断模式）:

```cpp
// LSM6DSV16X 诊断固件 — 验证 I2C 通信和 WHO_AM_I
#include <Arduino.h>
#include <Wire.h>

#define SDA_PIN 8
#define SCL_PIN 9
#define LSM6DSV16X_ADDR 0x6A
#define REG_WHO_AM_I 0x0F

void setup() {
    Serial.begin(115200);
    while (!Serial) delay(10);
    delay(2000);

    Serial.println("\n========================================");
    Serial.println("LSM6DSV16X Diagnostic — ESP32-S3 N16R8");
    Serial.println("========================================\n");

    Wire.begin(SDA_PIN, SCL_PIN);
    Wire.setClock(400000);

    // I2C 扫描
    Serial.println("[I2C] Scanning 0x01..0x7F...");
    int found = 0;
    for (byte addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            Serial.printf("  Found device at 0x%02X", addr);
            if (addr == 0x48) Serial.print(" (ADS1115 #1)");
            else if (addr == 0x49) Serial.print(" (ADS1115 #2)");
            else if (addr == 0x6A) Serial.print(" (LSM6DSV16X)");
            else if (addr == 0x6B) Serial.print(" (LSM6DSV16X alt)");
            Serial.println();
            found++;
        }
    }
    Serial.printf("[I2C] Total: %d device(s)\n\n", found);

    // WHO_AM_I 检查
    Wire.beginTransmission(LSM6DSV16X_ADDR);
    Wire.write(REG_WHO_AM_I);
    Wire.endTransmission();
    Wire.requestFrom((int)LSM6DSV16X_ADDR, (int)1);
    if (Wire.available()) {
        uint8_t whoami = Wire.read();
        Serial.printf("[LSM6DSV16X] WHO_AM_I = 0x%02X", whoami);
        if (whoami == 0x70) Serial.println(" (OK)");
        else Serial.printf(" (expected 0x70)\n");
    } else {
        Serial.println("[LSM6DSV16X] WHO_AM_I read FAIL");
    }

    Serial.println("\n========================================");
    Serial.println("Diagnostic Complete");
    Serial.println("========================================");
}

void loop() { delay(10000); }
```

### 5. 全量构建验证
```bash
cd glove_firmware
pio run                    # 主环境构建
pio run -e lsm6dsv16x-diag  # 诊断环境构建
pio test                   # 原生单元测试
```

### 6. 检查残留引用
```bash
# 确认无 BNO085 残留
grep -rn "BNO085\|Adafruit_BNO08x\|bno\.\|SH2_" glove_firmware/lib/ glove_firmware/include/
# 确认无 bno085-diag 残留
grep -rn "bno085-diag" glove_firmware/platformio.ini
```

## 交付物
- platformio.ini 已清理 BNO085 依赖
- lsm6dsv16x-diag 诊断环境可用
- `pio run` + `pio test` 全部通过
- 无 BNO085 残留引用
```

---

## Phase 5: 集成测试 (3天)

```
请执行 V6 固件的全链路集成测试验证。

## 测试矩阵

### 5.1 仿真模式验证 (无硬件)
```bash
cd glove_firmware
pio test  # native 环境
```
确认所有现有测试通过（SensorData 接口未变，测试应无变化）。

### 5.2 传感器数据验证 (需要硬件)
上传固件后通过串口监视器验证:
```bash
pio run -t upload
pio device monitor
```

检查以下输出:
- `[LSM6DSV16X] WHO_AM_I OK at 0x6A` — I2C 通信正常
- `[LSM6DSV16X] SFLP fusion enabled` — SFLP 融合可用
- `[SensorManager] LSM6DSV16X: OK` — 驱动初始化成功
- 传感器数据范围:
  - euler[0..2]: ±180 度（静止时接近 0）
  - gyro[0..2]: ±2000 dps（静止时接近 0）
  - quaternion[0]: 接近 1.0（静止时）

### 5.3 数据格式一致性验证
确认以下格式与 V5 完全一致:
- `SensorData.toFeatureArray()` 输出 11 维: flex[5] + euler[3] + gyro[3]
- `GlovePacket` 大小 69 字节
- ESP-NOW 广播格式不变

### 5.4 端到端链路验证 (需要完整硬件)
```
Glove(S3) → ESP-NOW → C6 → UART → P4 → USB → PC Relay → WebSocket → Frontend
```
验证:
1. C6 收到 GlovePacket（检查 magic="EG", version=6）
2. P4 正确组装 28 维特征向量
3. PC Relay 接收数据并转发
4. 前端 3D 手模型正确渲染

### 5.5 长时间稳定性测试
```bash
# 30 分钟连续运行，检查:
# - 无 I2C 总线挂死
# - 无内存泄漏
# - 传感器数据稳定（无异常跳变）
pio device monitor  # 保持 30 分钟
```

## 验证清单
- [ ] pio test — 全部通过
- [ ] WHO_AM_I 校验通过 (0x70)
- [ ] SFLP 四元数输出正常
- [ ] Euler 角范围正确 (±180°)
- [ ] 陀螺仪输出范围正确 (±2000 dps)
- [ ] GlovePacket 大小 = 69 字节
- [ ] ESP-NOW 传输正常
- [ ] P4 28 维特征计算正确
- [ ] 前端渲染正常
- [ ] 30 分钟稳定性测试通过
```

---

## Phase 6: 模型重训练 (5天，如需要)

```
请评估是否需要使用 LSM6DSV16X 数据重新训练 Tier1/Tier2 模型。

## 评估步骤

### 6.1 数据对比采集
使用新 IMU 采集与 V5 相同的 46 类手势数据:
```bash
# 采集脚本 (如已有)
cd glove_firmware/scripts
python collect_data.py --output dataset/v6_lsm6dsv16x/ --gestures 46 --samples 30
```

CSV 格式不变:
```csv
timestamp,hand_id,flex0,flex1,flex2,flex3,flex4,euler_x,euler_y,euler_z,gyro_x,gyro_y,gyro_z,gesture_id
```

### 6.2 特征分布对比
```python
import pandas as pd
import numpy as np

v5 = pd.read_csv("dataset/v5_bno085/all_data.csv")
v6 = pd.read_csv("dataset/v6_lsm6dsv16x/all_data.csv")

# 对比每个特征维度的均值和标准差
for col in ['euler_x','euler_y','euler_z','gyro_x','gyro_y','gyro_z']:
    print(f"{col}: V5={v5[col].mean():.2f}±{v5[col].std():.2f}, "
          f"V6={v6[col].mean():.2f}±{v6[col].std():.2f}")
```

### 6.3 决策逻辑
- 如果 V5 和 V6 的特征分布差异 < 10%: 直接使用 V5 模型，无需重训练
- 如果差异 10-30%: 使用 V6 数据微调 (fine-tune) V5 模型
- 如果差异 > 30%: 使用 V6 数据从头训练

### 6.4 重训练 (如需要)
```bash
cd glove_relay
python -m src.models.train --config configs/model_config.yaml \
    --data dataset/v6_lsm6dsv16x/ \
    --epochs 100 --batch-size 32
```

### 6.5 模型导出
```bash
# 导出 TFLite INT8 量化模型
python -m src.models.export_tflite \
    --model checkpoints/best_model.pt \
    --output firmware_models/tier1_v6_int8.tflite \
    --quantize int8
```

## 交付物
- 数据对比分析报告
- 重训练决策（是/否）
- 如重训练: 新 TFLite 模型文件
- 模型精度: Tier1 > 85%, Tier2 > 90%
```

---

## 附录: 关键文件路径速查

| 文件 | 路径 | 操作 |
|------|------|------|
| data_structures.h | `glove_firmware/include/data_structures.h` | 微改 (注释) |
| SensorManager.h | `glove_firmware/lib/Sensors/SensorManager.h` | 主改 (BNO→LSM6) |
| LSM6DSV16XManager.h | `glove_firmware/lib/Sensors/LSM6DSV16XManager.h` | **新建** |
| LSM6DSV16XManager.cpp | `glove_firmware/lib/Sensors/LSM6DSV16XManager.cpp` | **新建** |
| MadgwickFilter.h | `glove_firmware/lib/Filters/MadgwickFilter.h` | **新建** |
| platformio.ini | `glove_firmware/platformio.ini` | 改 (依赖清理) |
| Sensors.h | `glove_firmware/lib/Sensors/Sensors.h` | 微改 (版本号) |
| main.cpp | `glove_firmware/src/main.cpp` | 不变 |
| ADS1115Manager.h | `glove_firmware/lib/Sensors/ADS1115Manager.h` | 不变 |
| FlexManager.h | `glove_firmware/lib/Sensors/FlexManager.h` | 不变 |

## 附录: BNO085 vs LSM6DSV16X 对照表

| 项目 | BNO085 (V5) | LSM6DSV16X (V6) |
|------|-------------|-----------------|
| I2C 地址 | 0x4B | 0x6A (SDO=GND) |
| 轴数 | 9轴 (含磁力计) | 6轴 (无磁力计) |
| 融合方案 | SH-2 硬件融合 | SFLP 嵌入式融合 |
| 四元数格式 | sh2_SensorValue_t | int16 Q14 (归一化) |
| 陀螺仪单位 | rad/s (库自动转换) | LSB → 0.07 dps/LSB |
| 库依赖 | Adafruit_BNO08x | 无 (直接 I2C) |
| 成本 | $15-25 | $2-4 |
| 校准 | 自动 (SH-2) | SFLP 自动 (0.8s) |
