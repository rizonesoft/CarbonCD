---
name: process-phase
description: Attended runner in Antigravity that takes one phase of todo/implementation-plan.md to 100 percent. Repair the phase, gap-check it, then ship section after section via process-todo-section plus review-todo-section, parking only when every leftover row is blocked here. Use when the user says process, run, or finish a phase.
---

# Process Phase

One phase, start to 100%, or parked when the rest of it cannot run here. You do not stop in between.

Exactly three endings: zero open rows and a written closeout; every leftover row blocked or waiting in this context, so the phase is parked and `process-plan` moves to the next ready phase; or the operator's own pause. A parked phase is neither complete nor a stall.

The whole plan is `process-plan`, not this skill. When the session entered through `process-plan`, return to it after closeout or park; a session pinned to one phase ends here.

Attended means interruptible, not stoppable: answer a mid-run message briefly and continue the loop in the same turn. The one exception outranks everything: if the user tells you to stop or pause, obey immediately, record it in the run file, and wait.

Completion never buys completion with a bypass: no `--no-verify`, no amend of a candidate under review, no force-push. A red gate is work, not a skip.

## Step 0: open the run

Read `todo/README.md` (the format every TODO edit keeps) before the run's first write, then check that no other writer holds the tree (`git status`; ask about unfamiliar uncommitted work). Writes are serial: this run is the only writer. Open the run file `docs/phase-runs/<YYYY-MM-DD>-phase-<N>.md` (create `docs/phase-runs/` if absent) and append every finding the moment it is made: the file survives session death where chat scrollback does not.

```markdown
# Phase run: <heading>

## Phase repairs              (Step 1: what was wrong with the plan, what was corrected, where)
## Shipped-row verification   (Step 1b: each spine [x] row, stamp and re-run checkpoint result)
## Gap audit                  (Step 2: gaps found, where each was filed; the park record lands here)
## Sections                   (Step 3: one entry per section: ref, outcome, review verdict)
## Critical events            (every stop, pause, resume, session death)
## Lessons                    (what was learned this run, worth keeping)
```

Read the most recent prior run file for this phase, if one exists: anything unresolved there is this run's first input.

## Step 1: repair the phase before running it

1. `python scripts/todo-graph.py validate`: fix every refusal now.
2. `python scripts/todo-graph.py plan --check`: if stale, `plan --sync`.
3. For every open row in the phase: `python scripts/todo-graph.py resolve '<ref>' --have auto`. Record the exit code. Exit 4 with unmet dependencies outside this phase, or a waiting Need this session cannot satisfy (`owner-decision`, `third-party`, `operator`, or a host that is not reachable), is a leftover, not a stall: leave the row, ship every exit-0 row, and park when only leftovers remain. Exit 1 or 2 means the plan cites a section that does not exist: repair the reference against the TODO files; a broken reference blocks a park.
4. Read each open section's TODO file top to bottom for phase-level staleness only (per-section validation happens again inside `process-todo-section`): work already shipped elsewhere, sections made moot by a later decision, Current state blocks contradicted by the tree. Correct with dated `**Corrected YYYY-MM-DD:**` notes.

### Step 1b: shipped rows are verified, not trusted

`[x]` rows are claims. Re-check the shipped rows an open row in this phase depends on (the spine): `resolve` exits 3, the receipt matches its stamp, and the section's Test checkpoint still passes when re-run. A checkpoint that no longer passes means the section regressed: treat it as this phase's work through `review-todo-section` in audit stance before building on it.

## Step 2: gap-audit the phase for 100% completeness

Read the phase as a user would use it, end to end, against its governing documents (the milestone "Done when" in the project's governing plan, the SPEC sections the phase implements). Ask what is missing for a 100% complete phase: screens, dialogs, controls, settings consumers, refusal/error paths, undo/reverse paths, persistence read-backs, or handoffs between domains nobody specified. Split any coarse umbrella sections into small, single-responsibility sections and expand any summary checkboxes into concrete five-part micro-steps. File each gap with `add-todo` (evidence, owner, dependency) and sync the plan. Never cap planning or silently drop scope. A gap found is a gap filed the same turn.

## Step 3: ship the phase, one row at a time

In the generated plan's table order, for each open row: `process-todo-section`, then `review-todo-section`, which runs the review passes the pin's schedule sets (`python scripts/review_pin.py --check`; `[schedule] policy`: three-pass is plan review, one round, completion review and reconciliations after gaps, rotation is completion review before the rounds) and whose completion verdict every new stamp carries (`stamp --completion '<findings file>:<verdict>'`, beside the plan review's `--plan` findings file; three-pass verdicts are `complete`, `reconciled` and `filed`, rotation verdicts `complete`, `completed` and `filed`). A section that reaches the pin's `pass_cap` with gaps open stays unstamped: record it in its findings file and the run file and carry on with the next row. Every gap it files becomes a new section through `add-todo` in this phase or the one that owns it. Record each outcome in the run file, with the completion verdict and any sections filed. Commit the section work, then its stamp commit. After the stamp commit is pushed, run `python scripts/merge_prs.py` before claiming the next row: it merges green dependency pull requests (Dependabot, plus any producer the `merge` policy in `.conclave/ci.json` names) only when the tree is clean, `HEAD` is `origin/main` and no section is in progress, one at a time, and leaves any that fails for the operator. Record its last line in the run file when it merged or stopped.

Then check CI, red first: run `python scripts/ci_status.py` (the latest completed run of each watched workflow on main, and whether newer runs are still in progress) before claiming the next row.

- Exit 0: no watched workflow is red; carry on.
- Exit 1 (red): red comes before any new row. Read the failed job's log, bounded (`gh run view <id> --log-failed`, the last 200 lines into scratch). If it is an infrastructure or flaky failure (a runner outage, a network or download failure, or a test the run file already records as flaky), run `gh run rerun <id> --failed` once and record it; a second red on the same commit is a real failure. Otherwise diagnose and fix it at the owning layer: a fix commit with a regression test that fails before it, or a section through `add-todo` when the fix is real scope (that section is then the next row). Never skip, disable or loosen a check, test or workflow to make CI green.
- Exit 3 (`gh` missing, unauthenticated or failing): the state is unknown, never green. Record it in the run file and carry on; the next boundary checks again.
- Record every red in the run file's Sections or Critical events: workflow, commit, run URL, failed jobs, the cause and the outcome (rerun green, fix commit, or the section filed).

Long jobs never stall the run:

- One heavy job at a time: a build, an instrumented or sanitizer build, a review round and a long test suite never overlap, because other sessions may share the machine. Before each, read the free memory (`(Get-Counter '\Memory\Available MBytes').CounterSamples[0].CookedValue` on Windows, `free -m` on Linux); under about 3000 MB, wait (re-check every minute, up to 30 minutes) instead of starting a job the host may kill.
- After launching a background job, confirm within a few minutes that its process exists and its output file is growing or it has finished. Never wait on a job whose process is gone: a job that ended without its result (killed, no output, no notification) is relaunched once, alone, and the relaunch recorded in the findings file and the run file; a second loss is a Critical event, and the row continues with the next step that does not need that result or parks as a leftover.
- An idle wait longer than 15 minutes with no job alive is a stall: re-check the job, resume the row, and record it.

Skip rows whose `resolve` is not exit 0 here, and rows `resolve --have auto` holds for a host this session does not meet (exit 4, with the host in `missing_needs`; step 1.3 records which), and re-check them after each stamp: the graph moves as rows flip. When every remaining open row is a leftover, the phase parks: write a column-0 `PARKED <UTC stamp> run=<run id> <one-line reason>` line in the run file (the run id `acquire` printed, so the Stop hook lets the turn end) followed by the park record (each leftover, what blocks it, where the blocker lives), commit the run file, and return to `process-plan` (or end, if pinned).

## Step 4: closeout

When the phase table is all `[x]`: `process-todo-file` on any file the phase exhausted, re-run the full test suite once, confirm `query stats` and the regenerated plan show the phase complete, write the closeout under a `## Closeout run=<run id>` heading in the run file (what shipped, what was repaired, what was learned), commit, and report. A phase is complete when its table says so and the closeout is written: not before.

## Guardrails

- Do not invent a side loop that ships rows outside `process-todo-section` plus `review-todo-section`.
- Do not tick the generated plan (`implementation-plan.md`, or `layout.plan` in `todo/plan.json`) by hand. It is generated; sync it.
- Do not claim a phase complete while its table has `[ ]` rows, and do not call a parked phase complete or a stall.
- Do not end the turn on the audit. Ship, park, or close out.
- Do not claim a new row while `scripts/ci_status.py` reports main red, and never make CI green by skipping, disabling or loosening a check.
