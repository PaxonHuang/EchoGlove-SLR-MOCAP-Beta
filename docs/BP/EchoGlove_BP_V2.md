# EchoGlove 产业级商业计划书 V2.0

> **版本**: V2.0 (产业级, 替代旧版大学生创新大赛级BP)
> **日期**: 2026-07-23
> **用途**: 具身智能探月社区(北京海淀中关村具身智能创新产业园) / WRC / 京东 战略合作"专属直通送审通道"展位
> **状态**: V2.0 审核修正版 → 改写 Word
> **真实性原则**: 全文按"已实现 / 工程可实现(6-12月) / 需研发验证 / 长期方向"四级标注, 不写想象中的能力

---

## 战略冻结点索引

| # | 决策 | 选择 |
|---|------|------|
| 主航道 | 公司定位 | 具身智能人机交互入口公司 (三战略融合, 以具身智能为主) |
| D1 | 感知主线 | 视觉主导 + 可穿戴增强 |
| D2 | 目标市场 | 具身智能遥操作/数据采集优先 |
| D3 | 数据标准 | 双表示层 (MANO Layer + Robot Action Layer) |
| D4 | 对外定位 | 具身智能数据采集与人机交互基础设施 (非纯机器人设备) |
| D5 | 数据战略 | Open Core + Commercial Data Asset |
| D6 | 产品线 | Lite (消费) + Pro (企业/科研) 双线 |
| D7 | 第一代视觉 | 不进入硬件, 预留外接 EGO Camera 接口, 后期 AI 眼镜融合 |
| D8 | 手语模型 | 保留但战略降级 (Hand Token 的一种解释方式) |
| D9 | Pro视觉接口 | 双生态兼容 (USB-C/WiFi/BT + ROS2/Ethernet/USB3 Vision) |

---

## 第1章 执行摘要

**EchoGlove** 是一套面向具身智能时代的**低成本多模态手部感知与遥操作基础设施**。通过可穿戴硬件(柔性传感+IMU+力)与边缘AI融合, 实时重建人体手部状态, 以**双表示层**(MANO数字人参数 + 机器人动作向量)统一输出, 为VLA/WAM/遥操作/数字孪生/手语翻译提供高保真人类动作数据入口。

**核心论点**: 具身智能(Physical AI)的最大瓶颈不是移动, 而是**Manipulation(操作)**。操作学习需要大量人类示教数据, 而现有采集手段(UMI/ALOHA)成本高、缺人体本体感觉与触觉。EchoGlove 以可穿戴形态填补"低成本人体手部意图捕捉"空白, 定位为**Human Hand Intelligence Layer**——介于机器人本体/基础模型与应用之间的数据入口层。

**三大痛点对应**:
1. 听障沟通壁垒 → 手语翻译(首个商业验证场景)
2. 专业动捕设备成本高昂(Manus/SenseGlove 数千~数万元) → Lite版 <¥500 BOM
3. 纯视觉方案遮挡/无接触力/无本体状态 → glove+vision互补融合

**技术真实性现状**(投资人必读):
- ✅ 已实测: flex 内置ADC1采集、S3 ESP-NOW通信、P4 UART接收+USB-CDC输出、P4 standalone mock链路
- 🟡 工程可实现(6-12月): LSM6DSV16X IMU驱动、S3→P4有线UART、ROS2 SDK、MANO双表示层、MediaPipe+glove融合
- 🔬 需研发验证: 连续手语benchmark、多模态时间同步、柔性传感器量产一致性
- 🌌 长期方向: Human Hand Foundation Model (2028+)

**商业闭环**: 硬件销售(Lite消费/Pro企业) + SDK授权(Unity/ROS2/Python/TFLite/PyTorch) + 数据服务(Open基础数据+商业行业数据) + 行业方案(无障碍/XR/机器人)。

**本轮诉求**: 借中关村/WRC/京东直通通道, 寻求产业合作(数据采集联调、机器人厂商SDK对接、量产供应链)与战略融资, 推进V7原型落地与首批行业数据集建设。

---

## 第2章 市场机会

### 2.1 Physical AI 成为下一阶段AI基础设施
AI演进: 互联网数据→LLM→文本智能; 现实世界数据→多模态模型→**具身智能**。核心变化: AI需理解空间、物体、人类动作、操作过程。具身智能市场(机器人+智能硬件)预期千亿级, 数据采集与交互入口是上游刚需。

### 2.2 "手"是具身智能关键入口
机器人最大难点是操作而非移动。一次"拿杯子"包含视觉判断→腕调整→指展开→接触→力控→抓取→移动→释放, 手部状态贯穿始终。未来机器人训练需大量Human Demonstration Data, EchoGlove提供: **Human Hand State + Environment Context + Action Sequence**。

### 2.3 三大目标市场
| 市场 | 规模/特征 | EchoGlove切入 | 优先级 |
|------|----------|--------------|--------|
| 具身智能遥操作/数据采集 | 高客单、强生态、刚需 | Pro版 + 数据服务 + ROS2/VLA接口 | ★★★★★ (主航道) |
| 手部MOCAP/数字内容 | 对标Rokoko/Manus, 中等规模 | Lite/Pro + Unity/Unreal SDK | ★★★★ |
| 无障碍手语翻译 | 社会价值高, 商业天花板较低 | Lite消费版 + 公益/政府/医疗 | ★★★ (首个验证场景) |

### 2.4 竞争空白(详见第6章)
现有方案非此即彼: 数据手套贵且偏XR(Manus)、视觉方案无接触力(MediaPipe)、机器人采集平台贵且重(ALOHA/UMI)。**无人同时做到: 低成本 + 可穿戴 + 本体感觉+力 + 视觉融合 + 双表示层标准化 + 开放生态**——这是EchoGlove的定位区间。

---

## 第3章 产品体系

### 3.1 双产品线

| 维度 | EchoGlove Lite | EchoGlove Pro |
|------|---------------|---------------|
| 定位 | 规模化消费入口 | 具身智能数据入口 |
| MCU | ESP32-S3 N16R8 | ESP32-P4 (主) + ESP32-S3 (节点) |
| 传感 | 5×Flex✅ + LSM6DSV16X IMU🟡 | 高精度IMU🟡 + 柔性eSkin🔬 + 力接口🔬 + EGO Camera接口🟡 + Depth接口🔬 |
| 通信 | BLE/WiFi🟡 | ESP-NOW✅ + 有线UART🟡 + USB-C✅ + (生产)WiFi/UDP🟡 |
| 视觉 | 无 (预留) | 外接EGO Camera (D7, 第一代不内置) |
| BOM目标 | <¥500 | 企业级 (TBD, 量级¥1-2k) |
| 输出 | 11维/hand + 手语分类 | 双表示层 (MANO + Robot Action) |
| 目标用户 | 听障/教育/XR/消费 | 机器人厂商/AI公司/科研/工业遥操 |
| 上市 | 2026 H2 原型 / 2027 量产 | 2027 |

### 3.2 双表示层 (核心壁垒, D3)
**Layer A — MANO数字人表示**: MANO参数 + 21关节位置 + 网格形变 + 手部位姿。服务Unity/Unreal/Blender/XR/数字人/手语动画。兼容现有数字化生态。

**Layer B — 机器人动作表示**: 关节角向量 + 6DoF腕位姿 + 速度/加速度 + 接触状态 + 力估计。服务ROS2/机械臂/灵巧手/夹爪/VLA/RL/ACT。机器人不需要"这是什么手势", 需要"手处于什么状态"。

**统一中间表示**: Sensor → Hand Encoder → **Hand State Token** → 双输出。手语、动捕、遥操作都是Token的不同解释。

### 3.3 Pro视觉接口 (D9 双生态兼容)
- 消费侧: USB-C / WiFi / BT (接AI眼镜, 后期融合)
- 机器人侧: ROS2 / Ethernet / USB3 Vision / MIPI
- 第一代: 仅预留接口与协议栈, 不内置相机模组 (D7)

### 3.4 硬件资产复用判断
| 资产 | 处置 | 理由 |
|------|------|------|
| ESP32-S3 | 保留 | 双芯架构合理, 成本低 |
| ESP32-P4 | 保留 (Pro主控) | 400MHz RV32 + 32MB PSRAM, 适合边缘推理与显示 |
| LSM6DSV16X | 保留/升级 | 工业级IMU, SFLP融合, $2-4, 值得保留甚至升9轴 |
| 2.2" Flex | Lite保留 / Pro升级eSkin | Flex一致性/老化/标定/无力信息是最大风险, Pro必须升级 |
| ADS1115 | 已移除(V6) | internal ADC1替代, 节省BOM |
| UWB | 重新论证(结论: Pro可选/Lite不做) | egocentric VLA下可能冗余(视觉可给位姿), 增BOM成本; Pro可选定位增强, Lite不做 |

---

## 第4章 技术壁垒

### 4.1 五层壁垒
1. **低成本多模态硬件**: ESP32双芯 + LSM6DSV16X + 柔性传感, BOM显著低于Manus/SenseGlove
2. **双表示层数据标准**(核心): 竞品只输出动作或姿态, EchoGlove输出统一的Human Representation可同时驱动数字人与机器人
3. **数据闭环**: 设备→采集→训练→模型→SDK→应用→用户→数据 飞轮
4. **开放生态**: 兼容PyTorch/TFLite/ROS2/Unity/Unreal/MANO/MediaPipe/LucidVR(opengloves)
5. **人类操作数据资产**(长期): 目标成为手部操作数据的"ImageNet"

### 4.2 技术架构 (V7 目标)
```
Human
  ↓
多模态输入: Flex/eSkin + IMU(6/9DoF) + Force + EGO Camera + Depth
  ↓
边缘计算: ESP32-S3 Lite / ESP32-P4 Pro / FreeRTOS / TinyML(传感器预处理)
  ↓
Hand Foundation Representation: Hand State Token
  ↓
双输出: [MANO Layer→Unity/Unreal/XR/手语] + [Robot Layer→ROS2/IK/MPC/RL/VLA/ACT]
  ↓
应用: 数字人 / 机器人遥操 / MOCAP / 手语翻译 / 康复
```

### 4.3 算法架构
- **手语模型(保留, D8)**: Sensor序列→Temporal Encoder→Hand Representation→Language Decoder→手语NLP。手语降级为Token解释方式之一。
- **动捕/遥操**: Sensor Encoder + Vision Encoder + Transformer Fusion + Action Head。支持长序列任务理解(取→移→旋→放)。
- **VLA接口**: EchoGlove不直接竞争VLA, 定位为Human Action Data Provider, 输出Human Action Token喂给VLA/Diffusion Policy/ACT。
- **端侧分工(诚实)**: 传感器预处理在MCU端侧(TinyML); 完整Transformer/Fusion推理在edge gateway/移动/云——不在ESP32上跑完整大模型。

### 4.4 CV+深度融合方案
表述: "主动感知(可穿戴本体传感)+被动感知(视觉)融合"。视觉给World State(物体/环境/手物关系), 手套给Hand State(关节/力/接触/遮挡后状态)。第一代不内置CV(D7), 通过Pro双生态接口(D9)外接, 后期AI眼镜融合。深度方案选型见第7章roadmap。

---

## 第5章 商业模式

### 5.1 四收入模型
| 模型 | 内容 | 客户 | 阶段 |
|------|------|------|------|
| 硬件销售 | Lite(消费)/Pro(企业科研) | C端/B端 | 2026起 |
| SDK授权 | Unity/ROS2/Python/TFLite/PyTorch SDK | XR/机器人/AI公司 | 2026起 |
| 数据服务 | Open基础数据 + 商业行业数据 + 行业采集 | 机器人/AI/医疗 | 2027起 (长期价值最大) |
| 行业方案 | 无障碍(政府/公益/医疗) + XR数字人 + 机器人遥操 | 政府/企业 | 2026起 |

### 5.2 数据战略 (D5 Open Core)
- **Layer 1 开放基础数据**: Raw Sensor(IMU/Flex/eSkin/Camera/Depth) + 标定 + MANO基础动作。用途: 学术/算法/开源社区/Benchmark。建影响力。
- **Layer 2 开发者SDK**: Python `get_hand_state()` / ROS2 topic / Unity MANO rig / TFLite端侧部署。
- **Layer 3 商业数据资产**(壁垒, 不公开): 工业操作(装配/插拔/抓取/检测/维修)、机器人训练示教、医疗康复、专业连续手语。

### 5.3 数据飞轮
硬件→数据采集→AI训练→更好模型→更多应用→更多用户→更多数据。平台型公司核心。

### 5.4 不做什么 (战略边界)
- ❌ 不做机器人本体(机械臂/灵巧手/整机) — 资本重周期长
- ❌ 不做高端光学动捕替代(数十万元级)
- ❌ 不做单纯手语硬件(手语是首个场景非终局)
- ❌ 不直接竞争VLA(做数据入口不做策略模型)

---

## 第6章 竞争分析

> ✅ **已完成**: 四类竞品并行研究已落盘 `docs/BP/research_1~4_*.md`(数据手套/视觉手部追踪/机器人数据采集平台/学术算法与人体模型), 本章为送审精简版, 完整定量矩阵与来源见附录研究文档。覆盖: Manus/SenseGlove/HaptX/CyberGlove/Rokoko/Noitom/LucidVR/StretchSense; MediaPipe/Ultraleap/Meta/Apple/Move AI; UMI/ALOHA/GELLO/DexCap/AnyTeleop/TeleMoMa/TeleVision/Franka-DROID/RH20T/π0/OpenX; MANO/SMPL-X/MS-MANO/manopth/CorrNet/DSTA-SLR/PenSLR。

### 6.1 竞争格局总览

竞品分四类: ①专业数据手套 ②视觉手部追踪 ③机器人数据采集平台 ④学术算法与人体模型。完整定量矩阵见附录研究文档(`docs/BP/research_1~4_*.md`)。下表为送审精简版(机器人平台研究全表 15 项, 本章精简 10 项)。

### 6.2 专业数据手套 (商用)

| 产品 | 传感方案 | DoF | 精度(公开) | 力/触觉 | 接口 | 价格 | 空白点 |
|---|---|---|---|---|---|---|---|
| Manus Metagloves Pro | EMF电磁追踪 | 25 | 定性"毫米级" | 仅Haptic变体 | Unity/Unreal/OpenXR/ROS2 | €4,500+ | 价格高/无数据平台/消费级缺位 |
| SenseGlove Nova 2 | 线缆伸缩(4指,小指不追踪) | 未公开 | 未公开 | 有(磁摩擦刹车20N/指) | Unity/Unreal, ROS未公开 | €3,999-6,299 | 小指不追踪/无数据集/ROS缺位 |
| HaptX G1 | 磁式动捕+微流控 | 36 | 0.3mm RMS | 有(178N/手,135触觉点) | Unity/Unreal/ROS1&2 | ~$5,000(非官方) | 纯高端/重/价格不透明 |
| CyberGlove III | HyperSensor flex | 18-22 | <1°分辨率 | 无 | VirtualHand C++(老旧) | 询价 | SDK过时(WinXP/Vista)/无现代引擎/无开源 |
| Rokoko Smartgloves II | IMU+可选EMF | 39输出 | 定性"毫米" | 无 | Unity/Unreal/ROS/Blender | ~$2,500(375,000日元) | 无触觉/无数据集/OpenXR缺位 |
| Noitom PN3/Studio | IMU 9轴 | 未公开(手指) | Roll/Pitch1°/Yaw2° | 无 | Axis Studio(专有) | 询价 | 手指DoF不透明/闭源/无数据平台 |
| StretchSense | 柔性电容拉伸 | 22+ | 0.6%追踪 | 无 | "平台无关"细节不透明 | $895/双 | 无触觉/SDK不透明/无开源 |
| LucidVR/lucidgloves | 电位器+伺服力反馈 | ~5指 | 未公开 | 有(5×9g舵机) | SteamVR/OpenVR | ~$60 DIY | 无精度数据/工程化弱/无平台 |

### 6.3 视觉手部追踪 (纯CV)

| 方案 | 传感 | 关键点 | 精度(公开) | 接触力 | 致命局限 |
|---|---|---|---|---|---|
| Google MediaPipe | RGB单目 | 21(z相对深度) | 未公开 | 无 | z非绝对6DoF/遮挡退化/依赖光照 |
| Ultraleap | IR双目立体 | 27关节26+DoF | 未公开(近场亚mm) | 无 | 专用IR硬件/<60cm/视场有限 |
| Meta Quest Hand Tracking | IR+RGB透视 | 21(24DoF) | 未公开 | 无 | 仅Quest生态/FOV内/无全局6DoF |
| Apple Vision Pro | RGB+IR阵列 | 26(6DoF/关节) | 未公开(亚度级三方) | 无 | 仅VP/FOV外丢失/闭源 |
| Move AI/Move One | RGB | 全身(非手部专注) | 未公开 | 无 | 非实时(云端)/手指精度有限/按秒计费 |
| OpenXR/Unity XR Hands | API层 | 26关节 | 未公开(典型5-15mm) | 无 | 纯接口无实现/依赖头显/无独立部署 |

### 6.4 机器人数据采集平台 (EchoGlove主航道竞争区)

| 平台 | 形态 | 输出 | 力/触觉 | 成本(USD) | VLA/DP/ACT兼容 | 关键空白 |
|---|---|---|---|---|---|---|
| UMI | 手持夹爪+GoPro | 6DoF末端(VI-SLAM) | 否 | ~$2,195 | Diffusion Policy原生 | 无本体感觉/无力 |
| ALOHA/Mobile ALOHA | 双臂主从机械臂 | 关节角14DoF | 否 | ~$20k-32k | ACT原生 | 无灵巧手/无触觉/重 |
| GELLO | 3D打印桌面主控臂 | 关节角6-7DoF | 部分(重力补偿) | <$300 | 未公开 | 非可穿戴/桌面固定 |
| DexCap | **手套**(Rokoko EMF)+胸前RGB-D | 指尖+6DoF手部位姿 | **否** | 未公开 | Diffusion Policy | 手套但**无力触觉** |
| AnyTeleop/TeleMoMa | 视觉/VR/键鼠 | 关节角/末端 | 否 | $0-1,000 | BC | 纯视觉路线 |
| Open TeleVision | VR(AVP)+人形 | 绝对关节位置28D/19D | **否(论文明确缺haptic)** | AVP~$3,500+人形 | ACT | 缺haptic |
| Franka/DROID | 单臂7DoF | 6DoF末端+gripper | 机器人侧扭矩 | ~$30-40k | DP/ACT/VLA事实标准 | 机器人侧力非人手侧 |
| RH20T | 机械臂+触觉设备 | 关节角+TCP | **是**(F/T+指尖触觉阵列) | 未公开 | 兼容 | 珍贵正因含F/T |
| π0 (Physical Intelligence) | 机械臂7构型 | 关节角18D | 否 | 未公开 | VLA+Flow Matching | **10k小时私有数据,不开源** |
| Open X-Embodiment/DROID | 多机器人 | 7D末端 | **主流数据集无力字段** | — | RT-2/VLA | **无力/触觉标准化** |

### 6.5 竞争空白综合判断 (EchoGlove切入逻辑)

**结构性空白①——"手套形态+人手侧力/触觉"三元组无直接竞品**: 研究全表 15 个机器人平台/数据集中, 力信号仅 Baxter(SEA 扭矩)/Franka(关节扭矩)/RH20T(F/T+指尖触觉阵列)/LEAP(电流环近似)四家有且**全是机器人侧**(Baxter/LEAP 见 `research_3` 全表, 本章精简表未列); **无一家提供人手侧力/触觉流**——DexCap 是手套但无力触觉, AnyTeleop/TeleMoMa 纯视觉, TeleVision 论文明确承认缺 haptic。EchoGlove"可穿戴+本体感觉+力/触觉"在公开生态中独占。

**结构性空白②——价格带断层**: 高端€4,500-$5,000+纯企业, 低端$60 DIY工程化弱; **$200-800成品化+精度承诺消费级几乎无人占据**。π0已证明10000小时数据是VLA上限决定因素, 低成本是规模化采集唯一路径。

**结构性空白③——纯CV物理天花板**: 接触力=0(全域空白)、遮挡脆弱、无绝对6DoF世界位姿、光照依赖、本体状态缺失。这是**测量模态差距, 非参数优化差距, 无法被CV算法迭代弥补**。EchoGlove glove+vision融合补齐CV物理盲区。

**结构性空白④——数据平台蓝海**: 所有竞品"卖硬件+SDK", 无一家把"采集→标注→数据集→模型评估"作产品闭环。OpenX/DROID主流数据集**根本无力/触觉字段**——EchoGlove推动力/触觉数据标准化(扩展RLDS schema)既是劣势也是卡位机会。

**结构性空白⑤——多场景+国产化**: 无一产品同时覆盖手势识别+康复+VR/MR+机器人遥操+数据采集; Noitom虽国产但闭源/价格不透明/无数据平台。EchoGlove国产+开源+数据平台在京东/WRC/国产替代语境下无直接对手。

### 6.6 学术对标 (BP技术章节可引SOTA)

- **手部模型**: MANO(778顶点/16关节, 研究许可商用需Max Planck授权) + MS-MANO(CVPR2024, 肌肉骨骼+Unity集成) + manopth(PyTorch可微层) + SMPL-X(10475顶点/54关节全身)。
- **连续手语SOTA**: CorrNet(CVPR2023) PHOENIX-2014-T Test WER 20.5%; **DSTA-SLR(COLING2024)纯骨架输入轻量更快——与EchoGlove传感器驱动骨架天然契合, 端侧部署优势**; PenSLR(手套式IMU+柔性)词准确率94.58-96.70%; MDPI Sensors 23/6693 Attention-BiLSTM手套98.85%。
- **柔性传感精度基准**: Nature Comm. s41467-024-50101-w 拉伸手套关节角误差4.16°/指尖3D位置4.02mm——直接论证柔性传感手套可达毫米/度级精度。
- **传感器选型结论**: 综合精度/量产/寿命, **电容式+导电浆(石墨基)为EchoGlove主推**; 压电(PVDF)做动态事件补充; 磁感应做拇指对掌高精度关节。压阻式低成本但迟滞大需算法补偿。

### 6.7 EchoGlove 一句话定位
> EchoGlove是具身智能时代的"Human Data Capture Layer"——以百美元级可穿戴手套, 补齐当前UMI/ALOHA/DexCap生态缺失的"人手侧本体感觉+力/触觉"数据流, 为π0/OpenVLA/RT-2等VLA基础模型提供规模化、低成本、力控密集的高质量人类演示数据, 卡位具身智能人机交互入口与遥操作数据采集主航道。

> 注: 竞品硬指标均来自官方源(2026-07-23抓取), 未公开项如实标注, 未编造。Manus/Noitom/CyberGlove价格不透明, BP引用注明"询价/订阅制"。完整矩阵+来源见 `docs/BP/research_1~4_*.md`。

---

## 第7章 研发路线图

| 时间 | 里程碑 | 关键交付 | 真实性 |
|------|--------|---------|--------|
| 2026 Q3 | V7 架构启动 | Pro架构定型、外接EGO Camera接口与D9双生态协议栈设计、ROS2 SDK骨架、LSM6DSV16X驱动攻关 | 工程可实现 |
| 2026 Q4 | Lite原型定型 + Pro原型 | Lite原型定型、MANO双表示层、MediaPipe+glove融合验证、连续手语benchmark建立 | 工程可实现+需验证 |
| 2027 | Robotics Platform + 数据平台 | 机器人遥操作、VLA数据采集、机械臂示教联调、Open Core数据平台上线、首批商业行业数据集 | 需研发验证 |
| 2028+ | Human Hand Foundation Model | 人体手部智能数据基础设施、力反馈、AI眼镜深度融合 | 长期方向 |

**传感器升级路线**: Lite保留Flex(成本) → Pro升级柔性电阻/电容eSkin(主力) → 力传感接入 → (远期)sEMG。深度相机选型(结构光/双目/ToF)在2026 Q4融合验证阶段定型, 开发期用 RealSense 或等同结构光模组, 量产按成本切ToF模块。

**必须补测指标(诚实)**: 连续手语延迟/字词错误率、端到端E2E延迟、多模态时间同步精度、Flex量产一致性、IMU yaw漂移动态值。

---

## 第8章 融资与产业合作价值

### 8.1 产业合作诉求 (面向中关村/WRC/京东)
- **数据采集联调**: 与机器人/AI团队联合采集人类操作数据, 验证VLA训练有效性
- **SDK对接**: 接入主流机器人/灵巧手/机械臂厂商生态
- **量产供应链**: 柔性传感器量产、PCB/组装、AI眼镜模组合作
- **场景落地**: 京东物流仓储遥操作/分拣示教、WRC展会演示、中关村园区企业试点

### 8.2 对合作方价值
- 机器人厂商: 获得低成本人类示教数据入口, 加速策略学习
- AI公司: 获得标准化手部状态数据(双表示层), 降低数据采集成本
- 产业园/WRC: 培育具身智能上游数据基础设施, 形成生态锚点
- 京东: 物流遥操作/智能分拣的人机交互入口与数据资产

### 8.3 融资用途
V7原型量产、首批行业数据集建设、ROS2/Unity SDK完善、核心团队(算法+硬件+量产)扩充。

### 8.4 风险(诚实披露)
| 风险 | 等级 | 缓解 |
|------|------|------|
| 柔性传感器量产一致性 | ★★★★★ | 多供应商、eSkin升级、标定自动化 |
| 手部状态标准化被竞品抢先 | ★★★★★ | 双表示层+开放生态尽早占位 |
| 数据规模不足 | ★★★★★ | Open Core建社区+商业采集并行 |
| 多模态时间同步 | ★★★★ | 统一时间戳架构、硬件触发 |
| 机器人工业级精度要求 | ★★★ | 定位为数据入口非机器人本体, 规避指标绑定 |

---

## 附录: 真实性分级说明
- ✅ 已实现: 代码实测通过(PROGRESS.md 2026-07-10验证)
- 🟡 工程可实现: 6-12月, 技术路径清晰
- 🔬 需研发验证: 需建benchmark/补测
- 🌌 长期方向: 2028+, 战略愿景

> 本BP全文遵循实事求是原则, 旧版BP中"亚秒级连续手语识别""<3ms端侧推理"等未充分验证表述已降级为目标/roadmap。
