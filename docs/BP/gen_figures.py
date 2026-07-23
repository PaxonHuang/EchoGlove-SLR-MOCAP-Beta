#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""EchoGlove BP V2.1 科研制图生成
数据来源: docs/BP/research_1~4_*.md + 已审核 BP
风格: 咨询级 (取 GPT5.6 参考模板配色精华), 白底低饱和, 300dpi
"""
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import matplotlib.font_manager as fm
import numpy as np
from matplotlib.patches import FancyBboxPatch, FancyArrowPatch, Rectangle
import os

# ---- 字体 ----
CJK = '/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc'
CJK_B = '/usr/share/fonts/opentype/noto/NotoSansCJK-Bold.ttc'
fm.fontManager.addfont(CJK)
fm.fontManager.addfont(CJK_B)
plt.rcParams['font.family'] = 'Noto Sans CJK SC'
plt.rcParams['font.sans-serif'] = ['Noto Sans CCJK SC','Noto Sans CJK SC','DejaVu Sans']
plt.rcParams['axes.unicode_minus'] = False
plt.rcParams['svg.fonttype'] = 'none'

# ---- 配色 (取参考模板精华) ----
TEAL   = '#1B6B7A'   # 主色 青绿
SAGE   = '#529286'   # 鼠尾草
STEEL  = '#5B8DB8'   # 钢蓝
TERRA  = '#D4875A'   # 赤陶
BROWN  = '#B08050'   # 棕
GREY   = '#9AA0A6'   # 中性灰
LGREY  = '#E8ECEE'   # 浅灰底
RED    = '#C0504D'   # 警示红
GREEN  = '#6B9E47'   # 绿
INK    = '#1F2933'   # 墨黑

OUT = os.path.join(os.path.dirname(__file__), 'figures')
os.makedirs(OUT, exist_ok=True)

def save(fig, name):
    path = os.path.join(OUT, name)
    fig.savefig(path, dpi=300, bbox_inches='tight', facecolor='white', pad_inches=0.15)
    plt.close(fig)
    print('  saved', name)

def style_ax(ax):
    ax.spines['top'].set_visible(False)
    ax.spines['right'].set_visible(False)
    ax.spines['left'].set_color('#BFC4C9')
    ax.spines['bottom'].set_color('#BFC4C9')
    ax.tick_params(colors=INK, labelsize=10)
    ax.title.set_color(INK)

# ============================================================
# 图1: 目标市场规模 (分组柱状图)
# 数据: BP 2.3 + research 综合测算
# ============================================================
def fig_market_size():
    fig, ax = plt.subplots(figsize=(10, 5.6))
    years = ['2026', '2027', '2028']
    # 中国具身智能市场 (亿元, 含机器人本体/遥操作/数据服务)
    cn_embodied = [420, 780, 1200]
    # 全球机器人遥操作市场 (亿元, 据BP综合测算)
    gl_teleop = [180, 320, 520]
    # 中国手语翻译服务 (亿元, 较小但社会价值高)
    cn_sign = [12, 20, 32]
    x = np.arange(len(years))
    w = 0.26
    b1 = ax.bar(x - w, cn_embodied, w, label='中国具身智能市场', color=TEAL)
    b2 = ax.bar(x,      gl_teleop,   w, label='全球机器人遥操作市场', color=SAGE)
    b3 = ax.bar(x + w, cn_sign,      w, label='中国手语翻译服务市场', color=TERRA)
    for bars in (b1,b2,b3):
        for b in bars:
            ax.text(b.get_x()+b.get_width()/2, b.get_height()+15,
                    f'{int(b.get_height())}', ha='center', va='bottom',
                    fontsize=9, color=INK, fontweight='bold')
    ax.set_xticks(x); ax.set_xticklabels(years, fontsize=11)
    ax.set_ylabel('市场规模 (亿元)', fontsize=11, color=INK)
    ax.set_title('图1  EchoGlove 目标市场规模估算 (2026—2028)', fontsize=13, fontweight='bold', pad=12, color=INK)
    ax.set_ylim(0, 1400)
    ax.legend(loc='upper left', frameon=False, fontsize=10)
    ax.yaxis.grid(True, color='#EEF1F2', linewidth=0.8)
    ax.set_axisbelow(True)
    style_ax(ax)
    fig.text(0.99, 0.01, '数据来源: 综合测算 (具身智能产业报告 + 遥操作市场 + 无障碍服务市场)',
             ha='right', va='bottom', fontsize=7.5, color=GREY, style='italic')
    save(fig, 'fig_market_size.png')

# ============================================================
# 图2: 竞品核心能力雷达图
# 数据: research_1~3 + BP 表3, 6维度×5竞品
# ============================================================
def fig_competitive_radar():
    dims = ['低成本\n(BOM)', '关节精度', '多模态\n数据丰富度', 'MANO+\nROS2双生态', '端侧AI\n推理', '开放生态\n+数据平台']
    # 0-5 评分 (基于 research 硬指标)
    data = {
        'EchoGlove V6/V7': [4.5, 3.5, 4.5, 5.0, 4.0, 4.5],
        'Manus Polygon':  [1.0, 4.5, 3.0, 3.0, 1.0, 1.5],
        'MediaPipe/视觉': [5.0, 2.0, 1.5, 1.0, 3.0, 3.5],
        'UMI/ALOHA':      [2.0, 4.0, 2.5, 2.5, 1.0, 3.5],
        'lucidgloves':    [5.0, 1.5, 1.5, 1.0, 1.0, 2.5],
    }
    colors = [TEAL, TERRA, STEEL, SAGE, BROWN]
    N = len(dims)
    angles = np.linspace(0, 2*np.pi, N, endpoint=False).tolist()
    angles += angles[:1]
    fig, ax = plt.subplots(figsize=(8.4, 8.4), subplot_kw=dict(polar=True))
    for (name, vals), c in zip(data.items(), colors):
        v = vals + vals[:1]
        ax.plot(angles, v, color=c, linewidth=2.2, label=name)
        ax.fill(angles, v, color=c, alpha=0.10)
    ax.set_xticks(angles[:-1])
    ax.set_xticklabels(dims, fontsize=10.5, color=INK)
    ax.set_yticks([1,2,3,4,5])
    ax.set_yticklabels(['1','2','3','4','5'], fontsize=8, color=GREY)
    ax.set_ylim(0, 5.2)
    ax.set_rlabel_position(90)
    ax.grid(color='#D7DCE0', linewidth=0.7)
    ax.spines['polar'].set_color('#D7DCE0')
    ax.set_title('图2  竞品核心能力雷达对比 (0—5 评分)', fontsize=13, fontweight='bold', pad=24, color=INK, y=1.10)
    ax.legend(loc='upper right', bbox_to_anchor=(1.32, 1.10), frameon=False, fontsize=9.5)
    fig.text(0.99, 0.01, '评分依据: research_1~4 硬指标 (BOM/精度/模态/接口/开源/数据平台)',
             ha='right', va='bottom', fontsize=7.5, color=GREY, style='italic')
    save(fig, 'fig_competitive_radar.png')

# ============================================================
# 图3: V5 vs V6 BOM 成本对比 (堆叠柱状图)
# 数据: research_1 + BP 3.4 (V5 BNO085¥70+ADS1115¥32; V6 LSM6DSV16X$2-4+内ADC)
# ============================================================
def fig_bom_cost():
    fig, ax = plt.subplots(figsize=(10, 5.6))
    cats = ['IMU', 'ADC', 'MCU', 'Flex', '其他']
    v5 = [70, 32, 22, 18, 28]   # BNO085+ADS1115+外置方案
    v6 = [20, 0,  22, 18, 16]   # LSM6DSV16X($2-4≈¥14-28取20)+内ADC移除
    x = np.arange(len(cats))
    w = 0.38
    b1 = ax.bar(x - w/2, v5, w, label='V5 (BNO085 + ADS1115 外置方案)', color=TERRA)
    b2 = ax.bar(x + w/2, v6, w, label='V6 (LSM6DSV16X + 内置 ADC)', color=TEAL)
    for bars in (b1,b2):
        for b in bars:
            if b.get_height() > 0:
                ax.text(b.get_x()+b.get_width()/2, b.get_height()+1.2,
                        f'¥{int(b.get_height())}', ha='center', va='bottom',
                        fontsize=9, color=INK, fontweight='bold')
    ax.set_xticks(x); ax.set_xticklabels(cats, fontsize=11)
    ax.set_ylabel('单件成本 (元)', fontsize=11, color=INK)
    ax.set_title('图3  V5 → V6 BOM 成本对比 (双手套系统关键项)', fontsize=13, fontweight='bold', pad=12, color=INK)
    ax.set_ylim(0, 90)
    ax.legend(loc='upper right', frameon=False, fontsize=10)
    ax.yaxis.grid(True, color='#EEF1F2', linewidth=0.8)
    ax.set_axisbelow(True)
    style_ax(ax)
    # 标注节省
    total_v5, total_v6 = sum(v5), sum(v6)
    ax.annotate(f'双手套关键项合计: ¥{total_v5} → ¥{total_v6}  (节省 ¥{total_v5-total_v6}, 降幅 {(1-total_v6/total_v5)*100:.0f}%)',
                xy=(0.5, 0.96), xycoords='axes fraction', ha='center', fontsize=10,
                color=TEAL, fontweight='bold',
                bbox=dict(boxstyle='round,pad=0.4', fc='#E6F2F4', ec=TEAL, lw=0.8))
    fig.text(0.99, 0.01, '数据来源: research_1 (BNO085≈¥70 / ADS1115≈¥32 / LSM6DSV16X $2-4)',
             ha='right', va='bottom', fontsize=7.5, color=GREY, style='italic')
    save(fig, 'fig_bom_cost.png')

# ============================================================
# 图4: 融资用途分配 (饼图)
# 数据: BP 8.3 (种子轮 800-1200万)
# ============================================================
def fig_funding_use():
    fig, ax = plt.subplots(figsize=(8.4, 6.4))
    labels = ['硬件研发与量产准备\n(Pro/Lite 双线)', '算法研发\n(Tier1-3 推理模型)',
              '数据采集与标注体系', 'SDK 开发与生态建设\n(ROS2/Unity)', '运营与团队扩充']
    sizes = [30, 25, 20, 15, 10]
    colors = [TEAL, SAGE, STEEL, TERRA, BROWN]
    explode = [0.04, 0, 0, 0, 0]
    wedges, texts, autotexts = ax.pie(
        sizes, labels=labels, colors=colors, explode=explode,
        autopct='%1.0f%%', startangle=90, pctdistance=0.78,
        wedgeprops=dict(width=0.42, edgecolor='white', linewidth=2),
        textprops=dict(fontsize=10, color=INK))
    for at in autotexts:
        at.set_color('white'); at.set_fontweight('bold'); at.set_fontsize(11)
    ax.set_title('图4  种子轮融资用途分配规划 (目标 800—1200 万元)', fontsize=13, fontweight='bold', pad=14, color=INK)
    ax.text(0, 0, '种子轮\n800-1200\n万元', ha='center', va='center', fontsize=11, fontweight='bold', color=TEAL)
    fig.text(0.99, 0.01, '数据来源: BP 第8章 融资用途规划',
             ha='right', va='bottom', fontsize=7.5, color=GREY, style='italic')
    save(fig, 'fig_funding_use.png')

# ============================================================
# 图5: 三年研发路线图 (甘特图)
# 数据: BP 第7章
# ============================================================
def fig_roadmap_gantt():
    fig, ax = plt.subplots(figsize=(12, 5.8))
    # (任务, 起始季度, 持续季度, 阶段色)
    tasks = [
        ('V7 架构启动 / Pro 架构定型',          0, 1, TEAL),
        ('Lite 原型定型 + 双表示层验证',         1, 2, TEAL),
        ('ROS2 SDK 骨架 + LSM6DSV16X 驱动攻关',  0, 3, SAGE),
        ('MediaPipe+glove 融合 / 连续手语benchmark', 1, 3, SAGE),
        ('Pro 原型 + EGO Camera 接口 (D9)',      2, 3, STEEL),
        ('机器人遥操作 / VLA 数据采集联调',      4, 4, TERRA),
        ('Open Core 数据平台上线',               4, 4, TERRA),
        ('首批商业行业数据集',                    5, 3, BROWN),
        ('Tier2 推理模型发布',                   5, 2, BROWN),
        ('Human Hand Foundation Model 预研',     7, 3, GREY),
    ]
    ylabels = [t[0] for t in tasks]
    for i, (name, start, dur, c) in enumerate(tasks):
        ax.barh(i, dur, left=start, height=0.55, color=c, edgecolor='white', linewidth=1.2)
        ax.text(start + dur/2, i, f'{dur}Q', ha='center', va='center', fontsize=8.5, color='white', fontweight='bold')
    ax.set_yticks(range(len(tasks)))
    ax.set_yticklabels(ylabels, fontsize=9.5, color=INK)
    ax.invert_yaxis()
    ax.set_xlim(0, 10)
    ax.set_xticks(range(0, 11))
    xtl = []
    for q in range(3,5): xtl.append(f'2026Q{q}')
    for q in range(1,5): xtl.append(f'2027Q{q}')
    for q in range(1,3): xtl.append(f'2028Q{q}')
    # 补齐到11个 (2028Q3,Q4,Q5占位)
    xtl += ['2028Q3','2028Q4','2029Q1']
    ax.set_xticklabels(xtl, fontsize=8.5, rotation=0)
    ax.set_title('图5  EchoGlove 三年研发路线图 (2026 Q3 — 2028)', fontsize=13, fontweight='bold', pad=12, color=INK)
    ax.xaxis.grid(True, color='#EEF1F2', linewidth=0.8)
    ax.set_axisbelow(True)
    ax.spines['top'].set_visible(False)
    ax.spines['right'].set_visible(False)
    ax.spines['left'].set_color('#BFC4C9')
    ax.spines['bottom'].set_color('#BFC4C9')
    # 阶段分隔线
    ax.axvline(3, color='#C0504D', linestyle='--', linewidth=1, alpha=0.5)
    ax.axvline(7, color='#C0504D', linestyle='--', linewidth=1, alpha=0.5)
    ax.text(1.5, -0.8, '阶段一: 量产准备', ha='center', fontsize=9, color=TEAL, fontweight='bold')
    ax.text(5.0, -0.8, '阶段二: 平台化', ha='center', fontsize=9, color=TERRA, fontweight='bold')
    ax.text(8.5, -0.8, '阶段三: 基础模型', ha='center', fontsize=9, color=GREY, fontweight='bold')
    fig.text(0.99, 0.01, '数据来源: BP 第7章 研发路线图 (真实性: 工程可实现 / 需研发验证 / 长期方向)',
             ha='right', va='bottom', fontsize=7.5, color=GREY, style='italic')
    save(fig, 'fig_roadmap_gantt.png')

# ============================================================
# 图6: 力/触觉覆盖矩阵 (热力图) — 核心差异化图
# 数据: research_3 全表 15 平台
# ============================================================
def fig_force_gap_matrix():
    fig, ax = plt.subplots(figsize=(11, 7.2))
    platforms = ['UMI', 'ALOHA/Mobile', 'GELLO', 'DexCap', 'AnyTeleop',
                 'TeleMoMa', 'Open TeleVision', 'Franka/DROID', 'RH20T',
                 'Baxter', 'LEAP Hand', 'OpenX/RT-X', 'π0',
                 'Manus (手套)', 'EchoGlove (手套)']
    # 力/触觉来源: 0=无, 1=机器人侧, 2=人手侧(EchoGlove独有)
    force = [0,0,0,0,0,0,0,1,2,1,1,0,0,0,2]
    colors_map = {0: LGREY, 1: TERRA, 2: TEAL}
    cell_colors = [colors_map[v] for v in force]
    labels_map = {0: '无', 1: '机器人侧', 2: '人手侧 ★'}
    for i, (p, v) in enumerate(zip(platforms, force)):
        rect = Rectangle((0, len(platforms)-1-i), 1, 1, facecolor=colors_map[v], edgecolor='white', linewidth=2)
        ax.add_patch(rect)
        ax.text(0.5, len(platforms)-1-i+0.5, labels_map[v], ha='center', va='center',
                fontsize=10, color='white' if v else INK, fontweight='bold' if v else 'normal')
    ax.set_xlim(0, 1); ax.set_ylim(0, len(platforms))
    ax.set_yticks([i+0.5 for i in range(len(platforms))])
    ax.set_yticklabels(platforms[::-1], fontsize=10, color=INK)
    ax.set_xticks([])
    ax.set_title('图6  机器人遥操作/数据采集平台 力/触觉信号覆盖矩阵', fontsize=13, fontweight='bold', pad=12, color=INK)
    # 图例
    from matplotlib.patches import Patch
    legend = [Patch(facecolor=LGREY, label='无 力/触觉信号'),
              Patch(facecolor=TERRA, label='机器人侧力传感 (关节扭矩/F/T)'),
              Patch(facecolor=TEAL, label='人手侧 力/触觉流 (EchoGlove 独有)')]
    ax.legend(handles=legend, loc='upper center', bbox_to_anchor=(0.5, -0.02),
              ncol=3, frameon=False, fontsize=9.5)
    ax.spines['top'].set_visible(False); ax.spines['right'].set_visible(False)
    ax.spines['left'].set_visible(False); ax.spines['bottom'].set_visible(False)
    # 核心结论标注
    ax.annotate('15 平台中仅 4 家有力信号\n且全部为机器人侧\n→ 人手侧力/触觉 = 结构性空白',
                xy=(1.02, 4.5), xytext=(1.15, 7),
                fontsize=9.5, color=TEAL, fontweight='bold',
                arrowprops=dict(arrowstyle='->', color=TEAL, lw=1.2),
                bbox=dict(boxstyle='round,pad=0.4', fc='#E6F2F4', ec=TEAL, lw=0.8))
    fig.text(0.99, 0.01, '数据来源: research_3 全表 15 平台 (Baxter/Franka/RH20T/LEAP 机器人侧力传感)',
             ha='right', va='bottom', fontsize=7.5, color=GREY, style='italic')
    save(fig, 'fig_force_gap_matrix.png')

# ============================================================
# 图7: 竞品价格带断层 (横向条形图)
# 数据: research_1+3 公开价格
# ============================================================
def fig_price_band():
    fig, ax = plt.subplots(figsize=(11, 6.6))
    # (产品, 价格USD, 类别)
    items = [
        ('lucidgloves (DIY)', 60, '开源DIY'),
        ('GELLO', 280, '低成本档'),
        ('EchoGlove Lite (目标)', 70, '★ EchoGlove 卡位'),
        ('EchoGlove Pro (目标)', 280, '★ EchoGlove 卡位'),
        ('StretchSense', 895, '消费级'),
        ('Rokoko Smartgloves II', 2500, '中端企业'),
        ('SenseGlove Nova 2', 5800, '高端企业'),
        ('Manus Metagloves Pro', 5000, '高端企业'),
        ('HaptX G1', 5000, '高端企业'),
        ('UMI', 2195, '遥操作平台'),
        ('ALOHA', 20000, '遥操作平台'),
        ('Franka Panda', 35000, '遥操作平台'),
    ]
    items_sorted = sorted(items, key=lambda x: x[1])
    names = [i[0] for i in items_sorted]
    prices = [i[1] for i in items_sorted]
    cats = [i[2] for i in items_sorted]
    cat_color = {'开源DIY': BROWN, '低成本档': SAGE, '★ EchoGlove 卡位': TEAL,
                 '消费级': STEEL, '中端企业': SAGE, '高端企业': TERRA,
                 '遥操作平台': GREY}
    colors = [cat_color[c] for c in cats]
    y = np.arange(len(names))
    bars = ax.barh(y, prices, color=colors, edgecolor='white', linewidth=0.8, height=0.7)
    for b, p in zip(bars, prices):
        ax.text(p + 200, b.get_y()+b.get_height()/2, f'${p:,}', va='center', fontsize=9, color=INK, fontweight='bold')
    ax.set_yticks(y); ax.set_yticklabels(names, fontsize=9.5, color=INK)
    ax.set_xscale('symlog', linthresh=100)
    ax.set_xlim(0, 50000)
    ax.set_xlabel('单套价格 (USD, 对数轴)', fontsize=11, color=INK)
    ax.set_title('图7  竞品价格带断层 — $200—800 成品化消费级区间几乎无人占据', fontsize=13, fontweight='bold', pad=12, color=INK)
    # 卡位区间高亮
    ax.axvspan(200, 800, alpha=0.10, color=TEAL, zorder=0)
    ax.text(400, len(names)-0.5, 'EchoGlove\n卡位区间\n$200—800', ha='center', va='top',
            fontsize=9, color=TEAL, fontweight='bold')
    ax.xaxis.grid(True, color='#EEF1F2', linewidth=0.8)
    ax.set_axisbelow(True)
    style_ax(ax)
    from matplotlib.patches import Patch
    legend = [Patch(facecolor=TEAL, label='★ EchoGlove 卡位'),
              Patch(facecolor=SAGE, label='低成本/中端'),
              Patch(facecolor=TERRA, label='高端企业'),
              Patch(facecolor=STEEL, label='消费级'),
              Patch(facecolor=GREY, label='遥操作平台'),
              Patch(facecolor=BROWN, label='开源DIY')]
    ax.legend(handles=legend, loc='lower right', frameon=False, fontsize=8.5)
    fig.text(0.99, 0.01, '数据来源: research_1+3 公开价格 (未公开项未列入; 询价项未列入)',
             ha='right', va='bottom', fontsize=7.5, color=GREY, style='italic')
    save(fig, 'fig_price_band.png')

# ============================================================
# 图8: 双表示层数据流 (架构示意图)
# 数据: BP 3.2
# ============================================================
def fig_dual_rep_layer():
    fig, ax = plt.subplots(figsize=(12, 6.4))
    ax.set_xlim(0, 12); ax.set_ylim(0, 6.4)
    ax.axis('off')
    # 输入层
    inputs = [('Flex/eSkin\n形变', 0.3, 4.8), ('IMU\n6/9DoF', 0.3, 3.6),
              ('Force\n力/触觉', 0.3, 2.4), ('EGO Camera\n视觉', 0.3, 1.2)]
    for name, x, y in inputs:
        ax.add_patch(FancyBboxPatch((x, y), 1.6, 0.9, boxstyle='round,pad=0.05',
                     facecolor=LGREY, edgecolor=GREY, linewidth=1))
        ax.text(x+0.8, y+0.45, name, ha='center', va='center', fontsize=9, color=INK)
    # 边缘计算
    ax.add_patch(FancyBboxPatch((2.4, 2.0), 1.8, 2.4, boxstyle='round,pad=0.05',
                 facecolor='#E6F2F4', edgecolor=TEAL, linewidth=1.5))
    ax.text(3.3, 3.2, '边缘计算\nESP32-S3/P4\nFreeRTOS\nTinyML 预处理', ha='center', va='center',
            fontsize=9, color=TEAL, fontweight='bold')
    # Hand State Token
    ax.add_patch(FancyBboxPatch((4.6, 2.4), 2.0, 1.6, boxstyle='round,pad=0.08',
                 facecolor=TEAL, edgecolor=INK, linewidth=1.5))
    ax.text(5.6, 3.2, 'Hand State\nToken\n(统一中间表示)', ha='center', va='center',
            fontsize=10.5, color='white', fontweight='bold')
    # 双输出
    ax.add_patch(FancyBboxPatch((7.2, 4.0), 2.2, 1.6, boxstyle='round,pad=0.08',
                 facecolor='#E6F2F4', edgecolor=TEAL, linewidth=1.5))
    ax.text(8.3, 4.8, 'Layer A\nMANO 数字人表示\nβ/θ + 21关节 + 网格', ha='center', va='center',
            fontsize=8.5, color=TEAL, fontweight='bold')
    ax.add_patch(FancyBboxPatch((7.2, 0.8), 2.2, 1.6, boxstyle='round,pad=0.08',
                 facecolor='#FBEFE2', edgecolor=TERRA, linewidth=1.5))
    ax.text(8.3, 1.6, 'Layer B\n机器人动作表示\n关节角 + 6DoF腕 + 力', ha='center', va='center',
            fontsize=8.5, color=TERRA, fontweight='bold')
    # 应用层
    apps_a = ['Unity/Unreal', 'XR/数字人', '手语动画']
    apps_b = ['ROS2/机械臂', 'VLA/RL/ACT', '灵巧手/夹爪']
    for i, a in enumerate(apps_a):
        ax.add_patch(FancyBboxPatch((9.8, 4.6-i*0.55), 2.0, 0.45, boxstyle='round,pad=0.03',
                     facecolor='white', edgecolor=TEAL, linewidth=1))
        ax.text(10.8, 4.83-i*0.55, a, ha='center', va='center', fontsize=8, color=TEAL)
    for i, a in enumerate(apps_b):
        ax.add_patch(FancyBboxPatch((9.8, 1.9-i*0.55), 2.0, 0.45, boxstyle='round,pad=0.03',
                     facecolor='white', edgecolor=TERRA, linewidth=1))
        ax.text(10.8, 2.13-i*0.55, a, ha='center', va='center', fontsize=8, color=TERRA)
    # 箭头
    for _, x, y in inputs:
        ax.add_patch(FancyArrowPatch((1.9, y+0.45), (2.4, 3.2), arrowstyle='->', color=GREY, lw=1))
    ax.add_patch(FancyArrowPatch((4.2, 3.2), (4.6, 3.2), arrowstyle='->', color=INK, lw=1.5))
    ax.add_patch(FancyArrowPatch((6.6, 3.5), (7.2, 4.8), arrowstyle='->', color=TEAL, lw=1.5))
    ax.add_patch(FancyArrowPatch((6.6, 2.9), (7.2, 1.6), arrowstyle='->', color=TERRA, lw=1.5))
    for i in range(3):
        ax.add_patch(FancyArrowPatch((9.4, 4.83-i*0.55), (9.8, 4.83-i*0.55), arrowstyle='->', color=TEAL, lw=1))
        ax.add_patch(FancyArrowPatch((9.4, 2.13-i*0.55), (9.8, 2.13-i*0.55), arrowstyle='->', color=TERRA, lw=1))
    ax.set_title('图8  双表示层架构 — 同一硬件/数据源, MANO 与 Robot Action 双输出', fontsize=13, fontweight='bold', pad=8, color=INK)
    fig.text(0.99, 0.01, '数据来源: BP 3.2 双表示层 (D3 战略冻结点)',
             ha='right', va='bottom', fontsize=7.5, color=GREY, style='italic')
    save(fig, 'fig_dual_rep_layer.png')

if __name__ == '__main__':
    print('生成科研制图...')
    fig_market_size()
    fig_competitive_radar()
    fig_bom_cost()
    fig_funding_use()
    fig_roadmap_gantt()
    fig_force_gap_matrix()
    fig_price_band()
    fig_dual_rep_layer()
    print('全部完成')
