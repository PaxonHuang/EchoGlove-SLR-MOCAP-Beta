# EchoGlove V5.0 完整文档包

> 生成日期: 2026-06-02
> 项目: https://github.com/PaxonHuang/EchoGlove-SLR-MOCAP-Beta
> 分支: Flex2.2in-BNO085-ADS1115-ESP32S3N16R8-ArduinoFramework

---

## 文档清单

| 序号 | 文件名 | 说明 |
|------|--------|------|
| 1 | `01_SOP_SPEC_PLAN_V5.0.md` | 完整SOP-SPEC-PLAN文档，含6个Phase详细设计 |
| 2 | `02_CLAUDE_CODE_PROMPTS_V5.0.md` | Claude Code分阶段AI编程提示词(6个Phase) |
| 3 | `03_BOM_TABLE_V5.0.md` | 硬件BOM表，含版本对比和成本估算 |
| 4 | `04_WIRING_DIAGRAM_V5.0.md` | 接线图与注意事项 |
| 5 | `05_ARCHITECTURE_DIAGRAMS_V5.0.md` | 架构图集(Mermaid+ASCII) |
| 6 | `06_DECISION_SUMMARY_V5.0.md` | 15项核心架构决策记录 |

---

## 快速开始

1. **阅读决策总结** → `06_DECISION_SUMMARY_V5.0.md`
2. **阅读SOP-SPEC-PLAN** → `01_SOP_SPEC_PLAN_V5.0.md`
3. **按Phase执行Claude Code** → `02_CLAUDE_CODE_PROMPTS_V5.0.md`
4. **参考BOM采购硬件** → `03_BOM_TABLE_V5.0.md`
5. **按接线图焊接** → `04_WIRING_DIAGRAM_V5.0.md`
6. **查看架构图** → `05_ARCHITECTURE_DIAGRAMS_V5.0.md`

---

## V5.0核心变更 vs V3.0霍尔方案

| 方面 | V3.0 霍尔 | V5.0 Flex+IMU |
|------|----------|---------------|
| 传感器 | TMAG5273×5 + N52磁珠 | SpectraFlex×5 + BNO085 |
| 特征维度 | 21-dim (Hall×15+IMU×6) | 11-dim (Flex×5+IMU×6) |
| 磁干扰 | 🔴 致命问题 | ✅ 无磁性干扰 |
| 佩戴舒适度 | 🟡 磁珠机械结构复杂 | ✅ 柔性传感器贴合 |
| 双手架构 | 单MCU | 双MCU+接收器 |
| 推理架构 | L1+L2双层 | **Tier1/2/3三级热切换** |
| L1模型 | CNN+Attn | **Gated Bi-CrossAttn** |
| L2模型 | ST-GCN | **ST-GCN→MS-TCN→CTC** |
| 离线可用 | ❌ | ✅ Tier1始终运行 |
| 总成本 | ~¥280 | ~¥489 |

---

## 关键决策点（需用户审阅）

| # | 决策点 | 当前选择 | 备选 |
|---|--------|----------|------|
| 1 | 双手架构 | 双MCU(3×ESP32-S3) | 单MCU+I2C Mux |
| 2 | BNO085模式 | GRV 6轴(无磁力计) | 9轴(有磁力计) |
| 3 | Flex传感器 | SpectraFlex 2.2" | 国产替代¥5/个 |
| 4 | 接收器MCU | ESP32-S3 N16R8 | ESP32-S3 N8R8 |
| 5 | 热切换过渡 | 5帧线性渐变(50ms) | 10帧(100ms) |
| 6 | MS-TCN stage | PC:4-stage, 接收器:2-stage | 统一4-stage |
| 7 | CTC解码 | Greedy + Beam Search(k=10) | 纯Greedy |
| 8 | Unity渲染 | XR Hands 1.7 + OpenXR | 自定义骨骼 |

---

## 参考项目

- PSL Smart Glove (2025): Flex×5+MPU9250, LSTM, ~94%
- Lenguantec LSM (2026): Flex+IMU(ESP32-C3), ML, 97%
- DGCA (WWW 2025): Gated CrossAttention, +2.27%
- MS-TCN (ICASSP 2021): 时序分割, mF1=68.68
- DSLNet (2025): Dual-STGCN, 42节点
