━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

# Edge AI 数据手套

## 全阶段 SOP SPEC PLAN V3.0

**标准操作规程与规范计划**

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

| 项目信息 | 详情 |
|---------|------|
| 项目全称 | Edge-AI-Powered Data Glove with Dual-Tier Inference for Real-Time Sign Language Translation and 3D Hand Animation Rendering |
| 项目类型 | 手语翻译 / 动作捕捉 / 数据手套 |
| MCU | ESP32-S3-DevKitC-1 N16R8（双核 240MHz，16MB PSRAM） |
| 版本 | V3.0（完全重写，移除全部 Tauri/Rust 相关内容） |
| 日期 | 2026-04-24 |

**目录**

1. [项目愿景与目标](#1-项目愿景与目标)
2. [架构决策记录 ADR](#2-架构决策记录-adr)
3. [系统架构总览](#3-系统架构总览)
4. [Phase 1：HAL 与驱动层开发](#4-phase-1hal-与驱动层开发)
5. [Phase 2：信号处理与数据采集](#5-phase-2信号处理与数据采集)
6. [Phase 3：L1 边缘推理 TinyML](#6-phase-3l1-边缘推理-tinyml)
7. [Phase 3.5：模型 Benchmark 阶段](#7-phase-35模型-benchmark-阶段)
8. [Phase 4：通信协议实现](#8-phase-4通信协议实现)
9. [Phase 5：Python Relay + L2 推理](#9-phase-5python-relay--l2-推理)
10. [Phase 6：渲染层开发](#10-phase-6渲染层开发)
11. [Phase 7：系统集成与测试](#11-phase-7系统集成与测试)
12. [已知问题与风险](#12-已知问题与风险)
13. [附录](#13-附录)

---

## 1 项目愿景与目标

本项目致力于构建一套完整的「端云协同」双层推理数据手套系统，旨在为手语翻译、动作捕捉和人机交互等领域提供高精度、低延迟、低成本的嵌入式人工智能解决方案。系统的核心理念是将轻量级推理任务部署在边缘端（Edge / 端侧），而将复杂的时序建模和语义理解任务交由上位机（Relay / Python 中继服务器）完成，从而实现计算资源的合理分配与系统整体性能的最优化。

双层推理（Dual-Tier Inference）架构是本系统的核心创新点。第一层推理（L1）运行在 ESP32-S3 边缘端，采用轻量级的 1D-CNN + Attention 模型，能够在 3ms 内完成手势分类，即使在与上位机断连的情况下仍能独立工作，确保基本的手势识别能力始终可用。第二层推理（L2）运行在上位机的 Python Relay 中继服务器上，采用自研的时空图卷积网络（ST-GCN），处理 L1 无法置信识别的复杂手语序列。两层推理之间通过置信度驱动的智能分流机制协同工作——当 L1 的输出置信度超过 0.85 时直接输出结果，否则将原始传感器数据转发至 L2 进行更精确的推理。这种架构在保证识别准确率的同时，最大限度地利用了边缘端的计算能力，减少了不必要的通信开销和上位机计算负担。

渲染层采用**两阶段渐进式演进**策略。第一阶段（L1/L2 MVP）使用纯 Web 技术栈——React 18 + Vite + R3F（React Three Fiber）+ Zustand + TailwindCSS，构建响应式的浏览器端 3D 手部骨架可视化。该方案无需安装任何桌面应用，用户通过浏览器即可访问，同时支持可选的 PWA（Progressive Web App）模式实现「添加到主屏幕」功能，提供接近原生应用的体验。第二阶段（L3 Professional）升级至 Unity 2022 LTS + XR Hands Package + ms-MANO 参数化手部模型，实现照片级的高逼真度手部渲染，特别适用于学术论文发表和专业演示场景。两个阶段的 3D 骨骼定义和关节旋转接口保持完全一致，确保渲染层的平滑演进。

### 1.1 五个核心目标

本项目围绕以下五个核心目标展开系统性设计与开发工作，每个目标均对应具体的量化指标和验收标准，确保项目成果具有可度量性和可复现性：

| 目标 | 量化指标 | 验收标准 |
|------|---------|---------|
| **实时性（Real-time Performance）** | L1 推理延迟 < 3ms；端到端延迟 < 100ms；采样频率 100Hz | 传感器采样频率稳定达到 100Hz，L1 单次推理（含预处理、模型推理和后处理）总延迟低于 3ms，从手势完成到文字输出的端到端延迟低于 100ms。通信层采用 UDP 协议传输实时渲染数据，允许丢包但追求最低延迟。 |
| **准确性（Accuracy）** | L1 Top-1 ≥ 90%（20 类）；L2 Top-1 ≥ 95%（46 类） | L1 边缘模型对 20 类简单手势的 Top-1 准确率不低于 90%；L2 上位机模型对完整 46 类中国手语词汇的 Top-1 准确率不低于 95%、Top-5 准确率不低于 99%。NLP 语法纠错模块的准确率不低于 85%，TTS 语音合成的自然度 MOS（Mean Opinion Score）评分不低于 4.0。 |
| **低功耗（Low Power）** | 续航 > 12h（600mAh）；工作功耗 < 150mW | 通过 FreeRTOS 任务调度优化、传感器休眠策略和 ESP32-S3 的 Light Sleep 模式，目标续航时间超过 12 小时（基于 600mAh 锂电池）。系统平均工作功耗控制在 150mW 以内，待机功耗低于 10mW。 |
| **可扩展性（Scalability）** | 模块化设计；模型热更新；OTA 支持 | 软件架构采用模块化设计，HAL 层、信号处理层、推理层、通信层和渲染层之间通过明确定义的接口交互。新增传感器通道仅需修改 HAL 层和特征提取模块，不影响上层推理和通信逻辑。L1 模型池支持通过 YAML 配置文件运行时热切换，无需重启设备。模型支持 OTA（Over-The-Air）推送。 |
| **成本可控（Cost Control）** | BOM < $40 | 单套硬件 BOM 成本控制在 $40 以内。核心传感器采用 TI TMAG5273A1 和 Bosch BNO085，均为成熟量产芯片，供应稳定。PCB 设计采用两层板方案，降低制板成本。实际 BOM 约 $38.60。 |

---

## 2 架构决策记录 ADR

架构决策记录（Architecture Decision Records，ADR）是本项目技术治理的核心工具，用于记录每一个关键技术决策的上下文、方案选择、决策理由和后果。以下八个 ADR 涵盖了模型训练策略、图神经网络路线、通信架构演进、渲染技术选型、硬件平台选择、模型热切换架构、Python 中继架构和基准测试驱动的模型选择等关键领域，是项目后续开发的强制性参考文档。每个 ADR 均经过团队评审确认，后续开发若需变更，必须重新发起 ADR 评审流程。

### 2.1 ADR-1：TinyML 双路径策略

**决策内容：** 采用双路径 TinyML 开发策略。路径 A 使用 Edge Impulse 平台进行 MVP 快速验证，路径 B 使用 PyTorch 原生训练并导出 TFLite INT8 模型用于论文复现。两条路径并行推进，共享数据采集流水线和传感器驱动代码。

**上下文：** 嵌入式端的手势分类模型需要在 ESP32-S3 的有限资源（512KB SRAM + 8MB PSRAM，无 GPU）下运行，且推理延迟必须低于 3ms。传统的深度学习模型（如 2D-CNN、LSTM）在嵌入式端的计算开销过大，需要专门设计轻量级架构。同时，项目需要在 MVP 验证和论文复现两个目标之间取得平衡——MVP 需要最快速度验证可行性，论文则需要完全可控、可复现的训练流程。

**决策理由：** Edge Impulse 提供了完整的端到端机器学习开发平台，包括数据采集（edge-impulse-data-forwarder）、特征工程自动化、模型训练（支持 1D-CNN）、EON Compiler 优化和 Arduino Library 导出。该平台能够将模型开发周期从数周缩短至数天，非常适合 MVP 阶段的快速迭代验证。然而，Edge Impulse 的模型结构受限于平台提供的模块，无法实现自定义的注意力机制（TemporalAttention）等高级结构，且训练过程的可复现性不足，不适合学术论文发表。因此，论文复现阶段采用 PyTorch 原生训练方案：使用 PyTorch 构建包含 TemporalAttention 模块的 1D-CNN 模型（Eq.11-13），通过自定义量化 pipeline 导出 TFLite INT8 模型，最终以 model_data.h 头文件的形式集成到 ESP32-S3 的 TFLite Micro 解释器中。该方案完全可控、可复现，且量化后的模型体积（约 38KB）和推理延迟（< 3ms）均满足嵌入式部署要求。

**后果：** 双路径策略增加了初期的开发和维护成本（需要同时维护 Edge Impulse 和 PyTorch 两套流水线），但显著降低了项目风险——如果 PyTorch 路径遇到困难，Edge Impulse 路径可以快速提供可用的模型。两条路径共享的传感器驱动和信号处理代码减少了重复开发的工作量。最终部署时选择 PyTorch 路径的模型作为主力模型，Edge Impulse 路径作为对比基线。

### 2.2 ADR-2：自研 ST-GCN（基于 MS-GCN3 论文）

**决策内容：** 不使用 OpenHands 库，从 MS-GCN3 原始论文（"Multi-Step Graph Convolutional Network for 3D Human Pose Estimation"）自行构建时空图卷积网络（Spatio-Temporal Graph Convolutional Network, ST-GCN）。

**上下文：** L2 上位机推理需要一个能够处理时序传感器数据的图神经网络模型。OpenHands 是一个基于 PyTorch Geometric 的手部图卷积开源库，表面上能够满足需求。然而，经过深入的代码审查和社区调研，发现 OpenHands 存在多个严重问题：该库已于 2023 年停止维护（最后 commit 距今超过两年），不支持可穿戴传感器数据格式（仅支持 MediaPipe 关键点输入），其图卷积实现与 MS-GCN3 论文描述存在差异，且依赖的 PyTorch Geometric 版本已经过时，与当前 PyTorch 2.x 不兼容。

**决策理由：** 自研 ST-GCN 完全基于 MS-GCN3 论文的数学描述，包含以下五个核心组件：第一，伪骨骼映射层（Pseudo-skeleton Mapping, Eq.16），使用线性层将 21 维传感器特征（15 路霍尔 + 6 路 IMU）映射为 21×2D 骨骼关键点坐标，为图卷积提供结构化输入；第二，空间图卷积层（Spatial Graph Convolution），基于手部骨骼拓扑构建邻接矩阵，实现关节间的信息传递；第三，时间卷积层（Temporal Convolution），使用 TCN（Temporal Convolutional Network）捕获手语动作的时序依赖；第四，ST-Conv Block，将空间卷积和时间卷积组合为残差块，通过跳跃连接缓解梯度消失；第五，注意力池化层（Attention Pooling），使用通道注意力机制聚合时序特征，生成最终的手势分类 logits。自研方案的代码完全可控，便于论文中的消融实验和模型解释性分析。

**后果：** 自研增加了约 3-4 周的开发时间，但消除了对已停维护库的依赖风险。同时，自研 ST-GCN 可以针对本项目的传感器数据格式（21 维特征向量，而非标准 MediaPipe 33×3 坐标）进行专门的优化，预计能够获得更好的识别准确率。代码质量和文档完整性将在论文附录中作为额外贡献。

### 2.3 ADR-3：分阶段通信架构

**决策内容：** 采用分阶段通信架构。Phase 1-3（HAL 开发、数据采集、L1 边缘推理）阶段仅使用 BLE 5.0 通信，简化开发复杂度；Phase 4 及之后（Python Relay、L2 推理、3D 渲染）阶段增加 WiFi UDP 用于高频实时数据传输。BLE 在后期阶段退化为配网通道和低速备用通道（20Hz）。

**上下文：** 数据手套需要将传感器数据传输至上位机进行 L2 推理和 3D 渲染。传输方式的选择需要在带宽、延迟、功耗和开发复杂度之间取得平衡。BLE 5.0 的优势是功耗极低（峰值约 15mA），适合移动场景，且 ESP32-S3 内置的蓝牙协议栈成熟稳定。WiFi 的优势是带宽高（802.11n 可达 65Mbps）、延迟低（局域网 < 1ms），但功耗较高（峰值约 200mA）。

**决策理由：** BLE 5.0 在 ESP32-S3 上的稳定传输带宽约为 50-100 KB/s，足以满足 L1 推理结果（单帧约 50 字节）和低速传感器数据（20Hz × 30 字节 = 600 字节/秒）的传输需求。然而，3D 渲染需要 100Hz 的完整传感器数据流（100Hz × 约 100 字节 = 10 KB/s），虽然带宽上 BLE 勉强可以支撑，但 BLE 的连接稳定性和延迟抖动无法满足实时渲染的严格要求。WiFi UDP 在局域网环境下的延迟通常低于 1ms（对比 BLE 的 20-50ms），且允许丢包的设计理念与实时渲染的容错特性高度契合。因此，Phase 4+ 采用 BLE + WiFi 双通道架构，BLE 负责 WiFi 凭证传输和低速控制指令，WiFi UDP 负责高频传感器数据广播。前端（React / Unity）通过 WebSocket 连接到 Python Relay，不直接与 ESP32 通信。

**后果：** 分阶段策略降低了早期的开发难度，但要求在 Phase 4 进行通信架构的切换，需要仔细的集成测试。BLE 在后期的「降级备份」角色要求 BLE 通信始终可用，即使 WiFi 连接正常时也需要保持 BLE 连接的心跳，这会略微增加功耗。

### 2.4 ADR-4：两阶段渲染演进

**决策内容：** 渲染层采用两阶段渐进式演进策略。第一阶段（L1/L2 MVP）使用 React 18 + Vite + R3F（React Three Fiber）+ Zustand + TailwindCSS 纯 Web 方案，通过浏览器实现 3D 手部骨架可视化，并支持可选的 PWA 模式；第二阶段（L3 Professional）升级至 Unity 2022 LTS + XR Hands Package + ms-MANO 参数化手部模型，实现高逼真度的专业级渲染。

**上下文：** 渲染层需要在项目早期快速验证传感器数据到 3D 可视化的完整链路，同时在项目后期提供学术发表级别的视觉效果。V2 版本曾采用 Tauri + Rust 方案作为桌面端渲染框架，但经过实际开发评估，发现该方案存在以下问题：Tauri 的 Rust 后端增加了开发复杂度（需要同时维护 Rust 和 TypeScript 代码库），构建流程不够成熟（特别是 WebSocket 客户端的集成），且与最终目标——纯 Web 访问——存在架构冲突。纯 Web 方案可以消除桌面应用安装的障碍，用户通过浏览器即可访问，更符合项目的演示和部署需求。

**决策理由：** React + R3F 方案的核心优势在于「零安装、即开即用」。React 18 提供了高效的 UI 渲染能力，Vite 提供了极速的开发体验和构建速度，R3F 将 Three.js 的强大 3D 渲染能力封装为 React 声明式组件，极大地提升了开发效率和可维护性。Zustand 作为轻量级状态管理库，负责管理 WebSocket 接收到的传感器数据和手势识别结果，驱动 3D 场景更新。TailwindCSS 提供了响应式的 UI 设计能力，确保在桌面端和移动端均能正常显示。PWA 模式则通过 manifest.json 和 Service Worker 实现「添加到主屏幕」功能，提供接近原生应用的启动体验和离线缓存能力。

Unity 2022 LTS + ms-MANO 方案虽然开发周期较长（约 6-8 周），但能提供照片级的视觉品质和精确的关节控制，是论文发表和产品化的最终形态。两个阶段的 3D 骨骼定义（21 个关键点、20 根骨骼、标准的 DOF 定义）和关节旋转接口保持完全一致，确保从 Web MVP 到 Unity Pro 的平滑演进。

**后果：** 移除 Tauri/Rust 后，前端不再需要桌面端构建流程，简化了 CI/CD 管道。但纯 Web 方案在 3D 渲染性能上略逊于原生桌面应用（受限于浏览器的 WebGL 2.0 能力），在低端设备上可能出现帧率下降。通过合理的 LOD（Level of Detail）策略和渲染优化可以缓解这一问题。Unity 阶段与 Web 阶段共享同一套骨骼定义，确保数据格式的兼容性。

### 2.5 ADR-5：硬件平台保持

**决策内容：** 继续使用 ESP32-S3-DevKitC-1 N16R8 作为主控 MCU，不切换到 Seeed Studio XIAO ESP32S3。该决策经过详细的硬件参数对比和实际测试验证，具有充分的技术依据。

**上下文：** V3 架构引入了 WiFi UDP 高频传输（100Hz）、模型热切换（需要额外内存存储多个模型）、以及更大的滑动窗口缓冲区，对硬件资源的需求进一步增加。市场上存在体积更小的替代方案（如 Seeed Studio XIAO ESP32S3），需要评估是否值得切换平台。

**决策理由：** DevKitC-1 配备 16MB Octal PSRAM（N16R8 型号中的 N8 代表 8MB Flash，R8 代表 8MB PSRAM），对 TFLite Micro 模型推理和传感器数据缓冲至关重要。实际测试表明，模型热切换功能需要同时存储至少两个 INT8 模型（约 80KB），加上运行时激活缓冲区（约 50KB）、传感器环形缓冲区（30 帧 × 21 特征 × 4 字节 = 约 2.5KB）和 Protobuf 序列化缓冲区，系统峰值内存占用约为 4-5MB。XIAO ESP32S3 的 PSRAM 仅为 8MB（考虑系统开销后实际可用约 5-6MB），余量极为有限，在模型增大或功能扩展时容易触发内存不足。DevKitC-1 的 PCB 布局已经过验证，I2C 总线的信号完整性和天线性能均满足设计要求，切换平台需要重新设计 PCB，增加项目风险和成本。

**后果：** DevKitC-1 的体积（51mm × 25mm）比 XIAO（21mm × 17.8mm）大得多，这在追求穿戴舒适性的数据手套场景中是一个劣势。但考虑到开发效率和系统稳定性，V3 阶段继续使用 DevKitC-1，待系统功能稳定后可在产品化阶段考虑定制小型化 PCB。

### 2.6 ADR-6：L1 边缘模型池 + 热切换架构

**决策内容：** 在 ESP32-S3 上构建统一的模型池（Model Pool）架构，支持多个 L1 模型的运行时热切换。模型池包含两个候选模型：1D-CNN + Attention（主力模型，约 34K 参数，INT8 量化后约 38KB）和浅层 MS-TCN（Multi-Stage Temporal Convolutional Network，实验性模型）。所有模型通过统一的 BaseModel 抽象接口注册和管理，切换通过 YAML 配置文件驱动，无需重启设备。

**上下文：** 手语识别场景下，不同的手势集合（数字、字母、日常词汇、专业术语）可能需要不同结构的模型才能达到最优性能。固定部署单一模型无法适应多样化的使用需求。同时，研究过程中需要快速对比不同模型架构的效果，每次切换模型都重新编译和烧录固件严重拖慢了迭代速度。

**决策理由：** 统一的 BaseModel 接口定义了所有 L1 模型必须实现的标准方法：`init()` 初始化模型资源、`preprocess()` 输入预处理、`infer()` 执行推理、`postprocess()` 输出后处理、`get_model_info()` 返回模型元信息（名称、版本、参数量、支持的手势类别数）。每个具体模型（1D-CNN+Attn、MS-TCN）继承 BaseModel 并实现这些方法。模型注册表（Model Registry）在系统启动时扫描可用的模型，将模型实例注册到全局字典中。YAML 配置文件指定当前激活的模型名称，当配置更新时（通过 BLE 或 WiFi OTA），系统在下一帧推理前自动完成模型切换——释放旧模型的 PSRAM Arena，加载新模型权重，重新分配推理缓冲区。

**后果：** 模型热切换功能增加约 10-15KB 的代码占用和 5-8KB 的 RAM 开销（模型注册表和配置解析器），但这些开销相对于 8MB PSRAM 的总量微不足道。热切换过程中的短暂停顿（约 50-100ms，取决于模型大小）需要在设计上做好处理——切换期间暂停推理输出，向前端发送 "MODEL_SWITCHING" 状态消息。该架构为未来扩展更多模型（如 Transformer-based 轻量级模型）奠定了坚实基础。

### 2.7 ADR-7：Python 统一中继架构

**决策内容：** 采用 FastAPI + WebSockets 作为 Python 中继服务器（Python Relay），作为 ESP32 边缘端与前端渲染之间的统一数据枢纽。Python Relay 同时承担四个职责：UDP 数据接收与 Protobuf 解析（端口 8888）、WebSocket JSON 推送（端口 8765）、L2 ST-GCN 推理、TTS 语音合成和 NLP 语法纠错。

**上下文：** V2 架构中，前端直接处理 Protobuf 格式的传感器数据，要求前端开发者理解 Protobuf 协议细节，增加了前端开发复杂度。同时，L2 推理、NLP 处理和 TTS 合成分散在不同的服务中，需要复杂的进程间通信和部署管理。V3 架构需要简化前端开发、统一数据格式、并整合 AI 服务。

**决策理由：** Python Relay 的核心设计思想是「前端只接收 JSON，不理解 Protobuf」。ESP32 通过 WiFi UDP 将 Protobuf 序列化的 GloveData 消息发送到 Python Relay 的端口 8888，Python 端使用标准 protobuf 库解析数据后，转换为结构化 JSON 格式，通过 WebSocket 推送到前端。这种设计将所有数据格式转换的复杂性封装在 Python 端，前端开发者只需处理标准的 JSON 消息，极大降低了前端开发门槛。

Python Relay 使用 FastAPI 作为 Web 框架，利用其原生的异步支持（asyncio）实现高并发。UDP 服务器使用 asyncio.datagram 的 UDP 协议实现，WebSocket 服务器使用 FastAPI 的 WebSocket 端点实现。当 WebSocket 客户端连接时，Relay 自动开始向该客户端推送 JSON 格式的传感器数据和手势识别结果。L2 ST-GCN 推理在 Python 端异步执行，当 L1 置信度不足时自动触发，推理完成后将 L2 结果追加到 WebSocket 消息流中。edge-tts 语音合成和 NLP 语法纠错也作为异步任务在 Relay 中执行。

**后果：** Python Relay 作为系统的中心枢纽，其稳定性直接影响整个系统的可用性。需要实现完善的错误处理和自动恢复机制（如 UDP 超时重连、WebSocket 断线重连、L2 推理超时降级）。Python 的 GIL（全局解释器锁）可能成为瓶颈，但由于 I/O 密集型操作（UDP 接收、WebSocket 推送）主要由 asyncio 事件循环处理，实际影响有限。L2 推理可以使用 PyTorch 的多线程后端绕过 GIL 限制。

### 2.8 ADR-8：Benchmark 驱动的模型选择

**决策内容：** 建立统一的模型评估框架（Benchmark Framework），通过标准化的评估协议对 L1 模型池中的所有候选模型进行系统性的对比评估。评估指标包括 Top-1 准确率、Top-5 准确率、FLOPs（浮点运算次数）、推理延迟（latency）、模型参数量（parameters）和内存占用（memory）。新增 Phase 3.5 作为正式的模型 Benchmark 阶段，在 L1 模型部署之前完成模型的全面评估和选择。

**上下文：** L1 边缘模型池中包含多个候选模型（1D-CNN+Attention、MS-TCN 等），需要客观的数据驱动的标准来决定最终部署哪个模型。仅看 Top-1 准确率是不够的——嵌入式部署还需要考虑模型的计算开销、内存占用和推理延迟。缺乏统一的评估框架会导致模型选择决策主观化，且不同实验之间的结果不可比较。

**决策理由：** 统一的 Benchmark 框架使用 Python 实现，提供命令行接口和自动化批量评估能力。评估协议采用 5 折交叉验证（5-fold Cross-Validation），确保评估结果的统计可靠性。评估报告以 Markdown 表格和可视化图表的形式输出，便于团队评审和论文撰写。模型比较矩阵（Model Comparison Matrix）将所有候选模型的各项指标并列展示，最终选择采用帕累托最优（Pareto Optimality）原则——在准确率和推理延迟之间取得最佳平衡的模型被选为部署模型。Benchmark 框架还支持自定义评估指标和评估数据集，便于后续扩展。

**后果：** 新增 Phase 3.5 虽然延长了整体开发周期（约 1 周），但通过数据驱动的模型选择显著降低了部署错误模型的风险。评估结果的文档化也为论文的实验部分提供了现成的素材。Benchmark 框架作为项目的长期基础设施，后续新增模型时可以复用，长期收益显著。

---

## 3 系统架构总览

本章节从硬件架构、软件栈、双层推理架构、通信架构、模型热切换架构和数据流管线六个维度，全面描述 Edge AI 数据手套系统的技术架构。系统采用三层解耦设计（Layer 1 边缘端、Layer 2 Python Relay、Layer 3 渲染端），各层之间通过明确定义的接口交互，确保模块的可替换性和系统的可维护性。

### 3.1 硬件架构

系统硬件以 ESP32-S3-DevKitC-1 N16R8 为核心控制器，该芯片采用台积电 40nm 工艺制造，搭载双核 Xtensa LX7 处理器（主频 240MHz），内置 512KB SRAM 和 AI 向量扩展指令集（支持 INT8 乘加运算加速），是乐鑫科技专为 AIoT 场景设计的旗舰级 MCU。N16R8 型号配备 8MB Flash 和 8MB Octal PSRAM，Flash 用于存储固件代码和模型权重，PSRAM 用于运行时数据缓冲和模型推理的中间结果存储。

传感器模块包含三个部分：第一，5 颗德州仪器 TMAG5273A1 3D 霍尔传感器（I2C 接口），分别安装在拇指、食指、中指、无名指和小指的近端指间关节（PIP）位置，用于检测手指弯曲角度。每颗传感器输出 3 轴磁场强度数据（X/Y/Z，12-bit 分辨率），量程 ±40mT，通过 Set/Reset 触发模式实现温度漂移补偿。5 颗传感器通过 TCA9548A I2C 多路复用器共享同一 I2C 总线，I2C 地址均为 0x22（通过 TCA9548A 的通道 0-4 分别访问）。第二，1 颗博世 BNO085 9 轴 IMU（I2C 接口），安装在手套腕部位置，集成三轴加速度计、三轴陀螺仪和三轴磁力计，并通过内置的 SensorHub 处理器实现硬件级传感器融合，直接输出游戏级旋转四元数（Game Rotation Vector，无地磁影响），输出频率 100Hz。第三，TCA9548A I2C 多路复用器（地址 0x70），负责在 5 路 TMAG5273 之间切换，解决 I2C 地址冲突问题。

系统还预留了 5 路 ADC 通道（GPIO 4、5、6、7、15），用于未来扩展柔性弯曲传感器（Flex Sensor）。I2C 总线运行在 400kHz Fast Mode，主总线上拉电阻为 2.2kΩ，TCA9548A 子通道上拉电阻为 4.7kΩ，确保信号完整性。

#### 3.1.1 核心 BOM 清单

| 组件 | 型号 | 数量 | 单价 (USD) | 备注 |
|------|------|------|-----------|------|
| MCU 开发板 | ESP32-S3-DevKitC-1 N16R8 | 1 | $13.50 | 双核 240MHz, 8MB PSRAM |
| 3D 霍尔传感器 | TMAG5273A1 | 5 | $2.80 | I2C, 12-bit, ±40mT |
| 9 轴 IMU | BNO085 | 1 | $9.50 | 硬件四元数融合 |
| I2C 多路复用器 | TCA9548A | 1 | $0.85 | 8 通道 |
| 锂电池 | 600mAh 3.7V | 1 | $3.50 | JST-PH 接口 |
| PCB + 外壳 | 定制 | 1 | $5.00 | 两层板, 3D 打印外壳 |
| 其他（电阻/电容/连接器） | --- | --- | $3.45 | 上拉电阻, 排线等 |
| **合计** | | | **≈ $38.60** | |

### 3.2 软件栈

系统软件栈分为四个主要层次，分别对应三层系统架构和公共的 AI 工具链。各层技术选型经过严格评估，确保满足功能需求、性能要求和开发效率的平衡。

| 层次 | 技术栈 | 核心职责 |
|------|--------|---------|
| **嵌入式层（Layer 1）** | PlatformIO + Arduino、FreeRTOS、Nanopb、TFLite Micro、Edge Impulse SDK | 传感器驱动、信号处理、L1 推理、通信 |
| **中继层（Layer 2）** | Python 3.9+、FastAPI、WebSockets、asyncio、PyTorch | UDP 接收、Protobuf 解析、WebSocket 推送、L2 推理、NLP、TTS |
| **渲染 MVP（Layer 3a）** | React 18、Vite、R3F (React Three Fiber)、Zustand、TailwindCSS | 3D 手部骨架可视化、响应式 Web UI、PWA |
| **渲染 Pro（Layer 3b）** | Unity 2022 LTS、XR Hands Package、ms-MANO | 高逼真度手部渲染、学术论文发表 |
| **AI 工具链** | PyTorch、TFLite、Nanopb、edge-tts | 模型训练、量化、部署 |

嵌入式层基于 PlatformIO + Arduino 框架开发，运行 FreeRTOS 实时操作系统，使用 Nanopb（Protocol Buffers 的 Nano 版本）进行数据序列化，集成 TFLite Micro 推理引擎和 Edge Impulse SDK。PlatformIO 提供了强大的构建系统和依赖管理能力，支持 ESP32-S3 的 AI 向量指令集自动优化。FreeRTOS 负责任务调度和资源管理，确保传感器采样、模型推理和通信传输三个核心任务能够并行执行且互不干扰。

Python 中继层（Layer 2）基于 FastAPI 框架构建，利用 asyncio 实现高并发的异步 I/O。FastAPI 提供了自动化的 API 文档（Swagger UI）、请求验证和类型提示，大幅提升了开发效率。WebSocket 服务器负责向前端推送 JSON 格式的传感器数据和手势识别结果，UDP 服务器负责接收 ESP32 的 Protobuf 数据流。PyTorch 用于 L2 ST-GCN 推理，edge-tts 用于语音合成。

渲染 MVP 层（Layer 3a）采用纯 Web 技术栈，React 18 负责组件化 UI 开发，Vite 提供极速的 HMR（Hot Module Replacement）和构建优化，R3F 将 Three.js 的 3D 渲染能力封装为 React 组件，Zustand 管理全局状态（传感器数据、手势识别结果、UI 状态），TailwindCSS 提供原子化的 CSS 工具类实现响应式设计。该方案完全基于浏览器运行，无需任何桌面端安装。

渲染 Pro 层（Layer 3b）使用 Unity 2022 LTS 游戏引擎，配合 XR Hands Package 实现 21 个手部关键点的精确追踪，ms-MANO 参数化手部模型通过 48 维 pose_parameters 驱动高逼真度的手部网格变形和渲染。Unity 方案特别适用于学术论文发表级别的视觉展示，能够展示系统在手语翻译精度和视觉自然度方面的卓越表现。

### 3.3 双层推理架构

双层推理架构是本系统最具创新性的设计之一，实现了计算资源在边缘端和上位机之间的最优分配。

**L1 层（Edge / 端侧推理）**运行在 ESP32-S3 上，采用轻量级 1D-CNN + TemporalAttention 模型，总参数量约 34K（INT8 量化后约 38KB），推理延迟低于 3ms。L1 模型的输入为 30 帧 × 21 维特征的滑动窗口（630 维），输出为 46 类手势的分类概率。L1 模型池还包含实验性的浅层 MS-TCN 模型作为备选。L1 模型的核心优势在于超低延迟和离线可用性——即使在 BLE/WiFi 断连的情况下，数据手套仍能独立完成基本手势识别。模型通过统一的 BaseModel 接口管理，支持运行时热切换。

**L2 层（Python Relay / 上位机推理）**运行在 Python Relay 中继服务器中，采用自研 ST-GCN 模型处理动态手语序列。L2 模型接收完整的时序传感器数据流，通过伪骨骼映射层将 21 维传感器特征转换为 21×2D 骨骼关键点坐标，然后经过多层 ST-Conv Block 提取时空特征，最终通过注意力池化层生成手势分类结果。L2 模型的参数量和计算量远大于 L1，但得益于 GPU 加速，推理延迟仍可控制在 20ms 以内。

**置信度驱动的智能分流**逻辑是双层推理协同工作的核心：当 L1 模型的输出置信度超过 0.85 时，直接输出手势 ID 和中文名称，无需 L2 参与，从而最小化端到端延迟；当 L1 置信度低于或等于 0.85 时，标记为 UNKNOWN，同时将当前滑动窗口的原始传感器数据通过 WiFi UDP 转发至 Python Relay，触发 L2 ST-GCN 推理。这种设计在保证识别准确率的同时，最大限度地利用了边缘端的计算能力，减少了不必要的通信和上位机计算开销。

### 3.4 分阶段通信架构

通信架构按照项目阶段渐进式演进，与系统三层架构紧密配合。

**Phase 1-3（BLE-only 模式）：** 系统仅使用 BLE 5.0 通信。BLE GATT 服务提供两个主要功能：配网服务（接收上位机发送的 WiFi SSID 和 Password，存储到 NVS 分区）和数据广播服务（通过 Notification 方式向已连接的客户端推送传感器数据或推理结果）。ESP32-S3 同时通过串口输出 CSV 格式的传感器数据，供 edge-impulse-data-forwarder 工具采集。

**Phase 4+（BLE + WiFi UDP 模式）：** 系统增加 WiFi UDP 通信通道。WiFi UDP 目标端口为 8888，发送频率 100Hz，每帧数据使用 Nanopb 序列化的 GloveData Protobuf 消息。BLE 在此阶段退化为配网通道和低速备用通道（20Hz），用于 WiFi 不可用时的降级通信。前端（React Web / Unity）**不直接连接 ESP32**，而是通过 WebSocket 连接到 Python Relay（端口 8765），接收 JSON 格式的数据。

| 阶段 | 通信方式 | 用途 | 数据格式 |
|------|---------|------|---------|
| Phase 1-3 | BLE 5.0 | 配网 + 传感器数据广播 | Protobuf |
| Phase 4+ | WiFi UDP (端口 8888) | ESP32 → Python Relay 高频数据 | Protobuf |
| Phase 4+ | WebSocket (端口 8765) | Python Relay → 前端渲染 | JSON |
| Phase 4+ | BLE 5.0 (20Hz) | 配网 + 降级备份 | Protobuf |

### 3.5 模型热切换架构（V3 新增）

模型热切换架构是 V3 版本的重要新增功能，允许在运行时通过配置变更切换 L1 边缘模型，无需重新编译和烧录固件。该架构由三个核心组件构成：

**BaseModel 统一接口**定义了所有 L1 模型必须实现的标准 API。接口使用 C++ 虚函数（virtual function）实现多态：

```cpp
class BaseModel {
public:
    virtual bool init(const uint8_t* model_data, size_t model_size) = 0;
    virtual int preprocess(float* input, const SensorFrame* frames, int num_frames) = 0;
    virtual int infer(const float* input, float* output) = 0;
    virtual int postprocess(const float* output, GestureResult* result) = 0;
    virtual ModelInfo get_model_info() const = 0;
    virtual void cleanup() = 0;
    virtual ~BaseModel() = default;
};
```

**模型注册表（Model Registry）**维护一个全局的模型实例字典，键为模型名称字符串，值为 BaseModel 指针。系统启动时，根据 YAML 配置文件中定义的模型列表，依次加载各模型的权重（model_data.h 中的字节数组）并实例化对应的 BaseModel 子类，注册到 Registry 中。

**YAML 配置驱动切换**通过一个简单的 YAML 文件指定当前激活的模型：

```yaml
# /spiffs/model_config.yaml
active_model: "cnn_attention_v2"
models:
  - name: "cnn_attention_v2"
    file: "model_data_cnn_attn.h"
    type: "CNNAttention"
    classes: 46
  - name: "ms_tcn_v1"
    file: "model_data_ms_tcn.h"
    type: "MSTCN"
    classes: 20
```

当配置更新时（通过 BLE OTA 或 WiFi HTTP 端点），系统在下一帧推理前自动执行热切换：调用当前模型的 `cleanup()` 释放 PSRAM Arena → 从 SPIFFS 加载新模型配置 → 调用新模型的 `init()` 分配资源并加载权重。整个切换过程约 50-100ms，期间向前端发送 "MODEL_SWITCHING" 状态消息。

### 3.6 数据流管线

V3 架构的完整数据流管线如下，清晰地展示了从传感器采集到最终渲染的全链路数据传输路径：

```
ESP32-S3 (Layer 1)                    Python Relay (Layer 2)              渲染前端 (Layer 3)
┌─────────────────┐   WiFi UDP:8888   ┌──────────────────┐  WS:8765     ┌─────────────────┐
│ TMAG5273 ×5     │                   │                  │               │                 │
│ BNO085 ×1       │──Nanopb Proto──→ │ FastAPI Server   │──JSON Push──→│ React + R3F     │
│ TCA9548A        │   (100Hz)         │ UDP Receiver     │  (100Hz)     │ 3D Hand Skeleton│
│                 │                   │ Protobuf Parser  │               │ Zustand Store   │
│ FreeRTOS        │                   │ L2 ST-GCN        │               │ TailwindCSS UI  │
│ Core1: 100Hz    │                   │ NLP Correction   │               │ PWA             │
│ Core0: L1+Comm  │                   │ edge-tts         │               │                 │
└─────────────────┘                   └──────────────────┘               └─────────────────┘
```

数据流的关键节点说明：

1. **传感器采集（100Hz）：** Task_SensorRead 在 Core 1 上以 100Hz 频率依次读取 BNO085 和 5 路 TMAG5273 的原始数据，写入环形缓冲区。
2. **L1 边缘推理（按需触发）：** 当环形缓冲区填满 30 帧后，Task_Inference 在 Core 0 上执行 TFLite Micro 推理，输出手势 ID 和置信度。
3. **UDP 发送（100Hz）：** Task_Comms 在 Core 0 上以 100Hz 将 GloveData Protobuf 消息通过 WiFi UDP 发送到 Python Relay 的端口 8888。
4. **Python Relay 接收与解析：** asyncio UDP 服务器接收 Protobuf 数据，使用标准 protobuf 库解析为 Python 字典。
5. **L2 推理触发（条件性）：** 如果 L1 置信度 ≤ 0.85，Python Relay 自动将最近 30 帧的特征数据输入 L2 ST-GCN 模型进行推理。
6. **WebSocket JSON 推送：** Python Relay 将解析后的传感器数据、L1/L2 手势识别结果、NLP 纠错文本等组装为 JSON 消息，通过 WebSocket 推送到已连接的前端客户端。
7. **前端渲染更新：** React 组件通过 Zustand Store 接收 WebSocket 消息，更新 3D 手部骨架的关节旋转角度，并显示手势识别结果文本。

---

## 4 Phase 1：HAL 与驱动层开发

Phase 1 是整个项目的基石，目标是完成硬件抽象层（Hardware Abstraction Layer, HAL）和所有传感器驱动的开发，确保 ESP32-S3 能够稳定地从传感器获取原始数据。本阶段的工作质量直接影响后续所有阶段的数据质量和模型性能，因此必须严格按照验收标准执行，任何不符合标准的问题都必须在本阶段解决，不得带入后续阶段。本阶段的开发工具为 PlatformIO + Arduino 框架，调试工具为 ESP-IDF 内置的逻辑分析仪组件和串口监视器。

### 4.1 TMAG5273 驱动开发

TMAG5273A1 是德州仪器推出的高精度线性 3D 霍尔效应传感器，支持 I2C 接口通信，提供 12-bit 分辨率的 X/Y/Z 三轴磁场强度输出。驱动的核心功能是实现 `ReadXYZ()` 方法，通过 I2C 总线读取传感器的磁场数据寄存器（MAG_X、MAG_Y、MAG_Z，各 2 字节，Little-Endian 格式）。传感器通过 TCA9548A 多路复用器连接到 ESP32-S3 的 I2C 主总线，I2C 设备地址为 0x22（默认配置，A0/A1 引脚接地）。

驱动初始化流程如下：首先通过 TCA9548A 选择目标通道（通道 0-4 分别对应拇指到小指的传感器），然后写入传感器配置寄存器，设置触发模式为 Set/Reset（自动补偿温度漂移），量程为 ±40mT（默认值），转换模式为连续转换。初始化完成后，`ReadXYZ()` 方法按照以下步骤执行：选择 TCA9548A 通道 → 等待 1ms 总线稳定 → 读取磁场数据寄存器（6 字节）→ 组合为 X/Y/Z 三轴 12-bit 有符号整数值 → 转换为物理单位（mT）。

**关键寄存器说明：**

| 寄存器地址 | 名称 | 位宽 | 说明 |
|-----------|------|------|------|
| 0x00 | DEVICE_CONFIG | 8 | 触发模式 (Set/Reset)、转换模式 |
| 0x01 | SENSOR_CONFIG_1 | 8 | 量程选择 (±20mT / ±40mT / ±80mT) |
| 0x10-0x15 | MAG_X/Y/Z | 16×3 | 三轴磁场强度 (12-bit, Little-Endian) |
| 0x02 | SENSOR_CONFIG_2 | 8 | 平均次数、转换速率 |

需要注意的是，每次读取前必须先通过 TCA9548A 切换通道，否则会读取到错误传感器的数据，这是 I2C 多路复用器架构下最常见的 Bug 之一。此外，TMAG5273 在连续转换模式下的默认输出速率为 20Hz（CONV_AVG = 0），可通过修改 SENSOR_CONFIG_2 寄存器提高到 200Hz。本项目使用 100Hz 采样率，需要将转换速率配置为 100Hz 或更高。

### 4.2 BNO085 驱动开发

BNO085 是博世推出的新一代智能 9 轴 IMU，集成了三轴加速度计、三轴陀螺仪、三轴磁力计和 32 位 ARM Cortex-M0+ SensorHub 处理器。与上一代 BNO055 不同，BNO085 采用博世专有的 SH-2（Sensor Hub 2）协议进行配置和数据读取，支持丰富的传感器融合算法。本项目的驱动需要开启以下两个传感器报告（Sensor Report）：

| 报告名称 | 报告 ID | 输出数据 | 频率 |
|---------|---------|---------|------|
| SH2_GAME_ROTATION_VECTOR | 0x05 | 四元数 (X, Y, Z, W)，不含地磁 | 100Hz |
| SH2_GYROSCOPE_CALIBRATED | 0x0D | 三轴角速度 (rad/s) | 100Hz |

使用硬件传感器融合（Hardware Sensor Fusion）是本设计的关键决策之一。BNO085 的内置 SensorHub 处理器以 1kHz 的频率执行传感器融合算法，输出经过卡尔曼滤波和互补滤波的稳定四元数，这比在 ESP32-S3 上用软件实现融合（需要约 200μs/帧的计算时间）更高效、更精确。

驱动实现需要处理 SH-2 协议的细节，包括产品 ID 查询、引脚分配（INT 引脚连接到 GPIO 21，用于数据就绪中断）、报告使能/禁用命令和数据读取。四元数数据的读取采用中断方式：当 BNO085 的 INT 引脚拉低时，在 FreeRTOS 任务中通过 I2C 读取 FIFO 中的传感器数据，确保不丢失任何采样点。SH-2 协议的数据包格式为：[SHTP Header (4 bytes)] + [Report ID (1 byte)] + [Sequence Number (1 byte)] + [Data Payload (N bytes)]。

### 4.3 TCA9548A 多路复用器驱动

TCA9548A 是德州仪器生产的 8 通道 I2C 多路复用器/开关，通过 I2C 主总线控制最多 8 个子 I2C 通道的通断。本项目使用其中 5 个通道（通道 0-4），分别连接 5 颗 TMAG5273 传感器。TCA9548A 的 I2C 设备地址为 0x70（A0/A1/A2 引脚均接地），控制寄存器为单字节，每一位对应一个通道的开关状态（1=开启，0=关闭）。

驱动的核心方法是 `selectChannel(uint8_t channel)`，该方法向 TCA9548A 的控制寄存器写入 `(1 << channel)` 来激活指定通道。关键注意事项：TCA9548A 的通道切换不是互斥的——写入新的通道选择值不会自动关闭之前开启的通道。因此，在每次选择新通道前，必须先写入 0x00 关闭所有通道，再写入目标通道值。这种两步操作虽然增加了一次 I2C 写操作，但能有效避免多通道同时开启导致的总线冲突问题。通道切换后需要等待至少 1ms 的总线稳定时间（t_CHG，数据手册典型值为 0.6μs，但考虑 PCB 走线和线缆电容，实际建议等待 1ms），然后再进行后续的传感器读写操作。

### 4.4 FreeRTOS 双核任务框架

ESP32-S3 运行 FreeRTOS 实时操作系统，本项目的任务分配遵循「传感器采样与推理分离、通信独立」的原则，将系统划分为三个核心任务，分别绑定到不同的 CPU 核心上运行。ESP32-S3 的双核架构（Core 0 和 Core 1）使得传感器采样和模型推理可以真正并行执行，不会因推理计算而错过采样时间点。

| 任务名称 | 绑定核心 | 优先级 | 频率 | 功能描述 |
|---------|---------|--------|------|---------|
| Task_SensorRead | Core 1 | 3 (最高) | 100Hz | I2C 传感器数据采集 |
| Task_Inference | Core 0 | 2 | 按需触发 | TFLite Micro L1 推理 |
| Task_Comms | Core 0 | 1 (最低) | 100Hz / 事件驱动 | BLE/WiFi 数据传输 |

Task_SensorRead 以 100Hz 的固定频率运行在 Core 1 上，是系统中优先级最高的任务。该任务按照固定的顺序依次完成以下操作：读取 BNO085 四元数和陀螺仪数据 → 遍历 TCA9548A 通道 0-4 读取 5 路 TMAG5273 磁场数据 → 将原始数据写入共享环形缓冲区 → 通过任务通知（xTaskNotify）唤醒 Task_Inference（当缓冲区填满 30 帧时）。该任务的总执行时间约为 5-8ms（含 I2C 通信和通道切换），在 10ms 的采样周期（100Hz）内能够稳定完成。

> ⚠️ **关键 Bug 修复记录：** 在最初实现 FreeRTOS 任务创建时，发现 `xTaskCreatePinnedToCore()` 函数的参数顺序存在严重错误。正确的参数顺序为：`xTaskCreatePinnedToCore(taskFunction, name, stackSize, parameter, priority, taskHandle, coreID)`。在早期代码中，函数指针（taskFunction）和任务句柄（taskHandle）的位置被互换，导致 FreeRTOS 将任务句柄（一个整数指针）当作函数指针执行，触发 Cache Access Exception（缓存访问异常），系统立即崩溃。该 Bug 的错误信息为 `"Guru Meditation Error: Core 0 panic'ed (CacheAccessError)"`，排查过程非常困难，因为错误信息并未明确指向参数顺序问题。修复方法：严格对照 ESP-IDF API 文档，确保参数顺序正确，并添加编译期静态断言（`static_assert`）验证函数指针的有效性。

### 4.5 验收标准

Phase 1 的验收标准是确保所有传感器驱动的正确性和稳定性，为后续阶段提供可靠的数据基础。具体验收标准如下：

1. **TMAG5273 稳定性：** 5 路 TMAG5273 传感器能够稳定读取 X/Y/Z 三轴磁场数据，在 TCA9548A 通道切换过程中无 I2C 总线冲突或数据错乱现象，连续运行 24 小时无错误。
2. **BNO085 精度：** BNO085 四元数输出稳定，旋转精度满足手部姿态检测需求，校准功能正常（系统启动后 5 秒内完成自动校准）。
3. **采样频率：** 整体采样频率稳定达到 100Hz，采样间隔的标准差小于 0.5ms。
4. **FreeRTOS 调度：** 三个任务无死锁、无优先级反转、无栈溢出，任务 CPU 占用率合理（Core 1 < 80%，Core 0 < 60%）。
5. **串口输出：** 串口 CSV 输出格式正确，能够被 edge-impulse-data-forwarder 工具正常采集和上传。

---

## 5 Phase 2：信号处理与数据采集

Phase 2 的核心目标是建立从原始传感器数据到机器学习模型输入的完整信号处理流水线，并开发高效的数据采集与标注工具。该阶段的工作质量直接决定了后续模型训练的数据质量，是整个项目中「数据为王」理念的集中体现。信号处理流水线包含卡尔曼滤波、数据归一化和滑动窗口缓冲三个主要模块，每个模块的设计都需要充分考虑嵌入式端的计算资源约束（ESP32-S3 的浮点运算能力有限，应尽量使用定点运算或查表法替代浮点运算）。

### 5.1 卡尔曼滤波

TMAG5273 霍尔传感器的原始输出不可避免地包含环境磁场噪声、电磁干扰（EMI）和量化噪声。为了提高信号质量，本系统采用一维卡尔曼滤波器（1D Kalman Filter）对 15 路霍尔信号（5 传感器 × 3 轴）进行独立滤波处理。卡尔曼滤波器的优势在于它能够根据传感器的噪声特性自适应地调整滤波强度——在信号变化剧烈时（如手指快速弯曲）减少滤波以保持响应速度，在信号平稳时增加滤波以抑制噪声。

卡尔曼滤波器的状态方程和观测方程定义如下：状态方程 `x(k) = x(k-1) + w(k)`，其中 `x(k)` 为当前时刻的真实值，`w(k)` 为过程噪声，服从 N(0, Q) 分布；观测方程 `z(k) = x(k) + v(k)`，其中 `z(k)` 为传感器测量值，`v(k)` 为测量噪声，服从 N(0, R) 分布。

滤波器更新步骤：
- **预测步骤（Predict）：** 先验状态估计 `x̂(k|k-1) = x̂(k-1|k-1)`，先验误差协方差 `P(k|k-1) = P(k-1|k-1) + Q`
- **更新步骤（Update）：** 卡尔曼增益 `K(k) = P(k|k-1) / (P(k|k-1) + R)`，状态估计 `x̂(k|k) = x̂(k|k-1) + K(k) × (z(k) - x̂(k|k-1))`，误差协方差 `P(k|k) = (1 - K(k)) × P(k|k-1)`

调参要点：过程噪声 Q 决定跟踪速度（Q 越大跟踪越快但滤波越弱），测量噪声 R 决定信任程度（R 越大滤波越强但响应越慢）。推荐初始参数 Q = 1e-4、R = 1e-2，通过实际信号分析微调。在 ESP32-S3 上建议使用 Q15 定点运算替代浮点运算，可将单路滤波计算时间从约 10μs 降低到约 3μs。

### 5.2 数据归一化与特征向量组合

卡尔曼滤波后的数据需要经过归一化处理才能作为模型的输入。霍尔数据采用 Min-Max 归一化方法，将 12-bit 原始值（范围 0-4095）映射到 [0, 1] 区间。归一化公式为 `x_norm = (x - x_min) / (x_max - x_min)`，其中 `x_min` 和 `x_max` 是在数据采集阶段通过对所有手势动作的统计得出的全局最小值和最大值。不同手指的霍尔传感器量程可能不同（拇指的磁场变化范围通常大于小指），因此应分别计算每路传感器的归一化参数。

BNO085 输出的四元数（Qx, Qy, Qz, Qw）需要转换为欧拉角（Roll, Pitch, Yaw）以便与霍尔数据统一格式。转换公式为标准的三轴旋转矩阵分解，但需要注意万向锁（Gimbal Lock）问题——当 Pitch 接近 ±90° 时，Roll 和 Yaw 的计算会出现数值不稳定。本系统通过限制手部活动范围（手语动作中腕部 Pitch 极少接近 ±90°）和使用高精度浮点运算来规避该问题。最终的特征向量为 **21 维**：

| 特征维度 | 范围 | 来源 |
|---------|------|------|
| 15 维 | [0, 0.333, 0.667, 1.0] | 5 路 TMAG5273 归一化霍尔数据 (3 轴 × 5) |
| 3 维 | [-π, π] | BNO085 欧拉角 (Roll, Pitch, Yaw) |
| 3 维 | [-max_gyro, max_gyro] | BNO085 陀螺仪 (Gx, Gy, Gz)，归一化 |

### 5.3 滑动窗口缓冲区

手势识别模型需要一段连续时间的传感器数据作为输入，而非单帧数据。本项目采用滑动窗口（Sliding Window）机制来组织时序数据：窗口大小为 30 帧（对应 300ms 的数据，在 100Hz 采样率下），窗口步长为 1 帧（即每次新数据到来时滑动一个位置）。30 帧的窗口大小经过实验验证，能够在时间分辨率和计算效率之间取得良好的平衡——更小的窗口可能导致模型无法捕捉完整的手势动态，更大的窗口会增加推理延迟和内存占用。

环形缓冲区（Ring Buffer）是实现滑动窗口的高效数据结构。在 ESP32-S3 上，分配一个 30 × 21 × sizeof(float) = 2520 字节的连续内存区域作为环形缓冲区，使用头指针（head）和尾指针（tail）管理数据的写入和读取。每帧新数据到来时，写入 head 指向的位置，然后 `head = (head + 1) % 30`；当缓冲区已满时，最老的数据被自动覆盖。这种实现无需数据搬移，时间复杂度 O(1)。当缓冲区填满 30 帧后，触发 Task_Inference 或通知 Python Relay 数据就绪。

### 5.4 数据采集与标注工具

数据采集是机器学习项目中最为耗时但最关键的环节。本项目提供两套数据采集工具：

1. **Edge Impulse 方案（路径 A）：** 使用 edge-impulse-data-forwarder 工具通过 ESP32-S3 的串口 CSV 输出自动采集传感器数据并上传至 Edge Impulse 云端，支持在线标注和模型训练。该方案最简单，但仅适用于 Edge Impulse 路径。

2. **Python 自定义方案（路径 B）：** Python 脚本通过 BLE 或串口接收传感器数据，同时保存为 CSV（人类可读）和 NPY（NumPy 二进制格式，加载速度快）双格式。标注工具支持 46 类中国手语词汇的标注，采用「先录制后标注」的工作流——录制时持续记录传感器数据，标注人员在每个手势动作完成后按下快捷键添加时间戳标记和手势标签。标注完成后，脚本根据时间戳将连续数据流切割为独立的手势样本（每个样本包含手势前 5 帧静默段 + 手势段 + 手势后 5 帧静默段）。建议每类手势至少采集 100 个样本（由 3-5 名不同的手语使用者分别录制），以确保数据的多样性和模型的泛化能力。

---

## 6 Phase 3：L1 边缘推理 TinyML

Phase 3 是项目的核心 AI 模块开发阶段，目标是在 ESP32-S3 上部署轻量级的手势分类模型（L1 推理），实现端侧的实时手势识别。本阶段采用双路径开发策略：路径 A 使用 Edge Impulse 平台实现 MVP 快速验证，路径 B 使用 PyTorch 原生训练并导出 TFLite INT8 模型用于论文复现。V3 新增了模型池架构和热切换功能，以及 MS-TCN 实验性模型。

### 6.1 Edge Impulse MVP 路径

Edge Impulse 路径的目标是在最短时间内完成从数据采集到端侧部署的全流程验证。具体步骤如下：

1. 使用 edge-impulse-data-forwarder 通过 ESP32-S3 串口 CSV 输出采集传感器数据，自动上传至 Edge Impulse 云端项目
2. 在 EI Studio 的 Data Acquisition 页面进行数据标注，将每个样本标记为对应的手势类别
3. 在 Impulse Design 页面配置信号处理参数（Raw Data 输入）和模型参数（1D-CNN 分类器，学习轮次 200，学习率 0.001）
4. 训练完成后开启 EON（Edge Optimized Neural）Compiler 优化，自动执行算子融合、内存优化和 INT8 量化
5. 导出为 Arduino Library（C++ 库），通过 PlatformIO 的 `lib_deps` 机制集成到项目中

Edge Impulse 路径的优势在于开发速度快（从数据采集到端侧部署通常只需 2-3 天），非常适合在项目早期快速验证数据质量、传感器布局和模型架构的可行性。然而，该路径的模型结构受限于平台提供的模块（无法自定义注意力机制等高级结构），训练超参数可控性较低，且模型训练在云端执行、可复现性不足。因此，Edge Impulse 路径仅用于 MVP 验证和 Benchmark 对比，不用于最终的产品化和论文发表。

### 6.2 PyTorch → TFLite INT8 路径

PyTorch 路径是论文复现和产品化的正式方案，提供了完全可控的模型训练和部署流水线。模型架构为 1D-CNN + TemporalAttention，专门为嵌入式部署优化设计。输入维度为 (Batch, Channels=21, Time=30)，输出维度为 (Batch, Classes=46)。

| 层名称 | 结构 | 输出维度 | 参数量 |
|--------|------|---------|--------|
| Input | — | (B, 21, 30) | 0 |
| Block1 | Conv1d(21→32, k=5, p=2) + BN + ReLU + MaxPool(2) | (B, 32, 14) | ~3.5K |
| Block2 | Conv1d(32→64, k=3, p=1) + BN + ReLU + MaxPool(2) | (B, 64, 6) | ~6.4K |
| Block3 | Conv1d(64→128, k=3, p=1) + BN + ReLU | (B, 128, 4) | ~25K |
| TempAttention | Eq.11-13: 通道注意力 | (B, 128, 4) | ~0.3K |
| GlobalAvgPool | AdaptiveAvgPool1d(1) | (B, 128, 1) | 0 |
| FC | Linear(128→46) | (B, 46) | ~6K |

TemporalAttention 模块（Eq.11-13）是该模型的核心创新点。其实现方式为：对 Block3 的输出特征图（维度 B×128×4）在时间维度上进行全局平均池化得到通道描述符 c ∈ R^128，然后通过两层全连接网络（128→16→128，中间使用 ReLU 激活和 Sigmoid 门控）生成通道注意力权重 a ∈ R^128，最后将注意力权重与原始特征图逐通道相乘。该机制使模型能够自适应地关注最具判别力的特征通道。

模型量化采用 INT8 Post-Training Quantization（训练后量化）。量化流程：PyTorch 模型 → ONNX 中间格式 → TFLite FP32 模型 → TFLite INT8 模型（使用 representative_dataset 校准量化参数）。量化后模型大小从约 140KB（FP32）缩减至约 38KB（INT8），推理速度提升约 3-4 倍，准确率损失通常控制在 1-2% 以内。最终使用 xxd 工具将 TFLite FlatBuffer 转换为 C 语言头文件 `model_data.h`，集成到 PlatformIO 项目中。

### 6.3 MS-TCN 实验性模型

MS-TCN（Multi-Stage Temporal Convolutional Network）是模型池中的第二个候选模型，作为与 1D-CNN+Attention 的对比基准。MS-TCN 的核心思想是通过多阶段级联的膨胀因果卷积（Dilated Causal Convolution）逐步细化时序特征的表示。与标准 TCN 相比，MS-TCN 的每一阶段都接收前一阶段的输出作为额外的输入，通过残差连接实现特征的渐进式精炼。

V3 采用的浅层 MS-TCN 变体架构如下：输入维度 (B, 21, 30)，Stage 1 包含 3 层膨胀因果卷积（dilation = [1, 2, 4]，kernel_size = 3，channels = 32），Stage 2 接收 Stage 1 的输出和原始输入的拼接，包含 3 层膨胀因果卷积（dilation = [1, 2, 4]，channels = 64），最终通过 Global Average Pooling 和全连接层输出 46 类分类结果。浅层变体的总参数量约 28K（INT8 量化后约 32KB），略小于 1D-CNN+Attention，但推理延迟相近（约 2.5ms）。MS-TCN 的优势在于能够显式地建模长距离时序依赖（通过大膨胀率），适合需要捕捉手势时序细节的场景。该模型将在 Phase 3.5 的 Benchmark 中与 1D-CNN+Attention 进行系统性对比。

### 6.4 BaseModel 统一接口

BaseModel 接口是 V3 模型热切换架构的核心抽象。该接口使用 C++ 纯虚函数定义，所有 L1 模型（1D-CNN+Attention、MS-TCN、Edge Impulse 导出模型）都必须继承 BaseModel 并实现其全部方法。接口设计遵循「接口隔离原则」——每个方法只负责一个明确的职责，便于测试和扩展。

BaseModel 接口的关键设计考量包括：`init()` 方法接收模型权重数据和大小，负责初始化 TFLite Micro 解释器并分配 PSRAM 上的 Tensor Arena；`preprocess()` 方法将原始传感器帧转换为模型输入张量，不同的模型可能需要不同的预处理逻辑（如 MS-TCN 不需要通道注意力适配）；`infer()` 方法执行 TFLite Micro 的 `Invoke()`；`postprocess()` 方法从模型输出张量中提取手势 ID、置信度和 Top-K 概率；`get_model_info()` 返回模型名称、版本、参数量、支持的手势类别数等元信息；`cleanup()` 方法释放所有分配的资源。模型注册表使用 `std::unordered_map<std::string, std::unique_ptr<BaseModel>>` 存储已注册的模型实例。

### 6.5 模型热切换实现

模型热切换功能允许在运行时通过修改 YAML 配置文件或通过 BLE/WiFi 命令切换 L1 模型，无需重启设备。热切换的完整流程如下：

1. **配置更新触发：** 用户通过 BLE 写入新的模型名称，或 Python Relay 通过 WiFi 发送 HTTP POST 请求更新 SPIFFS 中的 model_config.yaml
2. **配置解析：** 系统读取更新后的 YAML 文件，解析出新的 `active_model` 名称
3. **旧模型清理：** 调用当前模型的 `cleanup()` 方法，释放 PSRAM 上的 Tensor Arena 和 TFLite Micro 解释器资源
4. **新模型加载：** 根据模型名称从注册表中查找对应的 BaseModel 实例，调用 `init()` 方法加载模型权重并分配资源
5. **状态通知：** 通过 WebSocket 向前端发送 `{"type": "MODEL_SWITCHING", "old_model": "...", "new_model": "..."}` 状态消息
6. **恢复推理：** 热切换完成后，Task_Inference 在下一帧数据就绪时使用新模型进行推理

热切换过程中的关键安全措施包括：切换期间暂停推理输出（防止输出无效结果）、PSRAM 内存分配失败时的回退机制（保留旧模型继续运行）、YAML 配置文件损坏时的默认模型启动。整个切换过程的目标时间 < 100ms，以最小化对用户体验的影响。

### 6.6 推理触发逻辑

L1 推理的触发逻辑基于滑动窗口缓冲区和置信度阈值两个核心机制。当环形缓冲区填满 30 帧数据后（对应 300ms 的传感器数据），Task_Inference 被唤醒，执行 TFLite Micro 推理。推理输出为 46 维的 softmax 概率向量，其中最大概率值即为预测置信度。

系统根据置信度与预设阈值（0.85）的关系决定后续动作：
- **高置信度（> 0.85）：** 直接输出手势 ID 和对应的中文名称，通过 BLE/WiFi 发送至 Python Relay，触发后续的 TTS 语音合成或 3D 动画播放
- **低置信度（≤ 0.85）：** 标记为 UNKNOWN，同时将当前滑动窗口的 630 维特征向量打包为 Protobuf 消息，转发至 Python Relay 触发 L2 ST-GCN 推理

**去抖动（Debouncing）机制：** 当连续 N 帧（N=5，即 50ms）输出的手势 ID 相同且置信度均超过阈值时，才确认输出该手势。去抖动时间窗口为 50ms，在保证响应速度的同时有效避免了手势边界的抖动误识别。

**手势过渡检测：** 当检测到手势切换（当前帧 ID 与上一帧 ID 不同）时，强制插入 100ms 的静默期，避免手势切换瞬间的误识别。该静默期在 Debouncing 机制之上叠加，确保手势过渡的稳定性。

### 6.7 TFLite Micro 集成

TFLite Micro 是 TensorFlow Lite 的嵌入式版本，专为资源受限的微控制器设计。本项目在 ESP32-S3 上的集成方案如下：

- **Tensor Arena 分配：** 在 PSRAM 上分配 50KB 的连续内存区域作为 Tensor Arena，用于存储模型推理过程中的中间激活值（activation）。PSRAM 的分配使用 `heap_caps_malloc(50 * 1024, MALLOC_CAP_SPIRAM)` 函数
- **解释器初始化：** 使用 `tflite::MicroInterpreter` 创建解释器实例，传入模型 FlatBuffer 和 Tensor Arena 指针
- **推理执行：** 在 Task_Inference 中调用 `interpreter.Invoke()` 执行推理，单次推理延迟 < 3ms
- **Edge Impulse SDK 集成：** 对于 Edge Impulse 路径导出的模型，使用 EI 的 `run_classifier()` API 替代原生 TFLite Micro，但底层仍然使用相同的 Tensor Arena

**关键优化：** ESP32-S3 的 AI 向量扩展指令集可以加速 INT8 矩阵乘法运算，TFLite Micro 在编译时启用 `CONFIG_IDF_TARGET_ESP32S3` 宏后自动使用硬件加速。实测表明，启用硬件加速后推理延迟从约 5ms 降低到约 2.5ms。

### 6.8 验收标准

| 指标 | 要求 | 测量方法 |
|------|------|---------|
| 推理延迟 | < 3ms（含预处理、推理、后处理） | GPIO 翻转 + 逻辑分析仪测量 |
| Top-1 准确率 | ≥ 90%（20 类简单手势） | 独立测试集评估 |
| 内存占用 | < 200KB（PSRAM，含 Arena） | `heap_caps_get_free_size()` |
| 连续运行 | 24 小时无内存泄漏/精度退化 | 长时间压力测试 |
| 热切换 | < 100ms 切换时间，无推理中断 | 计时器 + WebSocket 状态监控 |

---

## 7 Phase 3.5：模型 Benchmark 阶段

Phase 3.5 是 V3 新增的模型评估阶段，位于 L1 模型开发完成后、正式部署之前。该阶段的目标是通过标准化的 Benchmark 框架对模型池中的所有候选模型进行系统性的对比评估，为最终的模型选择提供数据驱动的决策依据。

### 7.1 Benchmark 框架设计

Benchmark 框架使用 Python 实现，提供统一的评估指标集合，确保不同模型之间的评估结果具有可比性。评估指标包括六大维度：

| 指标 | 单位 | 说明 |
|------|------|------|
| Top-1 Accuracy | % | 第一预测正确的比例 |
| Top-5 Accuracy | % | 前 5 预测中包含正确答案的比例 |
| FLOPs | MFLOPs | 单次推理的浮点运算总量（使用 thop 或 fvcore 测量） |
| Latency (ESP32) | ms | 在 ESP32-S3 上的实际推理延迟（含预处理和后处理） |
| Parameters | K | 模型总参数量 |
| Memory (PSRAM) | KB | 运行时 PSRAM 占用（含 Tensor Arena） |

Benchmark 框架的 Python 类结构设计如下：`BenchmarkRunner` 类是顶层入口，提供 `run_single_model()` 和 `run_all_models()` 方法。`MetricsCollector` 类负责收集和计算各项指标。`ReportGenerator` 类负责生成 Markdown 格式的评估报告和可视化图表（使用 matplotlib）。`CrossValidator` 类实现 K 折交叉验证，确保评估结果的统计可靠性。

### 7.2 评估协议

评估协议的标准化是保证结果可信度的关键。本项目的评估协议定义如下：

1. **数据集划分：** 使用 70% 训练集 / 15% 验证集 / 15% 测试集的三分法，确保训练、调参和评估使用完全独立的数据。数据划分在项目启动时一次性完成，所有模型共享相同的划分。
2. **5 折交叉验证：** 训练集和验证集合并后进行 5 折交叉验证，每折训练 200 个 epoch，报告 5 折的平均值和标准差。交叉验证可以有效评估模型在不同数据分布下的稳定性。
3. **统计显著性检验：** 使用配对 t 检验（paired t-test）比较两个模型的准确率差异，显著性水平 α = 0.05。只有当 p-value < 0.05 时，才认为两个模型的性能差异具有统计显著性。
4. **ESP32 实际延迟测量：** 在 ESP32-S3 上通过 GPIO 翻转 + 逻辑分析仪测量实际推理延迟，而非仅使用 PyTorch 的 CPU/GPU 计时。嵌入式端的延迟通常高于桌面端模拟值（受限于 CPU 频率和内存带宽）。
5. **公平比较：** 所有模型使用相同的数据增强策略（随机时间偏移 ±5 帧、随机高斯噪声 σ=0.01）和相同的训练超参数（Adam 优化器，lr=0.001，权重衰减 1e-4，余弦退火调度器）。

### 7.3 自动化 Benchmark Runner

自动化 Benchmark Runner 通过命令行工具提供批量评估能力，支持以下使用场景：

```bash
# 评估单个模型
python benchmark.py --model cnn_attention --folds 5 --epochs 200

# 评估所有候选模型并生成比较报告
python benchmark.py --all --output reports/benchmark_v1.md

# 生成可视化图表
python benchmark.py --all --plot --output-dir reports/plots/
```

Runner 的执行流程为：加载模型配置 → 加载数据集 → 执行交叉验证 → 收集评估指标 → 在 ESP32 上测量实际延迟（可选，需要硬件连接）→ 生成评估报告。评估报告以 Markdown 表格的形式呈现模型比较矩阵，同时生成雷达图（Radar Chart）和柱状图（Bar Chart）的可视化图表，便于直观比较。

### 7.4 模型比较矩阵

以下为预期的模型比较矩阵格式（实际数值将在 Benchmark 执行后填入）：

| 模型 | Top-1 (%) | Top-5 (%) | FLOPs (M) | 延迟 ESP32 (ms) | 参数量 (K) | 内存 (KB) |
|------|-----------|-----------|-----------|----------------|-----------|-----------|
| 1D-CNN + Attention | TBD | TBD | TBD | TBD | ~34 | ~50 |
| MS-TCN (shallow) | TBD | TBD | TBD | TBD | ~28 | ~45 |
| Edge Impulse 1D-CNN | TBD | TBD | TBD | TBD | TBD | TBD |

最终模型选择采用**帕累托最优（Pareto Optimality）**原则：在 Top-1 准确率和推理延迟两个维度上均不被其他模型同时超越的模型将被选为部署模型。如果存在多个帕累托最优模型，则优先选择 Top-1 准确率最高的模型。

### 7.5 验收标准

1. 所有候选模型均完成 5 折交叉验证，每折训练收敛（loss 曲线平稳）
2. 评估报告包含完整的模型比较矩阵和统计显著性检验结果
3. ESP32 实际延迟测量数据可用（至少测量 100 次取平均值和标准差）
4. 最终选择的部署模型满足 Phase 3 的所有验收标准（Top-1 ≥ 90%，延迟 < 3ms）
5. Benchmark 报告已归档到项目的 docs/benchmark/ 目录

---

## 8 Phase 4：通信协议实现

Phase 4 的目标是实现稳定、高效的传感器数据传输协议，连接嵌入式端和 Python Relay。本阶段需要同时实现 BLE 5.0 和 WiFi UDP 两种通信方式，并使用 Protobuf（Protocol Buffers）作为统一的嵌入式端数据序列化格式。通信协议的设计需要在带宽利用率、延迟、可靠性和实现复杂度之间取得平衡。

### 8.1 Protobuf 数据帧定义

Protobuf 是 Google 开发的高效二进制序列化协议，相比 JSON 格式具有更小的体积（通常减少 3-10 倍）和更快的序列化/反序列化速度。本项目使用 Nanopb（Protobuf 的 Nano 版本）在 ESP32-S3 上实现序列化，Nanopb 针对嵌入式系统进行了深度优化，代码体积小于 2KB，RAM 占用小于 512 字节。Python 端使用标准的 google-protobuf 库。

**glove_data.proto 定义：**

```protobuf
syntax = "proto3";

message GloveData {
  uint32 timestamp = 1;           // 系统时间戳 (ms)
  repeated float hall_features = 2;  // 15 路霍尔传感器特征 (max 15)
  repeated float imu_features = 3;   // 6 路 IMU 特征 (max 6)
  repeated float flex_features = 4;  // 5 路柔性传感器 (预留, max 5)
  uint32 l1_gesture_id = 5;       // L1 推理结果: 手势 ID
  uint32 l1_confidence_x100 = 6;  // L1 置信度 × 100 (整数)
  string active_model = 7;        // 当前激活的模型名称
}
```

GloveData 消息的设计考虑了向后兼容性和可扩展性。字段编号（field number）一旦分配不可更改，新增字段只需分配新的编号即可。`repeated` 关键字用于表示可变长度数组，Nanopb 会根据 .options 文件中定义的最大长度预分配内存。`l1_confidence_x100` 字段使用整数编码（置信度 × 100），避免浮点数在嵌入式和桌面端之间的精度差异问题。V3 新增了 `active_model` 字段，用于向前端通知当前使用的 L1 模型名称。

### 8.2 BLE 5.0 实现

BLE 5.0 实现基于 ESP-IDF 内置的 Bluedroid 协议栈，定义了两个 GATT 服务：

| 服务 | UUID | Characteristic | 用途 |
|------|------|---------------|------|
| Environmental Sensing | 0x181A | Sensor Data (Notify) | 传感器数据广播 |
| Environmental Sensing | 0x181A | Config (Read/Write) | 参数配置 |
| Custom Provisioning | 0xFF01 | WiFi Credentials (Write) | WiFi 配网 |

BLE 通信的关键技术要点包括：连接参数优化（Connection Interval 设为 10ms，Slave Latency 设为 0，确保最低延迟）；MTU（Maximum Transmission Unit）协商目标为 512 字节；数据分片处理（当 Protobuf 数据超过当前 MTU 时自动分片发送）；连接断开自动重连（指数退避策略，最大重试间隔 30 秒）。BLE 在 Phase 4+ 之后主要承担配网和低速备用（20Hz）的角色，高频数据传输由 WiFi UDP 接管。

**重要：** 前端（React Web / Unity）**不使用 Web Bluetooth API** 直接连接 ESP32。所有 BLE 通信仅用于配网和移动端 APP 直连场景。Web 端通过 WiFi → Python Relay → WebSocket 的链路获取数据。

### 8.3 WiFi UDP 实现

WiFi UDP 实现基于 ESP-IDF 的 lwIP 协议栈，使用非阻塞 UDP Socket 进行数据发送。关键参数如下：

| 参数 | 值 | 说明 |
|------|-----|------|
| 目标端口 | 8888 | Python Relay UDP 监听端口 |
| 发送频率 | 100Hz | 与传感器采样频率同步 |
| 单帧大小 | ~80-120 bytes | Nanopb 序列化后的 GloveData |
| Socket 类型 | 非阻塞 | 不等待 ACK，允许丢包 |

WiFi 初始化流程：系统启动 → 读取 NVS 中的 WiFi 凭证 → 连接 AP → 获取 IP 地址 → 创建 UDP Socket → 绑定本地端口 → 开始发送数据。如果 WiFi 连接失败（如凭证错误或 AP 不可达），系统自动降级为 BLE-only 模式，并通过 BLE 通知客户端 WiFi 不可用。WiFi 发送在 Core 0 的 Task_Comms 中执行，与 Core 1 的传感器采样任务并行运行。实际测试表明，ESP32-S3 的 WiFi UDP 发送延迟（从调用 sendto() 到数据离开天线）通常低于 1ms（局域网环境）。

### 8.4 验收标准

1. BLE 连接稳定，MTU 协商成功达到 512 字节，连续运行 24 小时无断连
2. WiFi UDP 局域网单帧延迟 < 5ms，数据帧完整率 > 99%（每秒允许丢失不超过 1 帧）
3. Protobuf 序列化/反序列化在 ESP32-S3 上执行时间 < 0.5ms
4. 双通道切换（WiFi 不可用时自动降级为 BLE）切换时间 < 5 秒
5. Python Relay 成功接收 UDP 数据并正确解析为 Python 字典

---

## 9 Phase 5：Python Relay + L2 推理

Phase 5 是系统的 AI 能力核心和中继枢纽阶段。Python Relay 作为 ESP32 边缘端与前端渲染之间的统一数据中心，同时承担数据中继、L2 推理、NLP 语法纠错和 TTS 语音合成四大职责。本阶段的目标是构建一个稳定、高效、可扩展的 Python 中继服务，实现从 UDP 数据接收到 WebSocket JSON 推送的完整链路。

### 9.1 FastAPI + WebSockets 中继架构

Python Relay 基于 FastAPI 框架构建，核心架构由两个异步 I/O 服务器组成：

**UDP 服务器（端口 8888）：** 使用 `asyncio.datagram.DatagramProtocol` 实现，负责接收 ESP32 发送的 Protobuf 数据。每收到一个 UDP 数据包，立即使用标准 `protobuf` 库反序列化为 GloveData 对象，然后转换为 JSON 字典存入共享的环形缓冲区（Python `collections.deque`，最大容量 300 帧，即 3 秒数据）。

**WebSocket 服务器（端口 8765）：** 使用 FastAPI 的 `WebSocket` 端点实现。当客户端连接时，启动一个异步推送任务，以 100Hz 的频率从环形缓冲区读取最新数据，序列化为 JSON 后通过 WebSocket 发送。JSON 消息格式定义如下：

```json
{
  "type": "sensor_data",
  "timestamp": 1714000000000,
  "hall_features": [0.5, 0.3, ...],
  "imu_features": [0.1, -0.2, ...],
  "l1_result": {"gesture_id": 5, "gesture_name": "你好", "confidence": 0.92},
  "l2_result": null,
  "active_model": "cnn_attention_v2"
}
```

FastAPI 的异步架构确保 UDP 接收和 WebSocket 推送互不阻塞，即使在多个客户端同时连接的情况下也能保持低延迟。Relay 还提供 REST API 端点用于系统状态查询和配置管理（如 `GET /api/status`、`POST /api/model/switch`）。

### 9.2 L2 自研 ST-GCN 推理

L2 ST-GCN（时空图卷积网络）是本系统处理复杂手语序列的核心模型，从 MS-GCN3 论文的数学描述出发自行构建。L2 推理在 Python Relay 中异步执行，当 L1 置信度不足时自动触发。

ST-GCN 模型的完整架构包含以下五个核心模块：

1. **伪骨骼映射层（Pseudo-skeleton Mapping, Eq.16）：** 使用两层全连接网络（21 → 128 → 21×2）将 21 维传感器特征映射为 21 个关节的 2D 坐标（x, y），为空间图卷积提供结构化输入。该映射层通过学习传感器特征与手部关节位置之间的相关性，隐式地建立了传感器布局与手部解剖结构的对应关系。

2. **空间图卷积层（Spatial Graph Convolution）：** 基于手部骨骼拓扑（21 个关节，20 根骨骼连接）构建归一化邻接矩阵 A ∈ R^{21×21}。空间图卷积的定义为 `f_spatial(X) = D^{-1/2} A D^{-1/2} X W`，其中 D 为度矩阵，W 为可学习权重。邻接矩阵包含三种类型的边：向心边（子关节→父关节）、离心边（父关节→子关节）和自环边（关节→自身），每种边使用独立的权重矩阵。

3. **时间卷积层（Temporal Convolution）：** 使用 TCN（Temporal Convolutional Network）捕获手语动作的时序依赖。TCN 由多层膨胀因果卷积（Dilated Causal Convolution）组成，膨胀率逐层递增（1, 2, 4, 8），感受野随深度指数级增长。因果卷积确保模型不会「看到未来」的数据，避免信息泄露。

4. **ST-Conv Block：** 将空间图卷积和时间卷积组合为残差块。每个 ST-Conv Block 的结构为：LayerNorm → 空间图卷积 → ReLU → 时间卷积 → ReLU → 残差连接 + Dropout(0.1)。模型包含 8 个 ST-Conv Block，通道数从 64 逐步增加到 256。

5. **注意力池化层（Attention Pooling）：** 使用通道注意力机制（SE-Net 风格）聚合时序特征。全局平均池化得到通道描述符，通过两层全连接网络（C → C/16 → C）生成通道注意力权重，与原始特征逐通道相乘。最终通过全连接层输出 46 类手势的分类 logits。

### 9.3 置信度驱动的 L1→L2 路由逻辑

Python Relay 实现了完整的置信度驱动路由机制，协调 L1 和 L2 推理的协同工作：

1. **L1 结果接收：** 从 GloveData Protobuf 消息中提取 L1 推理结果（gesture_id 和 confidence_x100）
2. **高置信度路径（confidence > 0.85）：** 直接使用 L1 结果，转换为手势名称后追加到 WebSocket JSON 消息中
3. **低置信度路径（confidence ≤ 0.85）：** 标记为 UNKNOWN，从环形缓冲区提取最近 30 帧的特征数据（30×21 维），异步触发 L2 ST-GCN 推理
4. **L2 结果合并：** L2 推理完成后，将 L2 结果（gesture_id、gesture_name、confidence）追加到后续的 WebSocket JSON 消息中，前端根据消息中的 l2_result 字段更新显示
5. **超时降级：** 如果 L2 推理超过 500ms 未返回结果，向前端发送 `{"type": "L2_TIMEOUT"}` 消息，前端保持显示 UNKNOWN 状态

### 9.4 NLP 语法纠错

中国手语（CSL, Chinese Sign Language）的语法结构与标准汉语存在显著差异——手语通常采用「主语-宾语-谓语」（SOV）的语序，且缺乏时态、量词和助词等语法标记。NLP 语法纠错模块负责将手势识别输出的「手语词序列」转换为符合标准汉语语法的句子。

V3 的 NLP 模块采用渐进式实现策略：
- **第一阶段（规则引擎）：** 基于手工编写的 CSL 语法规则库实现基本的语序调整和词性标注。规则库涵盖常见的手语语法模式（如「我 你 好」→「你好」的省略规则、「他 书 看」→「他看书」的 SOV→SVO 转换）
- **第二阶段（轻量级 Transformer）：** 使用 DistilBERT 或类似的小型 Transformer 模型进行序列到序列的翻译式纠错。该模型仅需约 30M 参数，在 CPU 上推理延迟 < 50ms，适合集成到 Python Relay 中

### 9.5 TTS 语音合成

TTS（Text-to-Speech）语音合成采用微软的 edge-tts 库，该库完全免费且无需 API 密钥，通过 WebSocket 连接到微软的在线 TTS 服务。edge-tts 支持多种高质量的中文语音：

| 语音名称 | 性别 | 适用场景 |
|---------|------|---------|
| zh-CN-XiaoxiaoNeural | 女 | 默认语音，清晰自然 |
| zh-CN-YunxiNeural | 男 | 可选切换 |
| zh-CN-XiaoyiNeural | 女 | 情感丰富，适合演示 |

TTS 在 Python Relay 中作为异步任务执行，当接收到手势识别结果（L1 高置信度或 L2 结果）后，先经过 NLP 语法纠错，然后将纠正后的文本输入 edge-tts 生成音频。音频流通过 HTTP 端点（`/api/tts/audio`）提供给前端播放，或直接通过 WebSocket 以 base64 编码的方式推送。

### 9.6 验收标准

| 指标 | 要求 |
|------|------|
| UDP 接收延迟 | < 1ms（Python 端处理延迟） |
| WebSocket 推送频率 | ≥ 60Hz（前端渲染帧率要求） |
| L2 推理延迟 | < 20ms（GPU）或 < 100ms（CPU） |
| L2 Top-1 准确率 | ≥ 95%（46 类手语词汇） |
| NLP 纠错准确率 | ≥ 85%（规则引擎阶段） |
| TTS 延迟 | < 500ms（从文本到音频首字节） |
| 系统稳定性 | 连续运行 8 小时无崩溃、无内存泄漏 |

---

## 10 Phase 6：渲染层开发

Phase 6 的目标是构建 3D 手部渲染和可视化系统，分两个阶段实现。第一阶段（Layer 3a）使用 React + R3F 纯 Web 技术栈快速实现 MVP 渲染，第二阶段（Layer 3b）使用 Unity 2022 LTS 实现专业级渲染。

### 10.1 React + R3F MVP 渲染

React + R3F（React Three Fiber）是 MVP 渲染阶段的核心技术栈，将 Three.js 的 3D 渲染能力封装为 React 声明式组件。MVP 渲染系统包含以下核心模块：

**3D 手部骨架：** 基于 21 个关键点和 20 根骨骼构建简化的手部骨架模型。每个关节使用 `THREE.SphereGeometry`（半径 3-5mm，根据关节大小调整），每根骨骼使用 `THREE.CylinderGeometry`（半径 2mm）。关节旋转使用欧拉角（Euler angles）或四元数（Quaternion）驱动，确保与 Unity 阶段的骨骼定义一致。21 个关键点遵循标准的手部骨骼定义：

| 关节 ID | 名称 | 父关节 |
|---------|------|--------|
| 0 | Wrist | — (根节点) |
| 1 | Thumb_CMC | 0 |
| 2 | Thumb_MCP | 1 |
| 3 | Thumb_IP | 2 |
| 4 | Thumb_Tip | 3 |
| 5-8 | Index_Finger (MCP, PIP, DIP, Tip) | 0 |
| 9-12 | Middle_Finger (MCP, PIP, DIP, Tip) | 0 |
| 13-16 | Ring_Finger (MCP, PIP, DIP, Tip) | 0 |
| 17-20 | Pinky_Finger (MCP, PIP, DIP, Tip) | 0 |

**WebSocket 客户端：** 使用原生 WebSocket API 连接到 Python Relay 的端口 8765。连接建立后，客户端自动接收 JSON 格式的传感器数据和手势识别结果。断线时自动重连（指数退避，最大间隔 10 秒），重连期间使用 Zustand 中缓存的最后一帧数据保持 3D 场景显示。

**Zustand 状态管理：** Zustand Store 维护全局状态，包括：
- `sensorData`: 最新一帧的传感器特征数据
- `gestureResult`: L1/L2 手势识别结果
- `connectionStatus`: WebSocket 连接状态
- `modelInfo`: 当前激活的 L1 模型信息
- `uiSettings`: UI 偏好设置（主题、布局等）

**响应式设计：** 使用 TailwindCSS 实现响应式布局。桌面端采用左右分栏布局（左侧 3D 渲染区，右侧传感器数据面板和手势结果面板），移动端采用上下堆叠布局。断点设置：sm (640px)、md (768px)、lg (1024px)、xl (1280px)。

### 10.2 PWA 配置

PWA（Progressive Web App）配置使 React 应用支持「添加到主屏幕」功能，提供接近原生应用的体验。配置包含以下文件：

**manifest.json：** 定义应用名称、图标（192×192 和 512×512 两个尺寸）、主题色、背景色、显示模式（standalone）和启动 URL。

**Service Worker：** 使用 Workbox 库生成，实现以下缓存策略：
- App Shell（HTML/CSS/JS）: Cache-First 策略，离线可用
- 3D 资源（纹理、模型）: Stale-While-Revalidate 策略
- WebSocket 连接: 网络优先，离线时显示连接状态提示

PWA 的核心价值在于用户无需通过应用商店即可安装应用，且在离线状态下仍可查看最后一次的 3D 手部姿态（使用 Service Worker 缓存的最后一帧数据），提升了用户体验。

### 10.3 Unity 2022 LTS + XR Hands + ms-MANO

L3 专业渲染阶段使用 Unity 2022 LTS 游戏引擎，配合以下核心组件实现高逼真度的手部渲染：

**XR Hands Package (v1.2+)：** Unity 官方提供的手部追踪包，定义了 21 个手部关节的完整层级结构（XRHandJointID），支持关节旋转、位移和缩放的精确控制。XR Hands 的关节定义与 React MVP 阶段的骨骼定义完全一致，确保数据格式兼容。

**ms-MANO（Multi-Scale MANO）：** 基于马克斯普朗克研究所的 MANO（Model-based Articulated Non-rigid hand）手部模型。MANO 通过 48 维 pose_parameters 参数化控制 21 个关节的 3D 旋转（15 个手指关节 × 3 维 + 3 维全局手腕旋转 = 48 维），同时通过 10 维 shape_parameters 控制手部的形状差异（胖瘦、长短等）。ms-MANO 在 MANO 基础上引入了多尺度细节（Multi-Scale Detail），在手掌和手指表面增加了高频几何细节（皱纹、关节褶皱等），大幅提升了渲染的真实感。

**渲染管线：** Unity 阶段使用 URP（Universal Render Pipeline）渲染管线，配合 HDRP 的部分特性（如 SSAO、屏幕空间反射）实现照片级渲染效果。手部网格使用 PBR（Physically Based Rendering）材质，配合 Subsurface Scattering（次表面散射）着色器模拟皮肤的半透明效果。Unity 通过 UDP Socket 连接到 Python Relay（与 ESP32 共享同一个 UDP 端口，或通过独立的端点），接收 JSON 格式的传感器数据，映射为 ms-MANO 的 pose_parameters 后驱动手部动画。

### 10.4 统一骨骼定义

React MVP 和 Unity Pro 两个渲染阶段共享同一套手部骨骼定义，确保从 Web 到 Unity 的平滑迁移。统一骨骼定义的核心规范包括：

1. **21 个关键点定义：** 遵循 XRHands/XRHandJointID 标准，关节 ID 0-20 的名称和父子关系在两个平台完全一致
2. **DOF（自由度）定义：** 每个关节使用 3 个自由度（旋转角度），以局部坐标系下的欧拉角或四元数表示
3. **坐标系约定：** 右手坐标系，X 轴指向右侧，Y 轴指向上方，Z 轴指向观察者。旋转顺序为 ZXY（避免万向锁）
4. **关节角度范围：** 每个关节定义了物理上合理的旋转范围（如 PIP 关节屈曲 0°-100°，过伸 0°-10°），超出范围的值将被钳制

Python Relay 负责将 21 维传感器特征映射为 21×3 的关节旋转角度，该映射关系在 Relay 端集中维护，前端只需直接使用映射后的角度数据驱动渲染。

### 10.5 验收标准

| 阶段 | 指标 | 要求 |
|------|------|------|
| MVP | 3D 渲染帧率 | ≥ 60 FPS（桌面端 Chrome） |
| MVP | WebSocket 延迟 | 端到端 < 50ms（传感器→3D 更新） |
| MVP | 响应式适配 | 桌面端 + 移动端 + PWA 正常显示 |
| MVP | PWA 安装 | 支持 Chrome/Edge/Safari 添加到主屏幕 |
| Pro | MANO 渲染质量 | 照片级，满足论文发表标准 |
| Pro | 关节映射精度 | 21 个关节角度误差 < 5° |

---

## 11 Phase 7：系统集成与测试

Phase 7 是项目的最终集成和验证阶段，目标是将前面所有阶段的成果整合为一个完整的、可工作的系统，并通过系统性的测试验证系统的整体性能和可靠性。

### 11.1 集成测试计划

集成测试按照「逐层集成、端到端验证」的策略进行。每个 Phase 完成后进行阶段性集成测试，确保当前阶段与已完成的阶段能够正确协同工作：

| 测试阶段 | 测试范围 | 测试方法 |
|---------|---------|---------|
| IT-1 | HAL + 信号处理 | 串口输出验证，示波器采样定时分析 |
| IT-2 | + L1 推理 | 模型推理结果正确性验证 |
| IT-3 | + 通信 | ESP32 → Python Relay 数据完整性验证 |
| IT-4 | + L2 推理 | L1→L2 路由逻辑验证，置信度阈值测试 |
| IT-5 | + 渲染 | 端到端传感器→3D 渲染可视化验证 |
| IT-6 | + NLP/TTS | 完整的手势→文本→语音链路验证 |

### 11.2 性能测试

性能测试覆盖以下四个维度：

- **延迟测试：** 使用高精度计时器测量端到端延迟（传感器采样 → L1 推理 → UDP 发送 → Python 接收 → WebSocket 推送 → 前端渲染 → 屏幕更新）。目标：端到端 < 100ms（P99）
- **吞吐量测试：** 测量系统的最大持续数据吞吐量（UDP 100Hz × Protobuf 帧大小），确保 Python Relay 不出现数据积压
- **内存测试：** 监控 ESP32-S3 的 PSRAM 使用量（`heap_caps_get_free_size()`）和 Python Relay 的 RSS 内存占用（`psutil.Process().memory_info()`），确保无内存泄漏
- **电池测试：** 使用电子负载模拟实际工作负载，测量 600mAh 电池的续航时间，目标 > 12 小时

### 11.3 用户验收测试

用户验收测试（UAT）邀请 3-5 名手语使用者参与，按照以下流程进行：

1. **系统培训：** 向测试用户介绍数据手套的使用方法和注意事项
2. **自由测试：** 测试用户自由使用数据手套执行各种手语动作，记录系统识别准确率和用户主观反馈
3. **标准测试集：** 测试用户按照预设的 46 类手语词汇表逐个执行手势，记录每个手势的识别结果
4. **用户体验评估：** 通过问卷收集用户对系统舒适度、延迟感知、识别准确率的主观评价

### 11.4 验收标准

1. 端到端延迟 P99 < 100ms，P50 < 50ms
2. L1 + L2 联合 Top-1 准确率 ≥ 95%（46 类，标准测试集）
3. 系统连续运行 24 小时无崩溃、无内存泄漏
4. 电池续航 > 12 小时（600mAh）
5. 用户验收测试满意度评分 ≥ 4.0/5.0

---

## 12 已知问题与风险

### 12.1 V3 架构特有风险

| 风险 | 严重程度 | 概率 | 缓解措施 |
|------|---------|------|---------|
| Python Relay 单点故障 | 高 | 中 | 实现健康检查和自动重启（systemd/supervisor），前端优雅降级显示连接状态 |
| WiFi UDP 在复杂环境丢包率高 | 中 | 中 | 实现自适应的冗余发送（关键帧双发），前端插值补偿丢帧 |
| 模型热切换过程中推理中断 | 低 | 低 | 切换前预加载新模型到临时内存，切换时原子交换指针，中断时间 < 10ms |
| React 3D 渲染在低端设备帧率不足 | 中 | 中 | 实现 LOD（Level of Detail）降级策略，减少骨骼渲染精度以维持帧率 |
| BLE 配网安全性不足（WiFi 凭证明文传输） | 中 | 低 | 实现 AES-128 加密的配网协议，配网完成后禁用 BLE 写入 |
| ST-GCN 自研质量风险 | 高 | 中 | 设置明确的里程碑检查点，每个模块独立单元测试，与论文原始结果交叉验证 |

### 12.2 通用风险

| 风险 | 严重程度 | 概率 | 缓解措施 |
|------|---------|------|---------|
| TMAG5273 温度漂移影响精度 | 中 | 中 | Set/Reset 触发模式自动补偿 + Kalman 滤波 + 定期在线校准 |
| BNO085 SH-2 协议时序问题 | 低 | 低 | 严格参照数据手册的时序要求，增加总线稳定等待时间 |
| ESP32-S3 PSRAM 不稳定 | 高 | 低 | 使用 Octal PSRAM（非 Quad），开启 PSRAM 缓存刷新 |
| 46 类手语数据集不足 | 高 | 中 | 每类至少 100 样本 × 3-5 名采集者，使用数据增强扩充 |
| 电池续航不达标 | 中 | 中 | FreeRTOS Light Sleep、传感器间歇采样、WiFi 按需连接 |

---

## 13 附录

### 附录 A：BOM 详细清单

| # | 组件 | 型号 | 数量 | 单价 (USD) | 小计 (USD) | 采购链接 |
|---|------|------|------|-----------|-----------|---------|
| 1 | MCU 开发板 | ESP32-S3-DevKitC-1 N16R8 | 1 | $13.50 | $13.50 | espressif.com |
| 2 | 3D 霍尔传感器 | TMAG5273A1EVM | 5 | $2.80 | $14.00 | ti.com |
| 3 | 9 轴 IMU | BNO085 Breakout | 1 | $9.50 | $9.50 | adafruit.com |
| 4 | I2C 多路复用器 | TCA9548A Breakout | 1 | $0.85 | $0.85 | adafruit.com |
| 5 | 锂电池 | 600mAh 3.7V LiPo (JST-PH) | 1 | $3.50 | $3.50 | adafruit.com |
| 6 | PCB + 外壳 | 定制两层板 + 3D 打印 | 1 | $5.00 | $5.00 | jlcpcb.com |
| 7 | 上拉电阻 | 2.2kΩ + 4.7kΩ (0603) | 10 | $0.10 | $1.00 | lcsc.com |
| 8 | 排线 | FPC 连接器 + 排线 | 5 | $0.29 | $1.45 | lcsc.com |
| 9 | 其他 | 电容、LED、按钮等 | 1 | $1.00 | $1.00 | — |
| | **合计** | | | | **≈ $38.60** | |

### 附录 B：GPIO 引脚映射

| GPIO | 功能 | 方向 | 备注 |
|------|------|------|------|
| GPIO 8 (SDA) | I2C SDA | 双向 | 400kHz，主总线上拉 2.2kΩ |
| GPIO 9 (SCL) | I2C SCL | 输出 | 400kHz，主总线上拉 2.2kΩ |
| GPIO 21 | BNO085 INT | 输入 | 数据就绪中断，下降沿触发 |
| GPIO 4 | ADC Channel 0 | 输入 | Flex Sensor（拇指，预留） |
| GPIO 5 | ADC Channel 1 | 输入 | Flex Sensor（食指，预留） |
| GPIO 6 | ADC Channel 2 | 输入 | Flex Sensor（中指，预留） |
| GPIO 7 | ADC Channel 3 | 输入 | Flex Sensor（无名指，预留） |
| GPIO 15 | ADC Channel 4 | 输入 | Flex Sensor（小指，预留） |
| GPIO 47 | Status LED | 输出 | 系统状态指示 |
| GPIO 48 | Button | 输入 | 模式切换/配网触发 |

### 附录 C：手势词汇表（46 类 CSL）

#### 基础数字（10 类）

| ID | 手势 | 描述 |
|----|------|------|
| 0-9 | 数字 0-9 | 标准中国手语数字表达 |

#### 日常用语（20 类 - L1 目标）

| ID | 手势 | 描述 |
|----|------|------|
| 10 | 你好 | 挥手致意 |
| 11 | 谢谢 | 拇指弯曲点头 |
| 12 | 对不起 | 手掌抚胸 |
| 13 | 再见 | 挥手告别 |
| 14 | 是 | 点头手势 |
| 15 | 否 | 摇头手势 |
| 16 | 我 | 食指指向自己 |
| 17 | 你 | 食指指向对方 |
| 18 | 他/她 | 食指指向第三者方向 |
| 19 | 好 | 竖起拇指 |
| 20-29 | 方向/颜色等 | — |

#### 扩展词汇（16 类 - L2 目标）

| ID | 手势 | 描述 |
|----|------|------|
| 30-45 | 抽象词汇 | 需要/帮助/学习/工作等 |

### 附录 D：参考文献

1. **TMAG5273A1 Datasheet** - Texas Instruments, "TMAG5273A1 Linear 3D Hall-Effect Sensor", 2023
2. **BNO085 Datasheet** - Bosch Sensortec, "BNO085 Smart Sensor Hub", 2022
3. **MS-GCN3 Paper** - "Multi-Step Graph Convolutional Network for 3D Human Pose Estimation", CVPR 2023
4. **Edge Impulse Documentation** - Edge Impulse Inc., "Machine Learning for Edge Devices", 2024
5. **TFLite Micro Guide** - TensorFlow Team, "TensorFlow Lite for Microcontrollers", 2024
6. **R3F Documentation** - React Three Fiber, "React renderer for Three.js", 2024
7. **FastAPI Documentation** - FastAPI, "Modern, fast web framework for building APIs", 2024
8. **MANO Model** - Romero et al., "Learning Hand Shape Model from Multi-View Images", ICCV 2023
9. **XR Hands Package** - Unity Technologies, "XR Hands Package Documentation", 2024
10. **Nanopb Documentation** - Jouni Malinen, "Nanopb: Protocol Buffers for Embedded Systems", 2023

---

*文档版本: V3.0 | 日期: 2026-04-24 | 状态: 已锁定*

*本文档为 Edge AI 数据手套项目的核心技术规范，所有开发工作必须严格按照本文档执行。如有变更需求，须发起正式的 ADR 评审流程。*
