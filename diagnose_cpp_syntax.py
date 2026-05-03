#!/usr/bin/env python3
"""Lightweight diagnostics for plugin_vst3.cpp build/syntax issues.

Checks:
- brace/paren/bracket balance with line numbers
- suspicious Windows min/max macro collisions around std::min/std::max/std::clamp
- malformed array indexing like name[] or extra ] near reported lines
- optionally runs the CMake build and prints first compiler errors
"""
from __future__ import annotations

import argparse
import os
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parent
DEFAULT_FILE = ROOT / "midnight-plugins" / "plugins" / "melody_maker" / "plugin_vst3.cpp"
DEFAULT_BUILD = ROOT / "midnight-plugins" / "build"

PAIRS = {"(": ")", "[": "]", "{": "}"}
OPENERS = set(PAIRS)
CLOSERS = {v: k for k, v in PAIRS.items()}


def strip_comments_and_strings(line: str, in_block: bool) -> tuple[str, bool]:
    out: list[str] = []
    i = 0
    n = len(line)
    in_string: str | None = None
    while i < n:
        ch = line[i]
        nxt = line[i + 1] if i + 1 < n else ""
        if in_block:
            if ch == "*" and nxt == "/":
                in_block = False
                i += 2
            else:
                i += 1
            out.append(" ")
            continue
        if in_string:
            out.append(" ")
            if ch == "\\":
                i += 2
                out.append(" ")
                continue
            if ch == in_string:
                in_string = None
            i += 1
            continue
        if ch == "/" and nxt == "/":
            out.extend(" " * (n - i))
            break
        if ch == "/" and nxt == "*":
            in_block = True
            out.extend("  ")
            i += 2
            continue
        if ch in {'"', "'"}:
            in_string = ch
            out.append(" ")
            i += 1
            continue
        out.append(ch)
        i += 1
    return "".join(out), in_block


def balance_check(path: Path) -> int:
    stack: list[tuple[str, int, int]] = []
    errors = 0
    in_block = False
    for lineno, raw in enumerate(path.read_text(encoding="utf-8", errors="replace").splitlines(), 1):
        clean, in_block = strip_comments_and_strings(raw, in_block)
        for col, ch in enumerate(clean, 1):
            if ch in OPENERS:
                stack.append((ch, lineno, col))
            elif ch in CLOSERS:
                if not stack or stack[-1][0] != CLOSERS[ch]:
                    print(f"UNMATCHED {ch!r} at {path}:{lineno}:{col}")
                    print(f"  {raw.rstrip()}")
                    errors += 1
                else:
                    stack.pop()
    for ch, lineno, col in stack[-20:]:
        print(f"UNCLOSED {ch!r} opened at {path}:{lineno}:{col}")
        errors += 1
    if not errors:
        print("Balance check: OK")
    return errors


def suspicious_patterns(path: Path) -> int:
    text = path.read_text(encoding="utf-8", errors="replace")
    errors = 0
    patterns = [
        (re.compile(r"\bstd::clamp\s*\([^\n;]*\[[^\]\n;]*\]"), "std::clamp with array indexing can trip Windows macros; use (std::clamp)(...)"),
        (re.compile(r"(?<!\w)\w+\s*\[[ \t]*\]"), "empty array index [] outside a declaration"),
        (re.compile(r"\]\s*\]"), "double closing bracket ]]"),
    ]
    for lineno, line in enumerate(text.splitlines(), 1):
        stripped = line.strip()
        if stripped.startswith("static") and "[]" in stripped and "=" in stripped:
            continue
        if stripped.startswith("//"):
            continue
        for rx, msg in patterns:
            if rx.search(line):
                print(f"SUSPICIOUS {path}:{lineno}: {msg}")
                print(f"  {line.rstrip()}")
                errors += 1
    if not errors:
        print("Suspicious pattern check: OK")
    return errors


def run_build(build_dir: Path) -> int:
    cmake = "cmake"
    env = os.environ.copy()
    cmake_bin = r"C:\Program Files\CMake\bin"
    env["PATH"] = cmake_bin + os.pathsep + env.get("PATH", "")
    cmd = [cmake, "--build", str(build_dir), "--config", "Release", "--target", "midnight_melody_vst3"]
    print("Running:", " ".join(cmd))
    proc = subprocess.run(cmd, cwd=str(ROOT), env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    lines = proc.stdout.splitlines()
    interesting = [ln for ln in lines if "error C" in ln or "warning C" in ln or "error:" in ln or "Build FAILED" in ln]
    if interesting:
        print("\nFirst compiler diagnostics:")
        for ln in interesting[:40]:
            print(ln)
    else:
        print(proc.stdout[-4000:])
    return proc.returncode


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("file", nargs="?", default=str(DEFAULT_FILE))
    parser.add_argument("--build", action="store_true", help="run CMake build and summarize compiler errors")
    parser.add_argument("--build-dir", default=str(DEFAULT_BUILD))
    args = parser.parse_args()

    path = Path(args.file).resolve()
    if not path.exists():
        print(f"Missing file: {path}")
        return 2

    print(f"Checking {path}")
    total = 0
    total += balance_check(path)
    total += suspicious_patterns(path)
    if args.build:
        total += 1 if run_build(Path(args.build_dir).resolve()) else 0
    return 1 if total else 0


if __name__ == "__main__":
    raise SystemExit(main())
