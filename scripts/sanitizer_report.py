#!/usr/bin/env python3
"""Reads the sanitizer logs of Cutter's test run, for .github/workflows/sanitizers.yml.

    asan <log dir>
        Fail if ASan or UBSan reported any error.
    lsan-extract <log dir> <out.json>
        Collect the leaks of one test run, keyed by their allocation stack.
    lsan-compare <base.json | -> <pr.json> <rizin include dir>
        Fail on the Cutter leaks that the PR adds. With "-" there is no base run to compare
        against, so every Cutter leak is listed and nothing fails.

A leak is only compared by the function names in its stack, so it matches across checkouts.
A leak that is already on the base branch, which runs against the same Rizin commit, is not new.

A new leak is Cutter's if the memory was last owned by Cutter: either it was allocated in Cutter's
own code, or the Rizin function Cutter called returns memory its caller must free. That set is read
from Rizin's headers on every run (RZ_OWN, or a non-const pointer that is not RZ_BORROW), so a
Rizin update needs no change here. Anything else stayed inside Rizin and is ignored.
"""

import json
import os
import re
import sys
from pathlib import Path

LEAK_RE = re.compile(r"^(Direct|Indirect) leak of (\d+) byte\(s\) in \d+ object\(s\) allocated from:")
FRAME_RE = re.compile(r"^\s*#\d+\s+0x[0-9a-fA-F]+\s+(?:in\s+(?P<func>.+?)\s+)?(?P<loc>\(\S+\)|\S+)\s*$")
MODULE_RE = re.compile(r"^\((?P<module>.*?)\+0x[0-9a-fA-F]+\)$")
ERROR_RE = re.compile(r"ERROR: AddressSanitizer|ERROR: UndefinedBehaviorSanitizer|runtime error:")

ALLOCATORS = {
    "malloc", "calloc", "realloc", "reallocarray", "strdup", "strndup", "__strdup", "__strndup",
    "aligned_alloc", "posix_memalign", "memalign", "valloc", "pvalloc",
    "operator new", "operator new[]",
}
# Enough frames to tell call sites apart; deeper frames are test plumbing.
STACK_DEPTH = 12


def base_name(func):
    return func.split("(", 1)[0].strip()


def frame_kind(func, loc):
    name = base_name(func)
    if name in ALLOCATORS or name.startswith("__interceptor_") or re.search(r"sanitizer|/lsan|/asan|lib[al]san", loc):
        return "alloc"
    if "/rizin/" in loc or "librz" in loc or "Rizin-" in loc:
        return "rizin"
    workspace = os.environ.get("GITHUB_WORKSPACE")
    if workspace and workspace in loc:
        return "cutter"
    if not workspace and re.search(r"/(src|test)/", loc) and "/usr/" not in loc:
        return "cutter"
    return "other"


def frame_name(func, loc):
    if func and func != "??":
        return func
    module = MODULE_RE.match(loc)
    return os.path.basename(module["module"]) if module else os.path.basename(loc)


def parse_log(path):
    leaks = []
    leak = None
    for line in path.read_text(errors="replace").splitlines():
        match = LEAK_RE.match(line)
        if match:
            leak = {"kind": match[1], "bytes": int(match[2]), "frames": [], "text": [line]}
            leaks.append(leak)
            continue
        if leak is None:
            continue
        frame = FRAME_RE.match(line)
        if frame:
            leak["frames"].append([frame["func"] or "??", frame["loc"]])
            leak["text"].append(line)
        elif leak["frames"]:
            leak = None
    return leaks


def signature(frames):
    names = [frame_name(f, l) for f, l in frames if frame_kind(f, l) != "alloc"]
    return "\n".join(names[:STACK_DEPTH])


def test_name(path):
    # log_exe_name=1 names the files <prefix>.<executable>.<pid>
    parts = path.name.split(".")
    return parts[1] if len(parts) > 2 else path.name


def extract(log_dir, out):
    leaks = {}
    for path in sorted(Path(log_dir).glob("lsan*")):
        for leak in parse_log(path):
            if leak["kind"] != "Direct" or not leak["frames"]:
                continue
            key = signature(leak["frames"])
            known = leaks.get(key)
            if known is None or leak["bytes"] > known["bytes"]:
                leaks[key] = {
                    "bytes": leak["bytes"],
                    "test": test_name(path),
                    "frames": leak["frames"],
                    "text": "\n".join(leak["text"]),
                }
    Path(out).write_text(json.dumps(leaks, indent=1))
    print(f"{len(leaks)} distinct leaks in {log_dir}")
    return 0


def owned_functions(include_dir):
    owned = set()
    declaration = re.compile(r"\bRZ_API\s+([^;{}()]*?)\b([A-Za-z_]\w*)\s*\(")
    for header in Path(include_dir).rglob("*.h"):
        text = re.sub(r"/\*.*?\*/", " ", header.read_text(errors="replace"), flags=re.S)
        text = re.sub(r"//[^\n]*", " ", text)
        for match in declaration.finditer(text):
            returns, name = match[1], match[2]
            if "RZ_OWN" in returns or (
                "*" in returns and "RZ_BORROW" not in returns and not re.search(r"\bconst\b", returns)
            ):
                owned.add(name)
    return owned


def owner(frames, owned):
    """Returns who last owned the leaked memory, and the Rizin function Cutter called, if any."""
    boundary = None
    for func, loc in frames:
        kind = frame_kind(func, loc)
        if kind == "rizin":
            boundary = base_name(func)
        elif kind == "cutter":
            if boundary is None or boundary in owned:
                return "cutter", boundary
            return "rizin", boundary
    return ("rizin" if boundary else "other"), boundary


def summary(lines):
    text = "\n".join(lines) + "\n"
    print(text)
    path = os.environ.get("GITHUB_STEP_SUMMARY")
    if path:
        with open(path, "a") as f:
            f.write(text)


def leak_details(leak, boundary):
    via = f" via `{boundary}`" if boundary else ""
    return [
        f"<details><summary>{leak['bytes']} bytes in {leak['test']}{via}</summary>",
        "",
        "```",
        leak["text"],
        "```",
        "</details>",
    ]


def compare(base_path, pr_path, include_dir):
    pr = json.loads(Path(pr_path).read_text())
    base = None if base_path == "-" else json.loads(Path(base_path).read_text())
    owned = owned_functions(include_dir)

    cutter, ignored = [], []
    for key, leak in pr.items():
        if base is not None and key in base:
            continue
        who, boundary = owner(leak["frames"], owned)
        (cutter if who == "cutter" else ignored).append((leak, boundary))

    lines = ["## LeakSanitizer", ""]
    if base is None:
        lines.append("No base run to compare against, so this lists every Cutter leak and fails nothing.")
        lines.append("")
    what = "Cutter leaks" if base is None else "New Cutter leaks"
    lines.append(f"**{what}: {len(cutter)}**")
    lines.append("")
    for leak, boundary in cutter:
        lines += leak_details(leak, boundary)
    if ignored:
        lines += ["", f"Ignored {len(ignored)} leaks that stay inside Rizin or system libraries.", ""]
        for leak, boundary in ignored:
            lines += leak_details(leak, boundary)
    summary(lines)
    return 1 if cutter and base is not None else 0


def asan(log_dir):
    reports = [
        path
        for path in sorted(Path(log_dir).glob("*"))
        if path.is_file() and ERROR_RE.search(path.read_text(errors="replace"))
    ]
    lines = ["## AddressSanitizer / UndefinedBehaviorSanitizer", "", f"**Errors: {len(reports)}**", ""]
    for path in reports:
        lines += [f"<details><summary>{test_name(path)}</summary>", "", "```",
                  path.read_text(errors="replace"), "```", "</details>"]
    summary(lines)
    return 1 if reports else 0


def main(argv):
    commands = {
        "asan": (asan, 1),
        "lsan-extract": (extract, 2),
        "lsan-compare": (compare, 3),
    }
    if len(argv) < 2 or argv[1] not in commands or len(argv) - 2 != commands[argv[1]][1]:
        print(__doc__, file=sys.stderr)
        return 2
    function, _ = commands[argv[1]]
    return function(*argv[2:])


if __name__ == "__main__":
    sys.exit(main(sys.argv))
