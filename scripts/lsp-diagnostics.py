#!/usr/bin/env python3
"""Run the project's language servers over a set of files and report their diagnostics.

    python scripts/lsp-diagnostics.py --all --changed
    python scripts/lsp-diagnostics.py --all --staged
    python scripts/lsp-diagnostics.py --server clangd src/main.c
    python scripts/lsp-diagnostics.py --server intelephense --commit <sha> --json

The servers are the ones in `.conclave/lsp.json` (`python scripts/lsp-servers.py list`). The files are everything
a change touched (`--changed`: staged, not yet staged and new files alike), the staged ones (`--staged`), the
ones changed and not yet staged (`--worktree`), those of a commit (`--commit <sha>`), or the paths given, read
from the project root. A file goes only to the servers that serve it: by its suffix, and under the server's `root`.
The client is read-only: it opens documents in the server's memory and writes nothing to the project.

Each server ends with one result: `ok`, `idle` (no file of its kind), `missing` (not installed), `skipped` (the
file its entry requires is absent), `failed` (it could not be run to the end: the reason says why) or `partial`
(some file was not answered or could not be read). A check that did not finish is never a pass.

Exit codes:
  0  every server that ran finished, with no diagnostic at or above its threshold
  1  a diagnostic at or above a server's threshold, or a server that is failed or partial
  2  bad usage, an unknown server, a git command that failed, or a table that does not load
  3  no requested server could check anything: each is missing, skipped or failed

With `--json` every run that gets past its arguments prints one JSON document on standard output (`servers`,
`diagnostics`, `failing`, `exit`, `error`), a refused run (exit 2) included: its `error` is the refusal.

Standard library only.
"""
import argparse
import json
import os
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent / "lib"))
import lsp_client  # noqa: E402
import lsp_table  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
GIT_INPUT = {
    # Everything the working tree differs from the last commit in: staged, not staged, and files git does not
    # track yet (never the ignored ones). It also works in a repository with no commit.
    "changed": ["status", "--porcelain=v1", "-z", "--untracked-files=all"],
    "staged": ["diff", "--cached", "--name-only", "-z", "--diff-filter=d"],
    "worktree": ["diff", "--name-only", "-z", "--diff-filter=d"],
    "commit": ["show", "--name-only", "-z", "--pretty=format:", "--diff-filter=d"],
}


class Refused(Exception):
    """Something that ends the run with one line and exit 2."""


def git_paths(root, form, revision=None):
    """The files a git form names, read NUL-separated so no name is quoted or split. A git command that fails is
    refused with git's reason: an unknown revision is never an empty input."""
    argv = ["git", "-C", str(root), *GIT_INPUT[form], *([revision, "--"] if revision else [])]
    try:
        proc = subprocess.run(argv, capture_output=True, timeout=60,
                              creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
    except (OSError, subprocess.TimeoutExpired) as exc:
        raise Refused(f"git could not be run ({exc})") from exc
    if proc.returncode != 0:
        reason = proc.stderr.decode("utf-8", errors="replace").strip().splitlines()
        raise Refused(f"git {GIT_INPUT[form][0]} failed: {reason[0] if reason else f'exit {proc.returncode}'}")
    fields = [field for field in proc.stdout.split(b"\x00") if field.strip()]
    if form == "changed":
        # `XY name`, and after a rename or a copy one more field, the name it had: that one is not a file now.
        entries, fields, old_name = fields, [], False
        for entry in entries:
            if old_name:
                old_name = False
                continue
            old_name = entry[:1] in (b"R", b"C")
            fields.append(entry[3:])
    names = [name.decode("utf-8", errors="surrogateescape") for name in fields]
    return [root / name for name in names if (root / name).is_file()]


def named_paths(root, names):
    """The files named on the command line, read from the project root. A name that is not a file there, or that
    leaves the project, is refused: a mistyped path is never an empty input."""
    paths = []
    for name in names:
        path = (root / name).resolve()
        try:
            path.relative_to(root)
        except ValueError:
            raise Refused(f"{name} is outside the project") from None
        if not path.is_file():
            raise Refused(f"{name} is not a file in the project")
        paths.append(path)
    return paths


def exit_code(results, failing):
    """The run's exit, from its results alone (the same whatever the output form)."""
    statuses = [result["status"] for result in results]
    if statuses and all(status in ("missing", "skipped", "failed") for status in statuses):
        return 3
    if failing or any(status in ("failed", "partial") for status in statuses):
        return 1
    return 0


def document(results, rows, failing, code, error=None):
    """The run as the one JSON document `--json` prints, for a refused run too (`error` is then the refusal)."""
    return {"servers": {result["name"]: {key: result[key] for key in ("status", "reason", "files", "unanswered",
                                                                      "unread")} for result in results},
            "diagnostics": rows, "failing": failing, "exit": code, "error": error}


def report_text(results, rows, failing, servers, verbose):
    for row in rows:
        code = f" [{row['code']}]" if row["code"] else ""
        print(f"{row['server']:14} {row['file']}:{row['line']}:{row['column']}  {row['severity']:11} "
              f"{row['message']}{code}")
    if rows:
        print()
    for result in results:
        name, status, entry = result["name"], result["status"], servers[result["name"]]
        if status == "missing":
            # Silence from a server nobody installed must never read as a pass.
            print(f"{name:14} SKIPPED -- {result['reason']}", file=sys.stderr)
        elif status == "skipped":
            print(f"{name:14} SKIPPED -- {result['reason']}", file=sys.stderr)
        elif status == "failed":
            print(f"{name:14} FAILED -- {result['reason']}", file=sys.stderr)
        elif status == "idle":
            print(f"{name:14} no file of its kind in this input")
        else:
            note = f" {entry['note']}" if entry["note"] else ""
            print(f"{name:14} {result['files']} file(s), {len(result['rows'])} diagnostic(s).{note}")
            if status == "partial":
                print(f"{name:14} PARTIAL -- {result['reason']}", file=sys.stderr)
        if verbose and result["stderr"].strip():
            print(f"{name:14} standard error (its end):\n{result['stderr'].rstrip()}", file=sys.stderr)
    print(f"\nlsp-diagnostics: {len(failing)} diagnostic(s) at or above threshold.")


def build_parser():
    """The script's argument parser: what `main` parses with, and what a documented command must fit."""
    parser = argparse.ArgumentParser(prog="lsp-diagnostics.py",
                                     description="Run the project's language servers over files; report diagnostics.")
    parser.add_argument("paths", nargs="*", help="files, read from the project root")
    parser.add_argument("--server", action="append", default=[], help="a server of .conclave/lsp.json (repeatable)")
    parser.add_argument("--all", action="store_true", help="every server in the table")
    parser.add_argument("--staged", action="store_true", help="the files staged in git")
    parser.add_argument("--changed", action="store_true", help="every file a change touched: staged, not staged "
                                                                "and new")
    parser.add_argument("--worktree", action="store_true", help="the files changed and not yet staged")
    parser.add_argument("--commit", help="the files of a commit")
    parser.add_argument("--severity", choices=tuple(lsp_client.SEVERITY_RANK),
                        help="the least severity that fails the run, for every server (default: each server's own)")
    parser.add_argument("--settle", type=float, help="seconds to keep listening after the last file is answered "
                                                     "(default: each server's own)")
    parser.add_argument("--timeout", type=float, default=float(os.environ.get("LSP_DIAGNOSTICS_TIMEOUT", "240")),
                        help="seconds a server gets for the whole run (default: 240)")
    parser.add_argument("--json", action="store_true", help="print the report as one JSON document")
    parser.add_argument("--verbose", action="store_true", help="show the end of each server's standard error")
    return parser


def main(argv=None, root=None):
    args = build_parser().parse_args(argv)
    root = Path(root or ROOT).resolve()
    forms = [form for form in ("changed", "staged", "worktree", "commit") if getattr(args, form)]
    try:
        if len(forms) + bool(args.paths) > 1:
            raise Refused("give one input: --changed, --staged, --worktree, --commit <sha>, or paths")
        if args.all == bool(args.server):
            raise Refused("name the servers with --server <name>, or run every one with --all")
        table = lsp_table.load_servers(root)
        if not table:
            raise Refused(f"{lsp_table.TABLE} holds no server: add a stack's with "
                          "python scripts/lsp-servers.py add <preset>")
        servers = {entry["name"]: entry for entry in table}
        unknown = [name for name in args.server if name not in servers]
        if unknown:
            raise Refused(f"no server named {unknown[0]!r} in {lsp_table.TABLE} (it holds: {', '.join(servers)})")
        wanted = list(servers) if args.all else list(dict.fromkeys(args.server))
        if forms:
            paths = git_paths(root, forms[0], args.commit)
        else:
            paths = named_paths(root, args.paths)
    except (Refused, lsp_table.TableError) as exc:
        print(f"lsp-diagnostics: {exc}", file=sys.stderr)
        if args.json:
            # A reader of the JSON form gets a document for a refusal too, never an empty output.
            print(json.dumps(document([], [], 0, 2, str(exc)), indent=2))
        return 2

    results = [lsp_client.run_server(servers[name], root, paths, timeout=args.timeout, settle=args.settle)
               for name in wanted]
    rows = [row for result in results for row in result["rows"]]
    rank = lsp_client.SEVERITY_RANK
    failing = [row for row in rows
               if rank[row["severity"]] <= rank[args.severity or servers[row["server"]]["severity"]]]
    rows.sort(key=lambda row: (row["server"], rank[row["severity"]], row["file"], row["line"]))
    code = exit_code(results, failing)
    if args.json:
        print(json.dumps(document(results, rows, len(failing), code), indent=2))
    else:
        if not paths:
            print("lsp-diagnostics: no file in the input.")
        report_text(results, rows, failing, servers, args.verbose)
    return code


if __name__ == "__main__":
    sys.exit(main())
