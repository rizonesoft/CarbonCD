---
name: process-plan
description: Front door in Codex for todo/implementation-plan.md. Audit the plan, then run process-phase on the first phase with a ready row, then the next ready phase after each closeout or park. Use when the user says process the plan. The argument --audit is audit only. A named phase is one phase and does not chain.
---

# Process the Implementation Plan

The only front door for the generated plan (`todo/implementation-plan.md`, or the project's `layout.plan` in `todo/plan.json`) when the user did not name a single phase. The file is a generated projection of the 100% complete development plan (small, single-responsibility sections and granular five-part micro-steps): processing it is not "pick a row and improvise", and it is not "tick the boxes" (boxes flip only through `stamp`).

This skill does not ship a section and does not write a stamp. It audits, then chains `process-phase` runs until the plan is 100% complete or every leftover row is blocked here. Writes stay serial: the run this skill starts is the only writer on the tree.

## The loop

One continuous loop. You do not stop between sections or phases:

1. Audit (below), start the run guard (below), then start `process-phase` on the first phase with a ready row, in this same turn.
2. After that phase's closeout or park, re-run `python scripts/todo-graph.py query ready --have auto` and start the next phase with a ready row: with `--have auto` it lists only the rows whose Needs this session meets (the kit's host probes, the project's `probes` in `todo/plan.json`, and what `CONCLAVE_HAVE` declares, such as `attended`), so a row it leaves out for a host is a leftover here.
3. Stop only when no phase has a ready row: every remaining `[ ]` row is blocked or waiting in this context, or the plan is done. That, and an operator stop, are the only halts.

A report to the operator is not a stop. An audit-only reply while a ready phase exists is a process defect: starting the phase is the next action, in the same turn. The self-correcting parts stay in the loop: drifted plan claims are corrected before building (inside `process-todo-section`), every gap and review leftover files through `add-todo`, and red gates are fixed, never bypassed (no `--no-verify`, no amend of a reviewed candidate, no force-push).

## 0. Route

```powershell
git status -sb
python scripts/todo-graph.py validate
python scripts/todo-graph.py query ready --have auto
```

| State | Do this |
| ----- | ------- |
| Another writer holds the tree | Stop and say so; never run two writers on one tree. |
| Argument is `--audit` | Run the audit below, then stop. Do not start a run. |
| Argument is a phase (`0`, `Phase 0`) | Run `process-phase` for that phase only; do not chain. |
| Empty argument, "the plan", or path | Audit below, then chain ready phases. |

## 1. Audit the whole plan

```powershell
python scripts/todo-graph.py validate
python scripts/todo-graph.py plan --check
python scripts/todo-graph.py query ready --have auto
python scripts/todo-graph.py query blocked
python scripts/todo-graph.py query stats
python -m unittest discover -s scripts/tests
python -m unittest discover -s tests/scripts
```

The two test runs are the kits' own tests, which kits install under `scripts/tests`, and the project's own script tests under `tests/scripts`; run both, skip only a folder the repository does not have, and add any further suite the project's `AGENTS.md` names. A project's tests are never dropped for the kits' or the other way round.

Read `todo/README.md` (the format every TODO edit keeps) before the first edit, then fix every validator refusal before talking about shipping. If `plan --check` is stale, run `plan --sync` and re-check. The deep phase repair belongs to `process-phase` step 1; this audit is the commands above plus five recorded lines in the first phase's run file (`docs/phase-runs/<date>-phase-<N>.md`), not as the turn's last words:

1. Graph: valid section count, or the refusal.
2. Plan currency: the `--check` result.
3. Ready rows: count and first ref per phase.
4. Blocked rows: count and what blocks them (unmet dependencies, waiting Needs, unreachable hosts).
5. The phase being started, and why it is first.

Ready means runnable now in this context: `query ready` already excludes rows waiting on dependencies, inactive files and people Needs; with `--have auto` it also excludes rows needing a host this session does not meet (`python scripts/todo-graph.py detect` prints the set), and `process-phase` re-checks each row with `resolve` rather than trusting a previous list.

If no phase has a ready row, report the leftovers with their blockers and stop. That is a genuine halt.

### Run guard

A run without a guard dies silently when the session stalls, so starting the first phase also starts the run guard, always. In Codex it is the Stop hook; Codex has no scheduled resume inside a session (checked against Codex 0.167.0: `codex queue` and `codex exec resume` act from outside a session).

- **Stop hook** (`.codex/hooks.json`, running `scripts/codex-campaign-hook.py`): while the run is open it answers Codex's Stop event with `{"decision": "block"}` and names the next ready row. It lets the turn end when the guard file is gone, the run file has a `## Closeout run=<run id>` heading or a column-0 `PARKED <UTC> run=<run id>` line, or `query ready` has nothing runnable, and it never blocks on its own failure (it answers `{}`). Its stall breaker lets a turn end after 3 pushes with no change to the tree and counts a trip: a turn let end is not a released run. Only `end` releases it, or the second trip counted through `heartbeat-still`. Codex runs the hook only once it is approved: `forge.py kit` approves it at install, and `python scripts/codex-hook-trust.py` approves it again after the hook changes. `python forge.py doctor . todo` reports `stop-hook:.codex` and `stop-hook-approved:.codex`.
- **No heartbeat in the session.** The run is held by the Stop hook alone. Only a run started through the unattended kit's launcher (`scripts/unattended-run.py start`, D02 T02 §5 to §7) is restarted and nudged from outside, and its heartbeat runs `heartbeat-still` on each resumption; installing that kit does not supervise a `process-plan` session started by hand.

The session is this Codex thread's id, `$CODEX_THREAD_ID` in a POSIX shell and `$env:CODEX_THREAD_ID` in PowerShell (the `session_id` the Stop hook receives). Never use another agent's variable such as `CLAUDE_CODE_SESSION_ID`: a Codex run started from a Claude Code session inherits it, and a guard acquired for it would hold the wrong session. Commands are written `python`; on Linux and WSL run them with `python3`. In PowerShell write each `$VAR` as `$env:VAR`.

#### Startup

Check that no other session owns the tree. It prints `NO GUARD`, or this session as the owner; any other owner means another run is live: start nothing and report it.

```sh
python scripts/campaign_guard.py whoami --session "$CODEX_THREAD_ID"
```

Mint a generation, then acquire. Codex keeps no timer, so the job id is `none`. A refused `acquire` means another session owns the run: start nothing and report the owner.

```sh
python scripts/campaign_guard.py mint-generation
python scripts/campaign_guard.py acquire --session "$CODEX_THREAD_ID" --phase <N> --run-file <run file> --cron-id none --generation <generation> --scope plan
```

Read the guard back, then record the session, generation and run id in the run file's Critical events.

```sh
python scripts/campaign_guard.py whoami --session "$CODEX_THREAD_ID" --generation <generation>
```

#### Phase change

At each phase change (section 2), write the new phase's run file, then re-point the guard to it with a fresh generation, for the same session, and read it back:

```sh
python scripts/campaign_guard.py mint-generation
python scripts/campaign_guard.py acquire --session "$CODEX_THREAD_ID" --phase <N> --run-file <run file> --cron-id none --generation <generation> --scope plan
python scripts/campaign_guard.py whoami --session "$CODEX_THREAD_ID" --generation <generation>
```

#### End

Every way a run ends goes through `end`, never a direct delete. The reason needs its marker in the run file first: `closeout` a `## Closeout run=<run id>` heading, `park` a column-0 `PARKED <UTC> run=<run id> <reason>` line, `escalation` a `PARKED ... escalation: <cause>` line; `plan-done` needs every row stamped, `stall` two trips, and `operator-stop` nothing. Then confirm the (empty) job cancellation and that the guard is gone:

```sh
python scripts/campaign_guard.py end --session "$CODEX_THREAD_ID" --reason <reason>
python scripts/campaign_guard.py cancel-confirmed --cron-id none
python scripts/campaign_guard.py whoami --session "$CODEX_THREAD_ID"
```

The last command prints `NO GUARD` once the run is released.

## 2. After a phase closeout or park

`process-phase` ends in exactly one of three ways: the phase table is all `[x]` with a written closeout; every leftover is blocked here and it parked (a `PARKED` line and park record in its run file); or the operator paused it. A parked phase is not complete and not a stall. Do not call it either.

Re-run `python scripts/todo-graph.py query ready --have auto`. If another phase has a ready row this session can run (Needs within reach, as in the loop above), re-point the guard to it first (Run guard, Phase change), then start `process-phase` on it in the same turn, same session, same rules. If none does, end the run (Run guard, End): `plan-done` only when `query stats` shows `verified` equal to `total`; otherwise write a column-0 `PARKED <UTC> run=<run id> <one-line reason>` line and the park record (each leftover and what blocks it) in the current run file, commit it, then end with `park`. Then report which: plan done, or the leftovers and their blockers, with the run files named.

## Deny

- Do not invent a side loop that ships rows outside `process-todo-section` plus `review-todo-section`.
- Do not start a second run, and do not resume a paused run unless the operator said resume.
- Do not start a run without its guard, and do not end, stop or pause one without releasing it through `end`.
- Do not tick the generated plan (`implementation-plan.md`, or `layout.plan` in `todo/plan.json`) by hand, and do not edit it at all: it is generated.
- Do not claim a phase complete while its table has `[ ]` rows.
- Do not treat a named phase as the whole plan. Chaining is this skill's job, and only this skill's.
- Do not end the turn on the audit table.
