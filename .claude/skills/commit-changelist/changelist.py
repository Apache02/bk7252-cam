#!/usr/bin/env python3
"""
Read JetBrains IDE changelists from .idea/workspace.xml.

Usage:
    changelist.py                      -- list all changelists with file counts
    changelist.py <name>               -- show files in named changelist
    changelist.py <name> --stage       -- git add/rm those files
    changelist.py <name> --comment     -- print the stored commit message draft
"""

import sys
import subprocess
import xml.etree.ElementTree as ET
from pathlib import Path

# workspace.xml is always two levels above this script's directory
# (.claude/skills/commit-changelist/ -> project root)
SKILL_DIR = Path(__file__).parent
PROJECT_ROOT = SKILL_DIR.parent.parent.parent
WORKSPACE = PROJECT_ROOT / ".idea" / "workspace.xml"
PROJECT_DIR_VAR = "$PROJECT_DIR$"


def parse_changelists():
    tree = ET.parse(WORKSPACE)
    root = tree.getroot()
    manager = next(
        (c for c in root.findall("component") if c.get("name") == "ChangeListManager"),
        None,
    )
    if manager is None:
        raise RuntimeError("ChangeListManager not found in workspace.xml")

    result = {}
    for lst in manager.findall("list"):
        name = lst.get("name")
        changes = []
        for ch in lst.findall("change"):
            after  = ch.get("afterPath",  "").replace(PROJECT_DIR_VAR + "/", "")
            before = ch.get("beforePath", "").replace(PROJECT_DIR_VAR + "/", "")
            if after and not before:
                changes.append(("new", after, ""))
            elif before and not after:
                changes.append(("del", "", before))
            elif before == after:
                changes.append(("mod", after, ""))
            else:
                changes.append(("mov", after, before))
        result[name] = {
            "default": lst.get("default") == "true",
            "comment": lst.get("comment", ""),
            "changes": changes,
        }
    return result


def main():
    args = sys.argv[1:]
    stage   = "--stage"   in args
    comment = "--comment" in args
    names   = [a for a in args if not a.startswith("--")]

    changelists = parse_changelists()

    if not names:
        for name, info in sorted(changelists.items(), key=lambda x: len(x[1]["changes"]) == 0):
            marker = " *" if info["default"] else ""
            count = len(info["changes"])
            print(f"{name}{marker}  ({count} file{'s' if count != 1 else ''})")
            for op, after, before in info["changes"]:
                if op == "mov":
                    print(f"  [mov] {before} -> {after}")
                elif op == "del":
                    print(f"  [del] {before}")
                else:
                    print(f"  [{op:3s}] {after}")
        return

    name = names[0]
    if name == "default":
        name = next((n for n, i in changelists.items() if i["default"]), None)
        if name is None:
            print("No default changelist found.", file=sys.stderr)
            sys.exit(1)

    if name not in changelists:
        avail = ", ".join(changelists)
        print(f"Changelist '{name}' not found. Available: {avail}", file=sys.stderr)
        sys.exit(1)

    info = changelists[name]

    if comment:
        print(info["comment"])
        return

    changes = info["changes"]
    if not changes:
        print(f"Changelist '{name}' is empty.")
        return

    for op, after, before in changes:
        if op == "mov":
            print(f"  [mov] {before} -> {after}")
        elif op == "del":
            print(f"  [del] {before}")
        else:
            print(f"  [{op:3s}] {after}")

    if stage:
        # For [mov], skip the whole entry if the destination doesn't exist on disk
        # (means the move was reverted but workspace.xml is still stale)
        skipped_rm = set()
        to_add = []
        for op, after, before in changes:
            if op in ("new", "mod"):
                to_add.append(after)
            elif op == "mov":
                if (PROJECT_ROOT / after).exists():
                    to_add.append(after)
                else:
                    print(f"  [warn] not found on disk (stale changelist?): {after}", file=sys.stderr)
                    skipped_rm.add(before)
        to_rm = [before for op, _, before in changes
                 if op in ("del", "mov") and before and before not in skipped_rm]

        # Files that belong to this changelist (all paths that should end up staged)
        changelist_paths = set(to_add) | set(to_rm)

        # Unstage anything currently staged that is NOT in this changelist
        staged = subprocess.run(
            ["git", "diff", "--name-only", "--cached"],
            cwd=PROJECT_ROOT, capture_output=True, text=True, check=True,
        ).stdout.splitlines()
        to_unstage = [p for p in staged if p not in changelist_paths]
        if to_unstage:
            subprocess.run(["git", "restore", "--staged", "--"] + to_unstage,
                           cwd=PROJECT_ROOT, check=True)
            print(f"Unstaged {len(to_unstage)} file(s) not in this changelist.")

        if to_add:
            subprocess.run(["git", "add", "--"] + to_add, cwd=PROJECT_ROOT, check=True)
        if to_rm:
            subprocess.run(["git", "rm",  "--"] + to_rm,  cwd=PROJECT_ROOT, check=True)
        print("Staged.")


if __name__ == "__main__":
    main()
