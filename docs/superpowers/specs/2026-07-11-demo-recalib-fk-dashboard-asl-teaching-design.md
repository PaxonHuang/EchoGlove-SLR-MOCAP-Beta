# 2026-07-11 Demo 重新校准 + 仪表盘 FK 动捕 + 手语教学 ASL 按钮设计

## 背景
现场演示误差严重。三处升级：(1) 重新交互校准 flex；(2) 仪表盘 keypoint-offset curl → 真 FK 关节链；(3) 手语教学页加 CSL+ASL 两组字符按钮。

用户决策（已确认）：先测 ch3 决定 4/6 字母；CSL+ASL 两组；仪表盘升级为 FK 关节链；仪表盘只显左手居中；教学页左画布+右按钮面板。全程中文回答；每完成阶段任务即更新 memory + git commit。

## Section 1 — 重新校准（ch3 测试 → 4 vs 6 字母）

**序列：**
1. 停 `demo_server.py`（PID 224912）释放 `/dev/ttyACM0`。
2. ch3 诊断：~15s 读 `$EG`，用户 open↔fist 数次，报告 5 通道 swing（raw count）。ch3 swing ≥ ~205 cnt（0.05 /4095）→ 6 字母可行；否则保持 4 字母。
3. 若 6 字母可行：patch `asl_classifier.py` — `ACTIVE_LETTERS=("A","B","I","L","W","Y")`、`ACTIVE_CHANNELS=(0,1,2,3,4)`、`CONF_SPAN`/距离注释、`save()` `active_channel_names`。4 字母则不动。
4. 运行 `calibrate_demo.py`：OPEN+FIST 范围 + 每字母模板，质量门 min inter-class dist ≥ 0.25，存 `demo_calibration.json`。
5. 重启 `demo_server.py`，验证自动加载新校准。

**理由：** `calibrate_demo.py` import 时即读取 `ACTIVE_LETTERS`，故 6 字母 patch 必须在 capture 前落地；ch3 诊断打破先后顺序僵局。

## Section 2 — 仪表盘 FK 关节链升级

**根因：** `useHandAnimation.ts` `applyFlexCurl` 用 keypoint 位置偏移（MCP 0.7×、PIP 2/3、DIP 1/3），非解剖学——指段不缩短而是旋转。教学页 `DemoHandRenderer` 已用正确 FK（嵌套 `<group rotation>` MCP→PIP→DIP）。修复：把 FK 链搬到仪表盘，由**实时校准 flex** 驱动。

**方案：提取共享 `FingerChain` + 两个薄渲染壳。** 不把 demo 动画 store 与 live sensor store 纠缠，不复制 FK 数学。

| 新/改 | 职责 |
|---|---|
| `components/Hand3D/FingerChain.tsx`（新） | 纯 FK 渲染：props `{jointAngles[15], wristQuat, mirror?}`。嵌套 MCP→PIP→DIP 绕局部 X 轴。复用 `SEGMENTS`/`REST_MCP`/`Bone`/`JointSphere`。 |
| `components/Hand3D/LiveHandRenderer.tsx`（新） | 仪表盘渲染。`useFrame` 每帧读 `useSensorStore.leftHand.flex`，算 `jointAngles`，渲染 `<FingerChain>`。左手居中，手腕固定（IMU=zeros→单位 quat，略前倾）。 |
| `hooks/useFlexToAngles.ts`（新） | `flexToJointAngles(flex[5]): number[15]` + 平滑 lerp 0.2 抖动。比例见下。 |
| `components/Hand3D/HandSkeleton.tsx` | 简化：仅 `<LiveHandRenderer>`，移除 RIGHT 切换 + keypoint-bone。 |
| `components/Hand3D/DemoHandRenderer.tsx` | 重构用共享 `<FingerChain>`（教学页行为不变）。 |

**Flex→15 关节角映射**（归一化 flex 0=straight..1=bent，度，线性）：
- index/middle/ring/pinky：MCP=flex×85, PIP=flex×105, DIP=flex×70
- thumb：CMC=flex×35, MCP=flex×55, IP=flex×75

拳头中 PIP 屈于 MCP 多（~105 vs ~85），DIP 最少（~70）；拇指对掌近似为 CMC 旋转。ch3/ring 硬件故障，该指在仪表盘也近静止（已知硬件限制）。比例首渲后可现场调。

**数据路径：** `demo_server` 已发 `left_hand.flex`（校准 0..1）→ `useSensorStore.updateFromRelay` → `LiveHandRenderer` 每帧读。无新 WS schema，无固件改动。

## Section 3 — 手语教学页：CSL + ASL 字符按钮

**方案：右面板两组按钮，ASL 字母作为单关键帧静态 pose。** 复用 `useDemoAnimation` transition/hold/settle 管线；每个 ASL 字母是 "rest→pose→hold 800ms→settle" 单帧 clip，与 CSL 同机制更短。

**布局**（左画布 + 右按钮面板）：
- 左 ~60%：`DemoCanvas`（全高 FK 手）。
- 右 ~40%：滚动面板，两组：
  - **CSL 手语** — 7 词按钮（你/好/再见/快乐/后悔/吃饭/睡觉），现有 `SIGN_DEFINITIONS`。
  - **ASL 字母** — 6 字母按钮（A/B/I/L/W/Y），新 keyframes。
- 右上播放控制（play-sequence/stop）保留；原抽屉变桌面端固定右面板。

**新 ASL 字母 keyframes**（`signLanguage.ts`，`makeAngles({thumb:[CMC,MCP,IP], index:[MCP,PIP,DIP],…})`）：

| 字母 | thumb | index | middle | ring | pinky | 备注 |
|---|---|---|---|---|---|---|
| A | 45,50,65 | 85,100,70 | 85,100,70 | 85,100,70 | 85,100,70 | 拳，拇指 tucked |
| B | 50,25,25 | 5,5,5 | 5,5,5 | 5,5,5 | 5,5,5 | 平掌，拇指折 |
| I | 40,45,60 | 85,100,70 | 85,100,70 | 85,100,70 | 5,5,5 | 拳+小指上 |
| L | 15,10,10 | 5,5,5 | 85,100,70 | 85,100,70 | 85,100,70 | 食指上+拇指出 |
| W | 40,45,60 | 5,5,5 | 5,5,5 | 5,5,5 | 85,100,70 | 三指上 |
| Y | 10,10,10 | 85,100,70 | 85,100,70 | 85,100,70 | 10,10,10 | 拇指+小指出 |

与 classifier `ASL_TEMPLATES` 意图一致（哪些指屈/直），屏幕 pose 与分类字母语义一致。

**动画时长：** 字母 transition-in 350ms easeOutCubic → hold 900ms → settle-back 400ms。CSL 保持 600ms hold。过渡 easeInOut，腕 slerp，关节 `lerpAngles`（`useDemoAnimation` 已有）。

## 不改动范围
`src/main.py` lifespan、固件 `$EG` 块 + ESP-NOW、WS JSON schema、`asl_classifier` classify 逻辑（仅 6 字母时改 active-set 常量）。`demo_server.py` 仅重启不编辑。

## 实施顺序
1. 写本 spec + commit。
2. 停 demo_server → ch3 诊断。
3. 条件 patch asl_classifier（若 6 字母）。
4. 运行 calibrate_demo.py 校准 + 重启 demo_server 验证。
5. 仪表盘 FK 升级（FingerChain 抽取 → LiveHandRenderer → HandSkeleton 简化 → DemoHandRenderer 重构）。
6. 手语教学 ASL keyframes + 右面板按钮布局。
7. 每阶段 memory 更新 + commit。
