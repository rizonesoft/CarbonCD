---
name: process-plan
description: Front door in Antigravity for todo/implementation-plan.md. Audit the plan, then run process-phase on the first phase with a ready row, then the next ready phase after each closeout or park. Use when the user says process the plan. The argument --audit is audit only. A named phase is one phase and does not chain.
---

# Process the Implementation Plan

The only front door for the generated plan (`todo/implementation-plan.md`, or the project's `layout.plan` in `todo/plan.json`) when the user did not name a single phase. The file is a generated projection of the 100% complete development plan (small, single-responsibility sections and granular five-part micro-steps): processing it is not "pick a row and improvise", and it is not "tick the boxes" (boxes flip only through `stamp`).

This skill does not ship a section and does not write a stamp. It audits, then chains `process-phase` runs until the plan is 100% complete or every leftover row is blocked here. Writes stay serial: the run this skill starts is the only writer on the tree.

## The loop

One continuous campaign loop held by the run guard, with Orchestrator (Carbon) planning one TODO section per `exit_plan_mode` turn:

1. Audit (below), start the run guard (below), and enter `process-phase` for the first phase with a ready row. Under Orchestrator (Carbon), Carbon plans one ready TODO section at a time before calling `exit_plan_mode`, and Hydrogen hands back to Carbon after each section's `stamp`, `merge_prs.py`, and `ci_status.py` check before resolving the next ready row (whether `Surface` is `yes` or `no`).
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

A run without a guard dies silently when the session stalls, so starting the first phase also starts the run guard, always. In Antigravity it has two parts.

- **Stop hook** (`.agents/hooks.json`, the todo kit's seed): while the run is open it answers Antigravity's Stop event with `{"decision": "continue"}`, which re-enters the loop, and names the next ready row. It lets the turn end when the guard file is gone, the run file has a `## Closeout run=<run id>` heading or a column-0 `PARKED <UTC> run=<run id>` line, `query ready` has nothing runnable, or the operator cancelled. Its stall breaker lets a turn end after 3 pushes with no change to the tree and counts a trip: a turn let end is not a released run. Only `end` releases it, or the second trip counted through `heartbeat-still`. `python forge.py doctor . todo` reports the hook as `stop-hook:.agents`.
- **Heartbeat**: a recurring timer made with Antigravity's `schedule` tool in this conversation (checked against agy 1.5.6, whose agent schedules timers and recurring cron jobs that `/tasks` lists). It resumes the session when it goes idle, runs `heartbeat-still` first, and ends itself when the run is over.

The session is this conversation's id, `$ANTIGRAVITY_CONVERSATION_ID` in a POSIX shell and `$env:ANTIGRAVITY_CONVERSATION_ID` in PowerShell (the `conversationId` the Stop hook receives). Never use another agent's variable such as `CLAUDE_CODE_SESSION_ID`. Commands are written `python`; on Linux and WSL run them with `python3`. In PowerShell write each `$VAR` as `$env:VAR`.

When no scheduled resume is available (the `schedule` tool is not offered, or the timer could not be made), the run is held by the Stop hook alone. Only a run started through the unattended kit's launcher (`scripts/unattended-run.py start`, D02 T02 §5 to §7) is restarted and nudged from outside; installing that kit does not supervise a `process-plan` session started by hand.

#### Startup

Check that no other session owns the tree. It prints `NO GUARD`, or this session as the owner; any other owner means another run is live: start nothing and report it.

```sh
python scripts/campaign_guard.py whoami --session "$ANTIGRAVITY_CONVERSATION_ID"
```

Mint a generation. Create the recurring timer with the `schedule` tool, cron `*/5 * * * *`, its prompt the heartbeat prompt below with `<N>`, `<run file>` and `<generation>` filled in, and note its id. Then acquire with that id; a refused `acquire` means another session owns the run: cancel the timer you just made, start nothing and report the owner.

```sh
python scripts/campaign_guard.py mint-generation
python scripts/campaign_guard.py acquire --session "$ANTIGRAVITY_CONVERSATION_ID" --phase <N> --run-file <run file> --cron-id <timer id> --generation <generation> --scope plan
```

When no timer can be made (the `schedule` tool is not offered, or it fails), the run is held by the Stop hook alone: acquire with the job id `none` instead, and end with `cancel-confirmed --cron-id none`, as the Codex copy does. Say so in the run file's Critical events.

```bash
python scripts/campaign_guard.py acquire --session "$ANTIGRAVITY_CONVERSATION_ID" --phase <N> --run-file <run file> --cron-id none --generation <generation> --scope plan
python scripts/campaign_guard.py end --session "$ANTIGRAVITY_CONVERSATION_ID" --reason <reason>
python scripts/campaign_guard.py cancel-confirmed --cron-id none
```

Read the guard back, then record the session, timer, generation and run id in the run file's Critical events.

```sh
python scripts/campaign_guard.py whoami --session "$ANTIGRAVITY_CONVERSATION_ID" --generation <generation>
```

#### Phase change

At each phase change (section 2), write the new phase's run file, cancel the old timer, mint a fresh generation, create a new timer for the new phase and run file, and re-point the guard to them for the same session:

```sh
python scripts/campaign_guard.py mint-generation
python scripts/campaign_guard.py acquire --session "$ANTIGRAVITY_CONVERSATION_ID" --phase <N> --run-file <run file> --cron-id <timer id> --generation <generation> --scope plan
python scripts/campaign_guard.py whoami --session "$ANTIGRAVITY_CONVERSATION_ID" --generation <generation>
```

#### End

Every way a run ends goes through `end`, never a direct delete. The reason needs its marker in the run file first: `closeout` a `## Closeout run=<run id>` heading, `park` a column-0 `PARKED <UTC> run=<run id> <reason>` line, `escalation` a `PARKED ... escalation: <cause>` line; `plan-done` needs every row stamped, `stall` two trips, and `operator-stop` nothing. `end` names the timer: cancel it with the `schedule` tool, confirm the cancellation, and check the guard is gone:

```sh
python scripts/campaign_guard.py end --session "$ANTIGRAVITY_CONVERSATION_ID" --reason <reason>
python scripts/campaign_guard.py cancel-confirmed --cron-id <timer id>
python scripts/campaign_guard.py whoami --session "$ANTIGRAVITY_CONVERSATION_ID"
```

The last command prints `NO GUARD` once the run is released.

#### Heartbeat

The timer's prompt, filled in:

```text
Run-guard heartbeat for Phase <N> (run file <run file>, generation <generation>). Check, then act, in this turn.

1. Run `python scripts/campaign_guard.py whoami --session "$ANTIGRAVITY_CONVERSATION_ID" --generation <generation>`. If it prints NOT THE OWNER or NOT THE CURRENT JOB, cancel this timer with the schedule tool, change nothing else, and reply with that line. If it prints MALFORMED GUARD, do step 2, report it to the operator and stop.
2. Run `python scripts/campaign_guard.py hook-error --session "$ANTIGRAVITY_CONVERSATION_ID"`. If it prints a line, append `- bookkeeping: hook error: <that line>` to <run file>'s Critical events, then run the same command with `--ack "<that line>"`.
3. Run `python scripts/campaign_guard.py pending-cancel`. For each timer it names, cancel it with the schedule tool, then run `python scripts/campaign_guard.py cancel-confirmed --cron-id <timer>`.
4. If step 1 printed NO GUARD, the run is over: run `python scripts/campaign_guard.py reset-state --expect-no-guard yes`, cancel this timer, and reply RUN FINISHED.
5. Run `python scripts/campaign_guard.py heartbeat-still --session "$ANTIGRAVITY_CONVERSATION_ID"`. If it printed `progress` or `still`, go on to step 6. If it printed `trip 1`, append `- bookkeeping: <that line> <UTC>` to <run file>'s Critical events and go on to step 6. Every line the heartbeat writes starts `- bookkeeping:`: the stall breaker's fingerprint ignores those, so a heartbeat's own notes never pass for progress and reset the trips. If it ended the run (`end: stall`), cancel this timer, run `cancel-confirmed` for it, append the stall to <run file>'s Critical events, and report it to the operator.
6. Append `- bookkeeping: heartbeat resumed <UTC>` to <run file>'s Critical events, re-run `python scripts/todo-graph.py query ready --have auto`, pick up the open section where it stopped, and keep shipping under process-plan. Stop only at closeout, park, or an operator stop.
```

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
