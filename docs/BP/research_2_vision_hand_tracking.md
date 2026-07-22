# 竞品研究 (2/4) — 视觉手部追踪方案 (纯 CV 路线)

> 来源: 并行研究Agent, 2026-07-23. 已核实官方源, 硬指标缺失项标"未公开", 未编造。

## 6.1 商业/平台级视觉手部追踪方案

| 方案 | 传感器 | 关键点/DoF | 延迟/帧率 | 精度 | 专用硬件 | 遮挡鲁棒性 | 接触力 | 接口/SDK | 价格/授权 | 开源 | 致命局限 |
|---|---|---|---|---|---|---|---|---|---|---|---|
| Google MediaPipe Hands | RGB 单目 | 21 关键点/手 (z相对深度) | CPU 17ms/GPU 12ms (Pixel6) ~58-82fps | 未公开 | 否 | 中等 | 无 | MediaPipe Tasks (全平台) | Apache2.0 免费 | 是 | z非绝对6DoF; 无力; 遮挡退化; 依赖光照 |
| Ultraleap (Gemini/Leap) | IR双目立体 | 27关节/手 26+DoF | 未公开 (~30Hz) | 未公开 (近场亚mm级报告) | 是 (LMC2/3Di) | 中等 (IR主动照明近场) | 无 | Unity/Unreal Plugin | 硬件~$139+SDK免费 | 否 | 专用IR硬件; 视场有限; <60cm; 无力 |
| Meta Quest Hand Tracking | IR/单色+RGB透视 | 21关节 24DoF | 未公开; Fast Motion 60Hz; 运动-光子~60-100ms | 未公开 | 是 (Quest) | 中等 | 无 | Meta Movement/Interaction SDK, OpenXR | 头显+SDK免费 | 否 | 仅Quest生态; FOV内; 无全局6DoF; 无力 |
| Apple Vision Pro | RGB+IR阵列 | 26关节/手 6DoF/关节 | 未公开 (~15-25ms 三方估) ~60Hz | 未公开 (亚度级三方称) | 是 (VP头显) | 中-高 | 无 | visionOS ARKit HandSkeleton | $3499+ | 否 | 仅VP; FOV外丢失; 闭源; 无力 |
| Apple ARKit (iOS) | RGB (+LiDAR可选) | iOS无独立手部API; ARBodyAnchor全身91关节 | 60fps | 未公开 | 否 (A12+) | 中等 | 无 | ARKit | 免费 | 否 | iOS无原生手关节追踪; 无力; 闭源 |
| Move AI / Move One | RGB (iPhone单目/Pro多相机) | 全身动捕 (非手部专注) | 未公开 (云端非实时) | 未公开 | 否/是 | 中等 | 无 | Move Platform导出多格式 | $0-$490/月按credit | 否 | 非实时; 手指精度有限; 无力; 无实时6DoF; 按秒计费 |

## 6.2 标准/接口层

| 方案 | 关键点/DoF | 精度 | 接触力 | 接口 | 开源 | 致命局限 |
|---|---|---|---|---|---|---|
| OpenXR Hand Tracking API | 26关节/手 每关节6DoF+速度 | 未公开 (典型5-15mm无标准保证) | 无 (仅pose; capsule仅几何) | C API xrLocateHandJointsEXT | 是 (Apache2.0/MIT) | 仅接口层无算法; 无力; 绝对6DoF依赖头显SLAM; 强依赖头显 |
| Unity XR Hands 1.7 | 26关节 (OpenXR布局) | 未公开 | 无 | C# XRHandSubsystem | 半开源 (Unity Companion) | 纯API层无实现; 无力; 依赖头显; 耦合Unity; 已知bug; 无独立部署 |

## 6.3 学术/开源融合方案

| # | 项目 | 任务 | 方法 | 关键点 | 精度 | 开源 | 接触力 | 局限 |
|---|---|---|---|---|---|---|---|---|
| 1 | GlobalPose (SIGGRAPH2025) | 全局人体姿态 | 6IMUs+物理优化 | SMPL 24j 72DoF | 论文内有数 | 是 | 无 | 仅Win/Py3.8; 无手部; 无力 |
| 2 | RobustCap (SIGGRAPH Asia2023) | 实时人体姿态 | 单目RGB+6 IMUs融合 | SMPL 24j 72DoF | 论文有数 | 是(MIT) | 无 | 无手部; 无力 |
| 3 | MAMMA (CVPR2026 Oral) | 多人无标记mocap | 多视角视觉+SMPL-X | SMPL-X含手 | 待发布 | 是(非商业) | 无 | 需多视角阵列; 离线处理 |
| 4 | HOI-DETR (arXiv2606.17384) | 手-物交互检测 | Transformer端到端 | 边界框 | Hands23 val AP50 86.1 vs 63.6 | 是(MIT) | 无 | 无时间建模; 仅检测无姿态/力 |
| 5 | OpenArm (Enactic) | 开源人形机械臂遥操 | 7DoF臂+VR遥操 | 7DoF/臂无手指 | 未报告 | 是(Apache2.0/CERN-OHL) | 部分(双边力反馈) | 非手部追踪器; $6500; 开发中 |
| 6 | Supervision (Roboflow) | CV工具包 | 模型无关连接器 | 无 | N/A | 是(MIT 48.3k★) | 无 | 工具包非追踪器 |
| 7 | Ego-Pi | VLA微调(自我中心迁移) | 人+机器人数据共训Pi0.5 | 每手29维双手58维 | 分拣37/40包装9/10装箱14/15 | 否 | 无 | 短视程简单拾放; 固定摄像头 |
| 8 | VirtuSync | 实时面+眼+手(VTuber) | Unity+MediaPipe | MediaPipe landmarks | 未报告 | 是(MIT) | 无 | 仅VRM0.x; 无评估 |
| 9 | HandTrackingWithUnity-Mediapipe | Unity手部追踪 | Unity+Python+MediaPipe | ~21 | 未报告 | 是(MIT) | 无 | 有bug; 无更新; 无基准 |
| 10 | UnityHandTrackingWithMediapipe | 实时手+手指(Unity) | MediaPipe on Android | 21 landmarks | 未报告 | 是(Apache2.0 322★) | 无 | 需USB; 仅Android; 无评估 |
| 11 | OpenHands (AI4Bharat) | 手语识别 | 基于姿态神经网络 | MediaPipe | 文档无数值 | 是(Apache2.0 不再维护) | 无 | 不再维护; 非追踪器; 无力 |

## 6.4 纯CV方案普遍局限小结 (EchoGlove glove+vision融合切入逻辑)

四类结构性硬伤, 恰对应EchoGlove差异化切入:

1. **接触力感知=0 (全域空白)**: 所有方案仅输出pose, 无一能测抓握力/捏合压力。OpenArm力反馈是机器人侧反推非人手本体。EchoGlove flex+IMU触觉层**直接补齐接触力全域空白**——机器人示教/遥操/康复最强差异点。

2. **遮挡脆弱性 (universal weakness)**: 指-指/手-物遮挡致所有CV退化或丢失。EchoGlove flex+IMU是**本体感知(proprioceptive)**, 不受光照/遮挡影响, 视觉丢失时维持关节估计——天然回退/融合源。

3. **无绝对6DoF世界位姿**: MediaPipe z是相对掌深; Quest/VP锚定头显坐标系; Move One输出body-relative BVH。**无一能给出接地世界空间6DoF手部位姿**。EchoGlove IMU+外部基准融合可提供绝对全局位姿。

4. **光照依赖+延迟-精度权衡+闭源生态**: RGB弱光/强光失效; IR需照明且<60cm; 云端非实时; Apple/Meta绑头显生态。EchoGlove三层推理(S3<3ms L1)独立硬件不依赖头显, 本地闭环。

5. **本体状态 vs 外部观测本质差异**: 纯CV是"看手"(external observation), 受视角/FOV/光照/遮挡约束; EchoGlove是"是手"(proprioceptive state), 关节角/角速度/接触力是直接物理量。**测量模态差距, 非参数优化差距, 无法被CV算法迭代弥补**。

**投资人核心结论**: 纯CV手部追踪是被巨头(Google/Meta/Apple/Ultraleap)+学术圈充分竞争的红海, 但**接触力、遮挡期连续性、绝对6DoF、本体感知**四维度是纯CV物理天花板。EchoGlove glove+vision融合不是"又一个手部追踪方案", 而是**补齐CV路线物理盲区的模态融合方案**——切入机器人遥操/远程示教/医疗康复等对接触力和位姿连续性有硬需求的产业场景。

## 关键来源
- MediaPipe: https://developers.google.com/edge/mediapipe/solutions/vision/hand_landmarker
- Ultraleap: https://docs.ultraleap.com/ (HTTP403, 建议供应商书面确认)
- Meta Quest: https://developers.meta.com/horizon/documentation/unity/unity-handtracking-overview/
- Apple: visionOS HandSkeleton / ARKit ARBodyAnchor
- Move One定价: https://docs.move.ai/knowledge/move-one-pricing
- OpenXR 1.1: https://registry.khronos.org/OpenXR/specs/1.1/html/xrspec.html
- Unity XR Hands 1.7: https://docs.unity3d.com/Packages/com.unity.xr.hands@1.7/manual/
- GlobalPose: github.com/Xinyu-Yi/GlobalPose · arXiv:2505.05010
- RobustCap: github.com/shaohua-pan/RobustCap · arXiv:2309.00310
- MAMMA: github.com/cuevhv/mamma · arXiv:2506.13040
- HOI-DETR: github.com/AhmadDarKhalil/HOI-DETR · arXiv:2606.17384
- OpenArm: github.com/enactic/openarm (注: openarmA 404, 正确为 openarm)
- Supervision: github.com/roboflow/supervision
- EgoPi: egopipaper.github.io
- VirtuSync / HandTrackingWithUnity-Mediapipe / UnityHandTrackingWithMediapipe: 见对应github
- OpenHands: github.com/AI4Bharat/OpenHands

## 投资人注意事项
1. Ultraleap/Meta/Apple官方**均不发布**mm/°精度与端到端延迟硬指标, 流传数字均为第三方估算。标"未公开"项经官方源核实, 未编造。建议涉数值比较附供应商书面确认。
2. iOS ARKit**无独立手部关节追踪API**, 仅visionOS有26关节HandSkeleton; iOS ARBodyAnchor是全身91关节骨架非手部专用。
3. `enactic/openarmA` 404, 正确为 `enactic/openarm`。
4. OpenArm是11项目中唯一接触感知系统(双边力反馈), 但为机器人臂非手部追踪器——反向印证接触力感知在纯追踪方案中的全域空白。
