# Windows → Ubuntu/Linux 开发环境迁移规划

## 背景与问题

团队需要将开发环境从 Windows 迁移到 Ubuntu/Linux，同时保持 Windows 和 Ubuntu 的协同开发能力。核心问题是解决 Git 行尾符（line endings）兼容性报错：`This diff contains a change in line endings from 'LF' to 'CRLF'.`

---

## 当前项目状态分析

### 1. 关键发现

| 项目 | 状态 | 说明 |
|------|------|------|
| `.gitattributes` | **不存在** | 这是行尾问题的根本原因 |
| `core.autocrlf` | 未知 | 需要检查并统一配置 |
| VS Code 设置 | Windows 绝对路径 | `.vscode/settings.json` 中有大量 `C:/Users/QuenchKidney/` 路径 |
| CLAUDE.md | PowerShell 指令 | 明确说明 "use PowerShell on Windows — bash produces no output" |
| PlatformIO | Windows 路径 | `.claude/settings.json` 中有 Windows 绝对路径的 hook |

### 2. 操作系统特定依赖

**Firmware (glove_firmware):**
- PlatformIO 构建系统（跨平台，但工具链路径不同）
- VS Code C/C++ 扩展配置中有 Windows 绝对路径

**Relay (glove_relay):**
- Python + FastAPI（跨平台）
- 依赖 `requirements.txt`

**Web (glove_web):**
- Node.js + Vite（跨平台）
- 标准 npm 项目

**Unity (glove_unity):**
- Unity 2022 LTS（Windows/Mac 为主，Linux 支持有限）

---

## 迁移方案

### 阶段 1: 行尾符标准化（关键！）

#### 1.1 创建 `.gitattributes` 文件

在项目根目录创建 `.gitattributes`，强制所有文本文件使用 LF 行尾：

```gitattributes
# Auto detect text files and perform LF normalization
* text=auto eol=lf

# Source code
*.c text eol=lf
*.cpp text eol=lf
*.h text eol=lf
*.hpp text eol=lf
*.py text eol=lf
*.js text eol=lf
*.ts text eol=lf
*.tsx text eol=lf
*.json text eol=lf
*.yaml text eol=lf
*.yml text eol=lf
*.md text eol=lf
*.ini text eol=lf
*.toml text eol=lf

# Shell scripts must use LF
*.sh text eol=lf
*.bash text eol=lf

# Windows batch files can use CRLF
*.bat text eol=crlf
*.cmd text eol=crlf
*.ps1 text eol=crlf

# Binary files - do not modify
*.png binary
*.jpg binary
*.jpeg binary
*.gif binary
*.ico binary
*.pdf binary
*.zip binary
*.gz binary
*.tar binary
*.rar binary
*.7z binary
*.exe binary
*.dll binary
*.so binary
*.dylib binary
*.bin binary
*.hex binary
*.elf binary

# Unity specific
*.unity binary
*.prefab binary
*.asset binary
*.meta text
*.cs text eol=lf
```

#### 1.2 标准化现有文件

执行以下命令标准化整个仓库：

```bash
# 1. 保存当前工作
git add -A
git commit -m "Pre-normalization checkpoint" || true

# 2. 添加 .gitattributes
git add .gitattributes
git commit -m "Add .gitattributes for LF normalization"

# 3. 强制重新规范化所有文件
git add --renormalize .
git commit -m "Normalize all line endings to LF"

# 4. 验证
# 应该没有 "warning: CRLF will be replaced by LF" 警告了
git ls-files --eol | grep -i crlf || echo "All files normalized to LF"
```

#### 1.3 Git 配置建议

**团队共享配置（添加到 `.gitconfig` 或文档）：**

```bash
# Windows 开发者配置
git config --global core.autocrlf input
git config --global core.eol lf

# Linux/Ubuntu 开发者配置
git config --global core.autocrlf input
git config --global core.eol lf
```

> **注意**: 使用 `core.autocrlf input` 而非 `true`，让 `.gitattributes` 完全控制行尾。

---

### 阶段 2: 跨平台构建配置

#### 2.1 Firmware - PlatformIO 配置

**当前问题**: `.vscode/settings.json` 包含 Windows 绝对路径

**解决方案**: 创建平台无关的 VS Code 配置

**文件**: `.vscode/settings.json` 修改：

```json
{
  "C_Cpp_Runner.cCompilerPath": "xtensa-esp32s3-elf-gcc",
  "C_Cpp_Runner.cppCompilerPath": "xtensa-esp32s3-elf-g++",
  "C_Cpp_Runner.debuggerPath": "xtensa-esp32s3-elf-gdb",
  
  // 使用 PlatformIO 的变量而非绝对路径
  "C_Cpp_Runner.includePaths": [
    "${workspaceFolder}/glove_firmware/src",
    "${workspaceFolder}/glove_firmware/lib",
    "${env:PLATFORMIO_PACKAGES_DIR}/framework-arduinoespressif32/libraries/**",
    "${env:PLATFORMIO_PACKAGES_DIR}/framework-arduinoespressif32/cores/esp32"
  ]
}
```

**文件**: `.vscode/extensions.json`（添加推荐扩展）：

```json
{
  "recommendations": [
    "platformio.platformio-ide",
    "ms-vscode.cpptools",
    "ms-python.python",
    "bradlc.vscode-tailwindcss",
    "esbenp.prettier-vscode"
  ]
}
```

**CLAUDE.md 更新** - 添加跨平台构建说明：

```markdown
### Cross-Platform Build Commands

#### Firmware (glove_firmware)

**Windows (PowerShell):**
```powershell
cd glove_firmware
pio run
```

**Linux/Ubuntu:**
```bash
cd glove_firmware
pio run
```

> PlatformIO is cross-platform. The same commands work on both Windows and Linux.
> Ensure PlatformIO Core is installed: https://platformio.org/install/cli
```

#### 2.2 Python Relay - 跨平台配置

**文件**: `glove_relay/pyproject.toml` 或 `setup.py` 添加 shebang：

```python
#!/usr/bin/env python3
# -*- coding: utf-8 -*-
```

**文件**: `glove_relay/run.sh`（Linux 启动脚本）：

```bash
#!/bin/bash
set -e
cd "$(dirname "$0")"
source .venv/bin/activate 2>/dev/null || source venv/bin/activate 2>/dev/null || true
uvicorn src.main:app --host 0.0.0.0 --port 8000 --reload
```

**文件**: `glove_relay/run.ps1`（Windows PowerShell 脚本）：

```powershell
$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot
.\venv\Scripts\Activate.ps1 2>$null
uvicorn src.main:app --host 0.0.0.0 --port 8000 --reload
```

#### 2.3 Web Frontend - 跨平台配置

Node.js/npm 本身就是跨平台的，无需特殊修改。

**可选**: 添加 `package.json` scripts：

```json
{
  "scripts": {
    "dev": "vite",
    "build": "tsc && vite build",
    "preview": "vite preview",
    "dev:win": "powershell -Command \"npm run dev\"",
    "dev:linux": "bash -c 'npm run dev'"
  }
}
```

---

### 阶段 3: 开发环境设置指南

#### 3.1 Ubuntu/Linux 开发环境安装

创建 `docs/UBUNTU_SETUP.md`：

```markdown
# Ubuntu/Linux 开发环境设置

## 1. 基础工具

```bash
sudo apt update
sudo apt install -y git curl wget build-essential

# Python
sudo apt install -y python3 python3-pip python3-venv

# Node.js (via nvm)
curl -o- https://raw.githubusercontent.com/nvm-sh/nvm/v0.39.0/install.sh | bash
source ~/.bashrc
nvm install 18
nvm use 18

# PlatformIO
pip3 install platformio
# 或
curl -fsSL https://raw.githubusercontent.com/platformio/platformio-core-installer/master/get-platformio.py -o get-platformio.py
python3 get-platformio.py
```

## 2. 项目设置

```bash
# 克隆仓库
git clone <repo-url>
cd Hall-BNO085-PlatformIOArduino

# 验证行尾设置
git config core.autocrlf input
git config core.eol lf

# Firmware
cd glove_firmware
pio pkg install

# Python Relay
cd ../glove_relay
python3 -m venv venv
source venv/bin/activate
pip install -r requirements.txt

# Web Frontend
cd ../glove_web
npm install
```

## 3. VS Code 设置

安装扩展：
- PlatformIO IDE
- C/C++
- Python
- ESLint
- Prettier

## 4. 串口权限（用于烧录 ESP32）

```bash
# 添加用户到 dialout 组
sudo usermod -a -G dialout $USER
# 重新登录生效
```
```

#### 3.2 Windows 开发环境（保持兼容）

更新 `docs/WINDOWS_SETUP.md`：

```markdown
# Windows 开发环境设置

## Git 配置（关键！）

```powershell
# 使用 LF 行尾，与 Linux 保持一致
git config --global core.autocrlf input
git config --global core.eol lf
```

## 其他工具安装
# ... 现有内容 ...
```

---

### 阶段 4: CLAUDE.md 更新

更新 CLAUDE.md 中的构建命令，添加跨平台说明：

**当前**（第 56-69 行）：
```markdown
### Firmware (glove_firmware)
```powershell
cd glove_firmware
# Build (use PowerShell on Windows — bash produces no output)
pio run
```

**修改为**：
```markdown
### Firmware (glove_firmware)

**Windows:**
```powershell
cd glove_firmware
pio run
```

**Linux/Ubuntu:**
```bash
cd glove_firmware
pio run
```

> PlatformIO 是跨平台的。Windows 上使用 PowerShell，Linux 上使用 Bash。
```

---

### 阶段 5: 验证清单

迁移完成后，验证以下项目：

#### 5.1 行尾符验证

```bash
# 检查是否有 CRLF 文件
git ls-files --eol | grep -i crlf

# 应该无输出（或仅 .bat/.cmd/.ps1 文件）
```

#### 5.2 构建验证

| 组件 | Windows | Ubuntu |
|------|---------|--------|
| Firmware | `pio run` ✓ | `pio run` ✓ |
| Relay | `python -m uvicorn` ✓ | `uvicorn` ✓ |
| Web | `npm run dev` ✓ | `npm run dev` ✓ |

#### 5.3 Git 操作验证

```bash
# 在 Windows 上修改文件
echo "test" >> test.txt
git add test.txt
git diff --cached

# 应该显示正常 diff，没有 "line endings" 警告
```

---

## 需要创建/修改的文件清单

### 新建文件

1. `.gitattributes` - 行尾标准化配置
2. `docs/UBUNTU_SETUP.md` - Ubuntu 开发环境指南
3. `glove_relay/run.sh` - Linux 启动脚本
4. `glove_relay/run.ps1` - Windows 启动脚本（可选，已有命令可简化）

### 修改文件

1. `.vscode/settings.json` - 移除 Windows 绝对路径
2. `CLAUDE.md` - 更新构建命令为跨平台版本
3. `docs/WINDOWS_SETUP.md` - 添加 Git LF 配置说明
4. `.claude/settings.json` - 更新 hook 为跨平台命令

---

## 迁移执行步骤

### 准备阶段（Windows 上执行）

1. **备份当前工作**
   ```bash
   git add -A
   git commit -m "Pre-migration checkpoint"
   ```

2. **创建 .gitattributes**
   ```bash
   # 复制上面的 .gitattributes 内容到文件
   ```

3. **标准化行尾**
   ```bash
   git add .gitattributes
   git commit -m "Add .gitattributes for LF normalization"
   git add --renormalize .
   git commit -m "Normalize all line endings to LF"
   ```

4. **推送更改**
   ```bash
   git push
   ```

### Ubuntu 设置阶段

1. **克隆仓库**
   ```bash
   git clone <repo-url>
   cd Hall-BNO085-PlatformIOArduino
   ```

2. **验证行尾**
   ```bash
   git config core.autocrlf input
   git config core.eol lf
   ```

3. **安装依赖**（按 `docs/UBUNTU_SETUP.md`）

4. **测试构建**
   ```bash
   cd glove_firmware && pio run
   cd ../glove_relay && source venv/bin/activate && uvicorn src.main:app
   cd ../glove_web && npm run dev
   ```

### 协同开发验证

1. **Windows 开发者拉取更新**
   ```bash
   git pull
   # 文件应该自动以 LF 行尾检出
   ```

2. **修改并提交**
   - Windows 开发者修改文件
   - 提交推送
   - Ubuntu 开发者拉取
   - 验证无行尾冲突

---

## 常见问题与解决

### Q1: "warning: CRLF will be replaced by LF"

**原因**: `.gitattributes` 配置正确，Git 正在按预期转换行尾。

**解决**: 这是正常行为，不是错误。如果希望消除警告，确保文件在提交前已经是 LF。

### Q2: "This diff contains a change in line endings from 'LF' to 'CRLF'"

**原因**: 文件从 LF 变成了 CRLF，通常是因为 `core.autocrlf=true`。

**解决**: 
1. 设置 `git config --global core.autocrlf input`
2. 删除并重新检出文件：`git rm --cached -r . && git reset --hard`

### Q3: PlatformIO 在 Linux 上找不到工具链

**原因**: PATH 配置问题。

**解决**:
```bash
# 添加 PlatformIO 到 PATH
echo 'export PATH="$HOME/.platformio/penv/bin:$PATH"' >> ~/.bashrc
source ~/.bashrc
```

### Q4: VS Code 在 Ubuntu 上找不到 include 路径

**原因**: `.vscode/settings.json` 中的 Windows 绝对路径。

**解决**: 使用上面提供的跨平台 `settings.json`。

---

## 总结

迁移的核心是：**通过 `.gitattributes` 强制所有文本文件使用 LF 行尾**，然后更新配置文件使其跨平台兼容。

关键文件：
- `.gitattributes` - 解决行尾问题的银弹
- `CLAUDE.md` - 更新构建命令
- `.vscode/settings.json` - 移除绝对路径

完成这些更改后，Windows 和 Ubuntu 开发者可以无缝协作，不再受行尾符问题困扰。
