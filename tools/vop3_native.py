#!/usr/bin/env python3
"""The native VOP3 port's tracker, docs/vop3_native/status.json (the brief is docs/vop3_native/README.md).

    vop3_native.py status                    every row and its stage
    vop3_native.py next [--claim]            the brief for the next row; exit 1 when paused, finished or nothing is ready.
                                             --claim (the routine's) counts a session on it and moves todo to reading
    vop3_native.py set <key> <stage> [--note TEXT]   reading, writing, checking, blocked or todo; a note replaces the last
    vop3_native.py check <key>               runs build/vop3_native_check <key>; identical marks the row done
    vop3_native.py pause | resume            stop or restart the routine's nudges

A row reaches done only through check, so done always means the checker said identical.
"""

import datetime
import json
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
STATUS = os.path.join(ROOT, "docs", "vop3_native", "status.json")
CHECKER = os.environ.get("VOP3_NATIVE_CHECK", os.path.join(ROOT, "build", "vop3_native_check"))
STAGES = ("todo", "reading", "writing", "checking", "blocked")
STUCK = 8   # sessions on one row before the brief says to block it rather than go round again


def now():
    return datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def load():
    return json.load(open(STATUS, encoding="utf-8"))


def save(doc):
    doc["updated"] = now()
    with open(STATUS, "w", encoding="utf-8", newline="\n") as f:
        json.dump(doc, f, indent=1, ensure_ascii=False)
        f.write("\n")


def row(doc, key):
    for r in doc["programs"]:
        if r["key"] == key:
            return r
    sys.exit(f"vop3_native: no row {key}; see `vop3_native.py status`")


def ready(doc):
    """The next row to work on: the first not done or blocked, retire-jit only once every other row is done."""
    rows = doc["programs"]
    for r in rows:
        if r["stage"] in ("done", "blocked"):
            continue
        if r["key"] == "retire-jit" and any(o["stage"] != "done" for o in rows if o["key"] != "retire-jit"):
            return None
        return r
    return None


def brief(r):
    k = r["key"]
    if k == "retire-jit":
        return ("Row retire-jit: every program is ported. Follow 'Retiring the JIT' in docs/vop3_native/README.md. "
                "When the removal commit is pushed, `python3 tools/vop3_native.py set retire-jit checking --note \"...\"` "
                "and ask the Team to review; the row is done when James merges vop3-native.")
    lines = [
        f"Row {k}: {r['chip']} {r['window']} program, {r['steps']} steps, stage {r['stage']}, session {r.get('sessions', 0)}.",
        f"It runs under: {', '.join(r['types'])}.",
        f"Start from: {r['start_from']}.",
    ]
    if r["note"]:
        lines.append(f"Notes so far: {r['note']}")
    if r["check"]:
        lines.append(f"Last check: {r['check']} ({r['passes']} passes).")
    lines += [
        f"Listing: build/vop3_native_check --dump {k} /tmp   File: src/fs1r/native/{k.replace('-', '_')}.cpp",
        f"Check: build/vop3_native_check {k}, then `python3 tools/vop3_native.py check {k}` once it says identical.",
        "Before you stop, record where you are: `python3 tools/vop3_native.py set "
        f"{k} <stage> --note \"...\"`, commit and push. The rules are in docs/vop3_native/README.md.",
    ]
    if r.get("sessions", 0) >= STUCK:
        lines.append(f"This row has had {r['sessions']} sessions. If you cannot see the way to identical, set it "
                     "blocked with a note saying exactly what differs and what you tried, and stop.")
    return "\n".join(lines)


def main(argv):
    cmd = argv[0] if argv else ""
    doc = load()
    if cmd == "status":
        for r in doc["programs"]:
            extra = f"  {r['check']}" if r["check"] else ""
            print(f"{r['key']:<10} {r['stage']:<9} {r['window']:<13} {len(r['types']):>2} types{extra}")
        print("paused" if doc["paused"] else "running", "| updated", doc["updated"])
    elif cmd == "next":
        if doc["paused"]:
            print("paused")
            return 1
        r = ready(doc)
        if not r:
            print("nothing ready: every row is done or blocked")
            return 1
        if "--claim" in argv:
            r["sessions"] = r.get("sessions", 0) + 1
            if r["stage"] == "todo":
                r["stage"] = "reading"
            r["updated"] = now()
            save(doc)
        print(brief(r))
    elif cmd == "set" and len(argv) >= 3:
        r = row(doc, argv[1])
        if argv[2] not in STAGES:
            sys.exit(f"vop3_native: stage is one of {', '.join(STAGES)}; done comes only from check")
        r["stage"] = argv[2]
        if "--note" in argv:
            r["note"] = argv[argv.index("--note") + 1]
        r["updated"] = now()
        save(doc)
    elif cmd == "check" and len(argv) == 2:
        r = row(doc, argv[1])
        p = subprocess.run([CHECKER, argv[1]], capture_output=True, text=True)
        sys.stdout.write(p.stdout)
        m = re.search(r"^RESULT (\S+) (\w+) (\d+) (\d+)$", p.stdout, re.M)
        if not m:
            sys.stderr.write(p.stderr)
            sys.exit(f"vop3_native: {CHECKER} gave no RESULT line (is it built?)")
        if m.group(2) == "unported":
            return 2   # nothing was checked, so nothing is recorded
        r["check"], r["passes"] = m.group(2), int(m.group(4))
        if r["check"] == "identical":
            r["stage"] = "done"
        r["updated"] = now()
        save(doc)
        return 0 if r["check"] == "identical" else 1
    elif cmd in ("pause", "resume"):
        doc["paused"] = cmd == "pause"
        save(doc)
    else:
        sys.exit(__doc__)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
