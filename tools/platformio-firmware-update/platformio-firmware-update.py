"""Require a completed update screen before USB firmware flashing."""

import os
from time import monotonic, sleep

import serial

Import("env")


def prepare_screen(env):
    deadline = monotonic() + 30.0
    accepted = False
    last_error = None
    while monotonic() < deadline:
        try:
            env.AutodetectUploadPort()
            port = env.subst("$UPLOAD_PORT")
            if not port or port.startswith("$"):
                sleep(0.25)
                continue
            # Avoid intentional reset pulses; native USB can still reset on port open.
            with serial.Serial(port=None, baudrate=115200, timeout=0.05, write_timeout=0.25) as device:
                device.dtr = False
                device.rts = False
                device.port = port
                device.open()
                device.reset_input_buffer()
                next_request = 0.0
                pending = bytearray()
                while monotonic() < deadline:
                    now = monotonic()
                    if not accepted and now >= next_request:
                        device.write(b"DAYRING PREPARE_UPDATE\n")
                        next_request = now + 0.25
                    pending.extend(device.readline())
                    while b"\n" in pending:
                        reply, _, pending = pending.partition(b"\n")
                        reply = reply.strip()
                        if reply == b"DAYRING READY":
                            print("Firmware update screen ready.")
                            return True
                        if reply == b"DAYRING PREPARING" and not accepted:
                            accepted = True
                            deadline = monotonic() + 15.0
                    if len(pending) > 4096:
                        pending.clear()
        except (OSError, serial.SerialException) as error:
            last_error = error
            if accepted:
                print(f"Update screen handshake interrupted: {error}. Upload stopped.")
                return False
            sleep(0.25)
    if accepted:
        print("Update screen did not finish refreshing; upload stopped. Retry to reconnect.")
    else:
        print("Update screen unavailable; upload stopped. Close serial monitors and check the device and font assets.")
        if last_error is not None:
            print(f"Last serial error: {last_error}")
        print("For first installation or recovery only, use DAYRING_SKIP_UPDATE_SCREEN=1 make upload.")
    return False


def before_upload(source, target, env):
    # FATFS provisioning must work even when missing fonts prevent UI startup.
    targets = {str(item) for item in (target or ())}
    if "uploadfs" in targets:
        print("Filesystem upload: skipping firmware update preparation.")
        return
    if os.environ.get("DAYRING_SKIP_UPDATE_SCREEN") == "1":
        print("Recovery upload: explicitly skipping firmware update screen.")
        return
    print("Waiting for firmware update screen (up to 30 seconds for startup).")
    if not prepare_screen(env):
        return 1


env.AddPreAction("upload", before_upload)
