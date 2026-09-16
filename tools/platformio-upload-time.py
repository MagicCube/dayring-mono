"""Inject host local time once per PlatformIO firmware upload."""

from __future__ import annotations

from datetime import datetime, timedelta
from pathlib import Path

from SCons.Script import COMMAND_LINE_TARGETS

Import("env")  # type: ignore[name-defined]  # noqa: F821


SYNC_REQUESTED = "upload" in COMMAND_LINE_TARGETS
BOOT_ALLOWANCE = timedelta(seconds=30)
output = Path(env.subst("$BUILD_DIR")) / "upload-rtc-config.hpp"  # type: ignore[name-defined]  # noqa: F821
output.parent.mkdir(parents=True, exist_ok=True)

if SYNC_REQUESTED:
    target = datetime.now().astimezone() + BOOT_ALLOWANCE
    stamp = target.strftime("%Y%m%d%H%M%S")
    content = (
        "#pragma once\n"
        "#define PAPERMONO_UPLOAD_RTC_SYNC 1\n"
        f"#define PAPERMONO_UPLOAD_RTC_STAMP {stamp}ULL\n"
    )
else:
    content = (
        "#pragma once\n"
        "#define PAPERMONO_UPLOAD_RTC_SYNC 0\n"
        "#define PAPERMONO_UPLOAD_RTC_STAMP 0ULL\n"
    )

if not output.exists() or output.read_text(encoding="utf-8") != content:
    output.write_text(content, encoding="utf-8")

env.Append(CPPPATH=[str(output.parent)])  # type: ignore[name-defined]  # noqa: F821
