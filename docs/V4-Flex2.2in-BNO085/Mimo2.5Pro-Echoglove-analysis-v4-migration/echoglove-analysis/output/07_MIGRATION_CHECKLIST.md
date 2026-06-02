# EchoGlove V3→V4 迁移检查清单

> **版本**: V4.0 | **迁移类型**: Hall+Magnet → Flex+ADS1115

---

## 0 迁移前准备

### 0.1 代码准备
- [ ] 创建 Git 迁移分支：`git checkout -b migration/v4-flex-sensor`
- [ ] 备份当前 V3 分支：`git tag v3.0-final`
- [ ] 导出当前 V3 的完整目录结构作为参考
- [ ] 记录当前 V3 的编译产物大小（Flash/PSRAM 占用）

### 0.2 硬件准备
- [ ] 采购 V4 新硬件（见 `05_BOM.md`）
- [ ] 准备测试面包板
- [ ] 准备万用表、逻辑分析仪（可选）
- [ ] 下载 ADS1115 和 BNO085 数据手册

### 0.3 环境准备
- [ ] PlatformIO 环境正常（`pio run` 能编译 V3）
- [ ] Python 3.10+ 环境（Relay 开发）
- [ ] Node.js 18+ 环境（Web 前端开发）
- [ ] Unity 2022 LTS 安装（可选，Phase 6）

---

## Phase 1：HAL 与驱动层

### 1.1 删除 V3 Hall 相关文件
- [ ] 删除 `glove_firmware/src/drivers/tmag5273.h`
- [ ] 删除 `glove_firmware/src/drivers/tmag5273.c`
- [ ] 删除 `glove_firmware/src/drivers/tca9548a.h`
- [ ] 删除 `glove_firmware/src/drivers/tca9548a.c`
- [ ] 搜索全项目 `TMAG5273`/`TCA9548A`/`tmag`/`tca9548` 引用并清理
- [ ] 更新 `CMakeLists.txt` 或 `component.mk`

### 1.2 新增 ADS1115 驱动
- [ ] 创建 `glove_firmware/src/drivers/ads1115.h`
- [ ] 创建 `glove_firmware/src/drivers/ads1115.c`
- [ ] 实现多实例支持（0x48, 0x49）
- [ ] 实现连续转换模式，860SPS
- [ ] 实现线程安全（FreeRTOS mutex）
- [ ] 编写 `test/test_ads1115.c` 并通过

### 1.3 修改 BNO085 驱动
- [ ] 修改 `glove_firmware/src/drivers/bno085.h`
- [ ] 修改 `glove_firmware/src/drivers/bno085.c`
- [ ] 禁用所有磁力计相关 Report（ROTATION_VECTOR, GEOMAG, MAG_FIELD）
- [ ] 启用 SH2_GAME_ROTATION_VECTOR (0x08) @ 100Hz
- [ ] 添加编译开关 `BNO085_USE_6DOF`
- [ ] 编写 `test/test_bno085_6dof.c` 并通过

### 1.4 新增 Flex 传感器模块
- [ ] 创建 `glove_firmware/src/sensors/flex_sensor.h`
- [ ] 创建 `glove_firmware/src/sensors/flex_sensor.c`
- [ ] 实现校准流程（开手 + 握拳）
- [ ] 实现 EMA 滤波（α=0.15）
- [ ] 实现死区处理（<1% 忽略）
- [ ] 实现 NVS 校准数据持久化
- [ ] 编写 `test/test_flex_sensor.c` 并通过

### Gate Review 1
- [ ] I2C 扫描发现 3 个设备（0x48, 0x49, 0x4B）
- [ ] ADS1115 读数正常（弯曲时有变化）
- [ ] BNO085 输出四元数（无磁力计数据）
- [ ] 所有驱动测试通过
- [ ] `pio run` 编译成功，无错误

---

## Phase 2：信号处理层

### 2.1 SensorManager 改造
- [ ] 修改 `glove_firmware/src/sensors/sensor_manager.h`
- [ ] 修改 `glove_firmware/src/sensors/sensor_manager.c`
- [ ] 移除 I2C Mux 切换逻辑
- [ ] 集成 ADS1115Manager + FlexManager + BNO085
- [ ] SensorData 结构体从 21 维改为 11 维
- [ ] 保留仿真模式（无硬件测试用）

### 2.2 Kalman 滤波器调整
- [ ] 修改通道数：21 → 11
- [ ] Flex 通道参数：Q=0.001, R=0.05
- [ ] IMU 通道参数保持：Q=0.01, R=0.1
- [ ] 验证滤波效果（静态噪声 < 1%）

### 2.3 滑动窗口调整
- [ ] 修改 `FEATURE_DIM`：21 → 11
- [ ] 窗口大小保持 30 帧
- [ ] PSRAM 占用从 ~25KB 降至 ~13KB

### 2.4 特征归一化
- [ ] Min-Max 归一化通道数调整为 11
- [ ] 校准流程适配 Flex 传感器（伸直+弯曲基准）

### Gate Review 2
- [ ] 传感器数据流畅通：ADS1115→Flex→Kalman→Window
- [ ] 数据维度正确：每帧 11 维特征
- [ ] 滤波后噪声水平可接受
- [ ] 采样率 ≥ 100Hz

---

## Phase 3：通信协议层

### 3.1 Protobuf V4
- [ ] 创建 `glove_firmware/proto/sensor_frame_v4.proto`
- [ ] 生成 nanopb C 代码：`sensor_frame_v4.pb.h/.c`
- [ ] 生成 Python protobuf：`sensor_frame_v4_pb2.py`
- [ ] 更新 `glove_firmware/src/comm/protobuf_tx.c`（V4 编码）
- [ ] 添加版本标记：首字节 0x04

### 3.2 WebSocket JSON V4
- [ ] 更新 Relay→前端 JSON 格式（flex 数组替换 hall 数组）
- [ ] 更新前端 Zustand store（useSensorStore）

### Gate Review 3
- [ ] ESP32 编码 V4 帧 → Python 成功解码
- [ ] 帧大小 ~30-35 字节
- [ ] V3/V4 自动检测正常

---

## Phase 4：Python Relay 层

### 4.1 解析器更新
- [ ] 修改 `glove_relay/src/parser.py`
- [ ] 实现 V3/V4 自动检测（首字节判断）
- [ ] V4 解析：flex 百分比 + 四元数
- [ ] Q15 四元数转 float：`q_f = q_i16 / 32768.0`
- [ ] 编写 `tests/test_parser_v4.py` 并通过

### 4.2 L2 ST-GCN 模型适配
- [ ] 修改 `glove_relay/models/l2_st_gcn.py`
- [ ] 重设计伪骨骼映射层（推荐混合方案 C）
- [ ] 5 个 Flex 值 → 5 个 MCP 节点
- [ ] BNO085 四元数 → 腕节点
- [ ] 其余节点通过 Feature Lifting 层学习插值
- [ ] 图拓扑和邻接矩阵不变

### 4.3 NLP 与 TTS（不变）
- [ ] 确认 CSL NLP 引擎不受影响
- [ ] 确认 edge-tts 不受影响
- [ ] 确认置信度路由器输入仅为 gesture_id + confidence

### Gate Review 4
- [ ] Relay 能同时处理 V3 和 V4 数据包
- [ ] L2 ST-GCN 前向传播通过（随机输入验证）
- [ ] NLP/TTS 功能正常

---

## Phase 5：L1 边缘推理

### 5.1 模型架构调整
- [ ] 修改 `glove_relay/models/l1_cnn_attention.py`
- [ ] 输入维度：30×21=630 → 30×11=330
- [ ] Conv1d 输入通道：21 → 11（或 9，如果只用 flex+quat）
- [ ] 参数量目标：~20K（从 34K 降低）
- [ ] Tensor Arena 目标：~50KB（从 80KB 降低）

### 5.2 训练数据采集
- [ ] 制作数据采集工具（Web 或串口）
- [ ] 46 个 CSL 手势 × 10 次重复 = 460 次采集
- [ ] 每次 ~3 秒 @ 100Hz = ~300 帧
- [ ] 总采集时间 ~23 分钟
- [ ] 数据格式：`[flex_thumb, flex_index, flex_middle, flex_ring, flex_pinky, qw, qx, qy, qz]`

### 5.3 模型训练
- [ ] 路径 A：Edge Impulse 快速原型（2-3 天）
- [ ] 路径 B：PyTorch 自定义训练（更高精度）
- [ ] 训练目标：Top-1 > 90% (46 类)
- [ ] INT8 量化导出 TFLite Micro

### 5.4 模型部署
- [ ] 导出 TFLite 模型文件
- [ ] 更新 Tensor Arena 分配
- [ ] 集成到 ESP32 固件
- [ ] 运行时推理验证

### Gate Review 5
- [ ] L1 推理延迟 < 5ms
- [ ] L1 Top-1 准确率 > 90%
- [ ] 模型大小 < 200KB
- [ ] Flash/PSRAM 占用在限制内

---

## Phase 6：Web 前端

### 6.1 3D 手部渲染更新
- [ ] 修改 `glove_web/src/components/HandSkeleton.tsx`
- [ ] 解析 V4 WebSocket JSON
- [ ] Flex% → 关节角度映射（MCP 50%, PIP 30%, DIP 20%）
- [ ] 四元数直接应用到腕部旋转
- [ ] 保持 V3 向后兼容（version==3 时用旧 IK）

### 6.2 校准 UI
- [ ] 添加"校准开手"按钮
- [ ] 添加"校准握拳"按钮
- [ ] 通过 WebSocket 发送校准命令到 ESP32

### 6.3 数据采集工具（可选）
- [ ] 创建 `glove_web/src/tools/DataCollector.tsx`
- [ ] 志愿者信息录入
- [ ] 手势录制流程（46 类 × 20 重复）
- [ ] 数据导出（JSONL 格式）

### Gate Review 6
- [ ] 3D 手部动画流畅（60 FPS）
- [ ] Flex 值正确映射到手指弯曲
- [ ] 四元数正确映射到腕部旋转
- [ ] 校准功能正常

---

## Phase 7：Unity 渲染（可选）

### 7.1 ms-MANO 适配
- [ ] 修改 `glove_unity/Scripts/HandPoseDriver.cs`
- [ ] Flex% → MANO 关节角度映射
- [ ] 四元数应用到腕部
- [ ] 添加校准模式（C 键=开手，F 键=握拳）
- [ ] 漂移补偿（静止 5 秒重置航向）

### Gate Review 7
- [ ] Unity 中开手/握拳姿态正确
- [ ] 帧率 > 90 FPS
- [ ] 漂移补偿有效

---

## Phase 8：集成测试

### 8.1 硬件在环测试
- [ ] 完整硬件连接运行 5 分钟
- [ ] 丢包率 < 0.1%
- [ ] 无 I2C 错误
- [ ] 无 FreeRTOS 看门狗触发

### 8.2 延迟测试
- [ ] 端到端延迟 < 100ms（传感器→L1→UDP→Relay→WS→前端渲染）
- [ ] 记录 P95/P99 延迟

### 8.3 准确率测试
- [ ] 10 个已知手势测试
- [ ] L1 Top-1 > 90%
- [ ] L2 Top-1 > 95%（如有训练数据）

### 8.4 漂移测试
- [ ] 静态 5 分钟，腕部朝向漂移 < 15°

### 8.5 电池测试
- [ ] 4.2V→3.3V 连续运行 > 2 小时

### Gate Review 8
- [ ] 所有集成测试通过
- [ ] 端到端延迟达标
- [ ] 准确率达标

---

## Phase 9：文档与发布

- [ ] 更新 `README.md`（V4 架构图、BOM、Quick Start）
- [ ] 更新 `docs/HARDWARE_ASSEMBLY_GUIDE.md`
- [ ] 更新 `docs/SOP_SPEC_PLAN_V3.md` → V4
- [ ] 更新 `CHANGELOG.md`
- [ ] 更新 `docs/CLAUDE_CODE_PROMPTS_V3.md` → V4
- [ ] Git tag `v4.0.0`

---

## 回滚计划

如果迁移失败，按以下步骤回滚：

1. **代码回滚**：`git checkout migration/v4-flex-sensor~1` 或 `git checkout v3.0-final`
2. **硬件回滚**：重新安装 V3 Hall 传感器硬件
3. **模型回滚**：使用 V3 已训练模型
4. **数据回滚**：V3 采集的数据集仍有效

**关键决策点**：
- Phase 1 Gate Review 失败 → 检查硬件接线，不回滚
- Phase 5 Gate Review 失败（准确率 < 85%）→ 考虑增加传感器维度或更换模型架构
- Phase 8 Gate Review 失败（延迟 > 200ms）→ 优化通信协议或降低采样率

---

## 迁移时间估算

| Phase | 预计时间 | 依赖 |
|-------|----------|------|
| Phase 1: HAL & 驱动 | 3-5 天 | 硬件到货 |
| Phase 2: 信号处理 | 2-3 天 | Phase 1 |
| Phase 3: 通信协议 | 1-2 天 | Phase 2 |
| Phase 4: Relay | 2-3 天 | Phase 3 |
| Phase 5: L1 模型 | 5-7 天 | Phase 2 + 数据采集 |
| Phase 6: Web 前端 | 2-3 天 | Phase 4 |
| Phase 7: Unity（可选） | 3-5 天 | Phase 4 |
| Phase 8: 集成测试 | 2-3 天 | All |
| Phase 9: 文档 | 1-2 天 | Phase 8 |
| **总计** | **21-33 天** | |

---

*每完成一个 Phase，运行对应 Gate Review。全部通过后方可进入下一 Phase。*
