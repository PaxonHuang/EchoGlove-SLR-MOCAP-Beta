# EchoGlove V5.0 — Claude Code 分阶段AI编程提示词

> 面向 Claude Code 的V5.0完整分阶段提示词  
> 三级热切换 + Gated Bi-CrossAttention + ST-GCN→MS-TCN→CTC  
> 分支: `Flex2.2in-BNO085-ADS1115-ESP32S3N16R8-ArduinoFramework`

---

## 使用说明

1. 每个 Phase 对应一次 Claude Code 会话
2. 复制对应Phase提示词到 Claude Code
3. 每Phase完成后 commit，再进入下一Phase
4. 所有路径相对于项目根目录 `EchoGlove-SLR-MOCAP-Alpha/`

---

## Phase 1: 单手固件稳定化 + Tier1模型部署

```
我正在将EchoGlove项目从霍尔传感器迁移到弯曲传感器方案，并在手套端部署Tier1边缘推理模型。

## 项目背景
- 仓库: https://github.com/PaxonHuang/EchoGlove-SLR-MOCAP-Alpha
- ESP32-S3 N16R8, Arduino Framework, PlatformIO
- 原方案: 5×TMAG5273 + 5×N52磁铁 + TCA9548A + BNO085
- 新方案: 5×SpectraFlex 2.2" + 2×ADS1115 + BNO085(GRV 6轴) + Tier1 TFLite推理

## 本次任务

### 1.1 移除旧传感器代码
- 删除TCA9548A和TMAG5273引用
- data_structures.h: 移除hall_xyz[15], 新增flex[5], FEATURE_COUNT=11
- SensorData.toFeatureArray() 输出 [flex0-4, euler0-2, gyro0-2]

### 1.2 新增ADS1115Manager
- 创建 lib/Sensors/ADS1115Manager.h
- 两片ADS1115: 0x48(通道0-2→Flex0-2), 0x49(通道0-1→Flex3-4)
- 增益±2.048V, 连续模式, I2C 100kHz
- readAll(float out[5]) 方法

### 1.3 新增FlexManager
- 创建 lib/Sensors/FlexManager.h
- 5点分段线性校准, 滑动平均滤波(窗口8)
- 温漂自动补偿(每30秒检测零点偏移)
- 输出0~90°弯曲角度(人体生理约束)

### 1.4 修改IMUManager
- BNO085使用SH2_GAME_ROTATION_VECTOR (GRV 6轴)
- 不启用磁力计

### 1.5 修改SensorManager
- 移除TCA9548A+TMAG5273, 新增ADS1115Manager+FlexManager
- readAll() 填充新的SensorData(11-dim)

### 1.6 修改通信协议
- BLE CSV: 移除15个Hall值, 新增5个Flex角度, buf扩大到512字节
- Protobuf: flex_features填5个float, hall_features不填充(保留字段)
- UDP binary: 移除60B Hall, 新增20B Flex

### 1.7 I2C配置
- Wire.setClock(100000) — 强制100kHz
- 三设备: 0x48, 0x49, 0x4B — 无冲突

### 1.8 修改Kalman/SlidingWindow/InferencePipeline
- Kalman: 11通道, Flex通道R=0.02-0.05
- SlidingWindow: 30×11=330
- InferencePipeline: input_dim=11

### 1.9 【V5.0新增】Tier1模型部署框架
- 创建 lib/Inference/Tier1Model.h
- TFLite-Micro int8模型加载(预留,模型文件后续Phase提供)
- infer() 方法: 输入11-dim, 输出Tier1Result{gesture_id, confidence, inference_time_ms}
- Protobuf消息新增tier1_result字段

## 验收标准
- pio run 编译通过
- 串口输出11维数据, 100Hz稳定
- Flex角度0~90°, 无跳变
- Tier1Model框架可编译(模型文件可先空占位)
```

---

## Phase 2: 单手数据采集 + L1模型训练 + Tier1导出

```
我已完成EchoGlove V5.0单手固件，现在采集数据集、训练L1模型、导出Tier1 TFLite模型。

## 本次任务

### 2.1 数据采集工具
创建 tools/collect_dataset.py:
- pyserial读取ESP32串口(11-dim: 5 Flex + 6 IMU)
- 按手势标签采集, 每标签10次×5秒
- 保存CSV + JSON元数据(手势ID、执行者、时间戳)
- 数据增强: 时间拉伸(0.8-1.2×), 高斯噪声(σ=0.01)

### 2.2 数据预处理
创建 tools/preprocess_dataset.py:
- CSV → Kalman滤波 → 归一化 → 滑动窗口(30帧, 步长10)
- 训练/验证/测试 = 7:1:2
- 保存为PyTorch .pt格式

### 2.3 L1模型训练
修改 glove_relay/src/models/l1_cnn_attention.py:
- input_dim = 11
- CNN + SE-Attention + FC → 46类
- 训练脚本 tools/train_l1.py: LR=1e-3, Adam, CosineAnnealing, Epoch=100, EarlyStop(patience=10)

### 2.4 【V5.0新增】Tier1 TFLite导出
创建 tools/export_tier1.py:
- PyTorch → ONNX(opset=13) → TensorFlow → TFLite int8量化
- 量化代表性数据集: 训练集随机1000个样本
- 输出 tier1_model.tflite (预期~148KB)
- 转为C数组: xxd -i tier1_model.tflite > tier1_model_data.cc
- 放入 glove_firmware/lib/Inference/tier1_model_data.cc

### 2.5 修改Relay层
- protobuf_parser.py: 解析flex_features(5) 替代hall_features(15)
- confidence_router.py: 特征组装 flex+imu = 11-dim
- udp_server.py: 滑动窗口 30×11

### 2.6 修改Web前端
- useSensorStore.ts: hall→flex
- useHandAnimation.ts: Flex→关节角度映射(MCP直接,PIP=0.67×MCP,DIP=0.5×MCP)
- types/index.ts: SensorMessage更新

## 验收标准
- 采集20+手势样本
- L1验证精度>85%
- tier1_model.tflite < 200KB
- ESP32-S3 Tier1推理<50ms
```

---

## Phase 3: Unity XR可视化

```
我已完成V5.0单手固件+Tier1模型，现在集成Unity XR Hands。

## 本次任务

### 3.1 Unity项目搭建
- 安装Unity XR Hands 1.7 + OpenXR
- 创建左右手Prefab(26关节标准)

### 3.2 串口数据接收
创建 Assets/Scripts/EchoGloveDriver.cs:
- SerialPort 115200, 异步读取+缓冲队列
- 解析5个Flex角度+4元数
- 支持isLeftHand参数

### 3.3 5DoF→26关节映射
创建 Assets/Scripts/HandPoseMapper.cs:
- MCP=flex[i], PIP=0.67×MCP, DIP=0.5×MCP
- 腕部旋转=BNO085四元数(注意w顺序: BNO085=w,x,y,z → Unity=x,y,z,w)
- 角度约束0~90°

### 3.4 双手管理器(预留)
创建 Assets/Scripts/DualHandManager.cs:
- 预留Tier2/3数据接口
- 单手/双手模式切换

### 3.5 【V5.0新增】Tier级别指示UI
- 绿色=Tier1(手套端), 黄色=Tier2(接收器), 红色=Tier3(PC)
- 实时显示当前推理级别和延迟

## 验收标准
- 单手虚拟手实时跟随, 延迟<50ms
- 26关节运动自然, 无穿模
- Tier级别UI正确显示
```

---

## Phase 4: 双手系统 + Gated Bi-CrossAttention + MS-TCN

```
我已完成V5.0单手系统，现在升级为双手+Gated Bi-CrossAttention+MS-TCN。

## 架构决策(已确认)
- L1融合: Gated Bidirectional CrossAttention (门控解决空闲手问题)
- L2时序: MS-TCN (4-stage精炼, 替代单阶段TCN)
- L2流水线: ST-GCN(42节点) → MS-TCN(4-stage) → CTC
- 特征向量: 28-dim = 左11+右11+相对6

## 本次任务

### 4.1 ESP-NOW通信(同V4.0)
- 手套端ESPNOWManager: 发送GlovePacket(69字节)
- 接收器: 汇聚+tick_id配对+USB转发
- SYNC_TICK广播同步(<2ms精度)

### 4.2 Protobuf V5.0
更新glove_data.proto:
- GloveData新增hand_id, tick_id, tier1_result字段
- 新增DualGloveData(left+right+relative+sync+inference_tier)
- 新增RelativeFeatures(6维)
- 新增InferenceTier(active_tier+blend_alpha+各级结果)

### 4.3 【V5.0核心】Gated Bi-CrossAttention
创建 glove_relay/src/models/l1_gated_cross_attention.py:

```python
class GatedCrossAttention(nn.Module):
    """门控交叉注意力: 解决空闲手噪声问题"""
    def __init__(self, d_model=64, num_heads=4):
        super().__init__()
        self.cross_attn = nn.MultiheadAttention(d_model, num_heads, batch_first=True)
        self.gate_proj = nn.Linear(d_model * 2, d_model)  # 门控投影
        self.norm = nn.LayerNorm(d_model)

    def forward(self, x_self, x_cross):
        attn_out, _ = self.cross_attn(query=x_self, key=x_cross, value=x_cross)
        gate = torch.sigmoid(self.gate_proj(torch.cat([x_self, attn_out], dim=-1)))
        output = gate * attn_out + (1 - gate) * x_self
        return self.norm(output + x_self)

class L1GatedBiCrossAttn(nn.Module):
    """V5.0 L1: Gated Bidirectional CrossAttention"""
    def __init__(self, input_dim=11, num_classes=46, d_model=64):
        super().__init__()
        # 单手特征提取
        self.left_encoder = SingleHandStream(input_dim, d_model)
        self.right_encoder = SingleHandStream(input_dim, d_model)
        # 门控双向交叉注意力
        self.gated_cross_l2r = GatedCrossAttention(d_model)
        self.gated_cross_r2l = GatedCrossAttention(d_model)
        # 相对特征
        self.rel_fc = nn.Linear(6, d_model)
        # 分类头
        self.classifier = nn.Sequential(
            nn.Linear(d_model * 3, 128), nn.ReLU(), nn.Dropout(0.3),
            nn.Linear(128, num_classes)
        )

    def forward(self, left_window, right_window, relative):
        left_feat = self.left_encoder(left_window)     # (B, 64)
        right_feat = self.right_encoder(right_window)   # (B, 64)

        # 门控双向交叉注意力
        left_enhanced = self.gated_cross_l2r(left_feat.unsqueeze(1), right_feat.unsqueeze(1)).squeeze(1)
        right_enhanced = self.gated_cross_r2l(right_feat.unsqueeze(1), left_feat.unsqueeze(1)).squeeze(1)

        rel_feat = torch.relu(self.rel_fc(relative))
        fused = torch.cat([left_enhanced, right_enhanced, rel_feat], dim=-1)
        return self.classifier(fused)
```

### 4.4 【V5.0核心】ST-GCN → MS-TCN → CTC 三阶段L2
创建 glove_relay/src/models/l2_stgcn_mstcn_ctc.py:

```python
class DualHandSTGCN(nn.Module):
    """42节点+6跨手边 ST-GCN"""
    # 输入: 28-dim → 投影到42节点×2坐标 → 3个ST-Conv块 → 每帧64维特征

class SingleStageTCN(nn.Module):
    """单阶段TCN: 10层膨胀因果卷积"""
    # dilation: 1,2,4,8,16,32,64,128,256,512
    # RF = 1+2(2^10-1) = 2047帧

class MSTCN(nn.Module):
    """4阶段MS-TCN: 迭代精炼"""
    # Stage1: 从ST-GCN特征 → Pred_1
    # Stage2-4: 从[上阶段预测; ST-GCN特征] → 精炼预测
    # Loss: CE + Smoothing(λ=0.15)

class L2STGCN_MSTCN_CTC(nn.Module):
    """V5.0 L2: ST-GCN → MS-TCN → CTC"""
    def __init__(self):
        self.stgcn = DualHandSTGCN()
        self.mstcn = MSTCN()
        self.ctc = CTCDecoder(beam_width=10)

    def forward(self, x):  # (B, T, 28)
        spatial_feat = self.stgcn(x)     # (B, T, 64)
        predictions = self.mstcn(spatial_feat)  # [pred_1..pred_4]
        return predictions[-1]  # 最终阶段预测
```

### 4.5 BiLSTM+CTC备选
创建 glove_relay/src/models/l2_bilstm_ctc.py:
- 简单备选: BiLSTM(28→128×2) → FC → CTC
- 用于无GPU部署场景

### 4.6 Relay层改造
- feature_fusion.py: 双手11-dim→28-dim拼接+相对6维计算
- 新增 tier_router.py: 三级推理路由(Phase5实现)
- confidence_router.py: 支持Gated Bi-CrossAttn模型

### 4.7 前端双手渲染
- useSensorStore.ts: leftHand{flex,imu} + rightHand{flex,imu} + relative
- DualHandCanvas.tsx: 两个HandSkeleton
- Unity DualHandManager.cs: 双手驱动

### 4.8 双手数据集采集
- 更新collect_dataset.py: 双手同步采集
- 采集双手协同手势(60+类)

## 验收标准
- Gated Bi-CrossAttn双手手势精度>87%
- MS-TCN手语边界F1>60%
- CTC连续3词WER<30%
- 双手3D渲染同步
```

---

## Phase 5: 三级热切换架构

```
我已完成V5.0双手系统，现在实现三级热切换: 手套→接收器→PC。

## 架构
- Tier1(手套): CNN+Attention 11-dim, ~148KB, ~30ms, ~80%
- Tier2(接收器): Gated Bi-CrossAttn+MS-TCN2s 28-dim, ~170KB, ~50ms, ~87%
- Tier3(PC): ST-GCN→MS-TCN→CTC, ~3MB, ~15ms(GPU), ~92%
- 热切换: 5帧线性渐变(~50ms), 温度校准对齐

## 本次任务

### 5.1 接收器Tier2模型部署
- export_tier2.py: 导出Gated Bi-CrossAttn+MS-TCN2s为TFLite int8
- receiver_firmware/lib/Tier2Model.h: 加载+推理
- PSRAM配置: CONFIG_SPIRAM=y, PSRAM 80MHz模式
- FreeRTOS: 独立推理任务, 优先级低于ESP-NOW接收

### 5.2 接收器主循环
- 每帧: 广播SYNC_TICK → 收两手数据 → 计算相对特征 → Tier2推理 → USB转发(含Tier2结果)
- 超时: 5tick内双手数据未齐备则仅转发可用数据
- LED: 绿=Tier1活跃, 黄=Tier2活跃, 红=Tier3活跃

### 5.3 【V5.0核心】TierRouter三级融合
创建 glove_relay/src/tier_router.py:
- 维护tier1_left, tier1_right, tier2_result, tier3_result
- _update_active_tier(): 根据PC可用性自动切换
- _blend_results(): 线性渐变融合, output=(1-α)·旧级 + α·新级
- _calibrate_temperature(): 温度校准, 各Tier模型softmax分布对齐
- blend_alpha: 0→1渐变, 5帧完成过渡(~50ms)

### 5.4 手套端Tier1结果嵌入
- 修改ESPNOWManager: 发送数据包中包含Tier1推理结果
- 修改Protobuf: GloveData包含Tier1Result(gesture_id, confidence, inference_time_ms)

### 5.5 PC端Tier3推理
- Tier3 L1: Gated Bi-CrossAttn + MS-TCN 4-stage (28-dim, PyTorch FP32)
- Tier3 L2: ST-GCN(42节点) → MS-TCN(4-stage) → CTC (PyTorch FP32)
- PC不可用时自动降级到Tier2

### 5.6 前端Tier级别显示
- Web: 添加TierIndicator组件(绿/黄/红 + 延迟数字)
- Unity: HUD显示当前Tier + 推理延迟

## 验收标准
- Tier3→Tier2切换: <100ms过渡, 无输出跳变
- Tier2→Tier1切换: <50ms过渡
- Tier1→Tier2→Tier3恢复: 自动完成
- 接收器Tier2推理: 稳定<60ms
- 温度校准后Tier间概率分布KL散度<0.1
```

---

## Phase 6: 进阶优化

```
我已完成V5.0三级热切换系统，现在进行进阶优化。

## 本次任务

### 6.1 MANO参数化(替换线性耦合)
- 集成manopth: 5个Flex → MANO β(10维) → 26关节+网格
- Unity端ms-mano-unity插件
- 角度误差<3°

### 6.2 多模态视觉融合
- MediaPipe手部42关键点
- 扩展Kalman: Flex(5)+IMU(6)+Vision(42)=53-dim
- Flex异常时视觉自动补全

### 6.3 BiLSTM+CTC备选完善
- 无GPU场景完整实现
- CPU推理<100ms
- 精度>80%

### 6.4 低功耗优化
- 空闲降采样(100→20Hz)
- Flex间歇供电
- 电池监测+低电提醒

### 6.5 系统集成测试
- 端到端延迟: 目标<100ms
- 2小时稳定性
- 多环境测试(WiFi干扰)
- 三级热切换压力测试

## 验收标准
- MANO误差<3°
- 2小时无崩溃
- 100+词汇精度>80%
- 三级切换100次无异常
```

---

## 快速参考: V5.0各Phase关键文件变更

| Phase | 新增文件 | 修改文件 |
|-------|----------|----------|
| **1** | ADS1115Manager.h, FlexManager.h, Tier1Model.h | SensorManager.h, data_structures.h, IMUManager.h, BLEManager.h, UDPTransmitter.h, KalmanFilter.h, glove_data.proto |
| **2** | collect_dataset.py, preprocess_dataset.py, train_l1.py, export_tier1.py, tier1_model_data.cc | l1_cnn_attention.py, protobuf_parser.py, confidence_router.py, useSensorStore.ts, types/index.ts |
| **3** | EchoGloveDriver.cs, HandPoseMapper.cs, DualHandManager.cs, TierIndicatorUI.cs | XR Hands Settings |
| **4** | ESPNOWManager.h, receiver_firmware/*, feature_fusion.py, l1_gated_cross_attention.py, l2_stgcn_mstcn_ctc.py, l2_bilstm_ctc.py, train_l1_dual.py, train_l2_mstcn.py, DualHandCanvas.tsx | stgcn_model.py, glove_data.proto, confidence_router.py, HandCanvas.tsx, useSensorStore.ts |
| **5** | export_tier2.py, Tier2Model.h, tier_router.py, TierIndicator.tsx | ESPNOWManager.h, receiver_firmware/main.cpp, glove_data.proto |
| **6** | mano_regressor.py, mediapipe_fusion.py | InferencePipeline.h, HandPoseMapper.cs |

---

## 避坑清单(V5.0新增)

| 坑 | Phase | 解决方案 |
|----|-------|----------|
| Gated CrossAttn门控退化为全0/全1 | 4 | 门控正则化: loss += λ·\|gate-0.5\|² |
| MS-TCN后期stage梯度消失 | 4 | 渐进训练: 先训Stage1→冻结→训Stage2→... |
| 接收器PSRAM不足Tier2模型 | 5 | MS-TCN降为1-stage，模型约100KB |
| Tier2推理超50ms | 5 | 降频至50Hz或简化模型 |
| 热切换输出跳变 | 5 | 延长blend至10帧+温度校准 |
| CTC blank类过多 | 4 | 调blank权重+语言模型约束 |
| ST-GCN图卷积量化困难 | 5 | Tier2不用ST-GCN，仅用1D CNN |
| LSTM量化TFLite不支持 | 6 | BiLSTM仅PC端运行，不部署到ESP32 |
| PSRAM看门狗超时 | 5 | task watchdog超时设>1s |
| Tier2与Tier3概率分布不对齐 | 5 | 验证集温度校准, KL散度<0.1 |
