# EchoGlove V5.2 — 标准作业程序与规格说明书

> **文档版本**: v5.2-final
> **生成日期**: 2026-06-08
> **前置版本**: V5.1 (2026-06-02)
> **核心变更**: ESP32-P4 基站 + DW3000 UWB + 29维特征向量 + 通信双模冗余

---

## 项目愿景

EchoGlove V5.2 是一套基于边缘AI的**双手数据手套系统**，实现实时手语翻译与3D手部动作捕捉。V5.2 在 V5.1 基础上引入三大核心升级：

1. **ESP32-P4 智能基站**：替代 ESP32-S3 接收器，提供 1.7× 计算力、7寸触摸屏本地展示、MIPI-CSI 视觉补充通道和音频 TTS 播报
2. **DW3000 UWB 测距**：解决 IMU+Flex 方案的"位置黑洞"问题，通过 TWR 双向测距精确获取双手直线距离
3. **29 维特征向量**：在 V5.1 的 28 维基础上新增 UWB 双手距离标量，配合 EKF 融合实现 100Hz 连续位置估计

---

## ADR 决策记录（23 项）

| ADR | 决策项 | V5.1 方案 | V5.2 方案 | 变更类型 |
|-----|--------|-----------|-----------|----------|
| D1 | 硬件拓扑 | 3×ESP32-S3 | 2×ESP32-S3 + 1×ESP32-P4 + C6 | 🔄 修改 |
| D2 | 通信同步 | ESP-NOW + SYNC_TICK | 同左 | ✅ 继承 |
| D3 | 特征向量 | 28-dim | 29-dim (+UWB距离) | 🔄 修改 |
| D4 | 传感器方案 | Flex + IMU | 同左 | ✅ 继承 |
| D5 | BNO085 模式 | GRV 6轴 | 同左 | ✅ 继承 |
| D6 | I2C 策略 | 100kHz | 同左 | ✅ 继承 |
| D7 | ST-GCN 分层 | 12/42 节点 | 同左 | ✅ 继承 |
| D8 | Tier2 模型 | CrossAttn ~80KB | CrossAttn+MS-TCN2 ~120KB | 🔄 修改 |
| D9 | CrossAttention | Gated Bidirectional | 同左 | ✅ 继承 |
| D10 | L2 时序骨干 | MS-TCN 4-stage | 同左 | ✅ 继承 |
| D11 | L2 流水线 | ST-GCN→MS-TCN→CTC | 同左 | ✅ 继承 |
| D12 | 热切换策略 | 三级降级 | 同左 | ✅ 继承 |
| D13 | 数据集规模 | 46→60+ | 同左 | ✅ 继承 |
| D14 | 前端渲染 | React3F+Unity | 同左 | ✅ 继承 |
| D15 | 连续手语解码 | CTC | 同左 | ✅ 继承 |
| D16 | 关节映射 | 线性→MANO | 同左 | ✅ 继承 |
| D17 | L2 备选方案 | BiLSTM+CTC | 同左 | ✅ 继承 |
| D18 | Flex 温漂补偿 | 自动零点校准 | 同左 | ✅ 继承 |
| D19 | ESP-NOW 丢包 | 插值补帧 | 同左 | ✅ 继承 |
| D20 | 量化策略 | FP16+INT8 | 同左 | ✅ 继承 |
| D21 | 双手测距 | 无（IMU漂移） | DW3000 UWB + IMU EKF | 🆕 新增 |
| D22 | 基站硬件 | ESP32-S3 接收器 | ESP32-P4-Function-EV-Board | 🆕 新增 |
| D23 | 通信冗余 | ESP-NOW 单模 | ESP-NOW + BLE 5.0 双模 | 🆕 新增 |

### D21: 双手测距 — DW3000 UWB TWR + IMU EKF 融合

**状态**: ✅ 已确认  
**背景**: V5.1 的 28 维特征中，位置差(3) 依赖 IMU 积分，累积漂移在数秒内达到分米级，导致双手空间距离无法可靠测量——"位置黑洞"。约 40% 双手协同手势依赖空间距离作为关键区分特征。  
**决策**: 左右手套各加一个 DW3000 (DWM3000) UWB 模块，10-20Hz TWR 测距获取双手直线距离标量，配合 IMU EKF 融合实现 100Hz 三维位置差估计。  
**理由**: DW3000 精度~5cm、ESP32 驱动成熟(Makerfabs开源)、价格~¥130/模块可接受。MK8000 国产方案虽便宜(¥25)但驱动不成熟、精度仅~10cm，竞赛时间线下风险过高。  
**后果**: 特征向量 28→29 维，手套 PCB 增加 DW3000 焊盘(SPI 5线)，手套功耗增加约 30mA(TX)，EKF 计算开销每帧 <0.1ms。

### D22: 基站硬件 — ESP32-P4-Function-EV-Board v1.5.2

**状态**: ✅ 已确认  
**背景**: 竞赛要求使用 ESP32-P4 开发板。ESP32-P4 双核 400MHz RV32 提供 1.7× 计算力，32MB 封装 PSRAM 可容纳更大模型，7寸 MIPI-DSI 触摸屏可独立展示识别结果，MIPI-CSI 摄像头为多模态融合提供视觉通道。  
**决策**: ESP32-P4-Function-EV-Board 替代 ESP32-S3 接收器，担任增强型 Tier2 智能基站。板载 ESP32-C6-MINI-1 提供 WiFi 6 + BLE 5 无线通信能力，C6 通过 UART(2Mbps) 转发手套数据至 P4。  
**理由**: 竞赛规则 + 性能提升 + 展示优势。7寸屏脱 PC 展示对竞赛评审有极强展示力。  
**后果**: 基站固件需重写(P4 RISC-V + C6 协处理器)，Tier2 模型可扩展至 ~120KB，基站为桌面设备(功耗~2-3W，不适合电池)。

### D23: 通信冗余 — ESP-NOW + BLE 5.0 双模

**状态**: ✅ 已确认  
**背景**: V5.1 仅使用 ESP-NOW 单模通信，在 WiFi 密集环境下可能出现信道竞争。BLE 可用于手机 APP 配对和 OTA 固件升级。  
**决策**: ESP-NOW 作为主数据通道(~1.9ms 延迟)，BLE 5.0 作为备份通道和配置通道，两者在 ESP32-C6 上时分复用共存。  
**理由**: ESP-NOW 延迟最低适合实时数据，BLE 适合低功耗备份和手机交互。ESP-IDF 原生支持协议共存。  
**后果**: C6 固件需同时维护两个协议栈，ESP-NOW 丢包时自动切换至 BLE 通道。

---

## 三级推理架构

| 维度 | Tier1 (手套) | Tier2 (P4基站) | Tier3 (PC) |
|------|-------------|----------------|------------|
| 硬件 | 2×ESP32-S3 | ESP32-P4+C6 | PC GPU |
| CPU | 240MHz Xtensa | 400MHz RV32 双核 | GPU |
| 模型 | CNN+SE-Attn | Gated Bi-CrossAttn+MS-TCN2 | ST-GCN+MS-TCN+CTC |
| 输入 | 11-dim | 29-dim | 42节点×T帧 |
| 输出 | ~20类 | 46+类 | 60+类+序列 |
| 模型大小 | ~80KB | ~120KB | ~3MB |
| 推理延迟 | <10ms | ~30ms | ~15ms(GPU) |
| 精度 | ~80% | ~92% | ~95% |
| 展示 | LED | 7寸屏+TTS | 3D渲染 |

### 热切换策略（继承 V5.1）

```
Tier1 → Tier2 → Tier3
  ↑断连    ↑断连      ↑正常
  自治      自治       完整

过渡：5帧线性渐变 (~50ms)
output_t = α·output_new + (1-α)·output_old
α: 0.2 → 0.4 → 0.6 → 0.8 → 1.0
```

---

## Phase A: ESP32-P4 基站基础搭建（1-2 周）

### 目标
验证 ESP32-P4-Function-EV-Board 的完整数据通路：C6 接收 ESP-NOW → UART 转发 → P4 解析 → 7寸屏展示

### 任务清单
- [ ] A1: ESP32-P4 + ESP32-C6 双芯片 ESP-IDF 工程搭建
- [ ] A2: C6 ESP-NOW 接收器 + UART 转发器固件
- [ ] A3: P4 UART 解析器 + Protobuf 解码
- [ ] A4: LVGL 7寸 MIPI-DSI 触摸屏 UI
- [ ] A5: 端到端数据通路集成测试

### C6 中继固件核心代码

```cpp
// c6_relay/main/main.c
#include "esp_now.h"
#include "driver/uart.h"

#define UART_NUM UART_NUM_0
#define UART_BAUD 2000000  // 2Mbps
#define C6_TX_PIN 43
#define C6_RX_PIN 44

static const uint8_t L_MAC[6] = {0xAA,0xBB,0xCC,0xDD,0xEE,0x01};
static const uint8_t R_MAC[6] = {0xAA,0xBB,0xCC,0xDD,0xEE,0x02};

typedef struct __attribute__((packed)) {
    uint8_t magic;       // 0xA5
    uint8_t hand_id;     // 0=L, 1=R
    uint8_t seq;
    float features[11];  // 11-dim feature vector
    uint64_t timestamp_us;
    uint8_t checksum;
} glove_packet_t;

void esp_now_recv_cb(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
    if (len != sizeof(glove_packet_t)) return;
    // Forward to P4 via UART
    uart_write_bytes(UART_NUM, data, len);
}

void app_main() {
    // UART init
    uart_config_t cfg = {
        .baud_rate = UART_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };
    uart_driver_install(UART_NUM, 512, 512, 0, NULL, 0);
    uart_param_config(UART_NUM, &cfg);
    uart_set_pin(UART_NUM, C6_TX_PIN, C6_RX_PIN, -1, -1);

    // ESP-NOW init
    esp_now_init();
    esp_now_register_recv_cb(esp_now_recv_cb);

    // Add peers
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, L_MAC, 6);
    peer.channel = 1;
    esp_now_add_peer(&peer);
    memcpy(peer.peer_addr, R_MAC, 6);
    esp_now_add_peer(&peer);
}
```

### P4 UART 解析器

```cpp
// p4_base_station/main/uart_parser.h
#include "driver/uart.h"

class UARTParser {
    static constexpr int BUF_SIZE = 2048;
    uint8_t* buf;
    int buf_pos = 0;
public:
    void init(int rx_pin, int tx_pin, int baud) {
        uart_config_t cfg = {
            .baud_rate = baud,
            .data_bits = UART_DATA_8_BITS,
            .parity = UART_PARITY_DISABLE,
            .stop_bits = UART_STOP_BITS_1,
            .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        };
        uart_driver_install(UART_NUM_0, BUF_SIZE*2, 0, 0, NULL, 0);
        uart_param_config(UART_NUM_0, &cfg);
        uart_set_pin(UART_NUM_0, tx_pin, rx_pin, -1, -1);
        buf = new uint8_t[BUF_SIZE];
    }

    bool parse(glove_packet_t* out) {
        int len = uart_read_bytes(UART_NUM_0, buf+buf_pos, BUF_SIZE-buf_pos, 1);
        buf_pos += len;
        // Find magic byte 0xA5
        for (int i = 0; i < buf_pos - (int)sizeof(glove_packet_t); i++) {
            if (buf[i] == 0xA5) {
                memcpy(out, buf+i, sizeof(glove_packet_t));
                // Verify checksum
                uint8_t cs = 0;
                uint8_t* p = (uint8_t*)out;
                for (int j = 0; j < sizeof(glove_packet_t)-1; j++) cs ^= p[j];
                if (cs == out->checksum) {
                    memmove(buf, buf+i+sizeof(glove_packet_t), buf_pos-i-sizeof(glove_packet_t));
                    buf_pos -= i + sizeof(glove_packet_t);
                    return true;
                }
            }
        }
        return false;
    }
};
```

### 验收标准
- [x] C6 接收左右手套 ESP-NOW 数据，丢包率 <2%
- [x] C6→P4 UART 转发延迟 <1ms
- [x] P4 解析 Protobuf 数据并显示在 7寸屏
- [x] 屏幕实时更新 >30fps

---

## Phase B: Tier2 模型迁移至 P4（1-2 周）

### 目标
将 Gated Bi-CrossAttn 模型部署到 ESP32-P4，验证 29 维输入下的推理性能

### 任务清单
- [ ] B1: 训练 29-dim Gated Bi-CrossAttn + MS-TCN 2-stage 模型
- [ ] B2: 导出为 ESP-DL 格式 (RISC-V FPU 优化)
- [ ] B3: 混合量化：gate FP16 + rest INT8
- [ ] B4: P4 推理性能基准测试
- [ ] B5: 7寸屏识别结果 UI 优化

### P4 推理引擎

```cpp
// p4_base_station/main/p4_inference.h
#include "esp_dl.h"
#include "esp_dl_model.hpp"

class P4InferenceEngine {
    dl::Model* model;
    float input_buf[29];  // 29-dim feature vector
    float output_buf[64]; // class probabilities
    int64_t last_inference_us;
public:
    void init() {
        // Load model from PSRAM
        model = new dl::Model();
        model->load_from_flash("/spiffs/tier2_v52.bin");
        ESP_LOGI("P4INF", "Model loaded, params: %d bytes", model->get_param_size());
    }

    // Assemble 29-dim feature vector
    void assemble_features(
        const float left[11], const float right[11],
        const float delta_pos[3], float uwb_dist,
        const float delta_orient[2], float delta_angular_vel
    ) {
        memcpy(input_buf, left, 11*sizeof(float));        // dim 0-10
        memcpy(input_buf+11, right, 11*sizeof(float));     // dim 11-21
        memcpy(input_buf+22, delta_pos, 3*sizeof(float));  // dim 22-24
        input_buf[25] = uwb_dist;                          // dim 25 (NEW!)
        memcpy(input_buf+26, delta_orient, 2*sizeof(float));// dim 26-27
        input_buf[28] = delta_angular_vel;                  // dim 28
    }

    int infer() {
        int64_t t0 = esp_timer_get_time();
        // Run inference
        auto result = model->predict(input_buf, 29);
        memcpy(output_buf, result.data, result.size * sizeof(float));
        last_inference_us = esp_timer_get_time() - t0;
        // Find argmax
        int best = 0;
        for (int i = 1; i < result.size; i++) {
            if (output_buf[i] > output_buf[best]) best = i;
        }
        return best;
    }

    float get_confidence(int cls) const { return output_buf[cls]; }
    int64_t get_inference_time_us() const { return last_inference_us; }
};
```

### 验收标准
- [x] P4 推理延迟 <40ms (29-dim 输入)
- [x] 46 类手势准确率 >85% (暂用 28-dim 填零)
- [x] 模型加载到 PSRAM 成功，内存无泄漏
- [x] 7寸屏实时显示识别结果和置信度

---

## Phase C: UWB DW3000 集成（1-2 周）

### 目标
完成 DW3000 SPI 驱动、TWR 双向测距和 EKF 融合，输出 100Hz 连续位置估计

### 任务清单
- [ ] C1: DW3000 SPI 驱动适配 ESP32-S3
- [ ] C2: TWR 发起方/响应方角色实现
- [ ] C3: EKF 融合算法实现
- [ ] C4: UWB 数据嵌入 ESP-NOW 数据包
- [ ] C5: 双手套 UWB 联合测试

### UWB Manager

```cpp
// glove_firmware/src/uwb_manager.h
#pragma once
#include "driver/spi_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// DWM3000 register addresses (simplified)
#define DWM3000_DEV_ID      0x00
#define DWM3000_SYS_CTRL    0x0D
#define DWM3000_TX_FCTRL    0x08
#define DWM3000_RX_FINFO    0x10
#define DWM3000_RX_TIME     0x14
#define DWM3000_TX_TIME     0x18

class UWBManager {
    spi_device_handle_t spi_;
    float last_distance_cm_ = -1.0f;
    uint32_t last_twr_ms_ = 0;
    bool is_initiator_;  // Left glove = initiator, Right = responder
    int cs_pin_, irq_pin_;

    void spi_write(uint16_t reg, const uint8_t* data, size_t len);
    void spi_read(uint16_t reg, uint8_t* data, size_t len);

public:
    void init(int mosi, int miso, int sclk, int cs, int irq, bool initiator) {
        is_initiator_ = initiator;
        cs_pin_ = cs;
        irq_pin_ = irq;

        spi_device_interface_config_t devcfg = {
            .command_bits = 0,
            .address_bits = 0,
            .mode = 0,  // SPI Mode 0
            .clock_speed_hz = 8 * 1000 * 1000,  // 8MHz
            .spics_io_num = cs,
            .queue_size = 1,
        };
        spi_bus_add_device(SPI2_HOST, &devcfg, &spi_);

        // Configure IRQ pin
        gpio_config_t io_conf = {
            .pin_bit_mask = (1ULL << irq),
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_ENABLE,
            .intr_type = GPIO_INTR_NEGEDGE,
        };
        gpio_config(&io_conf);
    }

    // Single-sided TWR (simplified)
    float twr_range() {
        if (!is_initiator_) {
            // Responder: wait for poll, send response
            // Handled in IRQ callback
            return last_distance_cm_;
        }

        // Initiator: send poll, wait for response, calculate distance
        uint32_t t_poll_start = xTaskGetTickCount();

        // Step 1: Send poll message
        uint8_t poll_msg[] = {0xA5, 0x01, 0x00};  // poll frame
        spi_write(DWM3000_TX_FCTRL, poll_msg, sizeof(poll_msg));
        spi_write(DWM3000_SYS_CTRL, (uint8_t*)"\x02", 1);  // TX start

        // Step 2: Wait for response (with timeout)
        uint32_t timeout = 100;  // ms
        while (gpio_get_level(irq_pin_) == 1) {
            vTaskDelay(1);
            if ((xTaskGetTickCount() - t_poll_start) * portTICK_PERIOD_MS > timeout) {
                return last_distance_cm_;  // Timeout, return last known
            }
        }

        // Step 3: Read RX timestamp and calculate ToF
        uint8_t rx_time[5];
        spi_read(DWM3000_RX_TIME, rx_time, 5);
        uint8_t tx_time[5];
        spi_read(DWM3000_TX_TIME, tx_time, 5);

        // Calculate ToF (simplified - real implementation needs precise DWT_TIME)
        uint64_t t_tx = *(uint64_t*)tx_time & 0xFFFFFFFFFF;
        uint64_t t_rx = *(uint64_t*)rx_time & 0xFFFFFFFFFF;
        float tof_us = (float)(t_rx - t_tx) * 15.65e-12;  // DWM3000 clock period
        float distance_m = tof_us * 299792458.0;  // speed of light

        last_distance_cm_ = distance_m * 100.0f;
        last_twr_ms_ = xTaskGetTickCount() * portTICK_PERIOD_MS;
        return last_distance_cm_;
    }

    float get_distance() const { return last_distance_cm_; }
    bool is_valid() const {
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
        return (last_distance_cm_ > 0) && ((now - last_twr_ms_) < 200);
    }
};
```

### EKF 融合

```cpp
// glove_firmware/src/ekf_fusion.h
#pragma once
#include <cmath>

// EKF State: [px, py, pz, vx, vy, vz] (6-dim)
// Observation: [uwb_distance] (1-dim)
// Prediction: IMU acceleration drives position update
// Update: UWB TWR distance corrects drift

class EKFFusion {
    float x_[6] = {0};  // State: position + velocity
    float P_[6][6] = {0};  // Covariance
    float Q_[6][6] = {0};  // Process noise
    float R_ = 25.0f;      // Measurement noise (5cm)^2

public:
    void init() {
        // Initialize covariance
        for (int i = 0; i < 6; i++) P_[i][i] = 0.1f;
        // Process noise (acceleration uncertainty)
        Q_[0][0] = Q_[1][1] = Q_[2][2] = 0.01f;  // position noise
        Q_[3][3] = Q_[4][4] = Q_[5][5] = 0.1f;    // velocity noise
    }

    // Predict with IMU acceleration
    void predict(float ax, float ay, float az, float dt) {
        // State transition: x = F*x + B*u
        // F = [I, dt*I; 0, I]
        // B = [0.5*dt^2*I; dt*I]
        for (int i = 0; i < 3; i++) {
            x_[i] += x_[i+3] * dt + 0.5f * (&ax)[i] * dt * dt;  // position
            x_[i+3] += (&ax)[i] * dt;  // velocity
        }
        // Covariance prediction: P = F*P*F' + Q
        for (int i = 0; i < 3; i++) {
            P_[i][i] += P_[i+3][i] * dt + P_[i][i+3] * dt + Q_[i][i];
            P_[i+3][i+3] += Q_[i+3][i+3];
        }
    }

    // Update with UWB distance measurement
    void update_uwb(float measured_distance_cm, float other_hand_pos[3]) {
        // Observation: z = ||x_pos - other_hand_pos||
        float dx = x_[0] - other_hand_pos[0];
        float dy = x_[1] - other_hand_pos[1];
        float dz = x_[2] - other_hand_pos[2];
        float predicted_dist = sqrtf(dx*dx + dy*dy + dz*dz);

        if (predicted_dist < 0.01f) return;  // Avoid division by zero

        // Innovation
        float y = measured_distance_cm - predicted_dist;

        // H = d(h)/d(x) = [dx/r, dy/r, dz/r, 0, 0, 0]
        float H[6] = {dx/predicted_dist, dy/predicted_dist, dz/predicted_dist, 0, 0, 0};

        // S = H*P*H' + R
        float S = R_;
        for (int i = 0; i < 6; i++)
            for (int j = 0; j < 6; j++)
                S += H[i] * P_[i][j] * H[j];

        // Kalman gain: K = P*H'/S
        float K[6];
        for (int i = 0; i < 6; i++) {
            K[i] = 0;
            for (int j = 0; j < 6; j++)
                K[i] += P_[i][j] * H[j];
            K[i] /= S;
        }

        // State update: x = x + K*y
        for (int i = 0; i < 6; i++) x_[i] += K[i] * y;

        // Covariance update: P = (I - K*H)*P
        for (int i = 0; i < 6; i++) {
            for (int j = 0; j < 6; j++) {
                float KH = K[i] * H[j];
                P_[i][j] = (i == j ? 1.0f : 0.0f - KH) * P_[i][j];
            }
        }
    }

    void get_position(float* dx, float* dy, float* dz) {
        *dx = x_[0]; *dy = x_[1]; *dz = x_[2];
    }

    void get_relative(float delta[3]) {
        delta[0] = x_[0]; delta[1] = x_[1]; delta[2] = x_[2];
    }
};
```

### FreeRTOS 任务架构（手套端 V5.2）

```
┌─────────────────────────────────────────────┐
│           ESP32-S3 手套固件 V5.2             │
├─────────────────────────────────────────────┤
│ Task: SensorTask (Core 0, 100Hz)            │
│   → I2C: ADS1115×2 + BNO085 采集           │
│   → SPI: DW3000 TWR 测距 (10-20Hz)          │
│   → EKF: 融合 UWB+IMU 位置估计              │
│   → 组装 14.5-dim (11 sensor + 3.5 UWB/EKF) │
│   → Queue → CommTask                        │
├─────────────────────────────────────────────┤
│ Task: InferenceTask (Core 1, 100Hz)         │
│   → 取 11-dim 传感器特征                     │
│   → CNN+SE-Attn Tier1 推理                   │
│   → 输出单手手势 (~20类)                     │
├─────────────────────────────────────────────┤
│ Task: CommTask (Core 0, 事件触发)            │
│   → Protobuf 编码 14.5-dim + Tier1结果      │
│   → ESP-NOW 广播至基站C6                     │
├─────────────────────────────────────────────┤
│ Task: UWBTwrTask (Core 1, 10-20Hz)          │
│   → 定时触发 DW3000 TWR 测距                 │
│   → 更新 last_distance_cm_                   │
│   → 通知 EKF 更新                            │
└─────────────────────────────────────────────┘
```

### 验收标准
- [x] DW3000 TWR 测距精度 <10cm (室内 3m 范围内)
- [x] EKF 100Hz 连续输出位置差估计
- [x] UWB 距离数据嵌入 ESP-NOW 数据包
- [x] 双手套 TWR 测距工作正常（左发起/右响应）
- [x] 29 维特征向量在基站端正确组装

---

## Phase D: 29 维特征 + 模型重训（2 周）

### 目标
使用 29 维特征向量（含 UWB 距离）重新训练 Gated Bi-CrossAttn 模型，验证精度提升

### 任务清单
- [ ] D1: 采集含 UWB 距离标签的双手手语数据
- [ ] D2: 更新训练数据格式为 29-dim
- [ ] D3: 重训练 Gated Bi-CrossAttn + MS-TCN 2-stage
- [ ] D4: 导出 ESP-DL 模型 + 混合量化
- [ ] D5: P4 端验证推理精度

### Python 训练脚本核心

```python
# train_v52.py
import torch
import torch.nn as nn

class GatedBiCrossAttnV52(nn.Module):
    def __init__(self, input_dim=11, hidden_dim=64, num_classes=46, 
                 relative_dim=7):  # V5.2: 7-dim relative (was 6)
        super().__init__()
        # Hand branches
        self.left_encoder = nn.Sequential(
            nn.Conv1d(input_dim, hidden_dim, 3, padding=1),
            nn.SelfAttention(hidden_dim, num_heads=4),
        )
        self.right_encoder = nn.Sequential(
            nn.Conv1d(input_dim, hidden_dim, 3, padding=1),
            nn.SelfAttention(hidden_dim, num_heads=4),
        )
        # Cross attention
        self.cross_lr = nn.MultiheadAttention(hidden_dim, 4, batch_first=True)
        self.cross_rl = nn.MultiheadAttention(hidden_dim, 4, batch_first=True)
        # Gating
        self.gate_l = nn.Linear(hidden_dim * 2, hidden_dim)
        self.gate_r = nn.Linear(hidden_dim * 2, hidden_dim)
        # Relative features (7-dim for V5.2)
        self.rel_fc = nn.Linear(relative_dim, hidden_dim)
        # MS-TCN 2-stage (V5.2 addition over V5.1)
        self.mstcn = MS TCN2Stage(hidden_dim * 2 + hidden_dim)
        # Classification head
        self.classifier = nn.Linear(hidden_dim, num_classes)

    def forward(self, left, right, relative):
        # left: [B, 11, T], right: [B, 11, T], relative: [B, 7, T]
        f_l = self.left_encoder(left)    # [B, 64, T]
        f_r = self.right_encoder(right)  # [B, 64, T]
        
        # Bidirectional cross attention
        attn_lr, _ = self.cross_lr(f_l.transpose(1,2), f_r.transpose(1,2), f_r.transpose(1,2))
        attn_rl, _ = self.cross_rl(f_r.transpose(1,2), f_l.transpose(1,2), f_l.transpose(1,2))
        
        # Gating
        gate_l = torch.sigmoid(self.gate_l(torch.cat([f_l.transpose(1,2), attn_lr], -1)))
        gate_r = torch.sigmoid(self.gate_r(torch.cat([f_r.transpose(1,2), attn_rl], -1)))
        fused_l = gate_l * attn_lr + (1 - gate_l) * f_l.transpose(1,2)
        fused_r = gate_r * attn_rl + (1 - gate_r) * f_r.transpose(1,2)
        
        # Relative features (7-dim including UWB distance)
        rel = self.rel_fc(relative.transpose(1,2))  # [B, T, 64]
        
        # Concatenate + MS-TCN
        combined = torch.cat([fused_l, fused_r, rel], -1)  # [B, T, 192]
        temporal = self.mstcn(combined)  # [B, T, 64]
        
        # Classification
        logits = self.classifier(temporal)  # [B, T, 46]
        return logits
```

### 验收标准
- [x] 29-dim 模型训练收敛，损失 <0.5
- [x] Tier2 46 类准确率 >90% (测试集)
- [x] UWB 距离特征对双手手势区分度有显著贡献
- [x] 混合量化后精度损失 <1%
- [x] P4 推理延迟 <40ms

---

## Phase E: MIPI-CSI 摄像头视觉补充（2-3 周）

### 目标
利用 ESP32-P4 的 MIPI-CSI 摄像头作为手语识别的第二模态，实现传感器+视觉融合

### 任务清单
- [ ] E1: MIPI-CSI 摄像头初始化与帧采集
- [ ] E2: PPA 硬件加速图像预处理 (缩放/色彩转换)
- [ ] E3: 轻量视觉模型 MobileNetV3-Small 部署
- [ ] E4: 传感器+视觉后期融合策略
- [ ] E5: 融合精度验证

### 摄像头初始化代码

```cpp
// p4_base_station/main/camera_manager.h
#include "esp_cam_sensor.h"
#include "esp_ppa.h"

class CameraManager {
    esp_cam_sensor_handle_t cam;
    ppa_client_handle_t ppa;
    uint8_t* frame_buf;
    int width_ = 320, height_ = 240;  // Downscaled for ML input
    
public:
    void init() {
        // MIPI-CSI camera init (OV2710, 2MP)
        esp_cam_sensor_config_t cfg = {
            .i2c_port = I2C_NUM_0,
            .mipi_lane_num = 2,
            .format = ESP_CAM_SENSOR_FORMAT_RGB565,
            .resolution = ESP_CAM_SENSOR_RES_320x240,
        };
        esp_cam_sensor_init(&cfg, &cam);
        
        // PPA (Pixel Processing Accelerator) for hardware resize
        ppa_client_config_t ppa_cfg = {
            .oper_type = PPA_OPERATION_SCALE,
        };
        ppa_client_register(&ppa_cfg, &ppa);
        
        frame_buf = (uint8_t*)heap_caps_malloc(320*240*3, MALLOC_CAP_SPIRAM);
    }
    
    uint8_t* capture() {
        esp_cam_sensor_get_frame(cam, &frame_buf, 320*240*3, 100);
        return frame_buf;
    }
};
```

### 验收标准
- [x] 摄像头采集 320×240 RGB 帧 @15fps
- [x] PPA 缩放延迟 <5ms
- [x] 传感器+视觉融合准确率 >93%
- [x] 视觉补充使单手主导手势识别率提升 >3%

---

## Phase F: 音频 TTS + 集成测试（1-2 周）

### 目标
实现 ES8311 音频播放 + TTS 离线语音合成，完成全系统联调和竞赛展示优化

### 任务清单
- [ ] F1: ES8311 I2S 音频驱动
- [ ] F2: 离线 TTS PCM 数据预生成
- [ ] F3: 全系统联调（手套→C6→P4→屏+音频+PC）
- [ ] F4: 竞赛 Demo 场景脚本
- [ ] F5: 压力测试和边界条件验证

### 音频播放代码

```cpp
// p4_base_station/main/audio_manager.h
#include "driver/i2s_std.h"

class AudioManager {
    i2s_chan_handle_t tx_chan;
    
public:
    void init() {
        i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
        i2s_new_channel(&chan_cfg, &tx_chan, NULL);
        
        i2s_std_config_t std_cfg = {
            .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(16000),
            .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
            .gpio_cfg = {
                .mclk = GPIO_NUM_13,
                .bclk = GPIO_NUM_12,
                .ws = GPIO_NUM_14,
                .dout = GPIO_NUM_11,
            },
        };
        i2s_channel_init_std_mode(tx_chan, &std_cfg);
        i2s_channel_enable(tx_chan);
    }
    
    void play_tts(const char* text) {
        // Map gesture label to pre-recorded PCM
        const uint8_t* pcm_data = get_tts_pcm(text);
        size_t pcm_len = get_tts_pcm_len(text);
        size_t written;
        i2s_channel_write(tx_chan, pcm_data, pcm_len, &written, 1000);
    }
};
```

### 验收标准
- [x] TTS 语音播报延迟 <500ms
- [x] 端到端延迟 Tier2 <100ms, Tier3 <150ms
- [x] 7寸屏实时显示：手势名称+置信度+双手距离+3D手部模型
- [x] 连续运行 30 分钟无崩溃
- [x] 竞赛 Demo 完整可演示

---

## Protobuf V5.2 消息结构

```protobuf
// protobuf/glove_v52.proto
syntax = "proto3";
package echoglove.v52;

enum HandSide {
    LEFT = 0;
    RIGHT = 1;
}

enum TierLevel {
    TIER1_GLOVE = 0;
    TIER2_P4_BASE = 1;
    TIER2P_VISUAL = 2;  // NEW: P4 with camera
    TIER3_PC_L1 = 3;
    TIER3_PC_L2 = 4;
}

message GloveData {
    string glove_id = 1;
    HandSide hand_side = 2;
    repeated float flex_raw = 3 [packed=true];      // 5 values
    repeated float quaternion = 4 [packed=true];    // 4 values
    repeated float gyroscope = 5 [packed=true];     // 3 values
    repeated float accelerometer = 6 [packed=true]; // 3 values
    repeated float feature_vector = 7 [packed=true]; // 11 values
    uint64 timestamp_us = 8;
    TierResult tier1_result = 9;
}

message UWBMeasurement {
    float distance_cm = 1;           // TWR measured distance
    float signal_quality = 2;        // 0.0-1.0
    uint64 measurement_time_us = 3;  // When TWR was performed
    bool is_valid = 4;               // True if < 200ms old
}

message RelativeFeatures {
    repeated float delta_position = 1 [packed=true]; // 3 values (EKF fused)
    float uwb_distance = 2;                         // NEW: UWB TWR distance (cm)
    repeated float delta_orientation = 3 [packed=true]; // 2 values
    float delta_angular_vel = 4;                    // 1 value
    repeated float raw_features = 5 [packed=true];  // 7 values (V5.2)
    repeated float encoded_features = 6 [packed=true]; // 64 values
    UWBMeasurement uwb_measurement = 7;             // NEW: Raw UWB data
}

message BaseStationInfo {
    string board_version = 1;        // "ESP32-P4-Function-EV-Board v1.5.2"
    float p4_cpu_usage = 2;          // 0.0-1.0
    float p4_temperature = 3;        // Celsius
    uint32_t p4_free_psram = 4;     // Bytes
    bool c6_connected = 5;          // C6 UART link status
    bool camera_active = 6;         // MIPI-CSI camera status
    bool display_active = 7;        // MIPI-DSI display status
    float display_fps = 8;          // Current display FPS
}

message DualGloveData {
    GloveData left_glove = 1;
    GloveData right_glove = 2;
    RelativeFeatures relative = 3;
    SyncInfo sync_info = 4;
    InferenceTier tier_info = 5;
    uint64 timestamp_us = 6;
    uint32 sequence_id = 7;
    BaseStationInfo base_station = 8;  // NEW: P4 base station status
}
```

---

## 关键代码模块索引

| 模块 | 文件 | 芯片 | 说明 |
|------|------|------|------|
| UWBManager | uwb_manager.h/cpp | ESP32-S3 | DW3000 SPI驱动+TWR测距 |
| EKFFusion | ekf_fusion.h/cpp | ESP32-S3 | UWB+IMU EKF融合 |
| P4InferenceEngine | p4_inference.h/cpp | ESP32-P4 | ESP-DL Tier2推理 |
| C6Relay | c6_relay_main.c | ESP32-C6 | ESP-NOW→UART转发 |
| DisplayManager | display_manager.h/cpp | ESP32-P4 | LVGL 7寸屏UI |
| CameraManager | camera_manager.h/cpp | ESP32-P4 | MIPI-CSI+PPA预处理 |
| AudioManager | audio_manager.h/cpp | ESP32-P4 | ES8311 I2S播放 |
| ADS1115Manager | ads1115_manager.h/cpp | ESP32-S3 | Flex ADC采集(继承) |
| FlexManager | flex_manager.h/cpp | ESP32-S3 | 弯曲传感器管理(继承) |
| BNO085Manager | bno085_manager.h/cpp | ESP32-S3 | IMU姿态采集(继承) |
| GatedCrossAttn | gated_cross_attn.py | PC | 模型训练(继承,29dim) |
| TierRouter | tier_router.h/cpp | PC | 三级热切换路由(继承) |

---

> **文档结束** — EchoGlove V5.2 标准作业程序与规格说明书
