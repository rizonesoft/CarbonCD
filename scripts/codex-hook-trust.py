#!/usr/bin/env python3
"""Approve this project's campaign Stop hook in Codex, through Codex's own app-server.

Codex runs a project's `.codex/hooks.json` only once each hook is approved: the approval is a `trusted_hash` under
`hooks.state` in the user's `config.toml`, which the interactive client writes. Codex's app-server offers the same
step to any client: `hooks/list` returns each hook's `key`, `currentHash` and `trustStatus` (`trusted`, `untrusted`,
`modified`), and `config/value/write` records the hash. This script does that for one hook only, the Stop hook that
runs `scripts/codex-campaign-hook.py` from this project's own `.codex/hooks.json`, so an unattended Codex run is held
by the run guard without a person at the keyboard. It never approves any other hook, and Codex computes every hash.

    python scripts/codex-hook-trust.py            # approve the campaign Stop hook when it is not trusted
    python scripts/codex-hook-trust.py --check    # report only: exit 0 when trusted, 1 when not

Exit 2 when Codex is not installed, its app-server does not answer within 30 seconds, or the project has no
campaign Stop hook. `CODEX_HOOK_TRUST_SERVER`, a JSON list of strings, names another server command (tests use a
stub).
"""
from __future__ import annotations

import json
import os
from pathlib import Path
import queue
import shutil
import signal
import subprocess
import sys
import threading

ROOT = Path(__file__).resolve().parents[1]
HOOK_SCRIPT = "codex-campaign-hook.py"
# Seconds to wait for each answer; CODEX_HOOK_TRUST_TIMEOUT overrides it (a slow machine, or a test of a silent server).
TIMEOUT = float(os.environ.get("CODEX_HOOK_TRUST_TIMEOUT") or 30.0)


class TrustError(Exception):
    """A refusal the command line prints as one line and exit 2."""


def _server_command() -> list[str]:
    override = os.environ.get("CODEX_HOOK_TRUST_SERVER")
    if override:
        try:
            command = json.loads(override)
        except ValueError:
            command = None
        if not (isinstance(command, list) and command and all(isinstance(part, str) for part in command)):
            raise TrustError("CODEX_HOOK_TRUST_SERVER must be a JSON list of strings (the server command)")
        return command
    codex = shutil.which("codex")
    if codex is None:
        raise TrustError("codex is not installed (not on PATH)")
    return [codex, "app-server"]


CREATE_SUSPENDED = 0x00000004


def _resume(pid: int) -> None:
    """Resume every thread of a process started with CREATE_SUSPENDED (Toolhelp32 snapshot, then ResumeThread)."""
    import ctypes
    from ctypes import wintypes

    class ThreadEntry(ctypes.Structure):
        _fields_ = [("dwSize", wintypes.DWORD), ("cntUsage", wintypes.DWORD), ("th32ThreadID", wintypes.DWORD),
                    ("th32OwnerProcessID", wintypes.DWORD), ("tpBasePri", ctypes.c_long),
                    ("tpDeltaPri", ctypes.c_long), ("dwFlags", wintypes.DWORD)]

    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel32.CreateToolhelp32Snapshot.restype = wintypes.HANDLE
    kernel32.OpenThread.restype = wintypes.HANDLE
    snapshot = kernel32.CreateToolhelp32Snapshot(0x00000004, 0)  # TH32CS_SNAPTHREAD
    if snapshot in (None, wintypes.HANDLE(-1).value):
        raise OSError(ctypes.get_last_error(), "cannot list the app-server's threads to resume it")
    try:
        entry = ThreadEntry()
        entry.dwSize = ctypes.sizeof(ThreadEntry)
        more = kernel32.Thread32First(snapshot, ctypes.byref(entry))
        while more:
            if entry.th32OwnerProcessID == pid:
                thread = kernel32.OpenThread(0x0002, False, entry.th32ThreadID)  # THREAD_SUSPEND_RESUME
                if thread:
                    kernel32.ResumeThread(thread)
                    kernel32.CloseHandle(thread)
            more = kernel32.Thread32Next(snapshot, ctypes.byref(entry))
    finally:
        kernel32.CloseHandle(snapshot)


def _kill_on_close_job(proc: subprocess.Popen):
    """A Windows job object holding `proc`, set to end every process in it when the job is closed or terminated;
    None when the job cannot be made (the caller then falls back to `taskkill /T`)."""
    import ctypes
    from ctypes import wintypes

    class IoCounters(ctypes.Structure):
        _fields_ = [(name, ctypes.c_ulonglong) for name in (
            "ReadOperationCount", "WriteOperationCount", "OtherOperationCount",
            "ReadTransferCount", "WriteTransferCount", "OtherTransferCount")]

    class BasicLimits(ctypes.Structure):
        _fields_ = [("PerProcessUserTimeLimit", ctypes.c_longlong), ("PerJobUserTimeLimit", ctypes.c_longlong),
                    ("LimitFlags", wintypes.DWORD), ("MinimumWorkingSetSize", ctypes.c_size_t),
                    ("MaximumWorkingSetSize", ctypes.c_size_t), ("ActiveProcessLimit", wintypes.DWORD),
                    ("Affinity", ctypes.c_size_t), ("PriorityClass", wintypes.DWORD),
                    ("SchedulingClass", wintypes.DWORD)]

    class ExtendedLimits(ctypes.Structure):
        _fields_ = [("BasicLimitInformation", BasicLimits), ("IoInfo", IoCounters),
                    ("ProcessMemoryLimit", ctypes.c_size_t), ("JobMemoryLimit", ctypes.c_size_t),
                    ("PeakProcessMemoryUsed", ctypes.c_size_t), ("PeakJobMemoryUsed", ctypes.c_size_t)]

    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel32.CreateJobObjectW.restype = wintypes.HANDLE
    job = kernel32.CreateJobObjectW(None, None)
    if not job:
        return None
    limits = ExtendedLimits()
    limits.BasicLimitInformation.LimitFlags = 0x2000  # JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE
    kernel32.OpenProcess.restype = wintypes.HANDLE
    handle = kernel32.OpenProcess(0x0100 | 0x0001, False, proc.pid)  # PROCESS_SET_QUOTA | PROCESS_TERMINATE
    assigned = bool(handle) and bool(
        kernel32.SetInformationJobObject(job, 9, ctypes.byref(limits), ctypes.sizeof(limits))  # extended limits
        and kernel32.AssignProcessToJobObject(job, handle))
    if handle:
        kernel32.CloseHandle(handle)
    if not assigned:
        kernel32.CloseHandle(job)
        return None
    return job


class AppServer:
    """`codex app-server` over stdio: one JSON-RPC object a line. Its whole process tree ends when it closes."""

    def __init__(self, command: list[str], cwd: Path, codex_home: Path | None = None):
        # On Windows the server (codex.cmd, then node, then codex.exe) starts suspended, joins a job that ends every
        # process in it when the job closes, and only then runs, so no descendant can start outside the job; elsewhere
        # the session's process group holds the tree.
        group: dict = ({"creationflags": subprocess.CREATE_NEW_PROCESS_GROUP | CREATE_SUSPENDED} if os.name == "nt"
                       else {"start_new_session": True})
        # Absolute, so the server (whose working folder is `cwd`) reads the home that was meant.
        env = None if codex_home is None else {**os.environ, "CODEX_HOME": str(Path(codex_home).resolve())}
        self.proc = subprocess.Popen(command, cwd=str(cwd), stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                     stderr=subprocess.DEVNULL, text=True, encoding="utf-8", env=env, **group)
        self.job = None
        if os.name == "nt":
            self.job = _kill_on_close_job(self.proc)
            if self.job is None:
                # Never run a launcher whose tree could not be contained: it is still suspended, so ending it now
                # leaves nothing behind.
                self.proc.kill()
                self.proc.wait(timeout=10)
                self._release_pipes()  # our ends of its pipes, or they leak (found on a CI runner, 2026-11-13)
                raise TrustError("could not put the Codex app-server in a job object; refusing to start it")
            try:
                _resume(self.proc.pid)
            except BaseException:
                self.close()  # the job ends the suspended process; nothing outlives a failed start
                raise
        self.replies: queue.Queue = queue.Queue()
        threading.Thread(target=self._read, daemon=True).start()
        self.next_id = 0

    def _read(self) -> None:
        assert self.proc.stdout is not None
        for line in self.proc.stdout:
            try:
                message = json.loads(line)
            except ValueError:
                continue
            if isinstance(message, dict) and "id" in message:
                self.replies.put(message)

    def call(self, method: str, params: dict) -> dict:
        self.next_id += 1
        assert self.proc.stdin is not None
        self.proc.stdin.write(json.dumps({"jsonrpc": "2.0", "id": self.next_id, "method": method, "params": params}) + "\n")
        self.proc.stdin.flush()
        while True:
            try:
                message = self.replies.get(timeout=TIMEOUT)
            except queue.Empty:
                raise TrustError(f"codex app-server did not answer {method} within {TIMEOUT:.0f} seconds") from None
            if message.get("id") != self.next_id:
                continue
            if "error" in message:
                raise TrustError(f"codex app-server refused {method}: {message['error']}")
            return message.get("result") or {}

    def notify(self, method: str) -> None:
        assert self.proc.stdin is not None
        self.proc.stdin.write(json.dumps({"jsonrpc": "2.0", "method": method}) + "\n")
        self.proc.stdin.flush()

    def _release_pipes(self) -> None:
        for pipe in (self.proc.stdin, self.proc.stdout):
            if pipe is not None:
                try:
                    pipe.close()
                except OSError:  # a pipe whose far end died mid-write is closed all the same
                    pass

    def close(self) -> None:
        """End the server's whole tree whether or not its first process is still running: a descendant can outlive
        a launcher that exited or crashed."""
        if os.name == "nt":
            if self.job is not None:
                import ctypes

                ctypes.windll.kernel32.TerminateJobObject(self.job, 1)
                ctypes.windll.kernel32.CloseHandle(self.job)
                self.job = None
            if self.proc.poll() is None:  # not in a job (assignment failed): end it and the tree it still links
                subprocess.run(["taskkill", "/T", "/F", "/PID", str(self.proc.pid)], capture_output=True)
        else:
            try:
                os.killpg(self.proc.pid, signal.SIGKILL)  # the group outlives its leader while any member runs
            except ProcessLookupError:  # every process in the group has ended already: nothing is left to stop
                pass
        self._release_pipes()  # the tree is ended: release our ends of its pipes
        try:
            self.proc.wait(timeout=10)
        except subprocess.TimeoutExpired:
            # A killed tree that has not been reaped in 10 seconds is said on stderr, never raised: close() runs in
            # trust()'s finally, where raising would hide the error that ended the call.
            print(f"codex-hook-trust: app-server {self.proc.pid} still running after kill", file=sys.stderr)


def _same_file(a: str, b: Path) -> bool:
    try:
        return os.path.normcase(os.path.realpath(a)) == os.path.normcase(os.path.realpath(str(b)))
    except (OSError, ValueError):
        return False


def campaign_hooks(listing: dict, root: Path) -> list[dict]:
    """The campaign Stop hooks among a `hooks/list` answer: project hooks from this project's `.codex/hooks.json`
    whose command runs `codex-campaign-hook.py`, at whatever index the file holds them."""
    hook_file = root / ".codex" / "hooks.json"
    found = []
    for entry in listing.get("data", []):
        for hook in entry.get("hooks", []):
            if (hook.get("source") == "project" and hook.get("eventName") == "stop"
                    and _same_file(str(hook.get("sourcePath", "")), hook_file)
                    and HOOK_SCRIPT in str(hook.get("command", ""))):
                found.append(hook)
    return found


def project_key(root: Path) -> str:
    """The key Codex files a project's trust under in `[projects]`: the path as Windows spells it in lower case
    (`r:\\work\\repo`), or the path itself elsewhere."""
    return os.path.normcase(str(root)) if os.name == "nt" else str(root)


def list_hooks(cwd: Path, codex_home: Path | None = None) -> list[dict]:
    """Every hook Codex lists for a folder (its `hooks/list`), with its key, hash, trust status and source;
    `codex_home` points Codex at another home (`CODEX_HOME`) for this call only."""
    server = AppServer(_server_command(), cwd, codex_home)
    try:
        server.call("initialize", {"clientInfo": {"name": "conclave-codex-hook-trust", "version": "1"}})
        server.notify("initialized")
        listing = server.call("hooks/list", {"cwds": [str(cwd)]})
    finally:
        server.close()
    return [hook for entry in listing.get("data", []) for hook in entry.get("hooks", [])]


def approve_hooks(cwd: Path, hooks: list[dict], codex_home: Path | None = None) -> list[str]:
    """Record Codex's own `currentHash` as the approval of each hook, one write each; returns the keys written.
    A refusal or a silent server raises TrustError after the writes already made, which stand."""
    written = []
    for hook in hooks:
        _write(cwd, "hooks.state", {hook["key"]: {"trusted_hash": hook["currentHash"]}}, codex_home)
        written.append(hook["key"])
    return written


def _list(root: Path) -> list[dict]:
    return campaign_hooks({"data": [{"hooks": list_hooks(root)}]}, root)


def _write(root: Path, key_path: str, value: dict, codex_home: Path | None = None) -> None:
    server = AppServer(_server_command(), root, codex_home)
    try:
        server.call("initialize", {"clientInfo": {"name": "conclave-codex-hook-trust", "version": "1"}})
        server.notify("initialized")
        server.call("config/value/write", {"keyPath": key_path, "mergeStrategy": "upsert", "value": value})
    finally:
        server.close()


def trust(root: Path = ROOT, check: bool = False) -> list[dict]:
    """Each campaign Stop hook's key, its trust status before, and whether it was approved now. Codex lists a
    project's hooks only once the project is trusted (`projects.<path>.trust_level`), so a project whose hooks are
    not listed is trusted first, as the interactive client does when its operator says yes."""
    if not (root / ".codex" / "hooks.json").is_file():
        raise TrustError(f"{root / '.codex' / 'hooks.json'} does not exist; install the todo kit for Codex")
    hooks = _list(root)
    if not hooks and not check:
        _write(root, "projects", {project_key(root): {"trust_level": "trusted"}})
        hooks = _list(root)
    if not hooks:
        raise TrustError("Codex lists no campaign Stop hook for this project (one whose command runs "
                         f"scripts/{HOOK_SCRIPT}); the project may not be trusted in Codex yet")
    results = []
    for hook in hooks:
        status = str(hook.get("trustStatus"))
        enabled = hook.get("enabled", True) is not False
        approved = False
        # A hook the operator disabled in Codex stays disabled: an approval would not make Codex run it, and the
        # choice is theirs. It is reported, never re-enabled.
        if status != "trusted" and enabled and not check:
            _write(root, "hooks.state", {hook["key"]: {"trusted_hash": hook["currentHash"]}})
            approved = True
        results.append({"key": hook["key"], "status": status, "enabled": enabled, "approved": approved})
    return results


def main(argv: list[str]) -> int:
    check = argv == ["--check"]
    if argv and not check:
        print(__doc__.split("\n\n")[1], file=sys.stderr)
        return 2
    try:
        results = trust(ROOT, check=check)
    except (TrustError, OSError) as error:
        print(f"codex-hook-trust: {error}", file=sys.stderr)
        return 2
    for result in results:
        word = "approved" if result["approved"] else result["status"]
        if not result["enabled"]:
            word += ", disabled in Codex"
        print(f"codex-hook-trust: {word}: {result['key']}")
    if any(not r["enabled"] for r in results) or (check and any(r["status"] != "trusted" for r in results)):
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
