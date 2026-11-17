---
name: review-todo-section
description: Quality-gate a just-implemented TODO section in Codex with self-review, independent review lenses and a bounded fix loop, then write the Verified stamp through the stamp command. Also runs in audit stance on shipped sections. Use after process-todo-section or when asked whether a section is really done.
---

# Review TODO Section

The gate between "code exists" and "the row says `[x]`". Nothing else may flip that row, and the flip happens only through `python scripts/todo-graph.py stamp`, which refuses a section with unticked items, an open prerequisite, a waiting Need, or no review evidence.

Review is mandatory and independent: the implementing session self-reviews, and every external pass comes from the reviewer pinned for its writer in `.conclave/reviewers.toml` (the `reviewer` kit: one reviewer of a different model family from the writer, no fallback; the pin as installed routes `claude` and `agy` to GPT-6.2 Astra through the Codex CLI, and the project owns its pin), run through `scripts/review-dispatch.py`. The writers are `claude`, `codex` and `agy` (`gemini` is another name for `agy`): name your own agent with `--writer`. The pin's `[schedule]` owns each pass's reasoning effort and lenses, so the writer never picks them, and it has two shapes, so read it first (next section). Every pass is recorded in a findings file; the stamp's receipt names the passes. A verdict with no record is an opinion.

## Use this skill when

- A section was just implemented (`process-todo-section` ends by invoking this).
- The user asks "is §N really done?", "re-verify §N", "audit §N": run in audit stance (below).
- Before closing a TODO file with `process-todo-file`.

## Step 0: resolve the argument

Same front door as `process-todo-section`: `python scripts/todo-graph.py resolve "<section-ref>"`. Exit 3 means the row is already `[x]`: that is the signal for audit stance, not an error.

## Which review schedule applies

Check the pin before the first pass: `python scripts/review_pin.py --check` loads it (it prints `ok` or the refusal), and its `[schedule]` says which flow runs. `python scripts/review_pin.py --lenses <kind>` prints a three-pass pin's lenses for a pass (`plan`, `round`, `completion` or `reconciliation`, with `--paths <file> ...` for its conditional lenses) and `--caps` prints its `pass_cap` and `no_progress_cap`.

- **Three-pass** (`policy = "three-pass"`, what a new install starts on): a plan review before the build, one round after it, one completion review of the final candidate, and reconciliations only after gaps. Step 3 and the three-pass stamp in step 7.
- **Rotation** (no `policy` key, kept by projects whose pin still says so): a plan review, a completion review before the rounds, then rounds until one has no P1 or P2 finding. Step 4 and the rotation stamp in step 7.

A section runs under the pin committed at its plan review's candidate (schedule and route both), so a pin change made later does not move a section already planned: finish it under the flow it began on.

## The three questions

Every review answers these with code and behavior evidence, never assertion:

1. **Does it make sense?** Does the implementation produce the exact outcome the section asked for, without unnecessary scope?
2. **Is it logical?** Do the contracts agree: threading rules, error paths, settings consumers, UI states, failure behavior?
3. **Did it break anything else?** Inspect callers, consumers, tests and adjacent workflows that read what changed.

## Workflow

### 1. Fix the candidate

Name the candidate: the implementation commit, or the commit range (`<BASE_SHA>..<TIP_SHA>`) when the section landed across multiple logical step commits or when fix-loop commits append (commits are never amended once review starts). Record the full SHA(s) in the findings file at `<receipts>/<NN-domain>/D<NN>-T<NN>-s<N>.md`. For an audit, the candidate is the section's own stamped commit plus every later change to the files it owns, up to today's tip (`<STAMPED_SHA>..HEAD -- <owned paths>`): a regression lives in those later changes. Read the actual diff (`git show` or `git diff <BASE_SHA>..<TIP_SHA>`), not memory of writing it. Map each ticked micro-step checklist item (its `Done when:` and `Cheaper substitute:`) to the code and test that satisfy it; an item with no corresponding change or a partially met `Done when:` (`partial-flip-shipped`) is not done (untick it) or was already true (note where). On a stamped section (audit stance) edit nothing in the section by hand: record the item in the findings file, and when it is a regression, run `unstamp` first; the item is unticked in the reopened section afterwards.

### 2. Self-review

Work the three questions across the diff and its blast radius. Fix what you find, then re-ask them. Look specifically for the failure modes this codebase invites:

- The failure modes the project's `AGENTS.md`, governing plan and SPEC call out for the area you touched: list them in the findings file and check each one.
- An error path that throws, aborts or swallows instead of returning the project's typed error, or a failure reported as success.
- Work on the wrong thread, mutable state shared across threads, or unbounded recursion or allocation on hostile input.
- A file write that is not temp-then-replace, or an original modified in place.
- A hard-coded colour, size or string where design tokens or settings own it, or a control without an accessible name.
- A setting nothing reads, a command nothing reaches, or a link or automation that acts without asking.
- A test that passes without exercising the behavior.

### 3. Review passes under a three-pass pin

The passes run in this order for the section's findings file (`<receipts>/<NN-domain>/D<NN>-T<NN>-s<N>.md`, `<receipts>` being `layout.receipts` in `todo/plan.json`, `docs/reviews` by default). Each pass is on a commit descending from the one before, the dispatcher refuses a pass out of order before a reviewer runs, and it refuses a second pass while one is running. Commit the pin before the section's plan review: the section runs under the pin committed at that review's candidate.

1. **Plan review**, before the build (`process-todo-section` step 3a): `--kind plan --candidate <commit holding the section>`.
2. **The round**, after the build, its gates and its commit. Fill the prompt into a scratch file from `docs/review-round-prompt.md` with `review-prompt.py` (the candidate, the section through `--ref`, what the change is for and the governing documents), then run it with the candidate (required) and your own agent as the writer, adding `--surface` on a `Surface: yes` section so the `design` lens joins:

```bash
python3 scripts/review-prompt.py --kind round --candidate <full tip SHA> --ref "D<NN> T<NN> §<N>" [--fidelity <design source>] --purpose "<one or two sentences>" --governing <the spec or plan> --out <scratch-round-prompt.md>
python3 scripts/review-dispatch.py --kind round --candidate <full tip SHA> --writer <claude|codex|agy> [--surface] --prompt <scratch-round-prompt.md> --record <receipts>/<NN-domain>/D<NN>-T<NN>-s<N>.md
```

   The dispatcher appends a `## Round 1` section and the pin's lenses (the lens table below is the vocabulary). Verify every finding against the code, fix what is real in new commits appended to the candidate, record why anything else is not real under a `## Writer note:` heading, and re-run the gates (step 5). There is no second round: the dispatcher refuses one.
3. **The completion review** of the final candidate: prompt from `docs/completion-review-prompt.md`, filled by `review-prompt.py` (the candidate SHA, the section and its TODO file's Goal and Outcome through `--ref`, the design source named in Fidelity as `--fidelity` (leave it out on `Surface: no`), what the feature is for and the governing documents):

```bash
python3 scripts/review-prompt.py --kind completion --candidate <full tip SHA> --ref "D<NN> T<NN> §<N>" [--fidelity <design source>] --purpose "<one or two sentences>" --governing <the spec or plan> --out <scratch-completion-prompt.md>
python3 scripts/review-dispatch.py --kind completion --candidate <full tip SHA> --writer <claude|codex|agy> --prompt <scratch-completion-prompt.md> --record <receipts>/<NN-domain>/D<NN>-T<NN>-s<N>.md
```

   It judges the built feature as a user meets it and against the strongest competitor or alternative: part 1 lists every control, state, error path and read-back the feature needs, each `present`, `missing-in-scope` or `missing-out-of-scope`, and part 2 lists where it falls short of beating the alternative, each `in-scope` or `out-of-scope`. It ends `COMPLETION: complete` or `COMPLETION: gaps`. A findings file takes one returned completion review.
4. **Reconciliation after gaps.** On `COMPLETION: gaps` the stamp is blocked and the failed review stays on record. Implement every in-scope gap in new commits without changing what the section promises (its checklist text: the stamp compares it between the failed completion and each reconciliation), add one line per gap under the review (`- Implemented: <gap> in <commit>`), re-run the gates, then run a reconciliation on the new tip with a prompt filled from `docs/reconciliation-review-prompt.md` (put the gate output in a file for `--gates`; after a reconciliation that returned gaps add `--last <the SHA that reconciliation reviewed>`, otherwise `--last` defaults to `--failed`):

```bash
python3 scripts/review-prompt.py --kind reconciliation --candidate <full new tip SHA> --ref "D<NN> T<NN> §<N>" --failed <the failed completion's SHA> --findings <receipts>/<NN-domain>/D<NN>-T<NN>-s<N>.md --gates <scratch-gates.txt> --governing <the spec or plan> --out <scratch-reconciliation-prompt.md>
python3 scripts/review-dispatch.py --kind reconciliation --candidate <full new tip SHA> --writer <claude|codex|agy> --prompt <scratch-reconciliation-prompt.md> --record <receipts>/<NN-domain>/D<NN>-T<NN>-s<N>.md
```

   It ends `RECONCILIATION: complete` or `RECONCILIATION: gaps`. After gaps, repair and run another. File each out-of-scope gap through `add-todo` as a new section and add a `- Filed: DNN TNN §N (<gap>)` line under the review that raised it. Under a last pass that ended complete write no `- Filed:` line: the stamp reads those lines under the last returned pass and takes them only with the `filed` verdict.

Every reply must carry one line `LENS <name>: clean` or `LENS <name>: findings` for each lens the dispatcher scheduled (it appends them to your prompt) and a `HIGHEST: P1|P2|P3|none` line before the final verdict lines. A `[[schedule.conditional]]` entry in the pin adds its lenses when the section changed a file under its paths, and a plan review takes those files from `--paths <file> ...`. Headings that start `## Round`, `## Completion review`, `## Plan review` or `## Reconciliation review` are reserved for what `--record` writes: a writer's own note uses another heading (for example `## Writer note: ...`), or the file stops being readable and the next review is refused with the offending line named.

**How a section ends, case by case.** Do not stop to ask the operator in any of them.

- **A reply recorded as incomplete** (exit 1: a missing `LENS` line, a missing `HIGHEST:` line, missing final verdict lines, or a complete verdict that declares a blocking severity; also any outage) is no review and did not count toward the cap. Its text is kept under a `none` section. Run the same command again; a session-only pass is not a substitute.
- **The last pass ended gaps and its worst finding is below the pin's `blocking` levels** (the installed pin: `P1` and `P2`). It takes no further pass and the dispatcher refuses a reconciliation. File each finding as a new section through `add-todo`, add a `- Filed: DNN TNN §N (<what>)` line for each under that last review in the findings file, and stamp with `--completion '<findings file>:filed'`. Without `blocking` in the pin every gap holds the stamp and must be reconciled.
- **A blocking gap** (a severity the pin's `blocking` lists) needs a reconciliation that ends `RECONCILIATION: complete`, stamped with `:reconciled`. Repair in new commits, re-run the gates and run the next reconciliation until one is complete.
- **The dispatcher refuses a reconciliation** because the section has spent the pin's `pass_cap` returned passes (plan, round, completion and every reconciliation count; 33 when the pin sets none), or because `no_progress_cap` reconciliations in a row made no progress. The section stays unstamped: there is no cap-outcome verdict under three-pass, so never stamp at the cap with gaps open. Record it in the findings file under a heading of your own (for example `## Cap reached`, never one that starts with a reserved heading above: passes spent, every open gap) and in the run file, and carry on with the phase's other ready rows. `python scripts/review_pin.py --caps` prints the caps.

### 4. Review passes under a rotation pin

Skip this step on a three-pass pin. Under rotation the completion review comes before the rounds, and the rounds run until one has no `P1` or `P2` finding.

#### Completion and excellence review (one pass, first)

Before the regular rounds, once per section, the pinned reviewer reviews the built feature as a user meets it and against the strongest competitor or alternative. Fill the prompt into a scratch file from `docs/completion-review-prompt.md` with `review-prompt.py` (the candidate SHA, the section and its TODO file's Goal and Outcome through `--ref`, the design source named in Fidelity as `--fidelity`), then run it, naming your own agent as the writer (`claude`, `codex` or `agy`):

```bash
python3 scripts/review-prompt.py --kind completion --candidate <full candidate SHA> --ref "D<NN> T<NN> §<N>" [--fidelity <design source>] --purpose "<one or two sentences>" --governing <the spec or plan> --out <scratch-completion-prompt.md>
python3 scripts/review-dispatch.py --kind completion --candidate <full candidate SHA> --writer <claude|codex|agy> --prompt <scratch-completion-prompt.md> --record <receipts>/<NN-domain>/D<NN>-T<NN>-s<N>.md
```

The dispatcher appends a `## Completion review` section (reviewer, model, candidate SHA), not a round: it is not counted against the cap, and a findings file takes one returned completion review. Part 1 lists every control, menu item, command, dialog, state, setting consumer, error and refusal path, undo or reverse path and persistence read-back the feature needs, each `present`, `missing-in-scope` or `missing-out-of-scope`. Part 2 names how the strongest competitor or alternative does the job and lists where the feature falls short of beating it (speed, steps, defaults, error recovery, accessibility, polish), each `in-scope` or `out-of-scope`. The review ends `COMPLETION: complete` or `COMPLETION: gaps`.

Then, before any regular round, implement every `missing-in-scope` gap and every `in-scope` excellence item in this section (new commits appended to the candidate, the section's checklist and gates covering them), and file every `missing-out-of-scope` gap and `out-of-scope` item through `add-todo` as a new section (an improvement that changes the designed surface is filed as a design-source change first). Under the completion review in the findings file, add one line per gap: `- Implemented: <gap> in <commit>`, or `- Filed: DNN TNN §N (<gap>)` for each section filed (the stamp reads these lines and refuses a ref that does not resolve), and add a `> **Deferred:** <what> -> XREF: DNN TNN §N` line to the section when its surface shows the filed item. The stamp takes `--completion '<findings file>:complete'` (no gaps), `:completed` (every gap implemented in the section) or `:filed` (some gaps filed as new sections, each on a `- Filed:` line). When the dispatcher exits 1 (the reviewer returned no completion review), run the pass again; the regular rounds wait for it.

#### Independent review rounds (the pinned reviewer)

The dispatcher picks each round's lenses and effort from the pin's schedule (the lens table below is the vocabulary), at least one round by the pinned independent reviewer. Fill the prompt into a scratch file from `docs/review-round-prompt.md` with `review-prompt.py` (the commit under review as `--candidate`, which the filler needs although a rotation round passes none to the dispatcher; the section through `--ref`), then run it, naming your own agent as the writer:

```bash
python3 scripts/review-prompt.py --kind round --candidate <full SHA under review> --ref "D<NN> T<NN> §<N>" [--fidelity <design source>] --purpose "<one or two sentences>" --governing <the spec or plan> --out <scratch-review-prompt.md>
python3 scripts/review-dispatch.py --writer <claude|codex|agy> --prompt <scratch-review-prompt.md> --record <receipts>/<NN-domain>/D<NN>-T<NN>-s<N>.md
```

The dispatcher appends a `## Round N` section naming the reviewer, its model, effort and lenses, or `none` with the reason when the reviewer returned no review. A round with no review counts for nothing: fix the cause (a CLI not signed in, a sandbox that cannot read the repository, so the diff goes in the prompt) and run the round again. Pass `--surface` on every round of a `Surface: yes` section so the rotation includes the `design` lens. Headings that start `## Round`, `## Completion review` or `## Plan review` are reserved for what `--record` writes: a writer's own note in a findings file uses another heading (for example `## Writer note: ...`), or the file stops being readable and the next review is refused with the offending line named. Complete that round with the candidate SHA, per-lens verdicts (`approve`, `needs-attention` with numbered findings naming `file:line`, or `advisory`), and the quoted gate outputs (the Live proof). On Windows run `python` instead of `python3`. A repository without `scripts/review-dispatch.py` gets it with `forge.py kit . reviewer`.

The lens vocabulary, for both schedules:

| Lens | Asks |
| ---- | ---- |
| `plan` | Will the section as written produce a 100% complete feature? (plan review only) |
| `plan-delivery` | Does the build deliver every planned item, with nothing quietly narrowed? |
| `adversarial` | How does this fail on hostile input, races, aborts, automation or links? |
| `consistency` | Does it match the module rules, naming, shared components and settings? |
| `integration` | Do callers and consumers still hold, with every contract kept? |
| `source-defect` | Is the upstream spec or SPEC read accurately with deviations noted? |
| `quality` | Is the code clear, tested where it can fail, and free of dead or duplicated logic? |
| `design` | Does the rendered UI match the design source and its tokens? |
| `optimization` | Is there avoidable work, allocation, latency or step count on a hot path? |
| `record` | Does evidence match what ran, with owned deferrals and earned flip? |

The rotation loop is bounded: at most 33 rounds (a round that returns no review does not count), and it ends early on severity. The round prompt in `docs/review-round-prompt.md` asks for a `P1`/`P2`/`P3` tag on every finding (an untagged one counts as `P1`) and for a `HIGHEST:` line before `OVERALL`. Severity is proportional: only a failure that can happen in real operation with real input is `P1` or `P2`; a contrived scenario is `P3`. The first round with no `P1` or `P2` finding (the pin's `severity_stop_after` sets the earliest round it may come from; `stamp` accepts the stop only when that round's line reads `HIGHEST: P3`) stops the loop, its findings are filed through `add-todo`, and the section stamps with `stopped-low-severity` and a `--cap-outcome` naming what was filed. The cap is room, not a quota. Findings are fixed in new commits appended to the candidate, gates re-run, and the affected lenses re-run against the new tip. A unit patched three rounds running is stopped and re-thought instead of patched again. When the rounds keep finding spellings that slip past a home-made parser or scanner of something a tool owns (includes, link inputs, symbols, encodings, file formats), move the proof to that tool's own output (the compiler's or linker's dependency reports, the platform's binary inspection tool, an OS API) and keep the scanner only as an early warning; that is the owning-layer fix, and spelling bypasses of the early warning stop being `P1` unless they also defeat the tool-reported proof. Round verdicts for the receipt: `clean` (no findings), `fixed` (findings found and fixed in a later round), `stopped-at-33` (cap reached; `stopped-at-22`, `stopped-at-13`, `stopped-at-8` and `stopped-at-3` remain valid on receipts written under earlier caps, but a new stamp refuses them; every leftover is filed through `add-todo` with an owner, or the run stops with no stamp when a leftover is safety, data-integrity or stamp-invalidating). At the cap nobody is asked, attended or not: a stamped section carries the outcome in its receipt (`--cap-outcome`), and an unstamped one records it in its findings file under `## Cap reached` (rounds spent, every leftover, where each was filed) and in the run file, and the phase carries on with its other ready rows. When the dispatcher exits 1 (the reviewer returned no review), record the outage in the findings file with an owner and a due date and stop without a stamp: a session-only pass is not a substitute.


Architecture check: on sections touching an architectural boundary the governing plan names (process or thread contracts, data ownership, module seams), one findings-file paragraph answers whether the change keeps the governing plan's architecture intact, naming any decision it effectively revises. A revision is recorded as a plan correction, never smuggled in as code.

### 5. Re-run the gates

After the last fix (under three-pass, after the round's fixes and after each repair before a reconciliation), re-run the section's Test checkpoint and its owed gates (affected test suites, build warnings clean, `validate`, the Freeze check in frozen files, any sanitizer build the section names). Quote the outputs in the findings file. A fix verified by reasoning is not verified. A red gate reopens the candidate: fix, re-run to green, and re-run the affected lens, because earlier verdicts rested on the broken candidate.

### 6. Surface, feature, and frozen checks

Surface and feature completeness: 100% of every control, menu item, dialog and state on the Fidelity counterpart is working (proven on the rendered surface with real data and read-back in this review) or deferred to a named, resolving section, and every required refusal/error/undo path is exercised. Refuse the stamp for an unaccounted control or a `Cheaper substitute` shortcut. Frozen files: every `**Freeze check:**` ran and passed with the result quoted; a frozen behavior that moved without a recorded operator approval means no stamp and a question for the operator.

### 7. Stamp

Tick nothing by hand. Confirm every checklist item except `Commit:` is `[x]`, then stamp with the command for the pin's schedule. Under a three-pass pin, with the last returned pass having reviewed the stamped commit:

```powershell
python scripts/todo-graph.py stamp 'DNN TNN §N' --sha <full tip SHA> --plan '<receipts>/<domain>/D<NN>-T<NN>-s<N>.md' --completion '<receipts>/<domain>/D<NN>-T<NN>-s<N>.md:<complete|reconciled|filed>' --review '<N>:<receipts>/<domain>/D<NN>-T<NN>-s<N>.md:<clean|fixed>' --check '<checkpoint command> -> <quoted result>'
```

The three-pass verdicts: `--completion` is `complete` (the completion found no gap), `reconciled` (it returned gaps and the last reconciliation ended complete) or `filed` (the last pass ended gaps whose worst finding is below the pin's `blocking` levels and every gap is on a `- Filed:` line). `completed` is not accepted. There is exactly one `--review`, for the one round that returned a review, under the number the dispatcher gave it: `1` when the first attempt returned, and the number in the `round` field of its summary (the `## Round N` heading that names a reviewer in the findings file) when an earlier attempt was an outage or an incomplete reply. Its verdict is `clean` when it approved, `fixed` when it found something that was then fixed. There is no `--cap-outcome`, no `stopped-` verdict and no severity stop. The receipt records `passes_spent` and `pass_cap`.

Under a rotation pin:

```powershell
python scripts/todo-graph.py stamp 'DNN TNN §N' --sha <full candidate tip SHA> --plan '<receipts>/<domain>/D<NN>-T<NN>-s<N>.md' --completion '<receipts>/<domain>/D<NN>-T<NN>-s<N>.md:complete' --review '1:<receipts>/<domain>/D<NN>-T<NN>-s<N>.md:clean' --check '<checkpoint command> -> <quoted result>'
```

One `--plan` (required, naming the section's own findings file; `stamp` refuses without it, with any other file, with a findings file that holds no `## Plan review` returned by another model family and ending `PLAN: complete` or `PLAN: gaps` then its `OVERALL:` line, or with a plan review whose candidate is not the commit that recorded the section's `Started:` line or an earlier one), one `--completion` (required; `stamp` refuses without it, with an unknown verdict, with a findings file that holds no returned `## Completion review`, with a verdict the review's `COMPLETION:` line does not support, or with a `filed` verdict whose `- Filed:` refs are missing or do not resolve), one `--review` per round (three-pass: the one round), one `--check` per gate actually run. `stamp` also replays the section's pin (its `[schedule]` at the plan review's candidate under three-pass, at the stamped commit under rotation) through the dispatcher's own scheduler and refuses any pass whose effort or lenses differ from what the schedule gave it, naming the pass, the expected and the recorded values; under three-pass it also checks the chain itself: plan, one round, completion, then reconciliations, each on a later candidate than the pass before, the last on the stamped commit, within `pass_cap`. Under rotation, with a `stopped-at-33` or `stopped-low-severity` verdict add `--cap-outcome '<what was done when the cap was reached>'` (which leftovers were filed where, or why the section stays unstamped); `stamp` refuses a stopped verdict without it. The receipt records the plan review (`plan`: findings file, candidate, reviewer, model, effort, lenses), the pin's schedule (`schedule`), the completion review (`completion`: findings file, verdict, candidate, reviewer, model, filed sections), `rounds_spent` (every round the dispatcher recorded in the findings file, outage rounds included), `round_cap` and `cap_outcome`, and under three-pass `passes_spent` and `pass_cap`; `python scripts/todo-graph.py query rounds` lists them across stamped sections so the cap can be tuned. `stamp` writes the receipt `<receipts>/<ref>.json`, ticks the `Commit:` item and the Implementation Order row, appends the `Verified:` and `Review:` lines, marks the file `done` when its last section stamps, and the CLI syncs the plan. Commit the stamp, receipt, findings file and regenerated plan together as `todo: stamp <ref>`. Add any `Deferred:` lines (each with its `-> XREF:` owner) and `Started:` to `Duration:` notes to the section before stamping, since a stamped section's checklist is never rewritten; a later change is a new section that names the old one.


### 8. Audit stance
On an already-`[x]` section: run steps 1, 2 and 5 to 6 and the review passes of step 3 or 4 (the round, and a completion review if wanted) against the section's own candidate and receipt (a receipt stamped before completion reviews existed has none and stays valid without one), changing nothing in the section itself until `unstamp` has reopened it. Record the audit's rounds in a findings file of their own, `<receipts>/<NN-domain>/audit-d<NN>-t<NN>-s<N>.md` (`--record` that path), never in the section's stamped findings file: its receipt names that file's last round, so a round appended there makes the receipt refuse validation and blocks `unstamp`. Confirm the stamp's evidence still holds (re-run the checkpoint) or find the regression. The only permitted row change is `[x]` back to `[ ]`, and only through `python scripts/todo-graph.py unstamp '<ref>' --reason '<what regressed; repair -> XREF: <ref>>'`, which removes the stamp lines, unticks the `Commit:` item and the row, reopens the file, keeps the receipt as `<receipts>/<ref>.revoked-<date>.json` and adds a dated `Unstamped:` note (a hand edit leaves the graph refusing a checkbox/stamp mismatch). File the repair with `add-todo` first so the reason can name it, and say plainly that downstream sections stamped on this one are suspect until it re-stamps: they keep their stamps (the graph accepts a stamped section whose prerequisite carries an `Unstamped:` note) and `unstamp` prints them. A re-confirmed row keeps its stamp; say so in one line. Under a three-pass pin the audit file is not named for a section, so it is a review outside a section: it has no order and no cap. Each `--kind round` call takes the pin's round lenses, `--candidate` is optional, and `--paths <file> ...` with the changed files adds the conditional lenses when the pin has `[[schedule.conditional]]` entries (the dispatcher refuses `--paths` under a rotation pin). Stop when a round's summary shows `"blocking": false` (under a pin without `blocking`, when the round ends `OVERALL: approve`).

## Guardrails

- Do not stamp without a findings file, its one plan review, and its completion review: under three-pass also its one round and a completion that was complete, reconciled or filed within the blocking levels; under rotation at least one independent round.
- Do not run a second round under a three-pass pin, use a stopped verdict or a `--cap-outcome` under it, or stamp at `pass_cap` with gaps open.
- Do not stamp an unaccounted control on a surface section, or a moved frozen behavior.
- Do not hand-write `Verified:` lines, hand-flip rows, or edit a receipt. `stamp` owns the record.
- Do not review the working tree when the candidate is a commit or range. Name the hashes.
- Do not transfer evidence across changed candidates: a new tip re-runs its gates.
