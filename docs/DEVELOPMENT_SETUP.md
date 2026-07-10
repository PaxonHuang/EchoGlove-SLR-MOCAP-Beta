# EchoGlove-SLR-MOCAP — 开发环境配置指南

> 目标：任何协作者在 **Ubuntu x64**（无需 CUDA）上 `git clone` 本项目后，一条命令完成开发环境配置，开箱即用。
> 平台要求：Ubuntu 20.04 / 22.04 / 24.04（x64）。macOS / Windows 不在本指南范围（Unity 部分保留 Windows）。

---

## 0. TL;DR — 最快上手

```bash
git clone https://github.com/PaxonHuang/EchoGlove-SLR-MOCAP-Beta.git
cd EchoGlove-SLR-MOCAP-Beta
```

进入 Claude Code 后输入：

```
/setup-env
```

或者在终端直接跑（不想用 Claude Code）：

```bash
./scripts/setup_env.sh            # 交互式：重组件会询问
./scripts/setup_env.sh --all      # 全自动安装一切（约 15–40 分钟，含 ESP-IDF 下载）
./scripts/setup_env.sh --relay --web   # 只装 relay + 前端（轻量，约 3 分钟）
```

脚本**幂等、检测优先**：已装的工具直接跳过，可安全反复执行。装完按 [§3 下一步](#3-下一步验证与运行) 操作即可。

---

## 1. 项目依赖全景（requirements 清单）

### 1.1 系统级（apt）依赖
| 包 | 用途 |
|----|------|
| `git curl wget` | 代码与安装器拉取 |
| `python3 python3-pip python3-venv` | PlatformIO / 脚本 |
| `build-essential cmake ninja-build pkg-config` | ESP-IDF / 原生测试编译 |
| `libusb-1.0-0-dev` | USB（P4 CDC / 烧录） |
| `udev` + `dialout` 用户组 | 串口设备访问权限 |

> 脚本的 `--base-os` 阶段自动安装并把当前用户加入 `dialout` 组。**加入后需注销/重新登录**串口权限才生效。

### 1.2 Python / Conda 环境
项目用 **miniconda** 管理两个独立环境（CPU-only，无 CUDA）：

| 环境 | 定义文件 | Python | 主要内容 | 用途 |
|------|----------|--------|----------|------|
| `pytorch_env` | `glove_relay/environment.yml` | 3.10 | torch(CPU) + torchvision + relay 需求 | Relay 服务器 + ST-GCN 训练/推理 |
| `tf_env` | `glove_relay/environment_tf.yml` | 3.10 | tensorflow-cpu + 转换工具 | 模型导出 / TFLite 转换 |

Relay 服务器核心依赖（`glove_relay/requirements.txt`，会被 `environment.yml` 通过 `pip: -r requirements.txt` 装进 `pytorch_env`）：
`fastapi uvicorn[standard] websockets pyserial-asyncio torch torchvision protobuf edge-tts numpy pydantic pyyaml scikit-learn matplotlib`

> **为什么不写死 CUDA torch？** 取舍：CPU 版通用、轻量、跨机器无 NVIDIA 驱动也能跑。训练慢一些但能跑；需要 GPU 时在 `environment.yml` 里去掉 `cpuonly` 并换成 `pytorch=*=*cuda*` 即可。

### 1.3 Node.js / 前端
- **nvm** 管理，Node **22 LTS**（Vite 6 + React 18 要求 ≥18）。
- 前端依赖：`glove_web/package.json`（React 18 + R3F + three + zustand + Tailwind 4 + TypeScript 5.7）。
- `npm install` 由 `--web` 阶段执行。

### 1.4 ESP32 嵌入式工具链（两套，分治）
| 目标 | 工具链 | 构建系统 | 版本要求 |
|------|--------|----------|----------|
| S3 手套 | **PlatformIO Core** | `pio run` | `espressif32@^6.5.0`（platformio.ini 锁定） |
| P4 / C6 基站 | **ESP-IDF** | `idf.py build` | **v5.4**（P4 BSP `esp32_p4_function_ev_board` 要求 idf≥5.4） |

- S3 手套固件依赖（`glove_firmware/platformio.ini` 的 `lib_deps`）由 PlatformIO 自动拉取：Nanopb、Adafruit BNO08x、Adafruit BusIO、NimBLE-Arduino、ArduinoJson、TensorFlowLite_ESP32。**注意：BNO085 驱动是项目内本地 driver，不在 PIO registry。**
- P4 固件 managed_components（`idf_component.yml`）由 ESP-IDF 在首次 `idf.py build` 时自动获取：esp_tinyusb、esp32_p4_function_ev_board (BSP)、lvgl、esp_lcd_* 等。

> **取舍：ESP-IDF 最重（~2GB）**。脚本默认交互式询问是否安装；只想搞前端/relay 的协作者可 `SKIP_IDF=1 ./scripts/setup_env.sh` 或只跑 `--relay --web`。

### 1.5 Claude Code
- 安装见 [官方文档](https://docs.claude.com/en/docs/claude-code)（`npm i -g @anthropic-ai/claude-code`，需 Node ≥18，与上面 nvm/Node 22 一致）。
- 项目自带 `.claude/` 目录：`settings.json`（共享配置）、`commands/setup-env.md`（本引导命令）、`skills/`、`agents/`。
- `.claude/settings.local.json` 被 gitignore，存平台相关私密配置（如 `GITHUB_PERSONAL_ACCESS_TOKEN`、本地串口路径），**不入库**。

---

## 2. setup_env.sh 使用详解

```
Stages: base-os | conda | node | platformio | esp-idf | relay | web | verify

./scripts/setup_env.sh                 # 默认交互：轻量阶段自动装，重阶段(IDF)问询
./scripts/setup_env.sh --all           # 全装，无询问
./scripts/setup_env.sh --idf --relay   # 只跑指定阶段
SKIP_IDF=1 ./scripts/setup_env.sh      # 环境变量跳过某阶段
```

**可调环境变量**（按需覆盖）：
| 变量 | 默认 | 说明 |
|------|------|------|
| `IDF_VERSION` | `v5.4` | ESP-IDF 版本 |
| `IDF_DIR` | `~/esp/esp-idf` | IDF 源码目录 |
| `IDF_TOOLS_DIR` | `~/esp/esp-idf-tools` | IDF 工具链目录（与源码分离，便于缓存共享） |
| `CONDA_DIR` | `~/miniconda3` | miniconda 安装目录 |
| `NODE_LTS` | `22` | nvm 安装的 Node 大版本 |
| `SKIP_<STAGE>=1` | — | 跳过任一阶段（`SKIP_BASE_OS SKIP_CONDA SKIP_NODE SKIP_PLATFORMIO SKIP_IDF SKIP_RELAY SKIP_WEB SKIP_VERIFY`） |

**设计取舍**：
- **检测优先而非盲装**：尊重用户已有的 `~/.nvm`、`~/miniconda3`、`~/esp/esp-idf`，已存在则复用，避免与用户全局环境冲突。
- **工具链版本管理交给工具本身**：不硬 `wget` 特定版本 torch，而是用 conda 的 `environment.yml` 声明式管理——可重现、可 diff、可 `--prune` 更新。
- **ESP-IDF 激活隔离**：不污染默认 shell，提供 `scripts/activate_idf.sh`，用时 `source` 即可。

---

## 3. 下一步：验证与运行

装完后逐项冒烟测试：

```bash
# 1. Relay（pytorch_env）
conda run -n pytorch_env python -m pytest glove_relay/tests/ -q          # 应全绿
conda run -n pytorch_env uvicorn glove_relay.src.main:app --port 8000    # 起服务

# 2. Web 前端
cd glove_web && npm run dev    # http://localhost:5173

# 3. S3 手套固件（PlatformIO）
cd glove_firmware && pio run                                      # 编译
pio run -t upload && pio device monitor                           # 烧录+监控

# 4. P4 / C6 基站（ESP-IDF，每开一个新终端都要先激活）
source scripts/activate_idf.sh
cd glove_firmware/p4_base_station/p4_firmware && idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

**常用端口**（CLAUDE.md 已记录，便于排查）：Relay HTTP `8000`、Relay WebSocket `8765`、S3 UDP 发送 `8888`、Web dev `5173`。

---

## 4. 故障排查

| 症状 | 解决 |
|------|------|
| `pio device monitor` / `idf.py flash` 报权限拒绝 | 用户未在 `dialout` 组：`sudo usermod -aG dialout $USER` 后**注销重登录**。 |
| `conda: command not found`（新终端） | miniconda 未加进 PATH：`~/miniconda3/bin/conda init bash` 再重开终端。 |
| `idf.py: command not found` | 没激活：`source scripts/activate_idf.sh`。 |
| `stage_idf` 下载慢/失败 | 走 IDF 镜像：`export IDF_GITHUB_ASSETS="dl.espressif.com/github_assets"` 后重跑 `--idf`；或离线包见 espressif 官网。 |
| npm/torch 下载慢 | 设国内镜像：`npm config set registry https://registry.npmmirror.com`；conda 用清华源（`conda config --add channels https://mirrors.tuna.tsinghua.edu.cn/anaconda/`）。 |
| 已有 ESP-IDF 但版本不符 | 脚本会询问是否 checkout v5.4；选 yes 自动切换。 |
| 只想搞前端/relay | `SKIP_IDF=1 ./scripts/setup_env.sh` 或 `./scripts/setup_env.sh --relay --web`。 |

---

## 5. 「不丢失记忆」——跨会话/跨设备状态保留

项目本身已设计好跨会话延续机制，clone 后即生效：

| 机制 | 文件 | 作用 |
|------|------|------|
| 项目指令 | `CLAUDE.md` | 每个 echo-glove 会话自动加载：架构、构建命令、硬件、当前阶段（V5.3 wired UART）、gotchas |
| 跨会话进度 | `PROGRESS.md` / `PROGRESS_CN.md` | 完成的 checkpoint、下一步。**新会话先读这个** |
| 设计/计划 | `docs/superpowers/specs/`、`docs/superpowers/plans/` | 各版本设计 spec + 实施计划 |
| V6 迁移 | `docs/V6/` | LSM6DSV16X 迁移 6 文件包 |
| Claude 命令/技能 | `.claude/commands/`、`.claude/skills/`、`.claude/agents/` | 共享工作流（如 `/setup-env`） |
| Claude 设置（共享） | `.claude/settings.json` | 入库的团队共享配置 |
| Claude 设置（私有） | `.claude/settings.local.json` | gitignore，存个人 token/串口路径，**需各人自配** |

**协作者 clone 后要做的唯一手动事**：把个人 token（如 `GITHUB_PERSONAL_ACCESS_TOKEN`）填进自己的 `.claude/settings.local.json`（该文件不入库，需自行创建）。其余全由 `/setup-env` 接管。

---

## 6. 跨平台与行尾

- 全平台 LF（`.gitattributes` 强制）；仅 Windows 脚本 `.bat/.ps1/.cmd` 用 CRLF。
- Unity 开发保留 Windows（Linux 支持有限），不在本 Linux 引导范围。
- `.claude/settings.local.json` 各平台独立、不入库。

---

## 附录：脚本创建/管理 conda 环境的手动命令

```bash
# 从定义文件创建
conda env create -f glove_relay/environment.yml
conda env create -f glove_relay/environment_tf.yml

# 更新（已存在时）
conda env update -f glove_relay/environment.yml --prune

# 列出 / 删除
conda env list
conda env remove -n pytorch_env
```

> **更新环境定义后**：提交 `environment.yml` / `environment_tf.yml`，协作者跑 `./scripts/setup_env.sh --conda` 即可同步。
