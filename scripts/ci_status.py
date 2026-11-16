#!/usr/bin/env python3
"""Report the CI state of main: the latest completed run of each watched workflow, and runs still in progress.

    python scripts/ci_status.py                  # the watched workflows on main
    python scripts/ci_status.py --json           # the same report as one JSON object
    python scripts/ci_status.py --workflow build.yml --branch main

For each watched workflow it reads the last 10 runs with `gh run list` and reports the latest completed run (green, red or cancelled), its verdict,
and how many newer runs are still queued or in progress. A cancelled run carries no verdict (a newer push usually
superseded it), so the verdict is the latest completed run that was not cancelled; with none in the last 10 runs the
verdict is `none`. For a red verdict it names the failed jobs from `gh run view <id> --json jobs`.

The watched workflows are the `--workflow` arguments, else the `workflows` list in `.conclave/ci.json` (the ones whose
results gate main), else every `.yml` and `.yaml` file in `.github/workflows/`.

Exit 0: no watched workflow is red. Exit 1: at least one is red (the workflow, the commit, the run's URL and its
failed jobs are printed). Exit 2: bad usage. Exit 3: `gh` is missing, not authenticated, or failed, so the state is
unknown; that is never treated as green. The red-first policy that consumes this is in the `process-phase` skill.
Stdlib only; the `gh` runner is injectable for tests.
"""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
CONFIG = Path(".conclave") / "ci.json"
FIELDS = "databaseId,headSha,status,conclusion,url,createdAt,event"
LIMIT = 10
GREEN = {"success", "neutral", "skipped"}
# A run that was stopped before it judged the commit: no verdict either way.
NO_VERDICT = {"cancelled", "stale"}
MAX_JOBS = 20
MAX_MESSAGE = 300


class Unavailable(Exception):
    """gh is missing, unauthenticated or failed: the CI state is unknown (exit 3)."""


def run_gh(args):
    """Run gh with args; returns (exit code, stdout, stderr). Raises Unavailable when gh is not installed."""
    # An explicit path: on Windows a bare which() also searches the working directory, where a planted gh could sit.
    executable = shutil.which("gh", path=os.environ.get("PATH", ""))
    if executable is None:
        raise Unavailable("gh is not installed or not on PATH")
    try:
        done = subprocess.run([executable, *args], cwd=ROOT, capture_output=True, text=True, encoding="utf-8",
                              errors="replace", timeout=120)
    except (OSError, subprocess.TimeoutExpired) as exc:
        raise Unavailable(f"gh could not run: {exc}") from None
    return done.returncode, done.stdout, done.stderr


def gh_json(runner, args):
    try:
        code, out, err = runner(args)
    except FileNotFoundError:
        raise Unavailable("gh is not installed or not on PATH") from None
    if code != 0:
        reason = (err or out or "").strip().splitlines()
        raise Unavailable(f"gh {' '.join(args[:2])} exited {code}: {(reason[0] if reason else 'no output')[:MAX_MESSAGE]}")
    try:
        return json.loads(out)
    except ValueError:
        raise Unavailable(f"gh {' '.join(args[:2])} did not return JSON") from None


def watched(root):
    """The workflows .conclave/ci.json names, else every workflow file; a malformed config is a usage error."""
    path = Path(root) / CONFIG
    if path.is_file():
        try:
            doc = json.loads(path.read_text(encoding="utf-8"))
        except ValueError:
            raise ValueError(f"{CONFIG.as_posix()} is not JSON") from None
        names = doc.get("workflows") if isinstance(doc, dict) else None
        if names is not None:
            if not (isinstance(names, list) and all(isinstance(n, str) and n for n in names)):
                raise ValueError(f"{CONFIG.as_posix()} workflows must be a list of workflow file names")
            return list(names)
    directory = Path(root) / ".github" / "workflows"
    if not directory.is_dir():
        return []
    return sorted(p.name for p in directory.iterdir() if p.is_file() and p.suffix in (".yml", ".yaml"))


def verdict_of(run):
    if run is None:
        return "none"
    conclusion = (run.get("conclusion") or "").lower()
    if conclusion in GREEN:
        return "green"
    if conclusion in NO_VERDICT:
        return "cancelled"
    return "red"


def brief(run):
    return None if run is None else {k: run.get(k) for k in ("databaseId", "headSha", "status", "conclusion", "url",
                                                              "createdAt", "event")}


def workflow_status(runner, workflow, branch):
    runs = gh_json(runner, ["run", "list", "--branch", branch, "--workflow", workflow, "--limit", str(LIMIT),
                            "--json", FIELDS])
    if not isinstance(runs, list) or not all(isinstance(r, dict) for r in runs):
        raise Unavailable(f"gh run list for {workflow} did not return a list of runs")
    runs = sorted(runs, key=lambda r: str(r.get("createdAt") or ""), reverse=True)
    completed = [r for r in runs if r.get("status") == "completed"]
    latest = completed[0] if completed else None
    decisive = next((r for r in completed if verdict_of(r) != "cancelled"), None)
    newer = [r for r in runs if r.get("status") != "completed"
             and (latest is None or str(r.get("createdAt") or "") > str(latest.get("createdAt") or ""))]
    entry = {"workflow": workflow, "verdict": verdict_of(decisive), "latest_completed": brief(latest),
             "latest_completed_state": verdict_of(latest), "verdict_run": brief(decisive), "in_progress": len(newer)}
    if entry["verdict"] == "red":
        entry["failed_jobs"] = failed_jobs(runner, decisive.get("databaseId"))
    return entry


def failed_jobs(runner, run_id):
    """The failed jobs of one run, or a one-line note when they cannot be read (the run is still red)."""
    try:
        view = gh_json(runner, ["run", "view", str(run_id), "--json", "jobs"])
    except Unavailable as exc:
        return [{"name": f"jobs unavailable: {exc}", "conclusion": None, "url": None}]
    jobs = view.get("jobs") if isinstance(view, dict) else None
    if not isinstance(jobs, list):
        return [{"name": "jobs unavailable: gh run view returned no job list", "conclusion": None, "url": None}]
    failed = [j for j in jobs if isinstance(j, dict) and verdict_of(j) == "red" and j.get("status") == "completed"]
    return [{"name": j.get("name"), "conclusion": j.get("conclusion"), "url": j.get("url")} for j in failed[:MAX_JOBS]]


def report(runner, workflows, branch):
    """The report dict and its exit code; Unavailable propagates (exit 3)."""
    entries = [workflow_status(runner, w, branch) for w in workflows]
    code = 1 if any(e["verdict"] == "red" for e in entries) else 0
    return {"branch": branch, "workflows": entries, "exit": code}, code


def describe(entry):
    run = entry["verdict_run"]
    where = f" at {run['headSha'][:12]} ({run['url']})" if run and run.get("headSha") and run.get("url") else ""
    lines = [f"{entry['workflow']}: {entry['verdict']}{where}"]
    if entry["latest_completed_state"] == "cancelled":
        lines.append(f"  latest completed run was cancelled: {entry['latest_completed']['url']}")
    if entry["in_progress"]:
        lines.append(f"  {entry['in_progress']} newer run(s) still queued or in progress")
    for job in entry.get("failed_jobs", []):
        lines.append(f"  failed job: {job['name']} ({job['conclusion']}) {job['url'] or ''}".rstrip())
    return lines


def main(argv=None, runner=run_gh, root=ROOT):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--json", action="store_true", help="print the report as one JSON object")
    parser.add_argument("--branch", default="main", help="the branch whose runs are read (default main)")
    parser.add_argument("--workflow", action="append", dest="workflows",
                        help="a workflow file in .github/workflows/ (repeatable; default: .conclave/ci.json, else all)")
    args = parser.parse_args(argv)
    directory = Path(root) / ".github" / "workflows"
    try:
        workflows = args.workflows or watched(root)
    except ValueError as exc:
        parser.error(str(exc))
    missing = [w for w in workflows if "/" in w or "\\" in w or not (directory / w).is_file()]
    if missing or not workflows or not args.branch.strip():
        parser.error(f"no such workflow in .github/workflows/: {', '.join(missing)}" if missing
                     else "no workflow to watch, or an empty --branch")
    try:
        result, code = report(runner, workflows, args.branch)
    except Unavailable as exc:
        if args.json:
            print(json.dumps({"branch": args.branch, "error": str(exc), "exit": 3}))
        else:
            print(f"ci-status unknown: {exc}")
        return 3
    if args.json:
        print(json.dumps(result, indent=2))
    else:
        for entry in result["workflows"]:
            print("\n".join(describe(entry)))
    return code


if __name__ == "__main__":
    sys.stdout.reconfigure(encoding="utf-8")
    sys.exit(main())
