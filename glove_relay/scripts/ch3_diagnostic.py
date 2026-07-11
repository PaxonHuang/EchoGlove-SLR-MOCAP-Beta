#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""ch3 诊断：~15s 读 $EG，报告 5 通道 swing，判定 4/6 字母。"""
import serial, time, statistics

PORT = "/dev/ttyACM0"
DURATION = 15.0
PREFIX = "$EG,"

print(f"ch3 诊断：{DURATION:.0f}s 内 open↔fist 反复数次，5 通道 swing 报告")
print(">>> 现在开始，张开↔握拳反复 5-6 次，尽量到极端")
for i in range(3, 0, -1):
    print(f"    {i}…", end="\r", flush=True)
    time.sleep(1)
print("    ● CAPTURING")

ser = serial.Serial(PORT, 115200, timeout=1.0)
cols = [[] for _ in range(5)]
end = time.time() + DURATION
n = 0
while time.time() < end:
    raw = ser.readline()
    if not raw:
        continue
    s = raw.decode("ascii", errors="replace").strip()
    if not s.startswith(PREFIX):
        continue
    parts = s[len(PREFIX):].split(",")
    if len(parts) < 5:
        continue
    try:
        v = [float(p) for p in parts[:5]]
    except ValueError:
        continue
    for i in range(5):
        cols[i].append(v[i])
    n += 1
ser.close()

names = ["thumb", "index", "middle", "ring ", "pinky"]
print(f"\n捕获 {n} 帧\n")
print(f"  ch  name    min     max     swing   swing_cnt  verdict")
THR = 0.05  # /4095 scale → 205 cnt
ring_ok = False
for i in range(5):
    if not cols[i]:
        print(f"  ch{i} {names[i]}  无数据")
        continue
    lo, hi = min(cols[i]), max(cols[i])
    sw = hi - lo
    cnt = int(sw * 4095)
    ok = "✓ OK" if sw >= THR else "✗ 太小"
    if i == 3:
        ring_ok = sw >= THR
        ok = "✓ ring 可用" if ring_ok else "✗ ring 故障"
    print(f"  ch{i} {names[i]}  {lo:.3f}  {hi:.3f}  {sw:.3f}   {cnt:5d}     {ok}")

print()
if ring_ok:
    print("判定：ch3 ring 可用 → 6 字母 (A/B/I/L/W/Y) 路径")
else:
    print("判定：ch3 ring 仍故障 → 保持 4 字母 (A/B/I/L)")
