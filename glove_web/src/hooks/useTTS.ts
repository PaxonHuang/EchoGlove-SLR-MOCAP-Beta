// ── useTTS: Web Speech API 语音合成 ──
// 点击文字按钮播放对应英文语音：CSL 手语播英文 nameEn（You/Good/...），
// ASL 字母播字母名（A..Y）。全部 en-US。
// 浏览器原生 SpeechSynthesis，零依赖；自动取消上一次、重置语音队列防重叠。

import { useCallback, useEffect, useRef } from 'react';

interface SpeakOptions {
  lang?: string;   // BCP-47，默认 'en-US'
  rate?: number;   // 语速 0.1–10，默认 1
  pitch?: number;  // 音高 0–2，默认 1
}

function pickVoice(lang: string): SpeechSynthesisVoice | undefined {
  if (typeof window === 'undefined' || !window.speechSynthesis) return undefined;
  const voices = window.speechSynthesis.getVoices();
  if (voices.length === 0) return undefined;
  // 精确语言前缀匹配优先，否则回退到任意 en 语种
  const prefix = lang.split('-')[0].toLowerCase();
  return (
    voices.find(v => v.lang === lang) ??
    voices.find(v => v.lang.toLowerCase().startsWith(prefix)) ??
    voices.find(v => v.lang.toLowerCase().startsWith(prefix === 'zh' ? 'cmn' : prefix))
  );
}

export function useTTS() {
  // 语音列表异步加载，缓存最新引用
  const voicesRef = useRef<SpeechSynthesisVoice[]>([]);
  useEffect(() => {
    if (typeof window === 'undefined' || !window.speechSynthesis) return;
    const load = () => { voicesRef.current = window.speechSynthesis.getVoices(); };
    load();
    window.speechSynthesis.onvoiceschanged = load;
    return () => {
      if (window.speechSynthesis) window.speechSynthesis.onvoiceschanged = null;
    };
  }, []);

  const speak = useCallback((text: string, opts: SpeakOptions = {}) => {
    if (typeof window === 'undefined' || !window.speechSynthesis || !text) return;
    const synth = window.speechSynthesis;
    // 取消上一次，防重叠堆积
    synth.cancel();
    const utter = new SpeechSynthesisUtterance(text);
    const lang = opts.lang ?? 'en-US';
    utter.lang = lang;
    utter.rate = opts.rate ?? 0.95;
    utter.pitch = opts.pitch ?? 1;
    const voice = voicesRef.current.length > 0
      ? pickVoice(lang)
      : pickVoice(lang);
    if (voice) utter.voice = voice;
    synth.speak(utter);
  }, []);

  const stop = useCallback(() => {
    if (typeof window !== 'undefined' && window.speechSynthesis) {
      window.speechSynthesis.cancel();
    }
  }, []);

  return { speak, stop, supported: typeof window !== 'undefined' && !!window.speechSynthesis };
}
