"""Generate 46 gesture name PCM files for TTS playback on P4."""
from __future__ import annotations
import os
import subprocess

GESTURE_NAMES = [
    "你好", "谢谢", "对不起", "是", "不是",
    "请", "再见", "早上好", "晚上好", "快乐",
    "悲伤", "生气", "害怕", "惊讶", "吃饭",
    "睡觉", "工作", "学习", "家", "学校",
    "朋友", "家人", "医生", "帮助", "喜欢",
    "不喜欢", "大", "小", "多", "少",
    "好", "坏", "新", "旧", "快",
    "慢", "左", "右", "上", "下",
    "来", "去", "看", "听", "说",
    "想",
]

OUTPUT_DIR = "tts_pcm"
SAMPLE_RATE = 16000

def generate_with_edge_tts(text: str, output_path: str):
    """Use edge-tts to generate audio, then convert to raw PCM."""
    import asyncio
    import edge_tts

    async def _gen():
        communicate = edge_tts.Communicate(text, "zh-CN-XiaoxiaoNeural")
        mp3_path = output_path + ".mp3"
        await communicate.save(mp3_path)
        subprocess.run([
            "ffmpeg", "-y", "-i", mp3_path,
            "-ar", str(SAMPLE_RATE), "-ac", "1", "-f", "s16le",
            output_path
        ], check=True, capture_output=True)
        os.remove(mp3_path)

    asyncio.run(_gen())

def main():
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    for i, name in enumerate(GESTURE_NAMES):
        pcm_path = os.path.join(OUTPUT_DIR, f"{i:02d}.pcm")
        print(f"[{i:02d}] Generating: {name} -> {pcm_path}")
        try:
            generate_with_edge_tts(name, pcm_path)
        except Exception as e:
            print(f"  FAILED: {e}")
    print(f"\nGenerated PCM files in {OUTPUT_DIR}/")

if __name__ == "__main__":
    main()
