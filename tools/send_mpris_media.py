#!/usr/bin/env python3
import argparse
import json
import signal
import subprocess
import sys
import time
from pathlib import Path
from urllib.parse import unquote, urlparse

import dbus
from PIL import Image, ImageOps

MPRIS_PREFIX = "org.mpris.MediaPlayer2."
SOURCE_ID = "linux"


def variant(value):
    return value if not hasattr(value, "variant_level") else value


def get_prop(bus, player, iface, prop):
    obj = bus.get_object(player, "/org/mpris/MediaPlayer2")
    props = dbus.Interface(obj, "org.freedesktop.DBus.Properties")
    return props.Get(iface, prop)


def list_players(bus):
    dbus_obj = bus.get_object("org.freedesktop.DBus", "/org/freedesktop/DBus")
    names = dbus.Interface(dbus_obj, "org.freedesktop.DBus").ListNames()
    players = []
    for name in names:
        if not str(name).startswith(MPRIS_PREFIX):
            continue
        try:
            players.append((str(name), str(get_prop(bus, name, "org.mpris.MediaPlayer2.Player", "PlaybackStatus"))))
        except Exception:
            pass
    return players


def select_player(players):
    return next((p for p in players if p[1] == "Playing"), None) or next((p for p in players if p[1] == "Paused"), None) or (players[0] if players else None)


def first_text(value):
    if isinstance(value, (list, tuple, dbus.Array)):
        value = value[0] if value else ""
    return "" if value is None else str(value)


def number(value):
    try:
        return int(value)
    except Exception:
        return 0


def art_to_hex(art_url):
    if not art_url:
        return None
    parsed = urlparse(str(art_url))
    if parsed.scheme != "file":
        return None
    path = unquote(parsed.path)
    try:
        with Image.open(path) as img:
            w, h = img.size
            side = min(w, h)
            left = (w - side) // 2
            top = (h - side) // 2
            img = img.crop((left, top, left + side, top + side)).resize((32, 32))
            img = ImageOps.autocontrast(img.convert("L")).convert("1", dither=Image.Dither.FLOYDSTEINBERG)
            bits = []
            byte = 0
            count = 0
            for y in range(32):
                for x in range(32):
                    byte = (byte << 1) | (1 if img.getpixel((x, y)) == 0 else 0)
                    count += 1
                    if count == 8:
                        bits.append(f"{byte:02x}")
                        byte = 0
                        count = 0
            return "".join(bits)
    except Exception:
        return None


def media_payload(bus, source_id):
    player = select_player(list_players(bus))
    if not player:
        return {"media": {"id": source_id, "clear": True}}

    name, status = player
    metadata = get_prop(bus, name, "org.mpris.MediaPlayer2.Player", "Metadata")
    title = first_text(metadata.get("xesam:title", ""))
    artist = first_text(metadata.get("xesam:artist", ""))
    pos_ms = max(0, round(number(get_prop(bus, name, "org.mpris.MediaPlayer2.Player", "Position")) / 1000))
    dur_ms = max(0, round(number(metadata.get("mpris:length", 0)) / 1000))
    media = {
        "id": source_id,
        "title": title,
        "artist": artist,
        "status": status,
        "pos": pos_ms,
        "dur": dur_ms,
        "ttl": 5000,
    }
    icon = art_to_hex(first_text(metadata.get("mpris:artUrl", "")))
    if icon:
        media["icon"] = icon
    return {"media": media}


def send_json(payload, passthrough):
    tool = Path(__file__).with_name("send_ble_gatt_notify.py")
    cmd = [sys.executable, str(tool), "--json", json.dumps(payload, ensure_ascii=False, separators=(",", ":")), *passthrough]
    return subprocess.run(cmd, check=False).returncode == 0


def main():
    parser = argparse.ArgumentParser(description="Continuously send Linux MPRIS media to RLCD over BLE.")
    parser.add_argument("--interval", type=float, default=2.0)
    parser.add_argument("--once", action="store_true")
    parser.add_argument("--source-id", default=SOURCE_ID)
    parser.add_argument("--device-name", default=None)
    parser.add_argument("--address", default=None)
    parser.add_argument("--adapter", default=None)
    args = parser.parse_args()

    passthrough = []
    for name in ("device_name", "address", "adapter"):
        value = getattr(args, name)
        if value:
            passthrough += ["--" + name.replace("_", "-"), value]

    running = True

    def stop(_signum, _frame):
        nonlocal running
        running = False

    signal.signal(signal.SIGINT, stop)
    signal.signal(signal.SIGTERM, stop)
    bus = dbus.SessionBus()
    while running:
        send_json(media_payload(bus, args.source_id), passthrough)
        if args.once:
            return 0
        time.sleep(max(1.0, args.interval))
    send_json({"media": {"id": args.source_id, "clear": True}}, passthrough)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
