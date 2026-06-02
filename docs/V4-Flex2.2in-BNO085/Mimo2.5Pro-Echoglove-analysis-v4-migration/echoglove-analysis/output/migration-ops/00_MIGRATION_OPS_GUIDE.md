# EchoGlove V3→V4 迁移操作指南

> **目标读者：** 你（项目负责人）+ Claude Code  
> **执行方式：** 每一步你手动在终端执行命令，Claude Code 执行代码变更  
> **预计总耗时：** 8–12 周（含数据集采集）  

---

## 核心原则

1. **同仓库、新分支** — 不开新仓库，不复制项目
2. **原地改文件** — 不需要新建项目文件夹
3. **更新 CLAUDE.md** — 让 Claude Code 理解 V4 架构
4. **按 Prompt 顺序执行** — 从 00 到 14，每步一个 commit
5. **每步验证** — 编译通过再提交，测试通过再进入下一步

---

## 文件清单

| 文件 | 用途 |
|---|---|
| `00_MIGRATION_OPS_GUIDE.md` | 本文件，总览 |
| `01_CLAUDE_MD_V4.md` | CLAUDE.md 模板，复制到项目根目录 |
| `02_GIT_WORKFLOW.md` | Git 分支策略、提交规范、回滚方案 |
| `03_STEP_BY_STEP_EXECUTION.md` | 逐步执行命令，终端操作手册 |
| `04_PROMPT_EXECUTION_ORDER.md` | 15 个 Prompt 的执行顺序、检查点、依赖关系 |

---

## 执行流程概览

```
Week 0: 准备
├── Step 1: 创建分支、打标签
├── Step 2: 复制 CLAUDE.md 到项目
├── Step 3: 复制迁移文档到项目
└── Step 4: 提交初始状态

Week 1-2: 硬件层迁移
├── Prompt 04: 删除 Hall/MUX 代码
├── Prompt 01: ADS1115 驱动
├── Prompt 02: BNO085 6DOF 模式
├── Prompt 03: FlexManager 模块
└── 硬件接线 & 验证

Week 3-4: 固件层迁移
├── Prompt 06: 采样任务重写
├── Prompt 05: Protobuf V4
├── Prompt 07: Relay 解析器
└── Prompt 08: R3F 前端

Week 5-6: 模型层
├── Prompt 10: L1 模型架构
├── Prompt 11: ST-GCN L2 模型
└── Prompt 12: Unity ms-MANO

Week 7-8: 数据 & 集成
├── Prompt 09: 数据采集工具
├── 数据集采集（并行）
├── Prompt 13: 集成测试
└── Prompt 14: 文档 & 发布

Week 9-12: 数据采集（持续）
└── 50+ 用户 × 200 手势 × 50 次
```

---

## 开始之前确认

- [ ] 你有 EchoGlove 项目的 git 仓库访问权限
- [ ] 当前 V3 代码可以正常编译 (`pio build`)
- [ ] Claude Code 可以读写项目目录
- [ ] 你已经阅读了 `01_ANALYSIS_REPORT.md` 理解迁移原因
- [ ] 你已经阅读了 `03_CLAUDE_CODE_PROMPTS_V4.md` 了解所有 Prompt 内容

---

*准备好了？进入 `03_STEP_BY_STEP_EXECUTION.md` 开始执行。*
