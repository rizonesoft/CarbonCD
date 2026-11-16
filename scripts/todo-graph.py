#!/usr/bin/env python3
"""Validate, query, resolve and stamp the canonical TODO graph, and keep todo/implementation-plan.md current.

    python scripts/todo-graph.py validate [--all]            # --all: every fault, one line each, then exit 2
    python scripts/todo-graph.py build                       # todo/ -> build/todo-cache.json
    python scripts/todo-graph.py render                      # mermaid dependency graph
    python scripts/todo-graph.py receipts [--config -]       # the receipts folder (todo/plan.json layout.receipts);
                                                             # --config -: that file's text read from stdin
    python scripts/todo-graph.py plan-path [--config -]      # the generated plan's path (todo/plan.json layout.plan)
    python scripts/todo-graph.py plan --sync | --check
    python scripts/todo-graph.py query ready | blocked | stats | deferred | adjacency | plan | rounds
    python scripts/todo-graph.py resolve 'D02 T01 §1'        # exit 0 ready, 1 no such section,
                                                             # 2 no reference in input, 3 verified,
                                                             # 4 waiting; a pasted plan row works too
    python scripts/todo-graph.py classify 'D02 T01 §1' ...   # dispositions as JSON
    python scripts/todo-graph.py stamp 'D02 T01 §1' --sha <40-char commit> \
        --plan '<findings file>' \
        --completion '<findings file>:<complete|reconciled|filed|completed>' \
        --review '<round>:<report path>:<clean|fixed|stopped-at-33|stopped-low-severity>' --check '<command> -> <result>' \
        [--cap-outcome '<what was done when the review cap was reached>']   # required with a stopped verdict
    python scripts/todo-graph.py unstamp 'D02 T01 §1' --reason '<what regressed, and the repair section>'

Format and rules: todo/README.md. Stdlib only.
"""
import argparse
from datetime import date
import json
from pathlib import Path
import re
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent / "lib"))
from todo_graph import (ROOT, Refused, adjacency_report, adopt_stamps, atomic, deferrals, disposition,  # noqa: E402
                        extract_ref, filing_couplings, frozen_files, graph, longest_chain, missing_needs, ordered,
                        plan_config, plan_file, plan_status, projections, receipts_folder, require, review_rounds,
                        session_needs, settled, stamp, suspects, unstamp)


def review_round(value):
    parts = value.split(":")
    require(len(parts) == 3 and parts[0].isdigit() and parts[1],
            f"--review takes round:report:verdict, not {value!r}")
    return {"round": int(parts[0]), "report": parts[1], "verdict": parts[2]}


def completion_review(value):
    report, _, verdict = value.rpartition(":")
    require(bool(report) and bool(verdict), f"--completion takes <findings file>:<verdict>, not {value!r}")
    return {"report": report, "verdict": verdict}


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = p.add_subparsers(dest="command", required=True)
    for name in ("build", "render"):
        sub.add_parser(name)
    sub.add_parser("detect", help="print the Needs this session meets: the kit's probes, todo/plan.json's probes and "
                                  "CONCLAVE_HAVE")
    v = sub.add_parser("validate")
    v.add_argument("--all", action="store_true",
                   help="record every fault and go on: one line each, '<path or reference>: <message>', then exit 2")
    rc = sub.add_parser("receipts", help="print the receipts folder, todo/plan.json's layout.receipts (docs/reviews)")
    rc.add_argument("--config", choices=["-"], help="read todo/plan.json's text from stdin (the hook passes the index's)")
    pp = sub.add_parser("plan-path", help="print the generated plan's path, todo/plan.json's layout.plan")
    pp.add_argument("--config", choices=["-"], help="read todo/plan.json's text from stdin (the hook passes the index's)")
    q = sub.add_parser("query")
    q.add_argument("kind", choices=["ready", "blocked", "stats", "deferred", "adjacency", "plan", "rounds", "frozen",
                                      "sequence"])
    q.add_argument("--have", help="the Needs this session meets, comma-separated, or 'auto' for detect's set: a row needing another (that no person answers) is held")
    r = sub.add_parser("resolve")
    r.add_argument("ref")
    r.add_argument("--have", help="the Needs this session meets, comma-separated, or 'auto' for detect's set: a row needing another (that no person answers) is held")
    c = sub.add_parser("classify")
    c.add_argument("refs", nargs="+")
    plan = sub.add_parser("plan")
    mode = plan.add_mutually_exclusive_group(required=True)
    mode.add_argument("--sync", action="store_true")
    mode.add_argument("--check", action="store_true")
    s = sub.add_parser("stamp")
    s.add_argument("ref")
    s.add_argument("--sha", required=True)
    s.add_argument("--review", action="append", default=[], type=review_round)
    s.add_argument("--check", dest="checks", action="append", default=[])
    s.add_argument("--date", default=date.today().isoformat())
    s.add_argument("--cap-outcome", help="with a stopped-at verdict: what was done when the review cap was reached")
    s.add_argument("--plan", help="required: <findings file>, the one-pass plan review before the build")
    s.add_argument("--completion", type=completion_review,
                   help="required: <findings file>:<verdict> of the completion review. complete: it found no gap. "
                        "reconciled (three-pass pins): it returned gaps and a later reconciliation ended complete. "
                        "filed: gaps filed as new sections, each on a '- Filed:' line (under three-pass only when "
                        "the last pass's worst finding is below the pin's blocking levels). completed (rotation pins "
                        "only): every gap was fixed in the section")
    ad = sub.add_parser("adopt-stamps", help="freeze, once, the stamps this project made before it took the kit "
                                             "(the adopted and adoption keys of the receipts folder's legacy file)")
    ad.add_argument("--phase-map", help="a JSON file from a section reference or TODO file path to its phase, for "
                                        "stamped sections with no Phase field (a section's key wins over its file's)")
    u = sub.add_parser("unstamp", help="an audit found a stamped section regressed: reopen it, keeping the receipt")
    u.add_argument("ref")
    u.add_argument("--reason", required=True)
    u.add_argument("--date", default=date.today().isoformat())
    args = p.parse_args()
    if args.command == "receipts":  # read by scripts/hooks/pre-commit; the configuration alone, not the graph
        print(receipts_folder(text=sys.stdin.read() if args.config == "-" else None))
        return 0
    if args.command == "plan-path":  # read by scripts/hooks/pre-commit, like receipts
        print(plan_file(text=sys.stdin.read() if args.config == "-" else None))
        return 0
    if args.command == "adopt-stamps":
        phase_map = None
        if args.phase_map:
            try:
                phase_map = json.loads(Path(args.phase_map).read_text(encoding="utf-8"))
            except ValueError as exc:
                raise Refused(f"the phase map {args.phase_map} is not JSON: {exc}") from None
        adopted = adopt_stamps(ROOT, phase_map)
        for ref, commit in adopted:
            print(f"adopted {ref} at {commit}")
        print(f"adopted {len(adopted)} stamps")
        return 0
    if args.command == "unstamp":
        revoked = unstamp(ROOT, args.ref, args.reason, args.date)
        left = []
        graph(ROOT, left)
        if left:  # a section reopened from adoption, in the project's own format: the plan syncs once it is repaired
            print(f"unstamped {args.ref}; receipt kept as {revoked}; plan not synchronized: repair first:")
            for where, message in left:
                print(f"  {where}: {message}")
        else:
            print(f"unstamped {args.ref}; receipt kept as {revoked}; plan synchronized")
        for ref in suspects(ROOT, args.ref):
            print(f"suspect until {args.ref} re-stamps: {ref}")
        return 0
    if args.command == "stamp":
        receipt = stamp(ROOT, args.ref, args.sha, args.review, args.checks, args.date, sync_plan=True,
                        cap_outcome=args.cap_outcome, completion=args.completion, plan=args.plan)
        print(f"stamped {args.ref}; receipt {receipt}; plan synchronized")
        return 0
    if args.command == "validate" and args.all:
        faults = []
        rows = graph(ROOT, faults=faults)
        for where, message in faults:
            print(f"{where}: {message}")
        if faults:
            return 2
        print(f"valid: {len(rows)} sections")
        return 0
    if args.command == "detect":
        print(",".join(sorted(session_needs())))
        return 0
    if args.command == "query" and args.kind == "frozen":  # the tree alone, so a tree with faults is reported too
        print(json.dumps(frozen_files(), indent=2, ensure_ascii=False))
        return 0
    rows = graph()
    have = None if getattr(args, "have", None) is None else (
        session_needs() if args.have == "auto" else {v.strip() for v in args.have.split(",") if v.strip()})

    def state(row):  # disposition, and a row this session cannot host is held like one that waits
        code = disposition(row, rows)
        return 4 if code == 0 and missing_needs(row, have) else code
    if args.command == "plan":
        projections(sync=args.sync)
        print("plan synchronized" if args.sync else "plan current")
    elif args.command == "resolve":
        ref, code = extract_ref(args.ref, rows)
        if code == 2:
            print("TODO refused: no section reference in input: " + args.ref, file=sys.stderr)
            return 2
        if code == 1:
            print("TODO refused: no such section: " + ref, file=sys.stderr)
            return 1
        row = rows[ref]
        unmet = [d for d in row.depends if not settled(d, rows)]
        extra = {} if have is None else {"missing_needs": missing_needs(row, have)}
        print(json.dumps({**row.public(), "body": row.body, "unmet_depends": unmet, **extra,
                          "skill_arg": ref, "exit": state(row)},
                         indent=2, ensure_ascii=False))
        return state(row)
    elif args.command == "classify":
        out = {}
        for item in args.refs:
            ref, code = extract_ref(item, rows)
            out[item] = disposition(rows[ref], rows) if code == 0 else code
        print(json.dumps(out, ensure_ascii=False))
    elif args.command == "query":
        if args.kind == "stats":
            result = {"total": len(rows), "verified": sum(r.done for r in rows.values()),
                      "ready": sum(state(r) == 0 for r in rows.values()),
                      "blocked": sum(state(r) == 4 for r in rows.values())}
        elif args.kind == "deferred":
            result = deferrals()
        elif args.kind == "adjacency":
            result = adjacency_report()
        elif args.kind == "rounds":
            result = review_rounds()
        elif args.kind == "sequence":
            result = {"chain": longest_chain(rows), "couplings": filing_couplings(ROOT, rows)}
        elif args.kind == "plan":
            # The plan's names come from plan_config and every section from the graph, so a reader such as the
            # program showing the plan never parses todo/ itself. graph() has already loaded todo/plan.json
            # strictly, so an unreadable one refused before this point.
            config = plan_config()
            result = {"title": config["title"], "acceptance": config["acceptance"],
                      "phases": {str(n): name for n, name in sorted(config["phases"].items())},
                      "sections": [{"ref": r.ref, "path": r.path, "title": r.title, "phase": r.phase,
                                    "status": plan_status(r, rows), "depends": r.depends} for r in ordered(rows)]}
        else:
            code = 0 if args.kind == "ready" else 4
            result = [r.public() for r in ordered(rows) if state(r) == code]
        print(json.dumps(result, indent=2, ensure_ascii=False))
    elif args.command == "build":
        atomic(ROOT / "build/todo-cache.json",
               json.dumps([r.public() for r in ordered(rows)], indent=2, ensure_ascii=False) + "\n")
        print(f"built {len(rows)} sections")
    elif args.command == "render":
        node = lambda ref: re.sub(r"[^A-Za-z0-9]", "", ref)  # noqa: E731
        lines = ["```mermaid", "flowchart LR"]
        for row in ordered(rows):
            lines.append(f'    {node(row.ref)}["{row.ref}: {row.title}"]')
            for dep in row.depends:
                lines.append(f"    {node(dep)} --> {node(row.ref)}")
        lines.append("```")
        print("\n".join(lines))
    else:
        print(f"valid: {len(rows)} sections")
    return 0


if __name__ == "__main__":
    # The output is a contract other programs parse; JSON is UTF-8, whatever the console's code page.
    sys.stdout.reconfigure(encoding="utf-8")
    sys.stderr.reconfigure(encoding="utf-8")
    try:
        sys.exit(main())
    except (Refused, OSError, ValueError) as exc:
        print("TODO refused: " + str(exc), file=sys.stderr)
        sys.exit(2)
