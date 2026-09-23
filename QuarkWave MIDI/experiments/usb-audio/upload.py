#!/usr/bin/env python3
"""Upload a QuarkWave Uno image from either USB mode on a connected Uno R4 WiFi."""

import json
from pathlib import Path
import subprocess
import sys
import time


FQBN = "arduino:renesas_uno:unor4wifi"
DEFAULT_IMAGE = Path("/private/tmp/quarkwave-usb-audio.bin")


def uno_ports() -> dict[str, str]:
    result = subprocess.run(
        ["arduino-cli", "board", "list", "--format", "json"],
        check=True, capture_output=True, text=True,
    )
    ports = {}
    for found in json.loads(result.stdout).get("detected_ports", []):
        port = found.get("port", {})
        props = port.get("properties", {})
        if props.get("vid") != "0x2341":
            continue
        if props.get("pid") == "0x1002":
            ports["bridge"] = port["address"]
        elif props.get("pid") == "0x006D":
            ports["native"] = port["address"]
    return ports


def upload(port: str, image: Path) -> int:
    return subprocess.run(
        ["arduino-cli", "upload", "--fqbn", FQBN,
         "--port", port, "--input-file", str(image)],
    ).returncode


def main() -> int:
    image = Path(sys.argv[1]) if len(sys.argv) > 1 else DEFAULT_IMAGE
    if not image.is_file():
        print(f"Firmware image not found: {image}", file=sys.stderr)
        return 2

    ports = uno_ports()
    if "bridge" in ports:
        return upload(ports["bridge"], image)
    if "native" not in ports:
        print("Uno not found. Connect it or double-tap RESET after startup.", file=sys.stderr)
        return 2

    # The patched native-USB core uses a watchdog reset for 1200-baud touch.
    # arduino-cli 1.6.0 may report a failed upload because it keeps the old
    # native port; the board has switched to its ESP bridge at this point.
    native = ports["native"]
    print(f"Requesting bootloader from {native}", flush=True)
    if upload(native, image) == 0:
        return 0
    deadline = time.monotonic() + 15
    while time.monotonic() < deadline:
        ports = uno_ports()
        if "bridge" in ports:
            print(f"Uploading through {ports['bridge']}", flush=True)
            return upload(ports["bridge"], image)
        time.sleep(0.25)
    print("Bootloader port did not appear; double-tap RESET and retry.", file=sys.stderr)
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
