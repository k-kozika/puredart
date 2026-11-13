#!/usr/bin/env python3
"""Load the native demo before the hookme release bundle executes."""

import argparse
import json
import os
import pathlib
import time


def select_device(remote, serial):
    import frida

    if remote:
        return frida.get_device_manager().add_remote_device(remote)
    if serial:
        return frida.get_device(serial, timeout=5)
    return frida.get_usb_device(timeout=5)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("native_directory")
    p.add_argument("--remote", default=os.environ.get("FRIDA_REMOTE") or None)
    p.add_argument("--serial", default=os.environ.get("ANDROID_SERIAL") or None)
    p.add_argument("--package", default="com.example.hermes.hookme")
    a = p.parse_args()
    device = select_device(a.remote, a.serial)
    pid = device.spawn([a.package])
    session = device.attach(pid)
    state = {"ready": False, "error": None}
    script = session.create_script(
        pathlib.Path(__file__).with_suffix(".js").read_text()
    )

    def message(m, data):
        print(json.dumps(m), flush=True)
        if m["type"] == "send":
            state["ready"] = True
            state["error"] = m["payload"]["result"]
        elif m["type"] == "error":
            state["error"] = m["description"]
            state["ready"] = True

    script.on("message", message)
    script.load()
    script.exports_sync.init("early", {"nativeDirectory": a.native_directory})
    device.resume(pid)
    for attempt in range(120):
        if state["ready"]:
            break
        time.sleep(0.25)
    if not state["ready"] or state["error"] != 0:
        raise RuntimeError(state)
    time.sleep(2)
    session.detach()
    print(
        json.dumps(
            {"pid": pid, "native_hooks_installed": True, "loader_detached": True}
        ),
        flush=True,
    )


if __name__ == "__main__":
    main()
