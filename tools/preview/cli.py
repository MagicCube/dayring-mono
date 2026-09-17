#!/usr/bin/env python3
"""Capture production C++ pages as deterministic PNGs for verification loops."""

from __future__ import annotations

import argparse
from datetime import datetime
import json
from pathlib import Path
import re
import subprocess
import sys
from urllib.parse import unquote, urlsplit

from build import ROOT, build
from errors import PreviewError
import png


class Parser(argparse.ArgumentParser):
    def error(self, message: str) -> None:
        raise PreviewError(2, "invalid_arguments", f"{message}. Run: {self.prog} --help")


def clock(value: str) -> str:
    if not re.fullmatch(r"(?:[01][0-9]|2[0-3]):[0-5][0-9]", value):
        raise argparse.ArgumentTypeError("time must be HH:MM (00:00 through 23:59)")
    return value


def battery(value: str) -> int:
    if not value.isascii() or not value.isdecimal() or not 0 <= int(value) <= 100:
        raise argparse.ArgumentTypeError("battery must be an integer from 0 to 100")
    return int(value)


def parser() -> Parser:
    result = Parser(prog="tools/preview/preview", description="Render real application pages to PNG on macOS.",
                    epilog="Start: tools/preview/preview capture 'app://shell/'\n"
                           "Discover: routes; capture --help; help typography.\n"
                           "Uses PIO Python and a native C++20 compiler. No browser or image packages required.",
                    formatter_class=argparse.RawDescriptionHelpFormatter)
    commands = result.add_subparsers(dest="command", required=True, parser_class=Parser)
    capture = commands.add_parser("capture", help="Capture one exact URL to PNG", description=
                                  "Capture a complete 480x800 device frame. Quote URLs containing & or #.",
                                  epilog="Defaults: .preview/<app>[--<page>][--<query>].png; "
                                  "current local time, battery 75%, charging off. Repeated inputs overwrite the same PNG.")
    capture.add_argument("url", help="app://application/path?parameters")
    capture.add_argument("--output", type=Path, help="PNG path (relative paths use the current working directory)")
    capture.add_argument("--time", type=clock, help="Override current local time, HH:MM")
    capture.add_argument("--battery", type=battery, default=75, help="Battery percentage, 0..100")
    capture.add_argument("--battery-charging", action="store_true", help="Show the charging indicator")
    routes = commands.add_parser("routes", help="Discover shared firmware routes and the Home alias")
    help_command = commands.add_parser("help", help="Show an application's routes and query parameters")
    help_command.add_argument("application", help="Registered application name, e.g. typography")
    views = commands.add_parser("views", help="List isolated View examples")
    view_help = commands.add_parser("view-help", help="Describe a View's typed examples")
    view_help.add_argument("view")
    view_capture = commands.add_parser("capture-view", help="Render a View without controllers or hardware")
    view_capture.add_argument("view")
    view_capture.add_argument("--example", required=True)
    view_capture.add_argument("--output", type=Path)
    for command in (capture, routes, help_command, views, view_help, view_capture):
        command.add_argument("--json", action="store_true", help="Emit one machine-readable JSON result")
        command.add_argument("--rebuild", action="store_true", help="Force native renderer recompilation")
    return result


def native(executable: Path, arguments: list[str]) -> tuple[dict, bytes]:
    try:
        result = subprocess.run([str(executable), *arguments], capture_output=True, timeout=30)
    except (OSError, subprocess.TimeoutExpired) as error:
        raise PreviewError(5, "renderer_unavailable", str(error)) from error
    if result.stderr:
        sys.stderr.write(result.stderr.decode(errors="replace"))
    header, separator, payload = result.stdout.partition(b"\n")
    try:
        metadata = json.loads(header)
        if not isinstance(metadata, dict):
            raise ValueError("header must be an object")
    except (ValueError, UnicodeError) as error:
        raise PreviewError(5, "invalid_protocol", f"Renderer returned invalid metadata (exit {result.returncode}).") from error
    if result.returncode:
        failure = metadata.get("error", {})
        status = result.returncode if result.returncode in (2, 3, 5) else 5
        raise PreviewError(status, failure.get("code", "render_failed"), failure.get("message", "Renderer failed."))
    if not separator or metadata.get("protocol") != 1:
        raise PreviewError(5, "invalid_protocol", "Unsupported or incomplete renderer protocol.")
    return metadata, payload


def default_output(resolved_url: str) -> Path:
    route = urlsplit(resolved_url)
    def readable(value: str) -> str:
        return re.sub(r"[^A-Za-z0-9_-]+", "-", unquote(value)).strip("-")
    page = "--".join(readable(part) or "encoded" for part in route.path.split("/") if part)
    label = route.netloc[:48]
    if page:
        label += "--" + page[:80]
    if route.query:
        label += "--" + (readable(route.query) or "query")[:48]
    return ROOT / ".preview" / f"{label}.png"


def capture(args: argparse.Namespace, executable: Path, cache_hit: bool) -> dict:
    capture_time = args.time or datetime.now().strftime("%H:%M")
    hour, minute = capture_time.split(":")
    metadata, pixels = native(executable, ["capture", args.url, hour, minute, str(args.battery), str(int(args.battery_charging))])
    if (metadata.get("width"), metadata.get("height"), metadata.get("format")) != (480, 800, "gray8"):
        raise PreviewError(5, "invalid_frame", "Expected a 480x800 gray8 framebuffer.")
    if len(pixels) != 480 * 800 or not isinstance(metadata.get("resolved_url"), str):
        raise PreviewError(5, "invalid_frame", "Incomplete framebuffer or resolved URL.")
    state = {"time": capture_time, "battery": args.battery, "charging": args.battery_charging}
    output = (args.output or default_output(metadata["resolved_url"])).absolute()
    try:
        png.publish(output, png.encode(480, 800, pixels))
    except OSError as error:
        raise PreviewError(6, "output_failed", f"Cannot write {output}: {error}") from error
    return {"ok": True, "output": str(output), "requested_url": args.url,
            "resolved_url": metadata["resolved_url"], "width": 480, "height": 800,
            "state": state, "cache_hit": cache_hit}


def describe(args: argparse.Namespace, executable: Path, cache_hit: bool) -> dict:
    metadata, payload = native(executable, ["routes"])
    if payload or not isinstance(metadata.get("routes"), list):
        raise PreviewError(5, "invalid_protocol", "Invalid route discovery response.")
    if args.command == "help":
        metadata["routes"] = [route for route in metadata["routes"] if route["application"] == args.application]
        if not metadata["routes"]:
            raise PreviewError(3, "unknown_application", "Application not registered. Run: tools/preview/preview routes")
    return {"ok": True, **metadata, "cache_hit": cache_hit}


def view_command(args: argparse.Namespace, executable: Path, cache_hit: bool) -> dict:
    if args.command != "capture-view":
        metadata, payload = native(executable, ["views"])
        if payload or not isinstance(metadata.get("examples"), list):
            raise PreviewError(5, "invalid_protocol", "Invalid View discovery response.")
        if args.command == "view-help":
            metadata["examples"] = [e for e in metadata["examples"] if e["view"] == args.view]
            if not metadata["examples"]:
                raise PreviewError(3, "unknown_view", "Run views to discover available Views.")
        return {"ok": True, **metadata, "cache_hit": cache_hit}
    metadata, pixels = native(executable, ["capture-view", args.view, args.example])
    if (metadata.get("width"), metadata.get("height"), metadata.get("format")) != (480, 800, "gray8") or len(pixels) != 480 * 800:
        raise PreviewError(5, "invalid_frame", "Expected a complete 480x800 gray8 framebuffer.")
    output = (args.output or ROOT / ".preview" / f"view-{args.view}--{args.example}.png").absolute()
    try:
        png.publish(output, png.encode(480, 800, pixels))
    except OSError as error:
        raise PreviewError(6, "output_failed", f"Cannot write {output}: {error}") from error
    return {"ok": True, "output": str(output), "view": args.view, "example": args.example,
            "width": 480, "height": 800, "cache_hit": cache_hit}


def report(result: dict, as_json: bool) -> None:
    if as_json:
        print(json.dumps(result, ensure_ascii=True))
    elif "output" in result:
        print(result["output"])
    elif "examples" in result:
        for example in result["examples"]:
            print(f'{example["view"]} --example {example["name"]}  bounds={example["bounds"]}')
    else:
        for route in result["routes"]:
            print(f'{route["url"]}  [{"fullscreen" if route["fullscreen"] else "status bar"}]')
            if route["help"]:
                print(f'  {route["help"]}')
        for alias, target in result.get("aliases", {}).items():
            print(f"{alias} -> {target}")
        print("Query: percent-decoded bytes; '+' is literal; first duplicate wins; unknown keys ignored.")


def main() -> int:
    as_json = "--json" in sys.argv[1:]
    try:
        args = parser().parse_args()
        is_view = args.command in ("views", "view-help", "capture-view")
        executable, cache_hit = build(rebuild=args.rebuild, target="view" if is_view else "route")
        if is_view:
            result = view_command(args, executable, cache_hit)
        else:
            result = capture(args, executable, cache_hit) if args.command == "capture" else describe(args, executable, cache_hit)
        report(result, args.json)
        return 0
    except PreviewError as error:
        if as_json:
            print(json.dumps({"ok": False, "error": {"code": error.code, "message": str(error)}}))
        print(f"preview: {error}", file=sys.stderr)
        return error.status
    except OSError as error:
        if as_json:
            print(json.dumps({"ok": False, "error": {"code": "build_io_failed", "message": str(error)}}))
        print(f"preview: build/cache I/O failed: {error}", file=sys.stderr)
        return 4
    except KeyboardInterrupt:
        return 130


if __name__ == "__main__":
    sys.exit(main())
