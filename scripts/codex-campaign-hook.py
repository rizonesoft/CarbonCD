#!/usr/bin/env python3
"""Codex's Stop hook for the process-plan run guard: the shared `campaign_guard.stop_hook` in Codex's JSON contract.

`.codex/hooks.json` runs this file. Codex sends a payload in Claude Code's shape (`session_id`, `cwd`,
`hook_event_name`, `stop_hook_active`) and holds the turn on `{"decision": "block", "reason": ...}`. A broken hook
must never stop a Codex run, so every failure prints `{}` and exits 0; the guard records what it can for the
heartbeat. Source: RasterFold `scripts/codex-campaign-hook.py` at b33493346141f96b6b304cd3e0c393595382f056.
"""
from pathlib import Path
import importlib.util
import json
import sys

MAX_PAYLOAD_CHARS = 2_000_000


def _guard():
    """The guard beside this file, loaded by its path: the hook runs from whatever folder Codex is in."""
    path = Path(__file__).resolve().parent / "campaign_guard.py"
    spec = importlib.util.spec_from_file_location("codex_campaign_guard", path)
    if spec is None or spec.loader is None:
        raise OSError(f"cannot load {path}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def main(stdin=None) -> int:
    stdin = stdin if stdin is not None else sys.stdin
    try:
        guard = _guard()
        root = str(Path(__file__).resolve().parents[1])
        payload = stdin.read(MAX_PAYLOAD_CHARS + 1)
        if len(payload) > MAX_PAYLOAD_CHARS:
            guard._record_hook_error(root, guard.read_guard(root), "Codex Stop payload over 2,000,000 characters")
            print("{}")
            return 0
        try:
            event = json.loads(payload)
            if isinstance(event, dict):
                # Codex's session_id names the session; a conversationId would switch the answer to Antigravity's.
                event.pop("conversationId", None)
                payload = json.dumps(event)
        except (ValueError, RecursionError):
            pass  # the guard records a payload it cannot read
        _status, response = guard.stop_hook(payload, root)
        print(response or "{}")
        return 0
    except BaseException as error:  # a hook that fails must let Codex stop: say why on stderr, answer {}
        print(f"codex-campaign-hook: {type(error).__name__}: {error}", file=sys.stderr)
        print("{}")
        return 0


if __name__ == "__main__":
    sys.exit(main())
