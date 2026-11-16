#!/usr/bin/env python3
"""Merge dependency pull requests (Dependabot and any configured producer) when it is safe, one at a time.

    python scripts/merge_prs.py              # merge what is eligible now
    python scripts/merge_prs.py --dry-run    # say what would merge, change nothing
    python scripts/merge_prs.py --scheduled  # the scheduled merge task: stand aside while a campaign runs

Safe to run: the working tree is clean, HEAD is origin/main after a fetch (nothing waits to be pushed or pulled), no
TODO section is in progress (open, started and ready), and, when scheduled, no campaign run guard is held.

Eligible: an open pull request into main, opened by one of the producers, carrying one of the labels, mergeable, its
head commit's checks all finished and green (at least one check ran) with every required check succeeded, and its branch
not on the excluded prefixes (pins that are section work, merged by that section instead). Each is re-read right before
it is judged. The scheduled run holds the campaign guard's lock for its whole window.

The policy comes from the optional `merge` object in `.conclave/ci.json`; without it only Dependabot pull requests that
change nothing outside `.github/` qualify, labelled `dependencies`, with no named required check:

    {"merge": {"required_checks": ["build", "plan-gates"],
               "labels": ["dependencies", "pins"],
               "producers": [{"authors": ["app/github-actions", "github-actions[bot]"],
                              "branch": "pins/[a-z0-9.-]+", "files": ["toolchain.json"]}],
               "excluded_branches": ["pins/section-"]}}

Each producer is an author set, a branch pattern that must match the whole head branch, and the only files it may
change; configured producers are added to Dependabot, never replace it.

Each merge: `gh pr merge <n> --rebase --match-head-commit <sha>` (linear history, and only the commit that was
checked), then `git pull --ff-only`, then `todo-graph.py validate`. The first failure stops the run, saying whether that
pull request is still open (the merge failed) or merged (the local follow-up failed); later ones wait for the operator.

Exit 0: merged, or nothing eligible, or not safe right now (said why). Exit 1: a merge or its follow-up failed.
"""
import argparse
import json
from pathlib import Path
import re
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts" / "lib"))
import todo_graph  # noqa: E402

sys.path.insert(0, str(ROOT / "scripts"))
import campaign_guard  # noqa: E402

# Who may open a pull request this merges, the branch that producer uses, and the files it may change (None: only
# files under .github/). An author alone proves nothing (any workflow can open a pull request as github-actions), so
# branch and files must also be the producer's own. Configured producers are appended by load_policy.
DEPENDABOT = ({"app/dependabot", "dependabot[bot]"}, re.compile(r"dependabot/[\w./-]+"), None)
PRODUCERS = (DEPENDABOT,)
LABELS = {"dependencies"}
# Branch prefixes of pin pull requests that belong to TODO section work and are merged there, never here.
EXCLUDED_BRANCHES: tuple[str, ...] = ()
GREEN = {"success", "neutral", "skipped"}
# The jobs that prove a change builds and keeps the plan valid. They are read from the head commit, not the pull
# request: GitHub does not list workflow_dispatch runs among a pull request's checks, so a producer that dispatches its
# builds onto its own commit is still judged by them.
REQUIRED_CHECKS: set[str] = set()
CONFIG = Path(".conclave") / "ci.json"
GUARD = Path("build") / "claude-campaign-guard.json"
# Written when a run stops after asking GitHub to merge (the merge failed, or its pull or validate did): every later run
# refuses until the operator has looked and deletes it, so no further pull request merges onto a state nobody checked.
STOPPED = Path("build") / "merge-prs-stopped.json"


class Unsafe(Exception):
    """Not safe to merge right now; nothing was changed."""


def _strings(value, what):
    if not (isinstance(value, list) and all(isinstance(v, str) and v for v in value)):
        raise RuntimeError(f"{CONFIG.as_posix()} merge.{what} must be a list of non-empty strings")
    return value


def load_policy(root):
    """Set PRODUCERS, LABELS, EXCLUDED_BRANCHES and REQUIRED_CHECKS from .conclave/ci.json's merge object; a missing
    file or object keeps the Dependabot-only defaults, a malformed one is an error (nothing merges on a guess)."""
    global PRODUCERS, LABELS, EXCLUDED_BRANCHES, REQUIRED_CHECKS
    PRODUCERS, LABELS, EXCLUDED_BRANCHES, REQUIRED_CHECKS = (DEPENDABOT,), {"dependencies"}, (), set()
    path = Path(root) / CONFIG
    if not path.is_file():
        return
    try:
        doc = json.loads(path.read_text(encoding="utf-8"))
    except ValueError:
        raise RuntimeError(f"{CONFIG.as_posix()} is not JSON") from None
    policy = doc.get("merge") if isinstance(doc, dict) else None
    if policy is None:
        return
    if not isinstance(policy, dict):
        raise RuntimeError(f"{CONFIG.as_posix()} merge must be an object")
    known = {"required_checks", "labels", "producers", "excluded_branches"}
    if set(policy) - known:
        raise RuntimeError(f"{CONFIG.as_posix()} merge has unknown keys: {', '.join(sorted(set(policy) - known))}")
    REQUIRED_CHECKS = set(_strings(policy.get("required_checks", []), "required_checks"))
    LABELS = set(_strings(policy.get("labels", ["dependencies"]), "labels")) or {"dependencies"}
    EXCLUDED_BRANCHES = tuple(_strings(policy.get("excluded_branches", []), "excluded_branches"))
    producers = policy.get("producers", [])
    if not isinstance(producers, list):
        raise RuntimeError(f"{CONFIG.as_posix()} merge.producers must be a list")
    extra = []
    for p in producers:
        if not isinstance(p, dict) or set(p) != {"authors", "branch", "files"} or not isinstance(p["branch"], str):
            raise RuntimeError(f"{CONFIG.as_posix()} each merge producer needs exactly authors, branch and files")
        try:
            branch = re.compile(p["branch"])
        except re.error:
            raise RuntimeError(f"{CONFIG.as_posix()} merge producer branch is not a valid pattern") from None
        extra.append((set(_strings(p["authors"], "producers.authors")), branch,
                      set(_strings(p["files"], "producers.files"))))
    PRODUCERS = (*extra, DEPENDABOT)


def run_cmd(args, root):
    # No console window per launch: the scheduled task runs without one (.conclave/automations/run.py).
    return subprocess.run(args, cwd=root, capture_output=True, text=True, encoding="utf-8", errors="replace",
                          timeout=600, creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))


def redact(text):
    """text with every URL and scp-style remote replaced, since a remote can carry a credential and this text reaches
    logs and email."""
    text = re.sub(r"\b[A-Za-z][A-Za-z0-9+.-]*://\S+", "<url>", text)
    return re.sub(r"\b[\w.-]+@[\w.-]+:\S+", "<remote>", text)


def check(run, root, args, what):
    try:
        result = run(args, root)
    except (OSError, subprocess.TimeoutExpired) as e:
        raise RuntimeError(f"{what} did not finish: {e}") from None
    if result.returncode != 0:
        detail = (result.stderr or result.stdout).strip()[:400]
        raise RuntimeError(f"{what} failed (exit {result.returncode}): {redact(detail)}")
    return result.stdout


def section_in_progress(root):
    """An open section with a Started line that is ready to work: someone is building it now. A started section that
    later became blocked is not in progress."""
    rows = todo_graph.graph(root)
    for row in todo_graph.ordered(rows):
        if (not row.done and re.search(r"^> \*\*Started:\*\*", row.body, re.M)
                and todo_graph.disposition(row, rows) == 0):
            return row.ref
    return None


def ensure_safe(run, root, scheduled, fetch=True):
    """Raise Unsafe unless this checkout may take a merge now. With fetch=False only local state is read (no network),
    which is what runs in the last instant before a merge request: no edit, section or campaign can start unseen
    during the pull request's own (network) judging."""
    if scheduled and (Path(root) / GUARD).exists():  # read under the guard lock (main), so no campaign starts after it
        raise Unsafe("a campaign run holds the guard; the loop merges at its own section boundaries")
    # Resolved before any fetch, so an origin that is not github.com is refused before its URL can reach a network
    # error message.
    REPO[0] = origin_repo(run, root)
    if check(run, root, ["git", "status", "--porcelain"], "git status").strip():
        raise Unsafe("the working tree has changes")
    if fetch:
        check(run, root, ["git", "fetch", "--quiet", "origin", "main"], "git fetch")
    head = check(run, root, ["git", "rev-parse", "HEAD"], "git rev-parse HEAD").strip()
    upstream = check(run, root, ["git", "rev-parse", "origin/main"], "git rev-parse origin/main").strip()
    if head != upstream:
        raise Unsafe(f"HEAD {head[:7]} is not origin/main {upstream[:7]}")
    ref = section_in_progress(root)
    if ref:
        raise Unsafe(f"section {ref} is in progress")


def judge(run, root, number):
    """(pull request, reason it may not merge or ""), from one bracketed read: the pull request, its head commit's
    checks and statuses, its test merge's, then the pull request again and its head commit's checks and statuses again.
    The second half is what decides, so a label, draft flag, base, head or check that changed while the first half was
    being read is seen. GitHub offers no snapshot across these calls, and its merge call can pin only the head commit,
    so what remains is the instant between this last read and the merge request (one local check)."""
    pr = current(run, root, number)
    reason = why_not(pr, check_runs(run, root, pr["headRefOid"]), commit_statuses(run, root, pr["headRefOid"]))
    if reason:
        return pr, reason
    merge_sha = test_merge_sha(run, root, number)
    if merge_sha and not test_merge_green(check_runs(run, root, merge_sha), commit_statuses(run, root, merge_sha)):
        return pr, "a check on its test merge with main is failing or unfinished"
    final = current(run, root, number)
    if final.get("headRefOid") != pr["headRefOid"]:
        return final, "its head moved while it was judged"
    return final, why_not(final, check_runs(run, root, pr["headRefOid"]), commit_statuses(run, root, pr["headRefOid"]))


def origin_repo(run, root):
    """github.com/owner/name of the repository behind origin: every gh call names it with its host, so an inherited
    GH_REPO or GH_HOST can never point the merges at another repository or server than the one this clone checked."""
    url = check(run, root, ["git", "remote", "get-url", "origin"], "git remote get-url origin").strip()
    m = re.fullmatch(r"(?:https://github\.com/|git@github\.com:|ssh://git@github\.com/)([\w.-]+/[\w.-]+?)(?:\.git)?/?", url)
    if m is None:
        # The URL itself is never repeated: it can carry a token, and this message reaches logs and email.
        raise RuntimeError("origin is not a github.com repository")
    return "github.com/" + m.group(1)


def api(path):
    """gh api arguments for a REST path on github.com, the host origin names (GH_HOST is never followed)."""
    return ["gh", "api", "--hostname", "github.com", path]


def check_runs(run, root, sha):
    """Every check run on a commit: (name, status, conclusion)."""
    out = check(run, root, [*api(f"repos/{owner_name()}/commits/{sha}/check-runs?per_page=100"), "--paginate",
                            "--jq", ".check_runs[] | [.name, .status, .conclusion]"], f"reading checks of {sha[:7]}")
    return [tuple(json.loads(line)) for line in out.splitlines() if line.strip()]


def commit_statuses(run, root, sha):
    """Every legacy commit status on a commit: (context, state). CI can report through these as well as check runs."""
    # Every page: the endpoint returns 30 statuses a page by default, and a failing one may sit past the first.
    out = check(run, root, [*api(f"repos/{owner_name()}/commits/{sha}/status?per_page=100"), "--paginate",
                            "--jq", ".statuses[] | [.context, .state]"], f"reading statuses of {sha[:7]}")
    return [tuple(json.loads(line)) for line in out.splitlines() if line.strip()]


def checks_green(runs):
    """Every check run on the commit completed green, and each required job succeeded (no checks never passes, even
    with no required check configured)."""
    # The required jobs must have actually run and passed (a skipped build proves nothing); any other check may also be
    # neutral or skipped, but every check must have finished.
    if not runs or not REQUIRED_CHECKS <= {name for name, _, conclusion in runs if conclusion == "success"}:
        return False
    return all(status == "completed" and conclusion in GREEN for _, status, conclusion in runs)


def why_not(pr, runs, statuses=()):
    """Why a pull request is not eligible, or None when it is. runs and statuses: the check runs and the legacy commit
    statuses on its head commit."""
    author = (pr.get("author") or {}).get("login")
    producer = next((p for p in PRODUCERS if author in p[0]), None)
    if producer is None:
        return "not from Dependabot or a configured producer"
    _, branch, files = producer
    if not branch.fullmatch(pr.get("headRefName", "")):
        return "not on its producer's branch"
    paths = [f.get("path", "") for f in pr.get("files") or []]
    if not paths or len(paths) >= 100 or not all(path in files if files else path.startswith(".github/")
                                                    for path in paths):
        return "changes files its producer never changes"
    if not LABELS & {label["name"] for label in pr.get("labels") or []}:
        return f"not labelled {' or '.join(sorted(LABELS))}"
    if pr.get("state") != "OPEN" or pr.get("baseRefName") != "main":
        return "not an open pull request into main"
    if pr.get("isDraft") is not False:
        return "a draft (GitHub refuses to merge one)"
    if EXCLUDED_BRANCHES and pr.get("headRefName", "").startswith(EXCLUDED_BRANCHES):
        return "a pin that is section work"
    if pr.get("mergeable") != "MERGEABLE":
        return f"GitHub says {pr.get('mergeable')}"
    if not checks_green(runs):
        return "its head commit's checks are not all green, or a required job did not run"
    if any(state != "success" for _, state in statuses):
        return "a commit status on its head commit is not success"
    # The pull request's check rollup (gh reads it from the head commit): any check that has run must have passed.
    # Dispatched runs do not appear here, so an empty rollup is allowed; the head commit's checks above prove the
    # change builds, and the test merge's own checks are read separately (test_merge_sha).
    for item in pr.get("statusCheckRollup") or []:
        if item.get("__typename") == "StatusContext":
            if item.get("state") != "SUCCESS":
                return "a check on the pull request (or its test merge) is not success"
        elif item.get("status") != "COMPLETED" or item.get("conclusion") not in {"SUCCESS", "NEUTRAL", "SKIPPED"}:
            return "a check on the pull request (or its test merge) is not success"
    return None


FIELDS = ("number,title,author,labels,baseRefName,headRefName,headRefOid,mergeable,state,isDraft,files,mergeCommit,"
          "statusCheckRollup")
REPO: list[str] = [""]  # github.com/owner/name, set from origin by merge_all before any gh call


def owner_name():
    return REPO[0].split("/", 1)[1]


SETTLE_TRIES, SETTLE_WAIT = 5, 3.0


def current(run, root, number):
    """A pull request as it is now. GitHub computes mergeability in the background and answers UNKNOWN meanwhile, so an
    UNKNOWN is asked again a few times; one still UNKNOWN is skipped this run."""
    for attempt in range(SETTLE_TRIES):
        pr = json.loads(check(run, root, ["gh", "pr", "view", str(number), "--repo", REPO[0], "--json", FIELDS],
                              f"gh pr view {number}"))
        if pr.get("mergeable") != "UNKNOWN" or attempt == SETTLE_TRIES - 1:
            return pr
        time.sleep(SETTLE_WAIT)


def test_merge_sha(run, root, number):
    """The commit GitHub has built for merging this pull request into current main (potentialMergeCommit), or "" when
    it has none. gh pr view's statusCheckRollup reads the head commit only, so the test merge is asked for directly."""
    owner, name = owner_name().split("/")
    query = ("query($owner: String!, $name: String!, $number: Int!) { repository(owner: $owner, name: $name) "
             "{ pullRequest(number: $number) { potentialMergeCommit { oid } } } }")
    out = check(run, root, ["gh", "api", "graphql", "--hostname", "github.com", "-f", f"query={query}",
                            "-F", f"owner={owner}", "-F", f"name={name}", "-F", f"number={number}",
                            "--jq", ".data.repository.pullRequest.potentialMergeCommit.oid // \"\""],
                f"reading #{number}'s test merge")
    return out.strip()


def test_merge_green(runs, statuses):
    """Every check run and status on the test merge commit has finished green (none at all is fine: no check ran there,
    as for a producer that dispatches its builds onto the head commit)."""
    return (all(status == "completed" and conclusion in GREEN for _, status, conclusion in runs)
            and all(state == "success" for _, state in statuses))


def open_prs(run, root):
    """Open pull requests into main, oldest first; each is re-read just before it is judged."""
    out = check(run, root, ["gh", "pr", "list", "--repo", REPO[0], "--state", "open", "--base", "main", "--limit",
                            "100", "--json", "number"], "gh pr list")
    return sorted(pr["number"] for pr in json.loads(out))


def stop(root, out, message):
    """Report message, and record it so later runs refuse until the operator clears it."""
    out(message)
    record_stop(root, message)
    return 1


def record_stop(root, message):
    path = Path(root) / STOPPED
    path.parent.mkdir(parents=True, exist_ok=True)
    tmp = path.with_suffix(".json.tmp")
    tmp.write_text(json.dumps({"stopped": message}, indent=2) + "\n", encoding="utf-8")
    tmp.replace(path)


def merge_all(run, root, args, out):
    stopped = Path(root) / STOPPED
    if stopped.exists():
        out(f"merge-prs: an earlier run stopped and left {STOPPED.as_posix()}; check main, then delete it to resume")
        return 1
    try:
        load_policy(root)
        ensure_safe(run, root, args.scheduled)
        numbers = open_prs(run, root)
    except Unsafe as e:
        out(f"merge-prs: not now: {e}")
        return 0
    except RuntimeError as e:
        out(f"merge-prs: {e}")
        return 1
    merged = 0
    for index, number in enumerate(numbers):
        if index:
            # Safe at the start is not safe for the whole batch: the operator may have started editing or a section
            # since the last merge, so each merge checks again.
            try:
                ensure_safe(run, root, args.scheduled)
            except Unsafe as e:
                out(f"merge-prs: stopped after {merged} merged: {e}; any later pull request waits")
                return 0
            except RuntimeError as e:
                out(f"merge-prs: {e}")
                return 1
        try:
            # Judged on its state right now, after any earlier merge: labels, base, mergeability and the checks of the
            # exact commit that will merge, and of its test merge.
            pr, reason = judge(run, root, number)
        except RuntimeError as e:
            out(f"merge-prs: {e}")
            return 1
        if reason:
            out(f"merge-prs: #{number} skipped: {reason} ({pr['title']})")
            continue
        if args.dry_run:
            out(f"merge-prs: #{number} would merge at {pr['headRefOid'][:7]} ({pr['title']})")
            continue
        # The checkout is checked again after the network reads, locally only, in the last instant before the request:
        # an edit, a section or a campaign started while the pull request was being judged stops the run. A retarget in
        # that instant is caught afterwards: the merge commit then is not in main, the follow-up fails and the run stops.
        try:
            ensure_safe(run, root, args.scheduled, fetch=False)
        except Unsafe as e:
            out(f"merge-prs: stopped after {merged} merged: {e}; any later pull request waits")
            return 0
        except RuntimeError as e:
            out(f"merge-prs: {e}")
            return 1
        # Recorded before GitHub is asked, cleared only once the merge is verified locally: a run killed in between
        # (the task's timeout, a crash) leaves the marker, and the next run refuses until the operator has looked.
        record_stop(root, f"merge-prs: merging #{number} was interrupted before its pull and validate finished")
        try:
            check(run, root, ["gh", "pr", "merge", str(number), "--repo", REPO[0], "--rebase", "--match-head-commit",
                              pr["headRefOid"]], f"merging #{number}")
        except RuntimeError as e:
            # A failed or timed-out merge may still have landed: say what GitHub reports, or that it is unknown.
            try:
                state = current(run, root, number).get("state", "UNKNOWN")
            except RuntimeError:
                state = "unknown (GitHub could not be read)"
            return stop(root, out, f"merge-prs: stopped at #{number}, whose state is now {state}: {e}; it and any later pull request "
                f"are left for you")
        # A successful exit can mean queued rather than merged: only GitHub's MERGED state, and its merge commit in the
        # pulled checkout, count.
        try:
            landed = current(run, root, number)
        except RuntimeError as e:
            return stop(root, out, f"merge-prs: #{number}'s state after merging could not be read: {e}; stopped")
        if landed.get("state") != "MERGED" or not (landed.get("mergeCommit") or {}).get("oid"):
            return stop(root, out, f"merge-prs: #{number} is {landed.get('state')} after the merge request (queued or refused); stopped, "
                f"any later pull request waits")
        merged += 1
        out(f"merge-prs: merged #{number} ({pr['title']})")
        try:
            check(run, root, ["git", "pull", "--ff-only", "--quiet", "origin", "main"], "git pull --ff-only")
            check(run, root, ["git", "merge-base", "--is-ancestor", landed["mergeCommit"]["oid"], "HEAD"],
                  f"#{number}'s merge commit in the local checkout")
            check(run, root, [sys.executable, str(Path(root) / "scripts" / "todo-graph.py"), "validate"],
                  "todo-graph validate")
            (Path(root) / STOPPED).unlink()
        except RuntimeError as e:
            return stop(root, out, f"merge-prs: #{number} merged on GitHub, but the local follow-up failed: {e}; stopped, any later pull "
                f"request is left for you")
    out(f"merge-prs: {merged} merged")
    return 0


def main(argv=None, run=run_cmd, root=ROOT, out=print):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--scheduled", action="store_true")
    args = parser.parse_args(argv)
    # Every run holds the campaign guard's lock for its whole window: two runs never interleave a merge with another's
    # pull and validate, and (for the scheduled run) a campaign's acquire waits, so none starts between the guard check
    # and the last merge.
    try:
        with campaign_guard._Lock(str(root)):
            return merge_all(run, root, args, out)
    except campaign_guard.GuardError as e:
        out(f"merge-prs: not now: {e}")
        return 0

if __name__ == "__main__":
    sys.exit(main())
