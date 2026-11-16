#!/usr/bin/env python3
"""Print the compiler cache's hit rate, and fail a rebuild that did not use the cache.

    python scripts/cache-stats.py --zero                    # before the build: reset the statistics
    python scripts/cache-stats.py --min-hit-rate 60         # after it: the rate, exit 3 when it is below

The cache read is the one the project's wrappers run (`cc-cache.py` records its choice), unless `--cache <name>`
names another; with neither, the first of ccache and sccache on `PATH`. The numbers come from what each cache
prints for a program: `ccache --print-stats` (the counters `direct_cache_hit`, `preprocessed_cache_hit` and
`cache_miss`) and `sccache --show-stats --stats-format json` (the sums of `stats.cache_hits.counts` and
`stats.cache_misses.counts`). The cache is run with no shell, for a bounded time and output.

Exit codes:
  0  the statistics were read (or reset), and the rate is not below --min-hit-rate
  2  bad usage, no cache installed, or the cache could not be run, read or reset: one line says which
  3  with --min-hit-rate: the rate is below it, or no compile went through the cache at all

Standard library only.
"""
import argparse
import importlib.util
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CACHES = ("ccache", "sccache")


class Failure(Exception):
    """The statistics could not be read or reset: the message is the line printed, and the exit is 2."""


def _sibling(name):
    path = Path(__file__).resolve().parent / name
    spec = importlib.util.spec_from_file_location("_" + name.replace("-", "_").replace(".py", ""), path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def kind_of(program):
    """Which cache a program is, by its name."""
    return "sccache" if Path(program).stem.lower().startswith("sccache") else "ccache"


def choose(project, named=None):
    """The cache to read, as (kind, program, why it was chosen): the one named, else the one the project's
    wrappers run, else the first on PATH."""
    cc_cache = _sibling("cc-cache.py")
    folder = cc_cache.wrapper_folder(project)
    if named:
        program = cc_cache.find(named, skip=[folder])
        if program is None:
            raise Failure(f"{named} is not installed")
        return named, program, "named by --cache"
    recorded = cc_cache.recorded_choice(project)
    if recorded is not None:
        return recorded[0], recorded[1], "the cache this project's wrappers run"
    for name in CACHES:
        program = cc_cache.find(name, skip=[folder])
        if program is not None:
            return name, program, "the first on PATH"
    raise Failure(f"no compiler cache is installed ({' or '.join(CACHES)})")


def run(kind, program, arguments, runner):
    """The output of the cache run with `arguments`. Each way it can go wrong has its own words."""
    result = runner([str(program), *arguments])
    shown = f"{kind} {' '.join(arguments)}"
    if result["error"]:
        raise Failure(f"{shown} could not be started ({result['error']})")
    if result["stopped"] == "time":
        raise Failure(f"{shown} was stopped: it ran past the time limit")
    if result["stopped"] == "output":
        raise Failure(f"{shown} was stopped: it printed more than the limit")
    if result["exit"] != 0:
        raise Failure(f"{shown} exited {result['exit']}")
    return result["output"]


def read(kind, program, runner):
    """The cache's (hits, misses), from the form it prints for a program."""
    if kind == "sccache":
        output = run(kind, program, ["--show-stats", "--stats-format", "json"], runner)
        try:
            document = json.loads(output[output.index("{"):])
            counts = [document["stats"][key]["counts"] for key in ("cache_hits", "cache_misses")]
            hits, misses = (sum(count.values()) for count in counts)
            if any(type(value) is not int or value < 0 for count in counts for value in count.values()):
                raise ValueError("a count that is not a number")
        except (ValueError, KeyError, TypeError, AttributeError, RecursionError) as exc:
            raise Failure(f"sccache --show-stats --stats-format json printed what is not its statistics "
                          f"({type(exc).__name__})") from exc
        return hits, misses
    output = run(kind, program, ["--print-stats"], runner)
    counters = {}
    for line in output.splitlines():
        name, _tab, value = line.partition("\t")
        if value.strip().isdigit():
            counters[name.strip()] = int(value)
    wanted = ("direct_cache_hit", "preprocessed_cache_hit", "cache_miss")
    if any(name not in counters for name in wanted):
        raise Failure("ccache --print-stats printed what is not its statistics (no "
                      f"{', '.join(name for name in wanted if name not in counters)})")
    return counters["direct_cache_hit"] + counters["preprocessed_cache_hit"], counters["cache_miss"]


def zero(kind, program, runner):
    """Reset the cache's statistics, and prove it by reading them back."""
    run(kind, program, ["--zero-stats"], runner)
    hits, misses = read(kind, program, runner)
    if hits or misses:
        raise Failure(f"{kind} --zero-stats did not reset the statistics: they still show {hits} hit(s) and "
                      f"{misses} miss(es)")


def main(argv=None, root=None, runner=None):
    parser = argparse.ArgumentParser(prog="cache-stats.py",
                                     description="Print the compiler cache's hit rate; fail a rebuild that missed it.")
    parser.add_argument("--cache", choices=CACHES, help="the cache to read (default: the one the project's wrappers "
                                                        "run, else the first on PATH)")
    parser.add_argument("--zero", action="store_true", help="reset the statistics, before a build")
    parser.add_argument("--min-hit-rate", type=float, metavar="PERCENT",
                        help="exit 3 when the hit rate is below this, or no compile went through the cache")
    parser.add_argument("--json", action="store_true", help="print the result as one JSON document")
    parser.add_argument("--root", help="the project (default: the folder above scripts/)")
    args = parser.parse_args(argv)
    if args.zero and args.min_hit_rate is not None:
        parser.error("--zero is for before a build and --min-hit-rate for after it: give one")
    if args.min_hit_rate is not None and not 0 <= args.min_hit_rate <= 100:
        parser.error("--min-hit-rate is a percentage from 0 to 100")
    project = Path(root or args.root or ROOT).resolve()
    runner = runner or _sibling("toolchain-check.py").run_bounded
    try:
        kind, program, why = choose(project, args.cache)
        if args.zero:
            zero(kind, program, runner)
            print(json.dumps({"cache": kind, "program": str(program), "reset": True}) if args.json
                  else f"{kind} ({why}): statistics reset")
            return 0
        hits, misses = read(kind, program, runner)
    except Failure as exc:
        print(f"cache-stats: {exc}", file=sys.stderr)
        return 2
    total = hits + misses
    rate = None if total == 0 else 100.0 * hits / total
    below = args.min_hit_rate is not None and (rate is None or rate < args.min_hit_rate)
    if args.json:
        print(json.dumps({"cache": kind, "program": str(program), "hits": hits, "misses": misses,
                          "hit_rate": None if rate is None else round(rate, 2), "exit": 3 if below else 0}))
    elif rate is None:
        print(f"{kind} ({why}): no compile went through the cache")
    else:
        print(f"{kind} ({why}): {hits} hit(s), {misses} miss(es), hit rate {rate:.2f}%")
    if below:
        reason = "no compile went through the cache" if rate is None \
            else f"the hit rate {rate:.2f}% is below {args.min_hit_rate:g}%"
        print(f"cache-stats: {reason}", file=sys.stderr)
        return 3
    return 0


if __name__ == "__main__":
    sys.exit(main())
