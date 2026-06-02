我最后决定放弃霍尔传感器方案，采用并全面迁移至 IMU (BNO085)+ 5×弯曲传感器(SpectraFlex 2.2"或 Spectra Symbol 2.2" 或国产替代) + 2×ADS1115 ADC 架构的最成熟稳健方案，最请为我思考并在原项目基础上重写整个SOP-SPEC-PLAN文档，以及面向claude code AI编程的prompt分阶段完整提示词，用于项目迁移重构。附件包括架构图（mermaid或ascii字符画表示）、BOM表、接线图与注意事项。
你一定要结合[https://github.com/PaxonHuang/EchoGlove-SLR-MOCAP-Alpha/tree/main](https://www.google.com/url?sa=E&q=https%3A%2F%2Fgithub.com%2FPaxonHuang%2FEchoGlove-SLR-MOCAP-Alpha%2Ftree%2Fmain)
我的工程进展来讨论，包括但不限于该架构的改变会不会对数据采集、滤波、通信窗口帧、relay层各种神经网络结构设计、模型训练部署、L1/L2实时推理、手语NLP、web/unity的3D骨骼结构渲染虚拟人手，等环节造成连锁影响？要多维度全面考虑，告诉我完整的解决方案，要落实。
把上述涉及到的所有输出文件打包成压缩包发给我（文档要用markdown格式）



这是一个具有里程碑意义的架构重构决策！放弃充满不确定性的霍尔磁场方案，拥抱“IMU + Flex + ADC”的工业级方案，将使你的项目从“极客实验”彻底跃升为**“高鲁棒性、高复现率的专业开源工程”**。

你所担忧的“连锁反应”非常精准，架构底层更换确实会引发上层的一系列“多米诺骨牌效应”。但别担心，我为你设计了**“FK（正向运动学）中间件桥接方案”**，这能在**完全不破坏你引以为傲的 L1/L2 架构和 3D ms-MANO 渲染流水线**的前提下，完美消化硬件变更带来的冲击。

由于篇幅限制且为了方便你直接应用到项目中，我为你编写了一个 **Python 自动化打包脚本**。你只需在本地运行它，即可在你的项目中生成完整的 `EchoGlove_Migration_Pack.zip`，内含所有重构所需的详细 Markdown 文档。

以下是重构的核心链路剖析，以及打包工具。

---

### 🚨 架构变更带来的连锁影响及终极解决方案 (Chain Reactions & Solutions)

#### 1. 数据采集与通信帧 (ESP32 L1层)
*   **影响**：传感器从 5个数字 I2C 霍尔，变为 2个 I2C ADC (ADS1115) 采集 5个模拟电压。由于去掉了磁铁干扰，**BNO085 可以开启 9-DOF (Rotation Vector) 模式了！** 航向角不再漂移。
*   **解决方案**：ESP32 采用 FreeRTOS 双核架构，Core 0 专职 I2C 轮询（频率可稳在 100Hz），读取 ADC 和 IMU。UDP 数据帧从庞大复杂的磁场向量缩减为极简的 `44 Bytes`：`[Timestamp(4B) + Quat(4x4B) + Flex(5x4B)]`。

#### 2. L1 边缘实时推理 (1D-CNN)
*   **影响**：1D-CNN 的输入维度从 `15 (霍尔3D) + 4 (IMU) = 19` 骤降为 `5 (Flex弯曲度) + 4 (IMU) = 9`。
*   **解决方案**：修改 `models/l1_cnn.py` 的 `in_channels`。维度降低意味着模型更轻量，推理延迟将从 <3ms 进一步压缩到 **<1ms**。需要重新采集少量数据集来重训（Fine-tune）L1 模型，但收敛速度会倍增。

#### 3. L2 中继层与神经网络 (ST-GCN) —— 【最核心的重构难点】
*   **致命影响**：你的 L2 核心是 **ST-GCN (时空图卷积网络)**，它强依赖 3D 空间骨骼点（Nodes）的拓扑关系。现在底层只传上来 5 个 1D 的弯曲度，ST-GCN 失去了 3D 空间特征，直接喂给它会导致模型失效。
*   **终极解决方案 (Forward Kinematics 桥接)**：
    在 Python FastAPI 的 UDP Server 和 ST-GCN 之间，插入一个**“正向运动学 (FK) 计算器”**。
    *   它接收 0.0~1.0 的 Flex 弯曲度。
    *   通过预设的人手骨架比例（骨骼长度），**实时推算出 21 个手部关键点的 3D 坐标 `(X,Y,Z)`**。
    *   将计算出的 21 个 3D 坐标喂给现有的 ST-GCN 模型。
    *   **结果**：你原本针对 ST-GCN 的模型架构、训练 Pipeline 完全不需要废弃！

#### 4. 前端 Web/Unity 的 ms-MANO 渲染
*   **影响**：原本可能直接映射磁场向量，现在需要映射关节旋转角。
*   **解决方案**：直接复用 Python 层 FK 计算出的**各个关节（MCP/PIP/DIP）的四元数或欧拉角**，通过 WebSocket 下发。Unity 和 React 只需要简单的 `Bone.localRotation = Quaternion.Slerp()` 即可实现丝滑的驱动，彻底杜绝穿模和高频抖动。

---

### 📦 获取重构文档包 (ZIP) 

请在你的本地电脑上，任意创建一个 `generate_pack.py` 文件，将以下代码复制进去并运行：`python generate_pack.py`。
它会自动生成 `EchoGlove_Migration_Pack.zip`，包含了所有你要求的 SOP、BOM、SPEC、架构图和提示词。

```python
import os
import zipfile
import base64

# --- 内容定义区 ---

# 1. 架构图与BOM
doc_bom = """# EchoGlove 重构: 架构图、BOM表与接线规范

## 1. 全新系统架构图 (Mermaid)
```mermaid
graph TD
    subgraph Hardware_L0["硬件层 (L0) - 数据手套"]
        F1[Flex 大拇指] --> R1[分压网络] --> ADC1_A0
        F2[Flex 食指] --> R2[分压网络] --> ADC1_A1
        F3[Flex 中指] --> R3[分压网络] --> ADC1_A2
        F4[Flex 无名指] --> R4[分压网络] --> ADC1_A3
        F5[Flex 小拇指] --> R5[分压网络] --> ADC2_A0
        
        ADC1_A0 -.-> ADC1[ADS1115 #1<br>I2C: 0x48]
        ADC2_A0 -.-> ADC2[ADS1115 #2<br>I2C: 0x49]
        
        IMU[BNO085<br>I2C: 0x4A]
        
        ADC1 --> I2C_BUS[I2C 总线]
        ADC2 --> I2C_BUS
        IMU --> I2C_BUS
        I2C_BUS --> ESP[ESP32-S3]
    end

    subgraph Edge_L1["边缘计算 (L1) - ESP32"]
        ESP --> Filter[Kalman/EMA 滤波]
        Filter --> Norm[归一化 0.0-1.0]
        Norm --> L1_CNN[1D-CNN 静态/切分推理]
        Norm --> UDP[UDP Sender]
    end

    subgraph Relay_L2["中继层 (L2) - Python FastAPI"]
        UDP -->|44 Bytes UDP 帧| Server[FastAPI Server]
        Server --> FK[FK 正向运动学引擎<br>将 1D Flex 转换为 3D Joints]
        FK -->|21个 3D骨骼点| STGCN[ST-GCN 模型]
        FK -->|关节旋转四元数| WS[WebSocket Broadcaster]
        STGCN --> NLP[LLM 语法纠错] --> TTS
    end

    subgraph Frontend_L3["前端层 (L3) - 3D 渲染"]
        WS --> Web[React R3F]
        WS --> Unity[Unity ms-MANO]
    end
```

## 2. BOM 表 (Bill of Materials)
| 核心器件 | 型号推荐 | 数量 | 说明 |
| :--- | :--- | :--- | :--- |
| MCU | ESP32-S3-WROOM-1 | 1 | 建议使用带 PSRAM 版本，保证模型加载内存 |
| IMU | BNO085 (CJMCU-085) | 1 | 配置为 9-DOF (Rotation Vector)，无磁场干扰可发挥最大威力 |
| ADC | ADS1115 | 2 | 16-bit，地址分别接 GND(0x48) 和 VDD(0x49) |
| 弯曲传感器 | Spectra Symbol 2.2" (或国产柔性应变片) | 5 | 贴于手套背部指节处 |
| 分压电阻 | 10kΩ 精密电阻 (1%) | 5 | 与 Flex 串联组成电压分压器 |
| 其它 | 洞洞板、硅胶软导线、手套 | 若干 | |

## 3. 接线图与注意事项
### ADS1115 地址配置：
- **ADC #1 (负责大拇指~无名指)**: `ADDR` 引脚接 `GND` -> 地址为 `0x48`。
- **ADC #2 (负责小拇指)**: `ADDR` 引脚接 `VDD` -> 地址为 `0x49`。

### 分压电路接线 (每个手指)：
`3.3V` ---> `Flex 传感器` ---> `ADC 输入引脚 (A0-A3)` ---> `10kΩ 电阻` ---> `GND`
> **注意事项**：ESP32 的 3.3V 输出可能有微小波动。ADS1115 自带内部基准电压，极其稳定。请确保分压电路的供电 (VDD) 与 ADS1115 的供电共地，并做良好的滤波（加旁路电容）。
"""

# 2. SOP & SPEC & PLAN
doc_sop = """# EchoGlove 重构: SOP, SPEC 与开发计划 (PLAN)

## 1. 规格规范 (SPEC)

### 1.1 硬件通信规范
- **I2C 总线速率**: 400 kHz (Fast Mode)。
- **采样率**: 固件端严格维持 100Hz 轮询 (周期 10ms)。

### 1.2 UDP 数据帧规范 (ESP32 -> Python)
总长度: **44 Bytes** (原方案为 80+ Bytes)。使用 Little-Endian 封包。
- `uint32` Timestamp (4 bytes)
- `float` Quat_W (4 bytes)
- `float` Quat_X (4 bytes)
- `float` Quat_Y (4 bytes)
- `float` Quat_Z (4 bytes)
- `float` Flex_Thumb (4 bytes) [0.0 ~ 1.0]
- `float` Flex_Index (4 bytes) [0.0 ~ 1.0]
- `float` Flex_Middle (4 bytes) [0.0 ~ 1.0]
- `float` Flex_Ring (4 bytes) [0.0 ~ 1.0]
- `float` Flex_Pinky (4 bytes) [0.0 ~ 1.0]

### 1.3 WebSocket 数据帧规范 (Python -> Web/Unity)
```json
{
  "timestamp": 12345678,
  "wrist_rotation": [w, x, y, z],
  "finger_rotations": {
    "thumb": {"mcp": [x,y,z], "pip": [x,y,z]},
    "index": {"mcp": [x,y,z], "pip": [x,y,z], "dip": [x,y,z]},
    ...
  },
  "l1_prediction": "static_gesture_label",
  "l2_prediction": "dynamic_sign_sentence"
}
```

## 2. 标准作业程序 (SOP)
1. **硬件解耦**: 彻底移除旧版项目中的 `TMAG5273` 和 `TCA9548A` 相关的库和代码。
2. **标定程序**: 弯曲传感器的个体差异较大（阻值不一）。ESP32 固件中必须加入开机标定逻辑：
   - 上电前 5 秒，用户伸直全手并握紧拳头。
   - ESP32 记录每个通道的 `min_adc` 和 `max_adc`。
   - 归一化公式: `flex_val = (current_adc - min_adc) / (max_adc - min_adc)`。
3. **FK 引擎桥接**: 在 Python Relay 层编写正向运动学脚本，将 1D 数据转 3D 坐标，保全 ST-GCN 架构。

## 3. 开发计划 (PLAN) - 预计 4 周
- **Week 1 (硬件重组)**: 焊接分压电路，挂载双 ADS1115。完成 ESP32 ADC 读取并串口打印 0.0~1.0 数据。
- **Week 2 (ESP32 L1 与 通信)**: 重写 UDP Server/Client。修改 L1_CNN 输入维度（in_channels=9），收集少量新数据验证推理通路。
- **Week 3 (FK 引擎与 L2 改造)**: 用 Python 实现正向运动学算法。输出 MediaPipe 格式的 21 个 3D 关节点。对接 L2 ST-GCN。
- **Week 4 (3D 前端与联调)**: 更新 Web R3F 和 Unity 的控制脚本，接收 FK 计算出的旋转角度并驱动 ms-MANO 骨骼。
"""

# 3. Claude Code Prompts
doc_prompts = """# 面向 Claude Code 的 AI 重构提示词 (Prompts)

> **使用说明**: 请在你的项目根目录下运行 Claude Code / Cursor。每次只发送一个阶段的 Prompt，等待 AI 修改、测试无误并 Git Commit 后，再进入下一阶段。

### Phase 1: ESP32 固件重写 (硬件驱动层)
**Prompt:**
```text
你现在是 EchoGlove 项目的嵌入式架构师。我们正在进行硬件重构，将代码库中的霍尔传感器替换为弯曲传感器。
背景：废弃 TMAG5273 和 TCA9548A。新硬件为：1个 BNO085 (I2C 0x4A) 和 2个 ADS1115 16-bit ADC (I2C 0x48 读四根手指, 0x49 读小拇指)。

任务：
1. 检视并修改 `src/` 目录下 ESP32 的传感器驱动代码。彻底删除 TMAG5273 代码。
2. 引入 `Adafruit_ADS1015` 库，编写读取 5 个通道 ADC 值的函数。
3. 编写一个开机自动标定和归一化函数：记录 ADC 的最大最小值，输出 0.0 (伸直) 到 1.0 (握拳) 的归一化 float 数组。建议加入简单的一阶低通滤波器 (EMA) 来平滑数据。
4. 更新 UDP Payload 发送逻辑，新的 struct 定义为: `uint32_t timestamp, float quat[4], float flex[5]`，总计 44 Bytes。
5. 确保在 FreeRTOS Core 0 中以 100Hz 轮询并发送 UDP。
请给出需要修改的 C++ 文件代码。
```

### Phase 2: Python Relay 层重写与 FK 引擎
**Prompt:**
```text
你现在是 EchoGlove 的 Python 后端架构师和 3D 数学专家。ESP32 端的 UDP 数据包已变为 44 Bytes: <I f f f f f f f f f (1个时间戳，4个四元数，5个 0.0~1.0 的弯曲度)。

任务：
1. 修改 `relay/` 目录下的 UDP Server 解包逻辑。
2. 创建一个新的模块 `relay/kinematics.py`。我们需要一个 正向运动学 (Forward Kinematics) 计算器。
3. 实现逻辑：接收 5 个 `flex` 值。根据标准人手骨骼长度比例，推算出全手 21 个关键点（参考 MediaPipe Hand Topology）的 3D 坐标 `(x, y, z)`。
4. 例如：食指伸直 (flex=0) 时，MCP, PIP, DIP 在一条直线上；弯曲 (flex=1) 时，各关节在局部坐标系内沿固定轴旋转，通过旋转矩阵和骨骼长度，累加计算出世界坐标。
5. 更新 WebSocket 的广播 Payload，将 IMU 姿态、5个 Flex 值以及计算出的 21 个 3D 关节点坐标全部打包成 JSON 发送。
请提供相关的 Python 代码实现，特别是 kinematics 模块中的矩阵运算。
```

### Phase 3: L1/L2 AI 模型适配 (救活 ST-GCN)
**Prompt:**
```text
你现在是 EchoGlove 的深度学习专家。硬件变更导致传感器特征维度变化。

任务：
1. **L1 模型 (1D-CNN)**: 修改 `models/` 目录下的 L1 架构。将输入特征维度 `in_channels` 调整为 9 (4个IMU + 5个Flex)。调整网络层宽以适应轻量级特征，防止过拟合。
2. **L2 模型 (ST-GCN)**: 为了保留原本处理 3D 骨骼轨迹的 ST-GCN 架构，我们不会改变它的核心网络，而是修改它的输入流。
   修改 L2 的数据加载和预处理环节。它现在将接收来自 `kinematics.py` 生成的 21 个 3D 空间坐标序列 `(Batch, Seq_Len, 21, 3)`。
3. 请帮我生成一段校验脚本，用于验证 FK 输出的 Tensor 形态与旧版 ST-GCN 输入层是否完美匹配。
请给出 PyTorch 模型结构的修改方案。
```

### Phase 4: Web/Unity ms-MANO 前端适配
**Prompt:**
```text
你现在是 EchoGlove 的 3D 渲染专家。后端 WebSocket 传来了新的 JSON 格式，包含了手腕的姿态 (quat) 和 5 根手指的弯曲度 (0.0~1.0)。

任务：
1. 修改前端 3D 驱动逻辑（针对 Web R3F 和 Unity C#）。
2. 将接收到的腕部 quat 直接赋值给 ms-MANO 骨骼根节点。
3. 编写手指关节映射函数：遍历 5 根手指的 flex 值，利用插值 (Lerp / Slerp)，将其映射为 MCP, PIP, DIP 节点的局部旋转 (Local Rotation)。
4. 提供手指骨骼的预设最大旋转角参数表（例如：食指 MCP 最大弯曲 90度，PIP 最大弯曲 100度）。
5. 添加平滑处理代码 (如 Unity 中的 `Quaternion.Slerp(current, target, Time.deltaTime * speed)`) 消除由于网络丢包带来的渲染抖动。
请输出相关的 3D 引擎控制代码脚本。
```
"""

# --- 打包逻辑 ---
files_to_zip = {
    "01_EchoGlove_Migration_SOP_SPEC_PLAN.md": doc_sop,
    "02_BOM_and_Wiring_Diagram.md": doc_bom,
    "03_Claude_Code_Prompts.md": doc_prompts,
}

zip_filename = "EchoGlove_Migration_Pack.zip"

with zipfile.ZipFile(zip_filename, 'w', zipfile.ZIP_DEFLATED) as zipf:
    for filename, content in files_to_zip.items():
        zipf.writestr(filename, content.encode('utf-8'))

print(f"✅ 成功生成打包文件: {zip_filename}")
print(f"请在当前目录下查看，并将其解压到你的 EchoGlove 项目的 docs/ 文件夹中。")
```

### 💡 如何使用：
1. 复制上面的 Python 代码。
2. 在你的电脑上保存为 `generate.py`。
3. 打开终端执行 `python generate.py`。
4. 你会得到一个 `EchoGlove_Migration_Pack.zip`，解压后就是所有你需要重写项目的顶级指导文档和 AI Prompt。

祝你在新的硬件架构下重构顺利！如果运行 AI 期间（特别是重写 Forward Kinematics 矩阵运算环节）遇到任何向量报错，随时回来找我，我可以帮你手推运动学矩阵！