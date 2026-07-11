#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
glove_relay/scripts/demo_server.py — Competition demo standalone bridge.

Standalone async server (does NOT import / touch src/main.py fragile lifespan):
    S3 glove (flex=ADC1 real, IMU=zeros)
        --USB Serial ASCII-->  this server
        -> RuleClassifier (5-flex thresholds)
        -> V5 dual-hand JSON
        -> ws://localhost:8765/ws  ->  React web (glove_web)

Demo ASCII line from firmware (glove_firmware/src/main.cpp, every 3rd frame):
    $EG,f0,f1,f2,f3,f4,tick\n     (f0..f4 normalized 0=straight..1=bent)

Reserved transport — UDP protobuf path (same WS output):
    S3 --ReceiverPacket protobuf--> this server UDP:8888 listener.
    Reuses proto/glove_data_pb2. Active only if --udp is given or a packet
    arrives; the ASCII path stays primary for the wired demo.

Run:
    /home/paxon/anaconda3/envs/pytorch_env/bin/python scripts/demo_server.py
    (then: cd glove_web && npm run dev)
"""
from __future__ import annotations

import argparse
import asyncio
import logging
import os
import sys
import time
from pathlib import Path
from typing import Any, Dict, Optional, Set

# ── Make src/ + proto/ importable when run as a script ────────────────────
_HERE = Path(__file__).resolve().parent               # .../glove_relay/scripts
_RELAY_ROOT = _HERE.parent                             # .../glove_relay
sys.path.insert(0, str(_RELAY_ROOT))                   # for `src...`
sys.path.insert(0, str(_RELAY_ROOT / "proto"))         # for glove_data_pb2

import websockets                                       # noqa: E402
from websockets.server import serve                     # noqa: E402

from src.inference.asl_classifier import ASLClassifier, DEFAULT_CALIBRATION_PATH  # noqa: E402
from src.inference.rule_classifier import RuleClassifier   # noqa: E402


def build_classifier(args: argparse.Namespace):
    """Pick the ASL template classifier if a calibration file exists, else the
    threshold rule classifier. Returns (classifier, kind, normalize_before).

    The ASL classifier re-normalizes each raw 5-channel frame with the captured
    per-channel range, then projects onto the 4 working channels (ring ch3 is a
    hardware fault — masked out) before matching A/B/I/L. The rule classifier
    expects already-normalized 0..1 input (firmware's raw/4095 fallback —
    mediocre but works once the ASL path is calibrated away).
    """
    calib = Path(args.calibration)
    if calib.exists():
        clf = ASLClassifier()
        if clf.load(calib):
            kind = "asl" + ("+captured" if clf.use_capt else "+theoretical")
            logger.info("Classifier: ASL 4-letter A/B/I/L (%s), calibration=%s", kind, calib)
            if clf.has_range:
                logger.info("  range min=%s max=%s polarity=%s",
                            [round(x, 3) for x in clf.range_min],
                            [round(x, 3) for x in clf.range_max], clf.polarity)
            return clf, kind, True
        logger.warning("Calibration file unreadable (%s); falling back to rules", calib)
    logger.info("Classifier: rule-based (no calibration — run calibrate_demo.py)")
    return RuleClassifier(), "rule", False

# Optional serial-asyncio (only needed for the live USB path)
try:
    import serial_asyncio  # type: ignore
    _HAS_SERIAL = True
except Exception:  # pragma: no cover - dep may be absent in some envs
    _HAS_SERIAL = False

logger = logging.getLogger("demo_server")

# ── Defaults ───────────────────────────────────────────────────────────────
DEFAULT_WS_HOST = "0.0.0.0"
DEFAULT_WS_PORT = 8765
DEFAULT_SERIAL_PORT = "/dev/ttyACM0"
DEFAULT_SERIAL_BAUD = 115200          # ARDUINO_USB_CDC_ON_BOOT Serial is not a UART; baud is symbolic
DEFAULT_UDP_PORT = 8888               # reserved ReceiverPacket path
ASCII_PREFIX = "$EG,"

ZERO_FLEX = [0.0, 0.0, 0.0, 0.0, 0.0]
ZERO_EULER = [0.0, 0.0, 0.0]
ZERO_GYRO = [0.0, 0.0, 0.0]
ZERO_RELATIVE = {"delta_euler": [0.0, 0.0, 0.0], "delta_quat_dist": 0.0,
                 "delta_gyro_norm": 0.0, "delta_gyro_axis": 0.0}


# ============================================================================
# WebSocket hub — broadcasts V5 JSON to every connected browser
# ============================================================================
class WSHub:
    def __init__(self) -> None:
        self.clients: Set[Any] = set()

    async def handler(self, websocket: Any) -> None:
        self.clients.add(websocket)
        peer = getattr(websocket, "remote_address", "?")
        logger.info("WS client connected: %s (%d total)", peer, len(self.clients))
        try:
            # Keep the connection open; we only push data, ignore inbound.
            async for _msg in websocket:
                pass
        except Exception as e:
            logger.debug("WS client loop ended: %s", e)
        finally:
            self.clients.discard(websocket)
            logger.info("WS client disconnected (%d remaining)", len(self.clients))

    async def broadcast(self, msg: Dict[str, Any]) -> None:
        if not self.clients:
            return
        payload = __import__("json").dumps(msg, ensure_ascii=False)
        dead = []
        for ws in list(self.clients):
            try:
                await ws.send(payload)
            except Exception:
                dead.append(ws)
        for ws in dead:
            self.clients.discard(ws)


# ============================================================================
# Message builder — emits V5 dual-hand JSON the web frontend expects
# ============================================================================
def build_v5_message(flex: list, result: Dict[str, Any], tick: int) -> Dict[str, Any]:
    """Build a V5SensorMessage (glove_web/src/types/index.ts).

    left_hand carries the real flex (single-glove demo = left).
    right_hand is zeros. inference + nlp_text carry the rule-classified sign.
    """
    gid = int(result.get("gesture_id", 0))
    conf = float(result.get("confidence", 0.0))
    text = str(result.get("text", "无手势"))
    return {
        "tier": "tier1",
        "blend_alpha": 1.0,
        "left_hand": {
            "flex": [round(float(x), 3) for x in flex],
            "euler": list(ZERO_EULER),
            "gyro": list(ZERO_GYRO),
            "gesture_id": gid,
            "confidence": round(conf, 3),
        },
        "right_hand": {
            "flex": list(ZERO_FLEX),
            "euler": list(ZERO_EULER),
            "gyro": list(ZERO_GYRO),
            "gesture_id": 0,
            "confidence": 0.0,
        },
        "relative": dict(ZERO_RELATIVE),
        "inference": {
            "gesture_id": gid,
            "confidence": round(conf, 3),
            "text": text,
        },
        "nlp_text": text,
        "tick_id": tick,
        "timestamp": time.time(),
    }


# ============================================================================
# ASCII parser — $EG,f0,f1,f2,f3,f4,tick
# ============================================================================
def parse_ascii_line(line: str) -> Optional[tuple]:
    """Parse one firmware ASCII line. Returns (flex[5], tick) or None."""
    s = line.strip()
    if not s or not s.startswith(ASCII_PREFIX):
        return None
    body = s[len(ASCII_PREFIX):]
    parts = body.split(",")
    if len(parts) < 5:
        return None
    try:
        flex = [float(p) for p in parts[:5]]
    except ValueError:
        return None
    tick = 0
    if len(parts) >= 6:
        try:
            tick = int(parts[5])
        except ValueError:
            tick = 0
    return flex, tick


# ============================================================================
# Serial reader — pyserial-asyncio line stream from the S3 USB CDC
# ============================================================================
class SerialReader:
    def __init__(self, port: str, baud: int, on_line, on_raw: Optional[Any] = None) -> None:
        self.port = port
        self.baud = baud
        self.on_line = on_line
        self.on_raw = on_raw
        self._running = False
        self._buf = bytearray()

    async def run(self) -> None:
        if not _HAS_SERIAL:
            logger.error("pyserial-asyncio not installed — cannot read %s", self.port)
            return
        self._running = True
        retry = 0
        while self._running:
            try:
                reader, _writer = await serial_asyncio.open_serial_connection(
                    url=self.port, baudrate=self.baud
                )
                logger.info("Serial connected: %s @ %d", self.port, self.baud)
                retry = 0
                while self._running:
                    chunk = await reader.read(256)
                    if not chunk:
                        break
                    if self.on_raw:
                        self.on_raw(chunk)
                    self._buf.extend(chunk)
                    # Split on newlines, process complete lines.
                    while b"\n" in self._buf:
                        nl = self._buf.index(b"\n")
                        raw_line = self._buf[:nl]
                        del self._buf[:nl + 1]
                        try:
                            line = raw_line.decode("ascii", errors="replace")
                        except Exception:
                            continue
                        await self.on_line(line)
            except Exception as e:
                retry += 1
                wait = min(2 ** min(retry, 5), 10)
                logger.warning("Serial error (%s); reconnecting in %ds (try %d)", e, wait, retry)
                await asyncio.sleep(wait)

    def stop(self) -> None:
        self._running = False


# ============================================================================
# UDP listener — RESERVED ReceiverPacket protobuf path (wireless upgrade)
# ============================================================================
class UDPListener:
    """Accepts V5 ReceiverPacket protobuf on UDP :8888.

    Reservation only — the ASCII USB path is primary for the wired demo.
    If a packet arrives, it is parsed and pushed to the same WS hub so the
    wireless S3→UDP path works without code changes.
    """

    def __init__(self, port: int, on_packet) -> None:
        self.port = port
        self.on_packet = on_packet
        self._running = False

    async def run(self) -> None:
        self._running = True
        try:
            import glove_data_pb2 as pb  # type: ignore
        except Exception as e:
            logger.warning("UDP path disabled (protobuf import failed: %s)", e)
            return
        loop = asyncio.get_running_loop()
        transport, _proto = await loop.create_datagram_endpoint(
            lambda: _UDPP(self.on_packet, pb),
            local_addr=("0.0.0.0", self.port),
        )
        logger.info("UDP listener (reserved) on 0.0.0.0:%d", self.port)
        try:
            while self._running:
                await asyncio.sleep(0.5)
        except asyncio.CancelledError:
            pass
        finally:
            transport.close()

    def stop(self) -> None:
        self._running = False


class _UDPP(asyncio.DatagramProtocol):
    def __init__(self, on_packet, pb) -> None:
        self.on_packet = on_packet
        self.pb = pb

    def connection_made(self, transport: asyncio.BaseTransport) -> None:
        self.transport = transport  # type: ignore

    def datagram_received(self, data: bytes, addr: tuple) -> None:
        try:
            rp = self.pb.ReceiverPacket()
            rp.ParseFromString(data)
            left = list(rp.left.flex)
            if len(left) >= 5:
                asyncio.ensure_future(self.on_packet(left, int(rp.tick_id)))
        except Exception as e:
            logger.debug("UDP packet parse failed from %s: %s", addr, e)


# ============================================================================
# Main
# ============================================================================
async def amain(args: argparse.Namespace) -> None:
    logging.basicConfig(
        level=logging.INFO,
        format="%(asctime)s %(levelname)-5s %(name)s: %(message)s",
        datefmt="%H:%M:%S",
    )
    logger.info("=== EchoGlove demo_server ===")
    logger.info("WS:   ws://%s:%d/ws  (broadcasts V5 JSON)", args.ws_host, args.ws_port)
    logger.info("SER:  %s @ %d  (ASCII $EG,...)", args.serial_port, args.serial_baud)
    logger.info("UDP:  0.0.0.0:%d  (reserved ReceiverPacket path)", args.udp_port)

    hub = WSHub()
    classifier, clf_kind, renorm = build_classifier(args)

    stats = {"frames": 0, "last_label": ""}

    async def on_ascii_line(line: str) -> None:
        parsed = parse_ascii_line(line)
        if parsed is None:
            return
        flex, tick = parsed
        if renorm:
            result = classifier.classify_raw(flex)
        else:
            result = classifier.classify(flex)
        msg = build_v5_message(flex, result, tick)
        stats["frames"] += 1
        if result["label"] != stats["last_label"]:
            stats["last_label"] = result["label"]
            logger.info("sign: %-10s id=%2d text=%s conf=%.2f d=%s (frame#%d)",
                        result["label"], result["gesture_id"], result["text"],
                        result["confidence"], result.get("distance", "-"),
                        stats["frames"])
        await hub.broadcast(msg)

    async def on_udp_packet(flex: list, tick: int) -> None:
        if renorm:
            result = classifier.classify_raw(flex)
        else:
            result = classifier.classify(flex)
        msg = build_v5_message(flex, result, tick)
        logger.info("UDP sign: %s (tick %d)", result["text"], tick)
        await hub.broadcast(msg)

    # ── WebSocket server ───────────────────────────────────────────────────
    ws_server = await serve(hub.handler, args.ws_host, args.ws_port,
                            ping_interval=20, ping_timeout=20)
    logger.info("WebSocket server listening on %s:%d", args.ws_host, args.ws_port)

    # ── Serial reader (primary) ────────────────────────────────────────────
    reader = SerialReader(args.serial_port, args.serial_baud, on_ascii_line)
    serial_task = asyncio.create_task(reader.run(), name="serial-reader")

    # ── UDP listener (reserved) ────────────────────────────────────────────
    udp = UDPListener(args.udp_port, on_udp_packet)
    udp_task = asyncio.create_task(udp.run(), name="udp-listener")

    # ── Heartbeat ──────────────────────────────────────────────────────────
    async def heartbeat() -> None:
        while True:
            await asyncio.sleep(5)
            logger.info("stats: frames=%d clients=%d last=%s",
                        stats["frames"], len(hub.clients), stats["last_label"] or "-")
    hb_task = asyncio.create_task(heartbeat(), name="heartbeat")

    logger.info("Ready. Open glove_web (npm run dev) → connect → bend fingers.")

    # Run until interrupted
    try:
        await asyncio.Event().wait()
    except asyncio.CancelledError:
        pass
    finally:
        logger.info("shutting down…")
        reader.stop()
        udp.stop()
        hb_task.cancel()
        serial_task.cancel()
        udp_task.cancel()
        ws_server.close()
        await ws_server.wait_closed()
        logger.info("stopped.")


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description="EchoGlove demo standalone bridge")
    p.add_argument("--ws-host", default=DEFAULT_WS_HOST)
    p.add_argument("--ws-port", type=int, default=DEFAULT_WS_PORT)
    p.add_argument("--serial-port", default=os.environ.get("DEMO_SERIAL_PORT", DEFAULT_SERIAL_PORT))
    p.add_argument("--serial-baud", type=int, default=DEFAULT_SERIAL_BAUD)
    p.add_argument("--udp-port", type=int, default=DEFAULT_UDP_PORT)
    p.add_argument("--calibration", default=os.environ.get(
        "DEMO_CALIBRATION", str(DEFAULT_CALIBRATION_PATH)),
        help="path to demo_calibration.json (produced by calibrate_demo.py)")
    return p.parse_args()


if __name__ == "__main__":
    try:
        asyncio.run(amain(parse_args()))
    except KeyboardInterrupt:
        print("\nbye.")
