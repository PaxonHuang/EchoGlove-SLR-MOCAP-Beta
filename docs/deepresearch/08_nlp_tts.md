# 08 · SLR 下游：规则语法校正 + edge-tts 语音合成

> **Date**: 2026-08-11 ｜ **依据**: `glove_relay/src/nlp/grammar_corrector.py` + `glove_relay/src/tts/tts_engine.py` ｜ **真实性**: 结构 ✅ / 规则覆盖 🔬 / TTS ✅(依赖在线服务)

---

## 1. 角色：手语 → 通顺中文 → 语音

46 类手势序列 → **CSL（中国手语）语法校正**为通顺普通话 → **edge-tts** 合成语音（展示头，不阻塞主识别链）。

```
 gesture_ids[46] ──► GrammarCorrector.correct ──► "我吃苹果" ──► TTSEngine.synthesize ──► MP3 bytes
      │  (含 -1 未知)        规则 DSL 序列             普通话                     zh-CN-XiaoxiaoNeural
```

---

## 2. 语法校正（真实代码，规则式 DSL）

**CSL 与普通话的典型差异**（模块 docstring）：主题前置（"你 名字 什么"）、SOV→SVO（"我 苹果 吃"）、省略系词（"我 学生"）、时间词前置（"昨天 去 学校 我"）。

**规则管线**（`GrammarCorrector._init_rules` 按序 apply）：

| 规则 | 触发 | 变换 | 类 |
|------|------|------|-----|
| 主题-评述插入 | 序列 == ["你","名字","什么"] | 插"叫"于 index1 → 你叫名字什么 | `_InsertWordRule` |
| SOV→SVO | 动词 ∈ {吃,喝,买,…} 且 i>0 | 与前一词（宾语）交换 | `_SwapVerbObjectRule` |
| 系词插入 | 恰好 2 词且首词是主语 | 中间插"是" | `_CopulaInsertionRule` |
| 时间词前置 | 时间词 ∈ {昨天,今天,…} | 全部移到句首 | `_TimeFrontingRule` |
| 体标记 | 动作动词 ∈ {去,来,吃,…} | 动词后插"了" | `_AspectMarkerRule` |
| 句末标点 | 以 什么/吗/呢 结尾 | 替换为 什么？/吗？/呢？ | `_postfix_map` |

```
correct(gesture_ids):
    words = [label(gid) for gid >= 0]          # 过滤未知
    for rule in rules: words = rule.apply(words)   # 规则按序
    sentence = "".join(words)
    for k,v in postfix_map:                      # 句末标点
        if sentence.endswith(k): sentence = ... + v
```

**规则引擎本质**：确定性词序变换 DSL——每规则匹配特定模式做局部重排/插入。**覆盖有限**（无完整句法分析、无语境消歧），对高频手语短语有效，长句/复杂句🔬。

---

## 3. 语音合成（真实代码，edge-tts）

`TTSEngine` 封装 Microsoft Edge 在线 TTS（`edge-tts` 库）：

- **异步**：`async def synthesize(text) → bytes`（MP3）。
- **语音**：`zh-CN-XiaoxiaoNeural`（默认，config 可调 rate/volume）。
- **本地缓存**：键 = `sha256(f"{voice}:{text}")[:16]`，`cache_dir/*.mp3`，命中跳过合成。

$$
\text{cache\_key} = \text{sha256}_{16}\big(\text{voice}\,:\,\text{text}\big)
$$

```
synthesize(text):
    if empty: return b""
    key = cache_key(text, voice)
    if cached & exists: return cached bytes            # 缓存命中
    async for chunk in edge_tts.Communicate(text, voice, rate, volume).stream():
        if chunk.type == "audio": audio += chunk.data
    write cache_dir/{key}.mp3;  return audio
```

**依赖**：在线服务（`edge-tts` 连 Microsoft 端点）——**网络依赖**；离线/嵌入场景需替换为本地 TTS（🔬/🌌）。

---

## 4. 端到端延迟与展示头定位

```
手势 → 校正(μs级, 纯规则) → TTS(数百 ms, 网络往返 + MP3 生成) → 播放
```

- 校正为 CPU 纯规则（可忽略延迟）✅；TTS 为网络往返（主导延迟），且有缓存优化。
- **定位**：NLP/TTS 是 SLR **下游展示头**，不阻塞 46 类识别主链（生产设计 §6）。

---

## 5. 现状与真实性

| 能力 | 标注 | 说明 |
|------|------|------|
| 规则语法校正（CSL→普通话） | ✅ 结构 | 代码 + 规则 DSL；`gesture_labels.json` 驱动 |
| 规则覆盖/精度 | 🔬 | 仅高频短语模式；复杂句需语料驱动方法 |
| edge-tts 合成 + 缓存 | ✅ 结构 | 在线依赖；缓存规避重复合成 |
| 离线 TTS / 端侧语音 | 🌌 | 在线服务无法离线；嵌入式 TTS 属后续 |

---

*本文档规则表与缓存公式为真实代码；覆盖度与端侧 TTS 属 🔬/🌌。*
