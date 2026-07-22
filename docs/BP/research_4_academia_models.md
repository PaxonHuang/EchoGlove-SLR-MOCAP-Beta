# 竞品研究 (4/4) — 学术算法与人体/手部参数模型

> 来源: 并行研究Agent, 2026-07-23. IEEE 9178440/9938436/8711745 因反爬未自动获取, 标待人工核对。

## 任务1: 手部/人体参数模型

### 1.1 MANO (MPI-IS)
- 学习来源: ~1000次31名受试者高分辨率3D手部扫描
- 参数: shape v1.1 unit-variance缩放; SMPL+H变体300形状分量。MANO-only社区常用: 10维shape + 15/45维PCA pose (官网未明示)
- 顶点/关节: 社区公认 778顶点, 16关节(15手指+1 wrist根) (官网未明示)
- 开源: 研究用途免费, 需注册签license, 非OSI开源
- 许可证: MANO Model License(研究); 商用联系Max Planck Innovation
- PyTorch: 经manopth/smplx接入; Unity: 官方无MANO-only集成, 经社区插件桥接
- 来源: https://mano.is.tue.mpg.de/

### 1.2 SMPL-X (MPI-IS)
- 全称: SMPL with eXpressive hands and face
- 顶点数: N=10,475; 关节数: K=54(含颈/颌/眼球/手指)
- 参数: M(θ,β,ψ), θ=pose β=shape ψ=表情
- 开源: 研究用途, 需注册license; 商用smpl@max-planck-innovation.de
- PyTorch官方实现(比Chumpy快8x); Unity 2021.6起提供集成; 另有Blender(2026)/Unreal
- 来源: https://smpl-x.is.tue.mpg.de/ , https://github.com/vchoutas/smplx

### 1.3 smplx仓库 (vchoutas/smplx)
- 2.7k★, 395 forks; 100% Python/PyTorch; `pip install smplx[all]`
- 支持SMPL-X/SMPL+H/SMPL/MANO统一loader, 男/女/中性
- 格式: SMPL-X .pkl/.npz; SMPL/SMPL+H .pkl; MANO .pkl
- 许可证: 非商业科研; 商用ps-licensing@tue.mpg.de

### 1.4 MS-MANO (CVPR 2024)
- 全称: MusculoSkeletal-MANO, MANO+肌肉骨骼系统, 生理真实力矩约束
- 出处: Southeast University & Shanghai Jiao Tong University, CVPR2024
- 开源: 是, MS-MANO-Unity + MS-MANO-Dynamics
- 用途: 肌肉驱动手部仿真, 仿真在环姿态精修(BioPR MLP), 对比MyoSuite
- Unity: ms-mano-unity基于RFUniverse+Kinesis+SMPL-X, Unity2022.3.x, TCP通信, 支持WSL/Windows
- 状态: 17★, v0.1.0(2024-06-13), 4 commits
- 来源: https://ms-mano.robotflow.ai/ , https://github.com/panoanx/ms-mano-unity

### 1.5 manopth (hassony2/manopth)
- MANO的PyTorch可微层(从chumpy移植), ManoLayer将pose/shape映射到顶点与关节
- 形状参数10维/样本; 姿态可配PCA分量+3维全局轴角(demo ncomps=6)
- PyTorch v0.4/v1.x; 许可证GPL-3.0; 694★
- 用途: CVPR19手物联合重建; BP用作PyTorch端手部可微层
- 来源: https://github.com/hassony2/manopth

## 任务2: 连续手语识别(SLR) SOTA

### 2.1 CorrNet (CVPR 2023)
- 方法: Correlation Network + 空间分解Identification Module, 基于VAC(ICCV2021)
- WER(ResNet18): PHOENIX-2014 Dev18.8%/Test19.4%; PHOENIX-2014-T Dev18.9%/Test20.5%; CSL-Daily Dev30.6%/Test30.1%; 另支持CSL
- 开源: 是(代码+预训练权重); PyTorch≥1.13, ctcdecode; Gradio demo; 171★, 更新至2025-11
- 端侧: 纯RGB, ResNet18+CTC偏重, 端侧需剪枝量化
- 来源: openaccess.thecvf.com/.../CorrNet + github.com/hulianyuyy/CorrNet

### 2.2 CorrNet+ (hulianyuyy/CorrNet_Plus)
- "CorrNet+: SLR and Translation via Spatial-Temporal Correlation", 统一CSLR+翻译, 仅RGB
- 38★; 端侧同CorrNet偏研究级

### 2.3 DSTA-SLR (hulianyuyy, COLING 2024)
- Dynamic Spatial-Temporal Aggregation for Skeleton-Aware SLR
- **纯骨架(pure skeleton)输入**, 精度可比RGB下速度显著更快
- 19★; **骨架输入轻量, 较RGB更适合端侧/低算力——对EchoGlove传感器驱动骨架尤为相关**
- 来源: github.com/hulianyuyy

### 2.4 AdaptSign
- hulianyuyy主页未检出, 可能私有/更名/未公开。待人工核对。

### 2.5 PenSLR (arXiv:2406.16388)
- 手套式波斯手语识别; IMU+5柔性传感器, 深度学习+CTC端到端(无需分割), Star Alignment多序列对齐集成
- 数据集: 自建PSL, 16手势, 3000+时序样本
- 精度: Subject-independent词准确率94.58%(集成+0.51%词级/+1.46%句级); Subject-dependent 96.70%(+1.32%词级/+4.00%句级)
- 开源: 未公开; 手套式IMU+柔性天然端侧, CTC端到端工程友好
- 来源: https://arxiv.org/abs/2406.16388

### 2.6 arXiv:2009.03988
- "A new architecture for hand-worn Sign language to Speech translator"
- 智能手套(Flex+加速度计+陀螺仪)+分布式处理单元+决策树(识别与纠错), ASL字母与词级
- 可借鉴: 分布式处理单元降耦合、硬件相关纠错算法

### 2.7 arXiv:2401.13254
- "A modular architecture for IMU-based data gloves" (Carfì等)
- 模块化开源硬件+软件架构, IMU定制数据手套, 附实验评估协议
- 开源: CC BY-NC-SA 4.0; 可借鉴标准化可复现模块化手套架构

### 2.8 arXiv:2512.22177
- "Real-Time ASL Recognition Using 3D CNN and LSTM: Architecture, Training, Deployment" (Dawnena Key)
- 3D CNN(时空)+LSTM(序列)混合; 部署AWS云+OAK-D边缘相机
- 数据集: WLASL(2000词)/ASL-LEX(~2700 signs)/100专家标注ASL手势
- 精度: F1 0.71-0.99, 未报WER
- 开源: github.com/dawnenakey/spokhandSLR, CC-BY 4.0, 专利申请中(US 63/918,518); OAK-D边缘推理已验证

### 2.9 Nature Comm. s41467-024-50101-w
- "Stretchable glove for accurate and robust hand pose reconstruction based on comprehensive motion data" (Park等)
- 一体化可拉伸传感手套, 同时估计指骨长度与关节角度, one-size-fits-all
- **精度: 骨长误差2.1mm, 关节角误差4.16°, 指尖3D位置误差4.02mm**
- 应用: 仿人手遥操作、手术机器人遥操作、VR/AR、运动数据采集
- 与EchoGlove相关性: **高, 直接论证拉伸式柔性传感手套可达毫米/度级精度**
- 注: 主题为手部姿态重建非SLR

## 任务3: 传感器+视觉融合相关

### 3.1 arXiv:2510.21571
- 实为"Scalable VLA Model Pretraining with Real-Life Human Activity Videos"(微软)
- 第一人称人类手部活动视频预训练VLA, 手=灵巧机器人末端执行器; 1M episodes/26M帧
- 可借鉴: 大规模未标注视频→VLA训练数据自动化管线; 手部作末端执行器建模
- 注: 非传感器+视觉融合, 是VLA预训练

### 3.2 MDPI Sensors 20(4):1074
- "Design of an Inertial-Sensor-Based Data Glove for Hand Function Evaluation" (Lin等)
- 模块化数据手套, 9轴IMU, 融合算关节ROM, 低成本易穿戴
- 可借鉴: 模块化IMU板设计、独立可扩展、多MCU适配

### 3.3 MDPI Sensors 23(15):6693
- "Dataglove for SLR via Wearable Inertial Sensors" (Ji等)
- 低成本多IMU数据手套, 4种ML(DT/SVM/KNN/RF)+Attention-BiLSTM, 20类动态手语
- **精度: Attention-BiLSTM 98.85%, RF 97.58%**
- 与EchoGlove相关性: **极高, 同题材IMU手套SLR**; 可借鉴传感器数量/位置优化、Attention-BiLSTM

### 3.4 MDPI Sensors 18(5):1545
- 实为足底压力鞋垫CoP估测(非手部)。侧切AP15±4mm/ML8.5±3mm等。
- 相关性低; 可借鉴压力传感阵列标定与误差量化方法论

### 3.5 MDPI Sensors 22(15):5628
- "A Survey on Hand Pose Estimation with Wearable Sensors and CV-Based Methods" (Chen等)
- **综述可穿戴+视觉手部姿态估计**, 涵盖运动学模型/数据手套/视觉传感器系统
- 相关性高, 直接给传感器+视觉融合综述框架

### 3.6 IEEE 9178440 / 9938436 / 8711745
- **未公开-待人工核对**: IEEE反爬返回空, Crossref/dblp未能解析。需在IEEE Xplore人工打开核对标题/方法/精度。

### 3.7 IJEAT F9167088619
- "Gesture To Speech Conversion using Flex sensors, MPU6050 and Python" (Mehra等)
- Arduino Uno+弯曲传感器+MPU6050, ASL标定, 手语转语音文本
- 相关性高, 同题材低成本手套原型; 可借鉴弯曲+IMU低成本方案、ASL标定流程

## 任务4: 可穿戴柔性传感器技术调研

### 选型小结表
| 传感原理 | 静态角度 | 动态 | 力 | 精度量级 | 成本 | 量产友好 | 寿命 |
|---|---|---|---|---|---|---|---|
| 导电浆(银/铜/石墨) | 是 | 是 | 间接 | GF 2-100+ | 低-中 | 高 | 中 |
| 压电(PZT/PVDF) | 否 | 是 | 是 | mV/Pa高灵敏度 | 中 | 中 | 长 |
| 压阻式 | 是 | 是 | 间接 | GF 2-50, 迟滞大 | 低 | 高 | 中 |
| 电容式 | 是 | 是 | 是 | 亚度级, 线性好 | 中 | 中 | 长 |
| 磁感应式 | 是 | 是 | 间接 | 高, 抗环境 | 中-高 | 中 | 长 |

### 各原理详情
- **拉伸导电浆eSkin(银/铜/石墨)**: 印刷在TPU/PDMS/Ecoflex, 拉伸改变电阻/电容。银浆GF~2-100, 石墨烯GF 100-1000+但线性窄。银浆易氧化需封装。卷对卷印刷量产友好。能测静态角度(标定后)+动态弯曲+应变, 不能直接测力(需力-应变标定)。**BP选型: 适合EchoGlove弯曲传感主方案, 石墨/银浆混合平衡成本与精度**。参考Nature s41467-024-50101-w(关节角误差4.16°)。
- **压电薄膜/陶瓷/晶体**: 机械应力→电荷, 只测变化量不能测静态(漏电)。PZT/石英寿命长(10^6+), PVDF柔性但易老化。**适合动态敲击/接触力检测, 不适用连续静态角度**。
- **PVDF基底**: d33约20-30pC/N, 频响宽, 柔性可贴合。寿命10^4-10^5。**可作动态手势事件触发, 需配合静态传感**。
- **压阻式**: 导电弹性体(碳纳米管/石墨烯/炭黑+PDMS), GF 2-50线性较好但迟滞大。寿命10^3-10^4。**适合低成本量产弯曲/压力复合, 需算法补偿迟滞**。
- **电容式**: 柔性平行板/叉指电极, 线性好迟滞低, 亚度级分辨率。寿命长(10^5+, 无直流)。**精度与寿命最优, 适合高端产品线, 需走线屏蔽设计**。
- **磁感应式**: 柔性线圈/磁体+霍尔/磁阻, 精度高抗潮湿氧化。非接触磁耦合寿命长。**适合关键关节(拇指对掌)高精度绝对角度**。

## 关键发现与建议
1. **真实可引SOTA数字**: CorrNet CVPR2023 PHOENIX-2014-T Test WER 20.5%; DSTA-SLR(COLING2024)骨架输入轻量更快; PenSLR手套式IMU+柔性词准确率94.58-96.70%; MDPI Sensors 23/6693 Attention-BiLSTM手套98.85%。可直接进BP技术对标章节。
2. **手部模型推荐**: EchoGlove Unity数字孪生→ MS-MANO(肌肉骨骼+Unity集成)+ manopth(PyTorch可微层); SMPL-X全身化扩展。**所有MPI模型研究用途许可, 商用需向Max Planck Innovation购买商用授权**——BP成本与合规章节须计入。
3. **待人工补全**: IEEE 9178440/9938436/8711745; AdaptSign仓库。
4. **编号不符提示**: arXiv 2510.21571实为VLA预训练(非传感器+视觉融合); MDPI 18/5/1545实为足底压力(非手部); Nature s41467-024-50101-w实为拉伸手套姿态(非SLR)。
5. **柔性传感器**: 综合精度/量产/寿命, **电容式与导电浆(石墨基)为EchoGlove主推**; 压电(PVDF)做动态事件补充; 磁感应做拇指对掌高精度关节。所有GF/精度数字终稿前以供应商datasheet与综述交叉核定。
6. **DSTA-SLR骨架输入轻量** —— 与EchoGlove传感器驱动骨架天然契合, 端侧部署优势明显, 是BP算法选型的关键论据。
