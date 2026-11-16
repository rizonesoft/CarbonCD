#!/usr/bin/env python3
"""Where a project keeps its findings files: `layout.receipts` in `todo/plan.json`, `docs/reviews` when the file or
the key is absent. The todo kit's graph (`scripts/lib/todo_graph.py`, `plan_config`) reads the same key under the same
rules; the SDK's tests hold the two readers to one table of cases. Only `layout` is read here: the rest of the file
is the todo kit's to check.

    python3 scripts/review_layout.py            the folder, repository-relative (exit 2 and the refusal if unreadable)
"""
from __future__ import annotations

import json
from pathlib import Path
import re
import sys

PLAN_CONFIG = "todo/plan.json"
RECEIPTS = "docs/reviews"
LAYOUT = ("receipts", "plan", "root_index")  # the todo kit checks plan and root_index; this reads receipts
# A folder name: letters, digits, `_`, `.` and `-`, a leading dot too (`.claude`), but never `.` or `..`, and so no drive,
# backslash or space.
LAYOUT_PART = re.compile(r"(?!\.\.?$)[A-Za-z0-9_.-]+")


class LayoutError(ValueError):
    """todo/plan.json's layout cannot be read."""


def linked(root, relative):
    """Whether a folder on the way to root/relative, or the path itself, is a symlink or a junction: a findings file
    is never read through a link, so its folder is never reached through one."""
    cur = Path(root)
    for part in relative.split("/"):
        cur = cur / part
        if cur.is_symlink() or cur.is_junction():
            return True
    return False


def receipts_folder(root):
    """The findings folder under root, repository-relative, from todo/plan.json's `layout.receipts`."""
    path = Path(root) / PLAN_CONFIG
    if not path.is_file():
        return RECEIPTS
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, ValueError) as exc:
        raise LayoutError(f"unreadable {PLAN_CONFIG}: {exc}") from None
    layout = data.get("layout", {}) if isinstance(data, dict) else {}
    if not isinstance(layout, dict):
        raise LayoutError(f"{PLAN_CONFIG}: layout must be an object naming {', '.join(LAYOUT)}")
    for name in layout:
        if name not in LAYOUT:
            raise LayoutError(f"{PLAN_CONFIG}: layout names {name!r}, which is not one of {', '.join(LAYOUT)}")
    receipts = layout.get("receipts", RECEIPTS)
    if not (isinstance(receipts, str) and all(LAYOUT_PART.fullmatch(p) for p in receipts.split("/"))
            and not linked(root, receipts) and (Path(root) / receipts).resolve().is_relative_to(Path(root).resolve())):
        raise LayoutError(f"{PLAN_CONFIG}: layout receipts must be a folder inside the repository reached through no "
                          f"link, its names (letters, digits, _ . -) separated by /")
    return receipts


if __name__ == "__main__":
    try:
        print(receipts_folder(Path(__file__).resolve().parents[1]))
    except LayoutError as exc:
        print(f"refused: {exc}", file=sys.stderr)
        sys.exit(2)
