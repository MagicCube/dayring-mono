"""Incremental, parallel native builds with reusable objects and dependency manifests."""

from __future__ import annotations

from concurrent.futures import ThreadPoolExecutor
import configparser
import os
from pathlib import Path
import shutil
import shlex
import subprocess
import tempfile

import cache
from errors import PreviewError

ROOT = Path(__file__).resolve().parents[2]
WORK = ROOT / ".cache/preview"
UI = ROOT / "freeink-sdk/libs/ui/FreeInkUI"
RTC = ROOT / "freeink-sdk/libs/hardware/Rtc/include"
NATIVE = Path(__file__).resolve().parent / "native"


def run(command: list[str], timeout: int = 180) -> subprocess.CompletedProcess:
    try:
        return subprocess.run(command, cwd=ROOT, capture_output=True, timeout=timeout)
    except (OSError, subprocess.TimeoutExpired) as error:
        raise PreviewError(4, "build_unavailable", f"Cannot run native compiler: {error}") from error


def checked(command: list[str]) -> subprocess.CompletedProcess:
    result = run(command)
    if result.returncode:
        raise PreviewError(4, "compilation_failed", result.stderr.decode(errors="replace"))
    return result


VIEW_SOURCES = [
    ROOT / "src/apps/shell/views/BatteryIndicatorView.cpp",
    ROOT / "src/apps/shell/pages/HomePage.cpp",
    ROOT / "src/apps/shell/pages/LockPage.cpp",
    ROOT / "src/apps/shell/components/StatusBar.cpp",
    ROOT / "src/apps/typography/TypographyPage.cpp",
    ROOT / "src/apps/common/LandingPage.cpp",
]


def source_files(target: str = "route") -> list[Path]:
    shared = [ROOT / "src/platform/fonts/Fonts.cpp", UI / "src/FreeInkUI.cpp",
              NATIVE / "FrameBuffer.cpp"]
    if target == "view":
        return sorted([*VIEW_SOURCES, *shared, NATIVE / "ViewMain.cpp"])
    if target != "route":
        raise ValueError(f"Unknown preview target: {target}")
    return sorted([
        *ROOT.glob("src/apps/**/*.cpp"),
        *ROOT.glob("src/platform/runtime/*.cpp"),
        *ROOT.glob("src/platform/runtime/services/*.cpp"),
        *ROOT.glob("src/platform/tasking/services/*.cpp"),
        *ROOT.glob("src/platform/ui/*.cpp"),
        *ROOT.glob("src/platform/hal/services/*.cpp"),
        *ROOT.glob("src/platform/time/services/*.cpp"),
        ROOT / "src/platform/hal/Frontlight.cpp",
        NATIVE / "Main.cpp", NATIVE / "HostHardware.cpp", *shared,
    ])


def home_flags() -> list[str]:
    config = configparser.ConfigParser(interpolation=None)
    try:
        config.read(ROOT / "platformio.ini")
        flags = shlex.split(config.get("env:papermono", "build_flags", fallback=""))
    except (configparser.Error, ValueError) as error:
        raise PreviewError(4, "invalid_build_config", f"Cannot read PaperMono Home configuration: {error}") from error
    return [flag for flag in flags if flag.startswith("-DDAYRING_HOME_URL=")]


def configuration(target: str = "route") -> tuple[str, list[str], str]:
    name = os.environ.get("HOST_CXX", "clang++")
    compiler = shutil.which(name)
    if compiler is None:
        raise PreviewError(4, "compiler_missing", f"Native compiler {name!r} not found. "
                           "Install Xcode Command Line Tools, or set HOST_CXX to a host C++20 compiler.")
    version = checked([compiler, "--version"]).stdout.decode(errors="replace")
    includes = [NATIVE / "include", ROOT / "src", UI / "include", RTC]
    if target == "view":
        includes = [ROOT / "src", UI / "include"]
    flags = ["-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror", "-fno-exceptions",
             *["-I" + str(path) for path in includes],
             *(home_flags() if target == "route" else [])]
    environment = {key: os.environ.get(key, "") for key in
                   ("SDKROOT", "DEVELOPER_DIR", "MACOSX_DEPLOYMENT_TARGET", "CPATH", "CPLUS_INCLUDE_PATH")}
    # A newly added header can shadow a prior include without changing its contents.
    # Inventory names as well as hashing known dependencies before each reuse.
    roots = [ROOT / "src", NATIVE, UI / "include", RTC]
    if target == "view":
        roots = [ROOT / "src", UI / "include"]
    for variable in ("CPATH", "CPLUS_INCLUDE_PATH"):
        roots.extend(Path(part) for part in os.environ.get(variable, "").split(os.pathsep) if part)
    inventory = sorted(str(path) for root in roots for path in root.rglob("*")
                       if path.suffix in (".h", ".hpp", ".inc") and path.is_file())
    implementation = [cache.file_hash(Path(__file__)), cache.file_hash(Path(cache.__file__))]
    identity = cache.digest([target, compiler, version, flags, environment, inventory, implementation])
    return compiler, flags, identity


def cached_unit(source: Path, identity: str, hashes: cache.FileHashes) -> Path | None:
    unit = cache.digest([identity, str(source)])
    previous = cache.read_manifest(WORK / "build/units" / f"{unit}.json")
    object_path = WORK / "build/objects" / f'{previous.get("object", "missing")}.o'
    if object_path.is_file() and cache.dependencies_match(previous, hashes):
        return object_path
    return None


def compile_unit(source: Path, compiler: str, flags: list[str], identity: str) -> Path:
    unit = cache.digest([identity, str(source)])
    manifest_path = WORK / "build/units" / f"{unit}.json"
    with tempfile.TemporaryDirectory(prefix="unit-", dir=WORK / "tmp") as temporary:
        output, depfile = Path(temporary) / "unit.o", Path(temporary) / "unit.d"
        checked([compiler, *flags, "-MD", "-MF", str(depfile), "-MT", "preview",
                 "-c", str(source), "-o", str(output)])
        dependencies = {str(path): cache.file_hash(path) for path in
                        cache.parse_dependencies(depfile.read_text()) | {source}}
        key = cache.digest([unit, dependencies])
        object_path = WORK / "build/objects" / f"{key}.o"
        os.replace(output, object_path)
        cache.publish_manifest(manifest_path, {"object": key, "dependencies": dependencies})
    return object_path


def build(*, rebuild: bool = False, target: str = "route") -> tuple[Path, bool]:
    compiler, flags, identity = configuration(target)
    for directory in ("tmp", "build/objects", "build/bin"):
        (WORK / directory).mkdir(parents=True, exist_ok=True)
    sources = source_files(target)
    hashes = cache.FileHashes()
    # Check cached dependencies once on the caller thread. Parallelize only real
    # compilation; warm captures should not pay thread-pool/lock contention costs.
    ready = [None if rebuild else cached_unit(source, identity, hashes) for source in sources]
    missing = [index for index, path in enumerate(ready) if path is None]
    if missing:
        with ThreadPoolExecutor(max_workers=min(8, os.cpu_count() or 1)) as pool:
            built = pool.map(lambda index: compile_unit(sources[index], compiler, flags, identity), missing)
            for index, path in zip(missing, built):
                ready[index] = path
    objects = [str(path) for path in ready]
    key = cache.digest([identity, objects])
    executable = WORK / "build/bin" / key / "preview-native"
    if executable.is_file() and not rebuild:
        return executable, not missing
    executable.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="link-", dir=WORK / "tmp") as temporary:
        output = Path(temporary) / "preview-native"
        checked([compiler, *flags, *objects, "-o", str(output)])
        os.replace(output, executable)
    return executable, False
