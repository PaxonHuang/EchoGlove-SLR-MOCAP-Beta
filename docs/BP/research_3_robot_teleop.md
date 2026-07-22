# 竞品研究 (3/4) — 机器人遥操作/数据采集平台 + 算法栈

> 来源: 并行研究Agent, 2026-07-23. 硬指标来自官方GitHub/项目页/arXiv。这是EchoGlove主航道(具身智能遥操作)最重要的竞争区。

## 一、遥操作/数据采集平台横向对比

| # | 平台 | 形态 | 输出格式 | 人类本体感觉 | 力/触觉 | 成本(USD) | 数据规模/开源 | VLA/DP/ACT兼容 | 接口 |
|---|---|---|---|---|---|---|---|---|---|
| 1 | UMI (Columbia/TRI/MIT) | 手持夹爪+GoPro视觉 | 6DoF末端位姿(ORB_SLAM3)+夹爪开合; Zarr | 否 | 否 | ~$2,195(BOM公开) | 多场景zarr; 开源MIT | Diffusion Policy原生 | PyTorch; 无ROS2 |
| 2 | ALOHA/Mobile ALOHA (Stanford) | 双臂主从机械臂(4×ViperX300)+底盘 | 关节角(14DoF双臂+底盘); HDF5 | 否 | 否 | 静态~$20k; Mobile~$32k | 每任务50demos; 开源MIT | ACT原生(act-plus-plus) | ROS1(ROS2测试中); PyTorch |
| 3 | GELLO | 3D打印桌面主控臂(Dynamixel) | 关节角(6-7DoF) | 否 | 部分(FACTR重力补偿) | <$300 | 12人×5任务; 开源MIT | 未公开(定位模仿学习采集) | ROS2 Humble(Franka FR3) |
| 4 | DexCap (Stanford) | **手套**(Rokoko EMF动捕)+胸前RGB-D | 3D指尖位置+6DoF手部位姿 | 否(动捕非本体感觉) | 否 | 未公开 | HuggingFace; 开源MIT | Diffusion Policy(点云,46维动作,20步) | PyTorch(robomimic) |
| 5 | AnyTeleop (Stanford) | 视觉(低成本RGB/RGBD) | 关节角(retarget_qpos, dex-retargeting) | 否 | 否 | 未公开(称low-cost) | BC 3K/5K/10K transitions; 开源(dex-retargeting MIT) | BC Policy | SAPIEN/IsaacGym, Pinocchio |
| 6 | LEAP Hand | 拟人机械手(16DoF,常作AnyTeleop目标) | 关节角(pos/vel/current≤500Hz) | N/A | 电流环(~300-550mA,可作力近似) | 未公开(业界~$2,000) | 开源(代码MIT, CAD CC BY-NC-SA非商用) | 兼容AnyTeleop/Manus; 有MANO→LEAP映射 | Python/C++/ROS/ROS2 |
| 7 | Baxter (Rethink,已停产) | 双臂(7DoF×2) | 关节角+末端6DoF | **是**(SEA扭矩反馈) | **是**(串联弹性致动器扭矩传感) | 原价~$22-30k; 二手<$5k | 无统一数据集; SDK BSD-3 | 研究栈通用 | ROS1 |
| 8 | Franka Panda遥操(DROID栈) | 单臂(7DoF) | 6DoF末端+gripper(7D) | 否 | **是**(7关节扭矩传感,可选F/T) | ~$30-40k | DROID 76k轨迹/350h; 开源 | DP/ACT/VLA均广泛兼容(事实标准) | ROS1/ROS2(FCI); PyTorch |
| 9 | Open TeleVision | VR(Apple Vision Pro)+人形(H1/GR-1) | 绝对关节位置(H1:28D; GR-1:19D) | 是(VR流式手/头/腕SE(3)@60Hz) | 否(论文明确缺haptic) | 未公开(AVP~$3,500+人形) | ~80episodes; 开源 | ACT(ResNet18→DinoV2,chunk60) | PyTorch; Pinocchio(IK) |
| 10 | TeleMoMa | 模块化视觉/VR/键鼠(RGB-D) | 6DoF末端delta+gripper+base速度(17D) | 否(MediaPipe视觉姿态) | 否 | $0-1,000(视觉模式仅需RGB-D) | 50-100demos/任务; 开源 | BC/BC-RNN | PyTorch(RoboMimic) |
| 11 | DeepMind RT-1数据采集 | 移动机械臂(Everyday Robots/Kuka) | 7D臂+3D底盘+1离散(11D token@3Hz) | 否 | 未公开 | 未公开(13台车队) | 130k+episodes,700+任务,17个月; 开源 | RT-1/RT-2(VLA) | TensorFlow |
| 12 | Open X-Embodiment (RT-X) | 多机器人(22种embodiment) | 7D(xyz rpy gripper) | 否 | 未公开 | 未公开 | 1M+轨迹,22机器人,21机构; 开源 | RT-2(VLA); 兼容DP/ACT(需转换) | RLDS/TFRecord; PyTorch loader |
| 13 | DROID | Franka Panda 7DoF | 6DoF末端delta+gripper(7D) | 否 | 未公开 | 未公开 | 76k轨迹/350h,564场景84任务,13机构; 开源CC BY 4.0 | DP/ACT原生; 兼容VLA | RLDS+HDF5; PyTorch |
| 14 | RH20T (SJTU) | 机械臂(6/7DoF,7构型)+触觉设备 | 关节角+TCP(xyz+quat 7D)+gripper | 否(含同步人类演示视频) | **是**—6DoF F/T@100Hz+指尖触觉阵列@200Hz | 未公开 | 110,000+序列,147任务; 开源(CC BY-SA/NC) | 兼容DP/ACT/VLA | .mp4+.npy+.json; PyTorch |
| 15 | Physical Intelligence π0 | 机械臂(7构型) | 关节角(max 18D,双臂+底座+躯干) | 否 | 否(仅RGB+语言+本体感觉) | 未公开 | ~10,000小时/903M timesteps,7构型68任务; **不开源** | VLA+Flow Matching(3B VLM+Action Expert) | 未公开 |

### 关键交叉发现
- **力/触觉全表普遍缺失**。仅Baxter(SEA扭矩)、Franka(关节扭矩)、RH20T(F/T+指尖触觉阵列)、LEAP(电流环近似)四家有真正力信号——**且全是机器人侧力传感**。
- **"手套形态+力/触觉"组合在公开生态中是空白**: DexCap是手套但无力触觉; AnyTeleop/TeleMoMa走纯视觉; TeleVision论文明确承认缺haptic。**EchoGlove的"可穿戴+本体感觉+力/触觉"三元组无直接竞品**。
- **关节角vs末端位姿分水岭**: π0/RH20T/GELLO/AnyTeleop/TeleVision输出关节角(与手套/外骨骼天然对齐); OXE/DROID/UMI输出末端位姿(需IK重映射)。
- **成本两极**: GELLO<$300、UMI~$2.2k、TeleMoMa~$1k为低成本档; ALOHA~$20-32k、Franka~$30-40k为中端; Mobile ALOHA+人形为高端。

## 二、算法栈(主流遥操作/VLA范式)

| 范式 | 输入→输出 | 数据需求 | 算力 | 与遥操作数据采集关系 |
|---|---|---|---|---|
| Diffusion Policy (RSS2023) | 图像/状态→动作序列(receding horizon ~8-16步) | ~50-200 demos/任务 | 单卡RTX3090/4090 | 纯模仿学习消费者,吃遥操作demos训策略 |
| ACT (RSS2023,配ALOHA) | 4×RGB@480×640+关节位→90步动作chunk@50Hz | ~50demos(10分钟)/任务 | 单卡可行 | ALOHA主从臂采集(图像+关节轨迹),ACT端到端学习 |
| VLA-RT-2 (DeepMind) | 图像+语言→动作token(7D) | 网络规模VQA+遥操作轨迹 | 12B-55B集群 | VLA预训练数据来自遥操作轨迹(OpenX) |
| VLA-OpenVLA (TRI/Stanford/Berkeley) | 图像+语言→连续动作(7DoF delta+gripper) | 970K OpenX预训练,LoRA微调 | 预训练64×A100/15天;微调单卡 | 开源权重(Apache2.0),可被遥操作数据微调 |
| VLA-π0 (Physical Intelligence) | 图像+文本+状态→电机指令@50Hz(flow matching) | OpenX+π Dataset(10k小时,8平台) | 3B未公开 | **当前最大私有遥操作数据集,证明遥操作数据规模是VLA上限决定因素** |
| WAM (World Action Model) | (state,action)→next state | off-policy(s,a,s')日志 | 单卡(DreamerV3)到集群 | 非公认命名范式,实质action-conditioned world model;遥操作轨迹是高质量(s,a,s')来源 |
| IK (逆运动学) | 笛卡尔位姿→关节配置(TRAC-IK/KDL) | 仅URDF | CPU实时(kHz) | 遥操作**前端**:人手/手套位姿→机器人关节轨迹(retargeting层) |
| MPC | 状态+参考+代价→控制序列(acados/OSQP) | 动力学模型 | CPU数十Hz-kHz | 遥操作**后处理**:平滑/约束化;采集时保证从臂安全跟踪 |
| RL (PPO/SAC/Dreamer/离线RL) | 状态→动作 | 百万级交互(仿真)或遥操作demos(离线RL/RLfD) | GPU集群 | 遥操作demos作RL初始化或离线RL数据;**自主化阶段** |

**栈层次结论**: 遥操作数据采集(UMI/ALOHA/DexCap/手套/VR+IK)位于栈**最前端**; ACT/DP是单任务模仿学习消费者; VLA(RT-2/OpenVLA/π0)是跨embodiment基础模型消费者; IK/MPC实时控制层; RL自主化延伸。**EchoGlove卡位在最前端——Human Data Capture Layer,向上喂给所有范式**。

## 三、EchoGlove定位小结

**定位**: 低成本可穿戴**Human Data Capture Layer**, 卡位具身智能栈最前端, 向上兼容ACT/Diffusion Policy/VLA全部主流范式。

### 相比UMI/ALOHA的优势
1. **成本断崖式下降**: UMI~$2,195, ALOHA~$20k, Mobile ALOHA~$32k, Franka~$30-40k; EchoGlove百美元级(对标GELLO<$300低成本档,但GELLO是桌面主控臂非可穿戴)。**π0已证明10000小时数据是VLA上限决定因素,低成本是规模化采集唯一路径**。
2. **可穿戴/移动性**: UMI需手持夹爪+GoPro背包, ALOHA固定双臂台架, Mobile ALOHA依赖~$32k底盘。EchoGlove直接穿戴人手, 操作者可在家庭/厨房/仓库/零售任意真实场景作业, 采集in-the-wild数据——UMI数据集核心价值主张, EchoGlove用更低成本+更自然交互实现。
3. **人类本体感觉+力/触觉(核心差异化)**: 公开生态**结构性空白**——15平台仅Baxter/Franka/RH20T/LEAP有力信号且全机器人侧; **无一家提供人手侧力/触觉流**; DexCap手套但无力触觉; TeleVision明确缺haptic。EchoGlove同时采集人手指尖触觉+关节角, 是训练**力控密集任务**(柔性物/接触-rich/精密装配)策略的稀缺数据(RH20T珍贵正因含F/T)。
4. **关节角天然对齐**: 与π0(关节角18D)/RH20T(关节角+TCP)/TeleVision(28D/19D)/GELLO直接对齐, 无需IK重映射(OXE/DROID/UMI末端位姿需IK转换)。
5. **同时覆盖双臂+灵巧手**: ALOHA仅双臂夹爪无灵巧手; DexCap仅手无臂; EchoGlove可同时覆盖手臂+灵巧手+触觉, 少数能支撑"人形机器人全身遥操作数据采集"。

### 相比UMI/ALOHA的劣势(实事求是)
1. **绝对精度不足**: UMI VI-SLAM毫米级, ALOHA关节编码器高精度; EchoGlove IMU+弯曲有累积漂移与标定误差, **不适合亚毫米级精密定位任务采集**, 需配外部视觉定位(RGB-D)校正。
2. **生态成熟度落后**: UMI/ALOHA/DexCap有Stanford/Columbia背书+活跃社区+配套数据集与策略代码(DP/act-plus-plus/robomimic)。EchoGlove需自建PyTorch dataloader/ROS2桥接/demo数据集/策略baseline, 生态冷启动成本高。
3. **无直接机器人本体**: UMI/ALOHA/GELLO/Franka是"采集即机器人可执行"闭环; EchoGlove采集人手运动, 必须经retargeting(IK/dex-retargeting)映射到具体机器人手, 存在embodiment gap。
4. **6DoF末端位姿需补充**: VLA主流动作空间(RT-2/OpenVLA/OXE/DROID)是7D末端位姿+夹爪非关节角。EchoGlove原生输出关节角, 兼容OpenX/DROID需提供关节角→末端转换工具链, 否则只能切π0/RH20T/TeleVision关节角路线。
5. **力/触觉标准化缺失**: OpenX/DROID主流数据集**根本无力/触觉字段**, EchoGlove力数据"无处安放"——需推动数据格式标准化(扩展RLDS schema)才能被生态吸收, 既是劣势也是卡位机会。

### BP用一句话定位
> EchoGlove是具身智能时代的"Human Data Capture Layer"——以百美元级可穿戴手套, 补齐当前UMI/ALOHA/DexCap生态缺失的"人手侧本体感觉+力/触觉"数据流, 为π0/OpenVLA/RT-2等VLA基础模型提供规模化、低成本、力控密集的高质量人类演示数据, 卡位具身智能人机交互入口与遥操作数据采集主航道。

## 来源
- UMI: github.com/real-stanford/universal_manipulation_interface
- ALOHA/Mobile ALOHA: mobile-aloha.github.io | arXiv:2401.02117
- GELLO: wuphilipp.github.io/gello_site/ | arXiv:2309.13037
- DexCap: github.com/j96w/DexCap | dex-cap.github.io
- AnyTeleop: yzqin.github.io/anyteleop/ | arXiv:2307.04577
- LEAP Hand: github.com/leap-hand/LEAP_Hand_API
- Open TeleVision: arXiv:2407.01512 | github.com/OpenTeleVision/TeleVision
- TeleMoMa: arXiv:2403.07869
- DROID: droid-dataset.github.io | arXiv:2403.12945
- Open X-Embodiment: robotics-transformer-x.github.io
- RH20T: rh20t.github.io
- π0: pi.website/blog/pi0 | arXiv:2410.24164
- RT-2: robotics-transformer2.github.io | arXiv:2307.15818
- OpenVLA: openvla.github.io | arXiv:2406.09246
- Diffusion Policy: diffusion-policy.cs.columbia.edu | arXiv:2303.04137
- ACT: tonyzhaozh.github.io/aloha/ | arXiv:2304.13705
