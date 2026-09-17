"""Best-effort device screen preparation immediately before USB flashing."""

from time import monotonic

import serial

Import("env")


def before_upload(source, target, env):
    accepted = False
    try:
        env.AutodetectUploadPort()
        port = env.subst("$UPLOAD_PORT")
        if not port or port.startswith("$"):
            return
        # Avoid intentional reset pulses; native USB can still reset on port open.
        with serial.Serial(port=None, baudrate=115200, timeout=0.05, write_timeout=0.25) as device:
            device.dtr = False
            device.rts = False
            device.port = port
            device.open()
            device.reset_input_buffer()
            deadline = monotonic() + 4.0
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
                        return
                    if reply == b"DAYRING PREPARING" and not accepted:
                        accepted = True
                        deadline = monotonic() + 5.0
                if len(pending) > 4096:
                    pending.clear()
        if accepted:
            print("Update screen did not finish refreshing; upload stopped. Retry to reconnect.")
            return 1
        print("Update screen unavailable; continuing with normal upload.")
    except (OSError, serial.SerialException) as error:
        if accepted:
            print(f"Update screen handshake interrupted: {error}. Upload stopped.")
            return 1
        print(f"Update screen skipped: {error}. Continuing with normal upload.")


env.AddPreAction("upload", before_upload)
