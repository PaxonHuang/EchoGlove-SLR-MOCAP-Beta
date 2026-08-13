# PROGRESS_CN.md — 关键里程碑摘要 (中文)

> **权威进度**: `PROGRESS.md`（英文，含代码核实状态表与技术债清单）。本文件为其精简的中文摘要，非全量翻译。历史 debug 细节见 `docs/archive/` 与 git 历史。

**最近核实**: 2026-07-10 ｜ **分支**: `feature/v6-dual-s3p4-flex-lsm6dsv16x`

---

## 当前系统状态（代码核实 2026-07-10）

| 层 | 状态 | 说明 |
|----|------|------|
| ✅ Flex 传感器 | 已实现 | ESP32-S3 内部 ADC1 (GPIO1–5), N=16 过采样, NVS 标定 — `InternalADCManager.h` |
| ✅ S3 通信 | 已实现 | ESP-NOW 广播 (`esp_now_send`, 69B `GlovePacket`) |
| ✅ C6→P4 中继 | 已实现(代码) | `c6_firmware` ESP-NOW→UART 2Mbps。⚠️ EV 板载 C6 是 ESP-Hosted，跑不了此桥 |
| ✅ P4 接收/输出 | 已实现 | P4 UART (GPIO37/38, 2Mbps) → Tier2 stub → USB CDC JSON |
| ✅ P4 单机 | 已验证 | LVGL+TFLite stub+ES8311 音频+TinyUSB CDC (`CONFIG_P4_INTERNAL_MOCK=y`, 提交 `4f541bb`) |
| 🟡 IMU (LSM6DSV16X) | **设计/未实现** | 驱动不存在; IMU 输出=全零。BNO085 已从活跃代码移除 |
| 🟡 有线 UART (S3→P4) | **设计/未实现** | `WIRED_UART` 标志不在代码中; S3 仍用 ESP-NOW。设计见 `specs/2026-07-08-s3-p4-wired-uart-design.md` |
| 🟡 Wi-Fi/UDP (后期) | 规划中 | C6 ESP-Hosted → Wi-Fi/UDP → P4 (量产) |
| ❌ BNO085 / ADS1115 | 已废弃 | 死代码 + 滞留 lib_deps — 见技术债 |

**今日唯一验证的端到端路径**: 仅 P4 单机 + 内部 mock 数据。

---

## V6.0 迁移进度

- **已完成**: Flex 内部 ADC1 迁移（TDD, 38/38 native 测试通过, `pio run` SUCCESS, 6 commits `94bcfdb`→`7ed6624`）。V6 设计文档 `docs/V6/01–07` 完成。开发环境一键化 `/setup-env`（2026-07-10）。
- **待实现**: ① LSM6DSV16X 驱动 (当前 IMU=零) + Madgwick 备选 ② S3→P4 有线 UART (`WIRED_UART`, TDD, 5 阶段未写) ③ 上机验证 V1–V7 (需硬件)
- **设计 spec**: `docs/V6/04_SOP-SPEC-PLAN_V6.md`；ADC 专题: `docs/V6/07_internal_adc_migration.md`

---

## 技术债（代码层，待单独 commit 清理）

- `lib_deps` 滞留 `Adafruit BNO08x` + `NimBLE-Arduino`（注意这是死依赖，无活跃代码引用）
- `lib/Sensors/ADS1115Manager.h` 未被引用（死代码）
- `Sensors.h` / `Comms.h` 聚合头注释仍为 V5 描述
- `CONFIG_UDP_PORT=8888` 标志无运行时实现（V5 历史残留）

---

## P4 基站验证 (A3, 2026-07-08)

- P4 单机验证 PASS（boot/LVGL/FramePairer/TFLite stub/ES8311/TinyUSB CDC），提交 `4f541bb`、`3cf2f3a`
- 板载 C6 = ESP-Hosted 协处理器，**不支持 ESP-NOW**，未烧录。开发期 S3→P4 直连 UART，C6 推迟到 Wi-Fi 集成
- 历史 I²C/ADS1115 breadboard 排障（2026-05/06）已被 V6 内部 ADC 迁移取代，见 `docs/archive/`

---

## V5.3 有线 UART（2026-07-09，设计阶段）

- 枢纽: 板载 C6 是 ESP-Hosted（无 ESP-NOW）→ 开发期 S3→P4 直连 UART（绕过 C6），C6 后期做 Wi-Fi
- S3 GPIO6 (TX) → P4 GPIO38 (RX) + GND; 双手走双 UART；73B CRC-16/MODBUS 帧
- **状态**: 仅设计。Phase 1–5 (UARTTransmitter TDD → main 并行 TX → P4 mock 关 → 硬件验证 → 双 UART) 均 pending

---

## V5.2 P4 基站（历史，代码已完成）

8/8 任务完成（提交 `f4e4d34`..`bef0c96`）：UART 帧(12/12)、C6 中继、P4 接收+配对+28维、Tier2 stub、LVGL、TTS+USB CDC、relay 扩展(88/88)、集成测试。Spec/plan 为历史（C6 ESP-NOW 假设已被 V5.3 取代）。

---

## 会话延续

1. 先读 `PROGRESS.md`（权威英文版，含完整技术债 + 状态表）
2. 注意 🟡/❌ 项**尚未实现**，勿当现役
3. 从最近 checkpoint 续接
