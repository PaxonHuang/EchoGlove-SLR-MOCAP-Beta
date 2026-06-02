# EchoGlove V4 Git 工作流

---

## 1. 分支策略

```
main (保护分支)
 │
 ├── v3.0-final (tag) ← V3 最终状态快照
 │
 └── v4/flex-sensor-migration (长期开发分支)
      │
      ├── commit: v4: remove Hall/MUX stack
      ├── commit: v4: add ADS1115 driver
      ├── commit: v4: BNO085 6DOF mode
      ├── commit: v4: add FlexManager
      ├── commit: v4: rewrite sensor sampling
      ├── commit: v4: protobuf schema V4
      ├── commit: v4: relay V3/V4 auto-detect
      ├── commit: v4: R3F flex→bone mapping
      ├── commit: v4: L1 model update
      ├── commit: v4: ST-GCN skeleton remapping
      ├── commit: v4: Unity ms-MANO update
      ├── commit: v4: dataset collection tool
      ├── commit: v4: E2E integration tests
      ├── commit: v4: documentation & release
      │
      └── v4.0.0 (tag) ← V4 发布标签
```

### 为什么不开新仓库？

| 考量 | 同仓库新分支 | 新仓库 |
|---|---|---|
| 代码历史 | ✅ 完整保留 | ❌ 丢失 |
| 变更对比 | ✅ `git diff v3.0-final..v4/...` | ❌ 需要手动对比 |
| 回滚 | ✅ `git checkout v3.0-final` | ❌ 需要切仓库 |
| Issue/CI | ✅ 统一管理 | ❌ 分散 |
| 协作 | ✅ PR review | ❌ 需要新权限 |
| 推荐 | ✅ **推荐** | ❌ 不推荐 |

---

## 2. 初始化操作

```bash
# 进入项目目录
cd echoglove

# 确认当前状态干净
git status
# 应该显示: nothing to commit, working tree clean

# 如果有未提交的修改，先处理
git stash  # 或 git add -A && git commit -m "chore: save WIP"

# 给 V3 最终状态打标签（如果还没有的话）
git tag -a v3.0-final -m "EchoGlove V3.0 final - Hall+Magnet architecture"

# 创建迁移分支
git checkout -b v4/flex-sensor-migration

# 确认分支
git branch
# * v4/flex-sensor-migration
#   main
```

---

## 3. 提交规范

### Commit Message 格式

```
v4: <type>: <简短描述>

<可选的详细说明>
```

### Type 说明

| Type | 用途 | 示例 |
|---|---|---|
| `remove` | 删除 V3 代码 | `v4: remove: delete TMAG5273 and TCA9548A drivers` |
| `add` | 新增 V4 代码 | `v4: add: ADS1115 dual-instance I2C driver` |
| `update` | 修改现有代码 | `v4: update: BNO085 to 6DOF Game Rotation Vector` |
| `rewrite` | 重写模块 | `v4: rewrite: sensor sampling task for 11-dim` |
| `feat` | 新功能 | `v4: feat: dataset collection web tool` |
| `fix` | 修复问题 | `v4: fix: I2C mutex deadlock on bus recovery` |
| `refactor` | 重构 | `v4: refactor: extract flex calibration to NVS` |
| `test` | 测试 | `v4: test: E2E integration test suite` |
| `docs` | 文档 | `v4: docs: migration guide and architecture` |
| `chore` | 杂项 | `v4: chore: update platformio.ini dependencies` |

### 示例

```bash
# 好的提交信息
git commit -m "v4: add: ADS1115 16-bit ADC driver (dual instance, 860SPS, mutex)

- I2C driver for ADS1115 at 0x48 and 0x49
- Configurable PGA (default ±4.096V)
- Single-shot and continuous modes
- Mutex-protected I2C access
- Unit tests included"

# 简短版
git commit -m "v4: add: ADS1115 driver"
```

---

## 4. 每步操作流程

每个 Prompt 执行的标准流程：

```bash
# 1. 确认当前状态
git status

# 2. 让 Claude Code 执行 Prompt
#    （在 Claude Code 中粘贴 Prompt 00 + 当前步骤的 Prompt）

# 3. 检查变更
git diff --stat  # 看改了哪些文件
git diff         # 看具体内容（可选）

# 4. 编译验证（固件相关）
cd glove_firmware && pio build
# 必须 0 errors

# 5. 运行测试（如果有）
pio test -e esp32s3

# 6. 提交
git add -A
git commit -m "v4: add: ADS1115 driver"

# 7. 进入下一步
```

---

## 5. 冲突处理

如果在迁移过程中需要同步 main 分支的更新：

```bash
# 方法 1: rebase（推荐，保持线性历史）
git fetch origin
git rebase origin/main
# 解决冲突后
git rebase --continue

# 方法 2: merge（保留合并历史）
git merge origin/main
# 解决冲突后
git commit
```

---

## 6. 回滚方案

### 场景 A：当前步骤出错，回退一步

```bash
# 撤销最后一次提交（保留文件修改）
git reset --soft HEAD~1

# 或者彻底撤销最后一次提交（丢弃文件修改）
git reset --hard HEAD~1
```

### 场景 B：迁移失败，回到 V3

```bash
# 方法 1：切换到 V3 标签（只读）
git checkout v3.0-final

# 方法 2：基于 V3 创建新分支继续开发
git checkout -b v3/hotfix v3.0-final

# 方法 3：放弃整个迁移分支
git checkout main
git branch -D v4/flex-sensor-migration
```

### 场景 C：迁移成功，发布 V4

```bash
# 合并到 main
git checkout main
git merge v4/flex-sensor-migration

# 打发布标签
git tag -a v4.0.0 -m "EchoGlove V4.0 - Flex Sensor Architecture"

# 推送
git push origin main --tags
```

---

## 7. .gitignore 更新

确认 `.gitignore` 包含以下内容：

```gitignore
# Build artifacts
glove_firmware/.pio/
glove_firmware/build/
glove_firmware/generated/

# Python
glove_relay/__pycache__/
glove_relay/*.pyc
glove_relay/.venv/
glove_relay/models/*.onnx
glove_relay/models/*.pt

# Node
glove_web/node_modules/
glove_web/build/
glove_web/dist/

# Unity
glove_unity/Library/
glove_unity/Temp/
glove_unity/obj/

# IDE
.vscode/
.idea/
*.swp
*.swo

# OS
.DS_Store
Thumbs.db

# Data (large files)
data/*.tfrecord
data/*.npz
*.tflite
*.bin

# Secrets
.env
*.pem
```

---

## 8. 大文件处理

数据集和模型文件不应该直接提交到 git：

```bash
# 使用 Git LFS 管理大文件
git lfs install
git lfs track "*.tflite"
git lfs track "*.onnx"
git lfs track "*.pt"
git lfs track "*.tfrecord"
git lfs track "*.npz"
git add .gitattributes
```

或者将数据集放在项目外部，通过软链接引用：

```bash
# 数据集目录（项目外）
mkdir -p ~/echoglove-data
ln -s ~/echoglove-data data
```

---

## 9. 分支保护建议

在 GitHub/GitLab 上设置：

- **main 分支：** 禁止直接 push，必须通过 PR
- **v4/flex-sensor-migration：** 允许直接 push（你一个人开发）
- **Tag 保护：** `v*.0.0` 标签禁止删除
