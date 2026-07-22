# 竞品研究 (1/4) — 数据手套/MOCAP手套竞品

> 来源: 并行研究Agent, 2026-07-23. 数值来自官方产品页/GitHub README, 未公开项标注, 未编造。

## 一、商用数据手套/MOCAP手套矩阵

| 产品 | 定位 | 传感器方案 | DoF/关节 | 采样率/延迟 | 精度 | 力反馈/触觉 | 接口 | 价格 | 开/闭源 | 用户 | EchoGlove空白点 |
|---|---|---|---|---|---|---|---|---|---|---|---|
| Manus Metagloves Pro | 企业/科研动捕, 机器人遥操, 具身AI数据 | 电磁场(EMF)追踪(非IMU/flex/光学), 无遮挡无漂移 | 25 DoF(解剖学全手) | 未公开(仅称最低延迟) | 定性"毫米级"无数值 | 仅Pro Haptic变体有触觉; 标准版无 | Manus Core3.0+SDK; Unity/Unreal/OpenXR/SteamVR/ROS2/Maya/MotionBuilder/Blender/OptiTrack/Xsens | Pro€4,500; Pro Haptic€4,500; Legacy Quantum€2,500 | 闭源 | 企业/科研 | 价格高; 无数据平台; 消费级缺位; 触觉仅高端变体 |
| SenseGlove Nova 2 | VR力反馈手套, 企业训练/虚拟原型/遥机器人 | 线缆伸缩弯曲(4指屈伸+拇指外展), 小指不追踪; 外部CV追踪 | 未公开(5路,4指) | 力反馈≤100Hz; 采样/延迟未公开(R1 1kHz外部) | 未公开(R1称毫米级) | 有,三合一:磁摩擦刹车4×≤20N/指+掌部主动压力带+拇食指音圈振动 | C#/C++ API, Unity, Unreal4; ROS/OpenXR/Python未公开 | Nova2 €6,299; 小批量€3,999; 学术折扣20% | API部分开源, 硬件闭源 | 企业/科研 | 小指不追踪; 缺数据集/平台; ROS/OpenXR缺位; 价位偏高 |
| HaptX Gloves G1 | 高端触觉手套, 企业VR训练/遥操/设计 | 专利磁式动捕+微流控触觉执行器 | 36 DoF/单手 | 追踪"无可感延迟"(无Hz/ms); 力反馈启用23ms | 0.3mm RMS位置分辨率 | 有,强:微流控阻力+振动; ≤178N/手, ≤35.6N/指, 135触觉执行器/手, 1.5mm触觉位移, 62kPa峰值, 刺激~75%手部受体 | HaptX SDK(Unity/Unreal), C++ API, ROS1&2; OpenXR未提 | 未公开(直销订阅; 2020报道G1开发套件~$5,000/双非官方) | 闭源(SDK对客户免费但专有) | 企业/科研 | 纯高端无消费级; 重且需外接; 无数据平台; 价格不透明 |
| CyberGlove III | 经典数据手套, 动捕/动画/人因 | 专利HyperSensor弯曲/flex+Active Palm Arch; 无IMU/无力 | 18或22传感器(DoF未明) | SD卡≤100Hz; USB/WiFi≤120Hz | 分辨率<1°; 重复性3°; 线性度≤0.6%SD | 无(纯输入; 触觉由CyberTouch/CyberGrasp承担) | VirtualHand SDK(C++); WiFi/USB/microSD; Unity/Unreal/ROS/OpenXR未公开 | 未公开(询价) | 闭源(专利) | 企业/科研 | 老旧SDK(官方OS仍列Win XP/Vista); 无现代引擎原生插件; 无数据平台; 无开源 |
| Rokoko Smartgloves II | 手指动捕, 动画/VR/AR/机器人/AI训练数据 | IMU(加速度+陀螺+磁力计)+可选Coil Pro EMF | 输出位姿39 DoF(Coil Pro 45); 骨骼25+ | IMU采样250Hz; 输出83.33Hz; 磁力计83.33Hz; 延迟未公开(称低延迟) | 定性"毫米精度"无数值 | 无 | Rokoko Studio+SDK; Unity/Unreal/Blender/Maya/ROS/OptiTrack/Qualisys/MOVIN; OpenXR未公开 | ¥375,000 JPY/双(约$2,500); USD未公开 | 闭源 | 企业/科研/专业半消费 | 无触觉; 无数据集产品化; OpenXR缺位; 价格中等 |
| Noitom 诺亦腾 PN3/PN Studio | 国产动捕, 动画/虚拟制作/生物力学/机器人科研 | IMU 9轴(PN3最多27传感器全身+手指; Studio弹性织物嵌入式IMU) | 手指关节DoF未公开(仅总传感器数) | PN3: 计算600Hz/输出60Hz/延迟<20ms; Studio: 100Hz/100Hz | PN3静态Roll1°/Pitch1°/Yaw2°; Studio Roll/Pitch0.7°/Yaw2.0°; 最小分辨率0.02° | 无 | Axis Studio(专有); Unity/Unreal/ROS/OpenXR未公开 | 未公开(询价400-107-8298) | 闭源 | 企业/科研/专业半消费 | 手指DoF不透明; 引擎插件不公开; 无数据平台; 价格不透明 |
| StretchSense | 柔性电容拉伸传感手套, 机器人AI/XR训练/游戏 | 专利超薄柔性电容拉伸传感器嵌入织物(无摄像头, 接触/自遮挡下可用) | 32传感器/手; "22+ DoF毫米精度"; 原生6DoF空间追踪 | 120Hz; 端到端延迟<10ms(设备端处理) | 0.6%追踪精度(营销页) | 无 | "APIs和SDKs", 平台无关, 设备端ML; ROS/Unity/Unreal/OpenXR未公开 | $895/双(预购消费价); 企业批量询价 | 闭源(专利) | 企业+消费 | 无触觉; SDK细节不透明; 无开源; 无数据集平台 |
| LucidVR/lucidgloves+opengloves-driver | DIY VR触觉手套, VR爱好者/创客 | 电位器+线轴张力机构(非flex/IMU); Proto4加伺服力反馈 | ~5指屈伸(5卷扬/张力器), DoF未明 | 未公开 | 未公开 | 有(Proto4: 5×9g舵机MG90S/SG90主动腱索力反馈) | SteamVR/OpenVR驱动(C++); 通讯:蓝牙串口(ESP32)/Named Pipes/串口USB; 无ROS/Unity/Unreal/OpenXR原生 | DIY BOM ~$60/双(目标价, 无逐件报价) | 开源MIT(2.4k★) | DIY/消费创客 | 无精度/延迟数据; 无数据平台; 仅VR输入; 工程化/产品化弱 |
| bHaptics TactGlove DK3 | 手部触觉手套, XR训练/医疗/国防/社交VR/HCI科研 | 无内置追踪, 依赖外部摄像头(如Quest3) | 未公开(追踪外部) | 延迟<20ms; 采样率未公开 | 未公开 | 有:7×高清LRA+1×高保真VCM(TITAN)/手; 8触觉点/手(16/双) | BLE5.0; SDK: Unity/Unreal/Python/JavaScript/Android | ¥54,900 | 闭源硬件 | 企业 | 纯触觉无传感/动捕; 价格高; 无数据平台 |

## 二、GitHub开源手套项目矩阵

| 仓库 | 目标 | 传感器 | MCU | DoF | 领域 | 接口 | 精度/延迟 | License | 空白点 |
|---|---|---|---|---|---|---|---|---|---|
| LucidVR/lucidgloves | DIY VR触觉手套+SteamVR | 电位器(线轴张力)+伺服力反馈 | Arduino Nano/ESP32 | ~5指(未明) | VR | 串口/BLE串口+opengloves-driver | 未公开 | MIT(2.4k★) | 无精度数据; 无数据平台; 工程化弱 |
| LucidVR/opengloves-driver | SteamVR/OpenVR驱动, 支持全指+splay+力反馈 | 硬件无关(驱动侧) | — | 全指+splay+逐关节 | VR | BT串口/Named Pipes/USB; SteamVR | 未公开 | MIT(518★) | 仅驱动层, 无硬件标准化 |
| farhanfuadabir/ASL-DataGlove | ESP32 ASL字母+词识别(14静态+3动态) | 5×flex+MPU6050(6轴IMU) | ESP32 | 11路(5flex+6IMU) | ASL | 串口(Pyserial)→Processing GUI; CSV录制 | 未公开 | MIT | 仅ASL; 无产品化; 无云端/平台 |
| Redgerd/Smart-Glove-Sign-Language-Predictor | ASL→文本+语音, 听障沟通 | 5×flex+MPU6050(6轴IMU) | Arduino Nano | 11路(5flex+6IMU) | ASL | BT→Flutter App+FastAPI后端; TTS | 准确率97%/精度98%/召回97%/F1 98%(~1.4万样本,80/20,3折CV) | 公开 | 仅ASL静态+少量动态; 无多模态; 无硬件产品化 |
| isurusasangaetam/Hand-Tracking-Glove | 实时动捕, VR/AR交互/手势识别 | MPU9250(9轴IMU)/指+掌, TCA9548A I2C多路 | ESP32 NodeMCU | 未公开(每MPU9250 9DoF) | VR/AR+手势 | Unity C#+Arduino .ino; I2C多路 | 未公开 | CC-BY-NC-4.0(7★) | 非商用许可; 无精度; 原型级 |
| ReikiC/CASA0018-Gloves-Edge-AI | 手语→文本 边缘ML(6手势) | 5×flex+LSM9DS1(9轴IMU) | Arduino Nano 33 BLE | 9轴IMU+5flex; 系统DoF未公开 | 手语识别(课程项目) | LCD2004 I2C显示; 串口115200 | 采样50Hz; 推理300ms; LCD200ms; 置信阈值0.60; 准确率未公开 | 学术开源 | 课程原型; 手势少; 无产品化 |
| weibayang/rehabilitation_glove | 便携软包康复手套, 闭环精细动作恢复 | 角度传感器(LED标定)+SMA形状记忆合金驱动 | STM32F407ZET6 | 未公开 | 手康复 | MATLAB GUI调试/采集; ST-Link/J-Link | 未公开; SMA经1万次循环验证 | MIT | 仅康复; 无多模态; 无数据平台; 精度未公开 |

## 三、EchoGlove相对空白点小结

基于9款商用+7款开源硬数据, 竞品格局空白:

1. **价格带断层, 中端消费级缺位**: 高端(HaptX/Manus/SenseGlove/bHaptics €4,500/$5,000+/¥54,900/询价)纯企业; 低端(LucidVR $60 DIY/StretchSense $895)工程化弱或偏高无触觉。**$200-800区间成品化+精度承诺消费级几乎无人占据**——EchoGlove定价此处填补最大空白。

2. **多模态融合普遍弱项**: 商用多单一传感(Manus=EMF, Rokoko/Noitom=IMU, CyberGlove=flex, StretchSense=电容, SenseGlove=线缆); 几乎无"flex+IMU+压力+触觉"多模态融合成品。开源ASL类多用"5flex+1IMU"基础组合。**EchoGlove做flex+IMU+压力(抓握力)+触觉多模态融合, 数据丰富度直接领先**。

3. **数据平台/数据集产品化几乎为零**: 所有竞品"卖硬件+SDK", 无一家把"采集→标注→训练数据集→模型评估"作产品闭环。ASL开源数据散落未平台化。**这是EchoGlove"硬件+数据平台"双轮驱动最大蓝海, 面向WRC/具身AI训练数据市场**。

4. **精度与延迟数据不透明**: Manus/SenseGlove/Rokoko/Noitom无数值化精度(仅定性毫米级)或延迟(仅Noitom PN3<20ms/StretchSense<10ms/bHaptics<20ms公开)。开源普遍无精度/延迟。**EchoGlove公开可复现精度/延迟基准即形成可信硬指标差**。

5. **引擎/生态接口碎片化**: OpenXR仅Manus明确支持; ROS仅HaptX/Manus/Rokoko明确。**EchoGlove同时提供Unity/Unreal/ROS/OpenXR/Python五件套即覆盖面超多数竞品**。

6. **应用场景单一**: 商用锁VR训练/动捕/遥操; 开源锁ASL或单点康复。**无一同时覆盖手势识别+康复+VR/MR+机器人遥操+数据采集多场景——EchoGlove多场景定位独有**。

7. **触觉反馈与传感分离**: 有触觉的(HaptX/SenseGlove/bHaptics/LucidVR)多纯输出或高价; 有传感的(CyberGlove/Rokoko/Noitom/StretchSense)多无触觉。**"传感+触觉"一体化中价位产品空白明显**。

8. **国产化与供应链**: Noitom虽国产但价格不透明/闭源/无数据平台; 其余均海外。**EchoGlove国产+开源+数据平台定位, 在京东/WRC/国产替代语境下无直接对手**。

## 数据可信度说明
所有数值来自官方产品页/GitHub README(2026-07-23抓取), 未公开项标注, 未编造。HaptX $5,000/LucidVR $60/SenseGlove €3,999-6,299/Rokoko ¥375,000 JPY/StretchSense $895/bHaptics ¥54,900为少数公开价格; Manus/Noitom/CyberGlove价格不透明, BP引用建议注明"询价/订阅制"。
