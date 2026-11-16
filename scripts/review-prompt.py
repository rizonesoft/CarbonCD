#!/usr/bin/env python3
"""Fill one of the review prompt templates in docs/ and write the prompt to send to the reviewer.

    python scripts/review-prompt.py --kind round --candidate <commit> --ref "D00 T01 §7" \
        --purpose "what the change is for" --governing docs/plan.md --out <scratch>/round.md
    python scripts/review-prompt.py --kind reconciliation --candidate <repaired SHA> --failed <completion SHA> \
        --findings docs/reviews/00-core/D00-T01-s1.md --gates <scratch>/gates.txt --governing docs/plan.md \
        --out <scratch>/reconciliation.md

`--kind` picks the template (plan, round, completion or reconciliation) from the docs/ folder beside this script's
parent folder. The prompt is the template's text after its `---` line, with every placeholder (a backtick span that
starts with `<` and ends with `>`) replaced by its value. A placeholder with no value is a refusal naming it, as is a
value the kind has no place for: exit 2, a message on stderr, and nothing written, an existing `--out` untouched.
Without `--ref` the lines that name the TODO section and its file are dropped; with `--ref` and no `--todo`, the file
comes from `scripts/todo-graph.py resolve`, and with `--ref` and no `--findings` the section's findings file is
`<receipts>/<NN-domain>/D<NN>-T<NN>-s<N>.md`, `<receipts>` being `todo/plan.json`'s `layout.receipts` (`docs/reviews`
by default, read by `scripts/review_layout.py`). Prints the output path and exits 0. Stdlib only; writes are atomic.
"""
from __future__ import annotations

import argparse
import importlib.util
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

TEMPLATES = {"plan": "plan-review-prompt.md", "round": "review-round-prompt.md",
             "completion": "completion-review-prompt.md", "reconciliation": "reconciliation-review-prompt.md"}
SHA = re.compile(r"[0-9a-f]{40}")
REF = re.compile(r"D\d{2} T\d{2} §\d+")
SPAN = re.compile(r"`[^`\n]+`")
SECTION_TOKEN = "<DNN TNN §N>"
TODO_FILE = "<path to the TODO file>"
FIDELITY_SPAN = "`<the design source the section names>`, or `none (Surface: no)`"
CANDIDATE_SPAN = "`<FULL_SHA>` or `<BASE_SHA>..<TIP_SHA>`"
DEFAULT_FIDELITY = "none (Surface: no)"


class Refused(Exception):
    pass


def one_line(value: str | None) -> str | None:
    """A single-line value (whitespace collapsed), or None when nothing was given."""
    value = " ".join((value or "").split())
    return value or None


def ticked(value: str) -> str:
    return value if "`" in value else f"`{value}`"


def read_template(kind: str) -> str:
    path = Path(__file__).resolve().parent.parent / "docs" / TEMPLATES[kind]
    try:
        text = path.read_text(encoding="utf-8")
    except OSError as error:
        raise Refused(f"template {path} cannot be read: {error.strerror or error}")
    head, rule, body = text.partition("\n---\n")
    if not rule:
        raise Refused(f"template {path} has no `---` line before the prompt")
    return body.lstrip("\n")


def resolve_todo(ref: str) -> str:
    graph = Path(__file__).resolve().parent / "todo-graph.py"
    if not graph.is_file():
        raise Refused(f"--todo is needed: {graph.name} is not beside this script to look up {ref}")
    try:
        result = subprocess.run([sys.executable, "-B", str(graph), "resolve", ref], capture_output=True, text=True,
                                timeout=60)
        path = json.loads(result.stdout).get("path")
    except (OSError, subprocess.SubprocessError, ValueError, AttributeError) as error:
        raise Refused(f"--todo is needed: looking up {ref} failed ({error})")
    if not isinstance(path, str) or not path:
        raise Refused(f"--todo is needed: {graph.name} resolve {ref!r} named no file (exit {result.returncode})")
    return path


def section_findings(ref: str) -> str:
    """The section's findings file for --ref: `<receipts>/<NN-domain>/D<NN>-T<NN>-s<N>.md`, the domain the one
    `todo/<NN-domain>/` folder its number names, the folder todo/plan.json's layout.receipts."""
    here = Path(__file__).resolve()
    spec = importlib.util.spec_from_file_location("review_layout", here.with_name("review_layout.py"))
    if spec is None or not here.with_name("review_layout.py").is_file():
        raise Refused(f"--findings is needed: review_layout.py is not beside this script to find {ref}'s file")
    layout = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(layout)
    root = here.parent.parent
    try:
        folder = layout.receipts_folder(root)
    except layout.LayoutError as error:
        raise Refused(str(error)) from None
    m = re.fullmatch(r"D(\d{2}) T(\d{2}) §(\d+)", ref)
    todo = root / "todo"
    domains = sorted(d.name for d in todo.iterdir() if d.is_dir() and d.name.startswith(f"{m[1]}-")) \
        if todo.is_dir() else []
    if len(domains) != 1:
        raise Refused(f"--findings is needed: no single todo/{m[1]}-<domain> folder for {ref}")
    return f"{folder}/{domains[0]}/D{m[1]}-T{m[2]}-s{m[3]}.md"


def fence(content: str) -> str:
    ticks = "`" * max(3, max((len(run) for run in re.findall(r"`+", content)), default=0) + 1)
    return f"{ticks}\n{content}\n{ticks}"


def fill(body: str, args: argparse.Namespace, todo: str | None, gates: str | None) -> tuple[str, set[str], list[str]]:
    """Returns the filled prompt, the options whose values landed, and the placeholders left without a value."""
    used: set[str] = set()
    unresolved: list[str] = []
    sha = {"<FULL_SHA>": args.candidate, "<COMPLETION_SHA>": args.failed, "<LAST_SHA>": args.last,
           "<BASE_SHA>": args.base, "<TIP_SHA>": args.candidate}
    option = {"<FULL_SHA>": "--candidate", "<COMPLETION_SHA>": "--failed", "<LAST_SHA>": "--last",
              "<BASE_SHA>": "--base", "<TIP_SHA>": "--candidate"}
    purpose, governing = one_line(args.purpose), one_line(args.governing)
    fidelity = one_line(args.fidelity)
    findings = one_line(args.findings)

    def need(value, flag, placeholder):
        if value:
            used.add(flag)
            return value
        unresolved.append(f"`{placeholder}` (give {flag})")
        return None

    def span(match: re.Match) -> str:
        text = match.group(0)
        inner = text[1:-1]
        for token, value in sha.items():
            if token in inner and value:
                inner = inner.replace(token, value)
                used.add(option[token])
        for token in sha:
            if token in inner:
                missing = f"`{token}` (give {option[token]})"
                if missing not in unresolved:
                    unresolved.append(missing)
        if SECTION_TOKEN in inner and args.ref:
            inner = inner.replace(SECTION_TOKEN, args.ref)
            used.add("--ref")
        if not (inner.startswith("<") and inner.endswith(">")):
            if inner == text[1:-1]:
                return text
            return f"`{inner}`"
        if inner == TODO_FILE:
            value = need(todo, "--todo", inner)
            return ticked(value) if value else text
        if inner.startswith("<one or two sentences"):
            value = need(purpose, "--purpose", inner)
            return value.rstrip(".") if value else text
        if inner.endswith("that applies>"):
            value = need(governing, "--governing", inner)
            return ticked(value) if value else text
        if inner.startswith("<docs/reviews/"):
            value = need(findings, "--findings", inner)
            return ticked(value) if value else text
        if inner == "<quoted output>":
            if gates:
                used.add("--gates")
                gate_hit[0] = True
                return gate_mark
            unresolved.append(f"`{inner}` (give --gates)")
            return text
        if f"`{inner}`" not in unresolved:
            unresolved.append(f"`{inner}`")
        return text

    out_lines: list[str] = []
    gate_mark = "\0" + os.urandom(8).hex() + "\0"   # NUL cannot arrive in an argument or a template line
    gate_hit = [False]
    for line in body.splitlines():
        if not args.ref and SECTION_TOKEN in line:
            continue
        if not args.ref and not todo and TODO_FILE in line:
            continue
        if CANDIDATE_SPAN in line:
            used.add("--candidate")
            if args.base:
                used.add("--base")
                line = line.replace(CANDIDATE_SPAN, f"`{args.base}..{args.candidate}`, tip `{args.candidate}`")
            else:
                line = line.replace(CANDIDATE_SPAN, f"`{args.candidate}`")
        if FIDELITY_SPAN in line:
            used.add("--fidelity")
            line = line.replace(FIDELITY_SPAN, ticked(fidelity or DEFAULT_FIDELITY))
        gate_hit[0] = False
        line = SPAN.sub(span, line)
        # Only where the template's own gate placeholder was just replaced, by a mark made for this run: a value the
        # writer supplied is never rescanned for it, whatever text it holds.
        if gate_hit[0]:
            if "\n" in gates or "`" in gates:
                line = line.replace(gate_mark, "").rstrip().rstrip(".").rstrip() + "\n\n" + fence(gates) + "\n"
            else:
                line = line.replace(gate_mark, f"`{gates}`")
        out_lines.append(line)
    return "\n".join(out_lines).rstrip("\n") + "\n", used, unresolved


def atomic_write(path: Path, text: str) -> None:
    fd, temp = tempfile.mkstemp(dir=path.parent, prefix=f".{path.name}.", suffix=".tmp")
    try:
        try:
            handle = os.fdopen(fd, "w", encoding="utf-8", newline="\n")
        except BaseException:
            os.close(fd)  # the descriptor is still ours: left open, Windows could not remove the temporary file
            raise
        with handle:
            handle.write(text)
        os.replace(temp, path)
    except BaseException:
        try:
            os.unlink(temp)
        except FileNotFoundError:
            pass  # the replace already moved it
        raise


def resolve_commit(name: str, value: str) -> str:
    """The full SHA a commit name stands for: a full lowercase SHA as given, any other name (an abbreviated SHA, HEAD,
    a branch) through `git rev-parse --verify <name>^{commit}` in this project. Only a name git cannot resolve to a
    commit is refused: a short SHA once cost a whole rerun (D02 T01 §3)."""
    if SHA.fullmatch(value):
        return value
    root = Path(__file__).resolve().parent.parent
    try:
        done = subprocess.run(["git", "-C", str(root), "rev-parse", "--verify", "--quiet", f"{value}^{{commit}}"],
                              capture_output=True, text=True, timeout=60) if value and not value.startswith("-") else None
    except (OSError, subprocess.SubprocessError) as error:
        raise Refused(f"--{name} {value!r} is not a full 40-character commit SHA, and git could not be asked to "
                      f"resolve it ({error})")
    resolved = done.stdout.strip() if done is not None and done.returncode == 0 else ""
    if not SHA.fullmatch(resolved):
        raise Refused(f"--{name} must be a full 40-character commit SHA or a name git resolves to a commit in "
                      f"{root}, got {value!r}")
    return resolved


def run(args: argparse.Namespace) -> Path:
    for name in ("candidate", "base", "failed", "last"):
        value = getattr(args, name)
        if value is not None:
            setattr(args, name, resolve_commit(name, value))
    if args.ref is not None and not REF.fullmatch(args.ref):
        raise Refused(f"--ref must look like 'D00 T01 §7', got {args.ref!r}")
    given_last = args.last
    if args.kind == "reconciliation" and args.failed and not args.last:
        args.last = args.failed
    body = read_template(args.kind)
    todo = one_line(args.todo)
    if not todo and args.ref and TODO_FILE in body:
        todo = resolve_todo(args.ref)
    if not args.findings and args.ref and "`<docs/reviews/" in body:
        args.findings = section_findings(args.ref)
    gates = None
    if args.gates is not None:
        try:
            gates = Path(args.gates).read_text(encoding="utf-8").strip("\n")
        except (OSError, UnicodeError) as error:
            raise Refused(f"--gates {args.gates} cannot be read: {getattr(error, 'strerror', None) or error}")
        if not gates.strip():
            raise Refused(f"--gates {args.gates} is empty")
    text, used, unresolved = fill(body, args, todo, gates)
    if unresolved:
        raise Refused("no value for " + "; ".join(unresolved))
    given = {"--base": args.base, "--ref": args.ref, "--todo": args.todo, "--fidelity": args.fidelity,
             "--purpose": args.purpose, "--governing": args.governing, "--gates": args.gates,
             "--findings": args.findings, "--failed": args.failed, "--last": given_last}
    # A value the template has no place for is refused, never dropped silently.
    extra = sorted(flag for flag, value in given.items() if value is not None and flag not in used)
    if extra:
        raise Refused(f"the {args.kind} prompt has no place for {', '.join(extra)}")
    out = Path(args.out)
    try:
        if out.is_dir():
            raise IsADirectoryError("is a directory")
        atomic_write(out, text)
    except OSError as error:
        raise Refused(f"--out {out} cannot be written: {error.strerror or error}")
    return out


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--kind", required=True, choices=sorted(TEMPLATES))
    parser.add_argument("--candidate", required=True,
                        help="the commit under review: a full SHA, or any name git resolves (an abbreviated SHA, HEAD)")
    parser.add_argument("--out", required=True, help="the file the filled prompt is written to")
    parser.add_argument("--base", help="the first commit before the range: the prompt names <base>..<candidate>")
    parser.add_argument("--ref", help="the TODO section, 'D00 T01 §7'; without it the TODO lines are dropped")
    parser.add_argument("--todo", help="the TODO file; found with todo-graph.py resolve when --ref is given")
    parser.add_argument("--fidelity", help=f"the design source the section names (default: {DEFAULT_FIDELITY})")
    parser.add_argument("--purpose", help="one or two sentences on what the change is for (round, completion)")
    parser.add_argument("--governing", help="the spec, plan or design file that applies (all but round need it too)")
    parser.add_argument("--gates", help="a file holding the gate output (reconciliation)")
    parser.add_argument("--findings", help="the findings file of the section (reconciliation)")
    parser.add_argument("--failed", help="the failed completion's candidate SHA (reconciliation)")
    parser.add_argument("--last", help="the last reviewed candidate SHA (reconciliation; default --failed)")
    args = parser.parse_args(argv)
    try:
        print(run(args))
    except Refused as error:
        print(f"review-prompt refused: {error}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
