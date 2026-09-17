"""Atomic per-translation-unit caches with content-validated dependency manifests."""

from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path
import tempfile


def digest(value) -> str:
    return hashlib.sha256(json.dumps(value, sort_keys=True).encode()).hexdigest()


def file_hash(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


class FileHashes:
    """Read each dependency at most once during the sequential cache check."""

    def __init__(self):
        self._values = {}

    def __call__(self, path: Path) -> str:
        if path not in self._values:
            self._values[path] = file_hash(path)
        return self._values[path]


def read_manifest(path: Path) -> dict:
    try:
        value = json.loads(path.read_text())
        return value if isinstance(value, dict) else {}
    except (OSError, ValueError):
        return {}


def publish_manifest(path: Path, value: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(mode="w", dir=path.parent, delete=False) as stream:
            temporary = Path(stream.name)
            json.dump(value, stream)
        os.replace(temporary, path)
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)


def dependencies_match(manifest: dict, hash_file=file_hash) -> bool:
    dependencies = manifest.get("dependencies")
    if not isinstance(dependencies, dict) or not dependencies:
        return False
    try:
        return all(hash_file(Path(name)) == expected for name, expected in dependencies.items())
    except OSError:
        return False


def parse_dependencies(text: str) -> set[Path]:
    """Read compiler Make dependencies, including escaped spaces and dollar signs."""
    result = set()
    for rule in text.replace("\\\n", "").splitlines():
        _, separator, values = rule.partition(":")
        if not separator:
            continue
        token = ""
        escaped = False
        for char in values + " ":
            if escaped:
                token += char
                escaped = False
            elif char == "\\":
                escaped = True
            elif char.isspace():
                if token:
                    result.add(Path(token.replace("$$", "$")))
                    token = ""
            else:
                token += char
    return result
