# EchoGlove V4 逐步执行手册

> **使用方式：** 按顺序执行每一步，每步完成后打勾确认  
> **终端操作：** 你手动执行  
> **Claude Code 操作：** 在 Claude Code 对话中粘贴对应 Prompt  

---

## Phase 0: 环境准备 (30 分钟)

### Step 0.1: 确认当前项目状态

```bash
# 进入项目目录
cd echoglove

# 确认在 main 分支
git branch
# 应该看到 * main

# 确认代码干净
git status
# 应该显示: nothing to commit, working tree clean

# 确认 V3 可以编译
cd glove_firmware && pio build
# 必须成功，0 errors

# 回到项目根目录
cd ..
```

- [ ] ✅ 确认在 main 分支
- [ ] ✅ 确认代码干净
- [ ] ✅ 确认 V3 编译通过

### Step 0.2: 创建迁移分支 & 打标签

```bash
# 给 V3 打标签
git tag -a v3.0-final -m "EchoGlove V3.0 final - Hall+Magnet architecture"

# 创建迁移分支
git checkout -b v4/flex-sensor-migration

# 确认分支
git branch
# * v4/flex-sensor-migration
#   main
```

- [ ] ✅ v3.0-final 标签已创建
- [ ] ✅ v4/flex-sensor-migration 分支已创建

### Step 0.3: 复制 CLAUDE.md 到项目

```bash
# 复制 V4 CLAUDE.md 到项目根目录
# 注意：如果项目已有 CLAUDE.md，先备份
cp CLAUDE.md CLAUDE.md.v3.backup 2>/dev/null || true

# 从迁移文档目录复制新的 CLAUDE.md
cp docs/v4-migration/migration-ops/01_CLAUDE_MD_V4.md CLAUDE.md

# 如果项目根目录没有 docs/v4-migration，先创建
mkdir -p docs/v4-migration
cp -r /root/.openclaw/workspace/echoglove-analysis/output/* docs/v4-migration/
```

- [ ] ✅ CLAUDE.md 已更新为 V4 版本
- [ ] ✅ 迁移文档已复制到 docs/v4-migration/

### Step 0.4: 更新 .gitignore

```bash
# 检查 .gitignore 是否需要更新
cat .gitignore

# 如果没有以下内容，追加：
cat >> .gitignore << 'EOF'

# V4 additions
glove_firmware/generated/
glove_relay/models/*.onnx
glove_relay/models/*.pt
data/*.tfrecord
data/*.npz
*.tflite
EOF
```

- [ ] ✅ .gitignore 已更新

### Step 0.5: 提交初始状态

```bash
git add -A
git commit -m "v4: chore: setup migration branch with CLAUDE.md and docs"

# 确认提交
git log --oneline -3
```

- [ ] ✅ 初始提交完成

---

## Phase 1: 删除 V3 Hall/MUX 代码 (Prompt 04)

### 执行

在 Claude Code 中粘贴：
1. **Prompt 00**（项目上下文）
2. **Prompt 04**（删除 V3 Hall 栈）

### 验证

```bash
# 检查文件是否删除
ls glove_firmware/components/tmag5273/  # 应该报错 "No such file"
ls glove_firmware/components/tca9548a/  # 应该报错 "No such file"

# 检查引用是否清除
grep -r "tmag5273" glove_firmware/       # 应该无结果
grep -r "tca9548a" glove_firmware/       # 应该无结果

# 编译验证（可能有其他依赖错误，但不应有 tmag/tca 相关错误）
cd glove_firmware && pio build 2>&1 | head -50
```

### 提交

```bash
git add -A
git commit -m "v4: remove: delete TMAG5273 Hall sensor and TCA9548A MUX drivers

- Deleted components/tmag5273/ (Hall effect sensor driver)
- Deleted components/tca9548a/ (I2C multiplexer driver)
- Removed all references from main.c and CMakeLists.txt
- Updated migration log"
```

- [ ] ✅ tmag5273 目录已删除
- [ ] ✅ tca9548a 目录已删除
- [ ] ✅ 所有引用已清除
- [ ] ✅ 已提交

---

## Phase 2: ADS1115 驱动 (Prompt 01)

### 执行

在 Claude Code 中粘贴：
1. **Prompt 00**
2. **Prompt 01**（ADS1115 驱动）

### 验证

```bash
# 检查文件创建
ls glove_firmware/components/ads1115/
# 应该有: ads1115.c, ads1115.h, CMakeLists.txt, Kconfig, test/

# 编译验证
cd glove_firmware && pio build
# 必须成功

# 运行测试（如果有硬件）
pio test -e esp32s3 -f test_ads1115
```

### 提交

```bash
git add -A
git commit -m "v4: add: ADS1115 16-bit I2C ADC driver (dual instance, 860SPS)

- components/ads1115/ads1115.c: I2C driver with mutex protection
- components/ads1115/ads1115.h: API header
- Support dual instances (0x48, 0x49)
- Configurable PGA (default ±4.096V)
- Single-shot and continuous modes
- Kconfig for menuconfig options
- Unit tests"
```

- [ ] ✅ ads1115 组件已创建
- [ ] ✅ 编译通过
- [ ] ✅ 已提交

---

## Phase 3: BNO085 6DOF 模式 (Prompt 02)

### 执行

1. **Prompt 00**
2. **Prompt 02**（BNO085 6DOF）

### 验证

```bash
# 检查修改
git diff glove_firmware/components/bno085/

# 编译验证
cd glove_firmware && pio build
```

### 提交

```bash
git add -A
git commit -m "v4: update: BNO085 to 6DOF Game Rotation Vector mode

- Changed initialization to SH2_GAME_ROTATION_VECTOR
- Disabled magnetometer reports
- Added quaternion→euler conversion
- Added bno085_get_euler() and bno085_get_gyro()"
```

- [ ] ✅ BNO085 驱动已更新
- [ ] ✅ 编译通过
- [ ] ✅ 已提交

---

## Phase 4: FlexManager 模块 (Prompt 03)

### 执行

1. **Prompt 00**
2. **Prompt 03**（Flex 传感器模块）

### 验证

```bash
ls glove_firmware/components/flex_manager/
# 应该有: flex_manager.c, flex_manager.h, CMakeLists.txt

cd glove_firmware && pio build
```

### 提交

```bash
git add -A
git commit -m "v4: add: FlexManager module (calibration, EMA filter, NVS)

- components/flex_manager/flex_manager.c
- Calibration with min/max tracking
- EMA filter (α=0.15)
- NVS persistence for calibration data
- Fault detection for out-of-range values"
```

- [ ] ✅ FlexManager 组件已创建
- [ ] ✅ 编译通过
- [ ] ✅ 已提交

---

## Phase 5: 采样任务重写 (Prompt 06)

### 执行

1. **Prompt 00**
2. **Prompt 06**（Core1 采样任务）

### 验证

```bash
# 检查 sensor_manager 修改
git diff glove_firmware/components/sensor_manager/

cd glove_firmware && pio build
```

### 提交

```bash
git add -A
git commit -m "v4: rewrite: sensor sampling task for 11-dim at 100Hz

- ADS1115 + BNO085 6DOF reads
- 11-dim feature vector assembly
- <8ms execution budget
- FreeRTOS task pinned to Core 1
- Frame overrun detection"
```

- [ ] ✅ SensorManager 已重写
- [ ] ✅ 编译通过
- [ ] ✅ 已提交

---

## Phase 6: Protobuf V4 (Prompt 05)

### 执行

1. **Prompt 00**
2. **Prompt 05**（Protobuf V4）

### 验证

```bash
# 检查 proto 文件
cat glove_firmware/proto/sensor_frame.proto

# 检查生成的代码
ls glove_firmware/generated/

cd glove_firmware && pio build
```

### 提交

```bash
git add -A
git commit -m "v4: update: protobuf schema V4 (flex_values, imu_euler)

- Replaced hall_values with flex_values[5]
- Replaced imu_quaternion with imu_euler[3]
- Removed imu_accel
- Version field = 4
- V3 backward compatibility maintained"
```

- [ ] ✅ Proto 文件已更新
- [ ] ✅ 代码已生成
- [ ] ✅ 编译通过
- [ ] ✅ 已提交

---

## Phase 7: Python Relay (Prompt 07)

### 执行

1. **Prompt 00**
2. **Prompt 07**（Relay 解析器）

### 验证

```bash
# 检查修改
git diff glove_relay/

# 运行测试
cd glove_relay && python -m pytest tests/ -v
```

### 提交

```bash
git add -A
git commit -m "v4: update: relay parser with V3/V4 auto-detection

- Unified SensorData output format
- V3→V4 conversion (quaternion→euler)
- Version auto-detection from protobuf field
- Updated ST-GCN mapper for flex% input"
```

- [ ] ✅ Relay 解析器已更新
- [ ] ✅ 测试通过
- [ ] ✅ 已提交

---

## Phase 8: R3F 前端 (Prompt 08)

### 执行

1. **Prompt 00**
2. **Prompt 08**（R3F 骨架）

### 验证

```bash
cd glove_web && npm run build
# 必须成功
```

### 提交

```bash
git add -A
git commit -m "v4: update: R3F hand skeleton flex→bone angle mapping

- flexToAngle() with kinematic ratios
- Thumb-specific CMC/MCP/IP mapping
- Wrist rotation from IMU euler
- Smooth animation (lerp)"
```

- [ ] ✅ R3F 前端已更新
- [ ] ✅ 构建通过
- [ ] ✅ 已提交

---

## Phase 9: L1 模型 (Prompt 10)

### 执行

1. **Prompt 00**
2. **Prompt 10**（L1 模型架构）

### 验证

```bash
# 检查模型文件
ls glove_firmware/models/

# 验证参数量
cd glove_firmware/models && python l1_model_v4.py
# 应该输出: Total parameters: ~28,000
```

### 提交

```bash
git add -A
git commit -m "v4: add: L1 model architecture for 330-dim input

- 1D-CNN+Attention, ~28K parameters
- Input: 30×11 = 330 dimensions
- Training script with augmentation
- INT8 quantization export
- Benchmark script"
```

- [ ] ✅ L1 模型代码已创建
- [ ] ✅ 参数量 < 50K
- [ ] ✅ 已提交

---

## Phase 10: ST-GCN L2 模型 (Prompt 11)

### 执行

1. **Prompt 00**
2. **Prompt 11**（ST-GCN）

### 验证

```bash
cd glove_relay && python -m pytest tests/test_st_gcn.py -v
```

### 提交

```bash
git add -A
git commit -m "v4: update: ST-GCN skeleton remapping for flex sensors

- Scheme A (direct) for MVP
- Scheme C (hybrid) recommended for production
- Flex% → 21-node skeleton mapping
- Kinematic ratios per finger"
```

- [ ] ✅ ST-GCN 映射已更新
- [ ] ✅ 测试通过
- [ ] ✅ 已提交

---

## Phase 11: Unity (Prompt 12)

### 执行

1. **Prompt 00**
2. **Prompt 12**（Unity ms-MANO）

### 验证

```bash
# Unity 编辑器中验证
# 或命令行构建（如果配置了）
```

### 提交

```bash
git add -A
git commit -m "v4: update: Unity ms-MANO mapper for flex sensor input

- MapFlexToMano() function
- IMU euler → wrist rotation
- Kinematic ratio configuration"
```

- [ ] ✅ Unity 脚本已更新
- [ ] ✅ 已提交

---

## Phase 12: 数据采集工具 (Prompt 09)

### 执行

1. **Prompt 00**
2. **Prompt 09**（数据采集）

### 验证

```bash
cd glove_web && npm run build
```

### 提交

```bash
git add -A
git commit -m "v4: feat: dataset collection web tool

- Gesture list display with reference
- Real-time recording at 100Hz
- Quality validation (duration, range, dropout)
- CSV/TFRecord/NumPy export
- Backend API for storage"
```

- [ ] ✅ 数据采集工具已创建
- [ ] ✅ 构建通过
- [ ] ✅ 已提交

---

## Phase 13: 集成测试 (Prompt 13)

### 执行

1. **Prompt 00**
2. **Prompt 13**（E2E 测试）

### 验证

```bash
# 固件测试
cd glove_firmware && pio test -e esp32s3

# Relay 测试
cd glove_relay && python -m pytest tests/ -v

# 前端测试
cd glove_web && npm test
```

### 提交

```bash
git add -A
git commit -m "v4: test: end-to-end integration test suite

- Firmware unit tests (ADS1115, FlexManager, BNO085)
- Relay integration tests (V3/V4 parsing, ST-GCN mapping)
- Frontend component tests
- Latency benchmarks"
```

- [ ] ✅ 固件测试通过
- [ ] ✅ Relay 测试通过
- [ ] ✅ 前端测试通过
- [ ] ✅ 已提交

---

## Phase 14: 文档 & 发布 (Prompt 14)

### 执行

1. **Prompt 00**
2. **Prompt 14**（文档）

### 验证

```bash
# 检查文档
ls docs/
cat CHANGELOG.md
```

### 提交

```bash
git add -A
git commit -m "v4: docs: migration guide, architecture, changelog

- Updated README.md for V4
- docs/V4_MIGRATION_GUIDE.md
- docs/CALIBRATION_GUIDE.md
- docs/DATASET_GUIDE.md
- docs/ARCHITECTURE.md
- CHANGELOG.md V4.0.0 entry"
```

### 发布

```bash
# 打发布标签
git tag -a v4.0.0 -m "EchoGlove V4.0.0 - Flex Sensor Architecture"

# 合并到 main（可选，等所有测试通过后）
git checkout main
git merge v4/flex-sensor-migration
git push origin main --tags
```

- [ ] ✅ 文档完成
- [ ] ✅ 已提交
- [ ] ✅ v4.0.0 标签已创建

---

## 检查点汇总

| Phase | Prompt | 检查项 | 状态 |
|---|---|---|---|
| 0 | — | 分支创建、CLAUDE.md 更新 | ⬜ |
| 1 | 04 | Hall/MUX 代码删除 | ⬜ |
| 2 | 01 | ADS1115 驱动 | ⬜ |
| 3 | 02 | BNO085 6DOF | ⬜ |
| 4 | 03 | FlexManager | ⬜ |
| 5 | 06 | 采样任务 | ⬜ |
| 6 | 05 | Protobuf V4 | ⬜ |
| 7 | 07 | Relay 解析器 | ⬜ |
| 8 | 08 | R3F 前端 | ⬜ |
| 9 | 10 | L1 模型 | ⬜ |
| 10 | 11 | ST-GCN | ⬜ |
| 11 | 12 | Unity | ⬜ |
| 12 | 09 | 数据采集 | ⬜ |
| 13 | 13 | 集成测试 | ⬜ |
| 14 | 14 | 文档发布 | ⬜ |
