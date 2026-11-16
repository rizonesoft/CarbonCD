"""A stdio LSP client for the servers of `.conclave/lsp.json`: start one, open files in it, read its diagnostics.

`run_server` is the whole of one server's part in a run and returns one of six results, none of which can pass for
another:

    ok       every file it was sent was answered
    idle     no file of its kind in the input
    missing  its program is not installed
    skipped  the file its entry `requires` is absent
    failed   it could not be contained, refused or never answered `initialize`, ended before answering, or was
             ended at a limit (the reason says which)
    partial  it ran, and some file was not answered or could not be read (each is named)

A server is started with no shell and no console window, contained before it runs (a job object on Windows, its own
session on POSIX), and its whole process tree is ended on every way out. It is read-only: documents are opened in
the server's memory and nothing is written to the project. Standard library only.
"""
import json
import os
import queue
import shutil
import signal
import subprocess
import sys
import tempfile
import threading
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import lsp_table  # noqa: E402

READ_LIMIT = 64 * 1024 * 1024      # bytes read from one server in a run, standard output and standard error together
MESSAGE_LIMIT = 32 * 1024 * 1024   # bytes one message may announce
STDERR_KEPT = 64 * 1024            # the end of a server's standard error that is kept for --verbose
SEVERITY = {1: "error", 2: "warning", 3: "information", 4: "hint"}
SEVERITY_RANK = {"error": 1, "warning": 2, "information": 3, "hint": 4}
SCRATCH = "${scratch}"


class ClientError(Exception):
    """A server that could not be started or contained. The message is the reason its result carries."""


def language_for(entry, path):
    """The language id an entry gives a file: that of the longest of its suffixes the file's name ends with, so
    `.blade.php` is not `.php`. None when the server does not serve the file."""
    name = Path(path).name
    best = None
    for suffix in entry["extensions"]:
        if name.endswith(suffix) and (best is None or len(suffix) > len(best)):
            best = suffix
    return None if best is None else entry["extensions"][best]


def serves(entry, project, path):
    """The language id when the server serves the file (its suffix, and the file lies under the server's `root`),
    else None."""
    root = (Path(project) / entry["root"]).resolve()
    try:
        Path(path).resolve().relative_to(root)
    except ValueError:
        return None
    return language_for(entry, path)


def fill_scratch(value, folder):
    """`value` with `${scratch}` replaced by `folder` in every string, at any depth."""
    if isinstance(value, str):
        return value.replace(SCRATCH, folder)
    if isinstance(value, list):
        return [fill_scratch(item, folder) for item in value]
    if isinstance(value, dict):
        return {key: fill_scratch(item, folder) for key, item in value.items()}
    return value


def uses_scratch(value):
    return SCRATCH in json.dumps(value)


# ------------------------------------------------------------------ containment
#
# The shape settled for toolchain-check.py (D04 T02 section 1): a child made before containment, a launcher that
# exits first, containment that fails and an interrupt in between are each a way a process tree escapes.

def _windows_job(proc):
    """A Windows job object holding the server, or None when one cannot be made. A job holds every process the
    server starts, so they can be ended after the server itself has exited."""
    import ctypes
    from ctypes import wintypes

    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel32.CreateJobObjectW.restype = wintypes.HANDLE
    kernel32.CreateJobObjectW.argtypes = [wintypes.LPVOID, wintypes.LPCWSTR]
    kernel32.OpenProcess.restype = wintypes.HANDLE
    kernel32.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
    kernel32.AssignProcessToJobObject.restype = wintypes.BOOL
    kernel32.AssignProcessToJobObject.argtypes = [wintypes.HANDLE, wintypes.HANDLE]
    kernel32.CloseHandle.argtypes = [wintypes.HANDLE]
    job = kernel32.CreateJobObjectW(None, None)
    if not job:
        return None
    _kill_on_close(kernel32, job)
    handle = kernel32.OpenProcess(0x0101, False, proc.pid)  # PROCESS_SET_QUOTA | PROCESS_TERMINATE
    assigned = bool(handle) and bool(kernel32.AssignProcessToJobObject(job, handle))
    if handle:
        kernel32.CloseHandle(handle)
    if not assigned:
        kernel32.CloseHandle(job)
        return None
    return job


def _kill_on_close(kernel32, job):
    """Make a job end its processes when its last handle closes, so a caller that is killed outright (a bridge
    whose agent ends it without asking) leaves no server behind: the system closes the handle. Where the
    system refuses the limit the job still holds the tree for `_end_tree`, as before."""
    import ctypes
    from ctypes import wintypes

    class Basic(ctypes.Structure):
        _fields_ = [("PerProcessUserTimeLimit", ctypes.c_int64), ("PerJobUserTimeLimit", ctypes.c_int64),
                    ("LimitFlags", wintypes.DWORD), ("MinimumWorkingSetSize", ctypes.c_size_t),
                    ("MaximumWorkingSetSize", ctypes.c_size_t), ("ActiveProcessLimit", wintypes.DWORD),
                    ("Affinity", ctypes.c_size_t), ("PriorityClass", wintypes.DWORD),
                    ("SchedulingClass", wintypes.DWORD)]

    class Extended(ctypes.Structure):
        _fields_ = [("BasicLimitInformation", Basic), ("IoInfo", ctypes.c_uint64 * 6),
                    ("ProcessMemoryLimit", ctypes.c_size_t), ("JobMemoryLimit", ctypes.c_size_t),
                    ("PeakProcessMemoryUsed", ctypes.c_size_t), ("PeakJobMemoryUsed", ctypes.c_size_t)]

    limits = Extended()
    limits.BasicLimitInformation.LimitFlags = 0x2000   # JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE
    kernel32.SetInformationJobObject.restype = wintypes.BOOL
    kernel32.SetInformationJobObject.argtypes = [wintypes.HANDLE, ctypes.c_int, ctypes.c_void_p, wintypes.DWORD]
    return bool(kernel32.SetInformationJobObject(job, 9, ctypes.byref(limits), ctypes.sizeof(limits)))


def _windows_resume(proc):
    """Let a server that was started suspended run. It is started suspended so that it is in its job before it can
    start anything. True when it is running."""
    import ctypes
    from ctypes import wintypes

    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel32.OpenProcess.restype = wintypes.HANDLE
    kernel32.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
    kernel32.CloseHandle.argtypes = [wintypes.HANDLE]
    ntdll = ctypes.WinDLL("ntdll")
    ntdll.NtResumeProcess.restype = wintypes.LONG
    ntdll.NtResumeProcess.argtypes = [wintypes.HANDLE]
    handle = kernel32.OpenProcess(0x0800, False, proc.pid)  # PROCESS_SUSPEND_RESUME
    if not handle:
        return False
    try:
        return ntdll.NtResumeProcess(handle) == 0
    finally:
        kernel32.CloseHandle(handle)


def _windows_job_end(job):
    import ctypes
    from ctypes import wintypes

    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel32.TerminateJobObject.argtypes = [wintypes.HANDLE, wintypes.UINT]
    kernel32.CloseHandle.argtypes = [wintypes.HANDLE]
    kernel32.TerminateJobObject(job, 1)
    kernel32.CloseHandle(job)


def _end_tree(proc, job):
    """End a server and everything it started. Each step may find the process already gone, which is the outcome
    wanted, so that is not an error here."""
    if os.name == "nt":
        if job is not None:
            _windows_job_end(job)
    else:
        try:
            os.killpg(proc.pid, signal.SIGKILL)  # the server leads its own session, so the group is its tree
        except (ProcessLookupError, PermissionError):
            pass
    try:
        proc.kill()
    except OSError:
        pass
    try:
        proc.wait(timeout=10)
    except subprocess.TimeoutExpired:
        pass


# ------------------------------------------------------------------ the client

class Client:
    """One running language server. Use as a context manager: leaving it ends the server's whole process tree."""

    def __init__(self, entry, project, program, read_limit=READ_LIMIT, message_limit=MESSAGE_LIMIT,
                 outbox_limit=None, keep=None):
        """`read_limit` caps what is read from the server in the client's life (None: no cap, for a caller that
        keeps a server running; the cap on one message stays). `outbox_limit` caps the bytes waiting to be
        written to it: a server that stops reading is ended when it is passed. `keep` is the set of document
        URIs whose diagnostics are kept (None: every document's), which the caller changes as it opens and
        closes documents."""
        self.entry = entry
        self.root = (Path(project) / entry["root"]).resolve()
        self.settings = entry["settings"]
        self.read_limit = read_limit
        self.message_limit = message_limit
        self.outbox_limit = outbox_limit
        self.keep = keep
        self.diagnostics = {}
        self.published = {}       # uri -> (the order its last publication arrived in, the version it carried)
        self.sequence = 0         # how many publications have arrived
        self.responses = {}
        self._abandoned = set()   # requests given up on: their late responses are dropped
        self._queued = 0          # bytes waiting to be written to the server
        self.failure = None       # why the client stopped reading the server, when it did
        self.stderr = b""
        self._read = 0
        self._id = 0
        self._outbox = queue.Queue()
        self._unsent = 0          # messages queued and not yet written: a server that stopped reading keeps it above 0
        self._writer = None
        self._count = threading.Lock()
        self._alive = True
        self._job = None
        self._closed = False
        windows = os.name == "nt"
        try:
            # Contained before it runs. POSIX: its own session, made before the program is executed. Windows:
            # started suspended (CREATE_SUSPENDED), put in a job object, then resumed.
            self.proc = subprocess.Popen(
                [program, *entry["command"][1:]], cwd=str(self.root),
                stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                creationflags=(subprocess.CREATE_NO_WINDOW | subprocess.CREATE_NEW_PROCESS_GROUP | 0x00000004)
                if windows else 0,
                start_new_session=not windows)
        except OSError as exc:
            raise ClientError(f"could not be started: {exc.strerror or exc}") from exc
        try:
            if windows:
                self._job = _windows_job(self.proc)
                if self._job is None:
                    raise ClientError("could not be contained (Windows refused the job object), so it was not run")
                if not _windows_resume(self.proc):
                    raise ClientError("could not be resumed inside its job object, so it was not run")
            self._reader = threading.Thread(target=self._read_loop, daemon=True)
            self._errors = threading.Thread(target=self._stderr_loop, daemon=True)
            self._writer = threading.Thread(target=self._write_loop, daemon=True)
            self._reader.start()
            self._errors.start()
            self._writer.start()
        except BaseException:
            self.close()
            raise

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()

    @property
    def alive(self):
        return self._alive

    # ---------------------------------------------------------------- io

    def _counted(self, size):
        """Count bytes read from the server; False once the run's limit is passed."""
        with self._count:
            if self.read_limit is None:
                return True
            self._read += size
            if self._read > self.read_limit and self.failure is None:
                self.failure = f"was stopped: it wrote more than {self.read_limit} bytes"
            return self._read <= self.read_limit

    def _send(self, payload):
        body = json.dumps(payload).encode("utf-8")
        header = f"Content-Length: {len(body)}\r\n\r\n".encode("ascii")
        # Queued, never written here: a server that has stopped reading would block the write for ever, past
        # every time limit. The caller's deadline runs from now, and ending the server's tree frees the writer.
        data = header + body
        with self._count:
            if self.outbox_limit is not None and self._queued + len(data) > self.outbox_limit:
                # It has stopped reading: nothing more is queued for it, and its caller ends it.
                self.failure = self.failure or (f"was stopped: it stopped reading (more than {self.outbox_limit} "
                                                "bytes were waiting to be written to it)")
                self._alive = False
                return
            self._unsent += 1
            self._queued += len(data)
        self._outbox.put(data)

    def _write_loop(self):
        while True:
            data = self._outbox.get()
            if data is None:
                return
            try:
                self.proc.stdin.write(data)
                self.proc.stdin.flush()
            except (OSError, ValueError):
                self._alive = False
                return
            with self._count:
                self._unsent -= 1
                self._queued -= len(data)

    def _read_loop(self):
        stream = self.proc.stdout
        try:
            while True:
                length = None
                while True:
                    line = stream.readline(65536)
                    if not line or not self._counted(len(line)):
                        return
                    line = line.strip()
                    if not line:
                        break
                    if line.lower().startswith(b"content-length:"):
                        try:
                            length = int(line.split(b":", 1)[1])
                        except ValueError:
                            length = None
                if length is None:
                    continue
                if length > self.message_limit or length < 0:
                    self.failure = (f"was stopped: it announced a message of {length} bytes, over the "
                                    f"{self.message_limit} byte limit")
                    return
                body = b""
                while len(body) < length:
                    chunk = stream.read(min(65536, length - len(body)))
                    if not chunk or not self._counted(len(chunk)):
                        return
                    body += chunk
                try:
                    message = json.loads(body)
                except ValueError:
                    continue
                if isinstance(message, dict):
                    self._dispatch(message)
                    if self.failure:
                        return
        except (OSError, ValueError):
            return
        finally:
            self._alive = False

    def _stderr_loop(self):
        stream = self.proc.stderr
        try:
            while True:
                chunk = stream.read1(65536)
                if not chunk:
                    return
                self.stderr = (self.stderr + chunk)[-STDERR_KEPT:]
                if not self._counted(len(chunk)):
                    self._alive = False
                    return
        except (OSError, ValueError):
            return

    def _dispatch(self, message):
        method = message.get("method")
        if method == "textDocument/publishDiagnostics":
            params = message.get("params")
            uri = params.get("uri") if isinstance(params, dict) else None
            found = params.get("diagnostics", []) if isinstance(params, dict) else None
            if not isinstance(uri, str) or not isinstance(found, list):
                self.failure = self.failure or "sent a publishDiagnostics message that is not valid"
                return
            if self.keep is not None and uri not in self.keep:
                return   # a document the caller does not have open: nothing of it is held
            version = params.get("version")
            self.sequence += 1
            self.published[uri] = (self.sequence, version if type(version) is int else None)
            self.diagnostics[uri] = found
            return
        # A request from the server must be answered or the server may wait for ever. For
        # workspace/configuration the answer matters: Tailwind registers no linting without a settings object.
        if method and "id" in message:
            result = None
            if method == "workspace/configuration":
                params = message.get("params")
                items = params.get("items") if isinstance(params, dict) else None
                if not isinstance(items, list) or not items:
                    items = [{}]
                result = [self._config_for(item.get("section") if isinstance(item, dict) else None)
                          for item in items]
            elif method == "workspace/applyEdit":
                # This client reads. A server that asks for an edit is told it was not made.
                result = {"applied": False, "failureReason": "this client is read-only"}
            self._send({"jsonrpc": "2.0", "id": message["id"], "result": result})
            return
        if "id" in message and method is None:
            if message["id"] in self._abandoned:
                self._abandoned.discard(message["id"])   # too late: its request was given up
            else:
                self.responses[message["id"]] = message

    def _config_for(self, section):
        node = self.settings or {}
        if not isinstance(section, str) or not section:
            return node
        for part in section.split("."):
            if isinstance(node, dict) and part in node:
                node = node[part]
            else:
                return None
        return node

    # ---------------------------------------------------------------- protocol

    def request(self, method, params, timeout=60.0, stop=None):
        """The server's response message to a request, or None when it did not answer in time or ended, or when
        `stop` (an event) was set while waiting."""
        self._id += 1
        number = self._id
        self._send({"jsonrpc": "2.0", "id": number, "method": method, "params": params})
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            if number in self.responses:
                return self.responses.pop(number)
            if not self._alive:
                return self.responses.pop(number, None)
            if stop is not None and stop.is_set():
                break
            time.sleep(0.02)
        self._abandoned.add(number)   # a response that comes now is dropped, not kept
        self.responses.pop(number, None)
        return None

    def notify(self, method, params):
        self._send({"jsonrpc": "2.0", "method": method, "params": params})

    def initialize(self, options, timeout, stop=None):
        """None when the server is initialized, else why it is not. `stop` (an event) ends the wait."""
        response = self.request("initialize", {
            "processId": os.getpid(),
            "rootUri": self.root.as_uri(),
            "rootPath": str(self.root),
            "workspaceFolders": [{"uri": self.root.as_uri(), "name": self.root.name}],
            "clientInfo": {"name": "conclave-lsp-diagnostics", "version": "1"},
            "initializationOptions": options,
            "capabilities": {
                "textDocument": {"publishDiagnostics": {"relatedInformation": True, "versionSupport": True},
                                 "synchronization": {"dynamicRegistration": False},
                                 "hover": {"contentFormat": ["plaintext", "markdown"]},
                                 "definition": {"linkSupport": False}, "references": {}},
                "workspace": {"workspaceFolders": True, "configuration": True,
                              "didChangeConfiguration": {"dynamicRegistration": True}},
                "window": {"workDoneProgress": True},
            },
        }, timeout=timeout, stop=stop)
        if response is None:
            if self.failure:
                return self.failure
            if stop is not None and stop.is_set():
                return "was stopped while it was being initialized"
            if not self._alive:
                return "ended before answering initialize"
            return f"did not answer initialize within {timeout:g} seconds"
        if "error" in response:
            error = response["error"] if isinstance(response["error"], dict) else {}
            return f"refused initialize: {str(error.get('message') or response['error'])[:300]}"
        self.notify("initialized", {})
        if self.settings:
            self.notify("workspace/didChangeConfiguration", {"settings": self.settings})
        return None

    def open(self, path, language):
        """Open a file in the server and return its URI. Raises OSError when the file cannot be read: a file that
        was not read is never opened as an empty one."""
        text = Path(path).read_text(encoding="utf-8", errors="replace")
        uri = Path(path).resolve().as_uri()
        self.notify("textDocument/didOpen", {"textDocument": {"uri": uri, "languageId": language, "version": 1,
                                                              "text": text}})
        return uri

    def change(self, uri, text, version):
        """Tell the server an open document's whole text changed."""
        self.notify("textDocument/didChange", {"textDocument": {"uri": uri, "version": version},
                                               "contentChanges": [{"text": text}]})

    def close_document(self, uri):
        """Close a document in the server and drop what was kept of it."""
        self.notify("textDocument/didClose", {"textDocument": {"uri": uri}})
        self.diagnostics.pop(uri, None)
        self.published.pop(uri, None)

    def close(self):
        """End the server and whatever it started, on every way out: asked politely first when it is still
        answering, then by ending its process tree."""
        if self._closed:
            return
        self._closed = True
        try:
            # Asked only when it is still reading what it is sent.
            if self._alive and self.proc.poll() is None and self._unsent == 0:
                self.request("shutdown", None, timeout=3.0)
                self.notify("exit", None)
        finally:
            _end_tree(self.proc, self._job)
            # The tree is gone, so a write that was blocked has failed and the writer can be told to stop.
            self._outbox.put(None)
            if self._writer is not None and self._writer.is_alive():
                self._writer.join(timeout=10)
            for stream in (self.proc.stdin, self.proc.stdout, self.proc.stderr):
                try:
                    stream.close()
                except (OSError, ValueError):
                    pass


# ------------------------------------------------------------------ one server's part in a run

def _result(entry, status, reason=None, files=0, rows=(), unanswered=(), unread=(), stderr=""):
    return {"name": entry["name"], "status": status, "reason": reason, "files": files, "rows": list(rows),
            "unanswered": list(unanswered), "unread": list(unread), "stderr": stderr}


def start_server(entry, project, timeout=90.0, stop=None, **options):
    """Start one server of the table, as a check and a long-lived caller both must: its program found, its
    `requires` file present, `${scratch}` in its `init` and `settings` replaced by a temporary folder outside
    the project, the process contained and initialized in its `root`. `options` go to `Client`; `stop` (an
    event) ends the wait for its initialization.

    Returns (status, reason, client, scratch, stderr): `ok` with the running client and the folder to remove
    when it is closed (None when the entry names none), else `missing`, `skipped` or `failed` with the reason
    and nothing left running."""
    project = Path(project).resolve()
    program = lsp_table.find_program(project, entry)
    if program is None:
        return "missing", f"not installed. Install: {entry['install']}", None, None, ""
    if entry["requires"] and not (project / entry["root"] / entry["requires"]).is_file():
        where = entry["requires"] if entry["root"] == "." else f"{entry['root']}/{entry['requires']}"
        return "skipped", f"requires {where}", None, None, ""
    scratch, client, started = None, None, False
    try:
        if uses_scratch(entry["init"]) or uses_scratch(entry["settings"]):
            # Outside the repository: a server's index or cache never lands in the project.
            scratch = tempfile.mkdtemp(prefix="conclave-lsp-")
            entry = dict(entry, init=fill_scratch(entry["init"], scratch),
                         settings=fill_scratch(entry["settings"], scratch))
        try:
            client = Client(entry, project, program, **options)
        except ClientError as exc:
            return "failed", str(exc), None, None, ""
        refused = client.initialize(entry["init"], timeout=timeout, stop=stop)
        if refused:
            return "failed", refused, None, None, _text(client.stderr)
        started = True
        return "ok", None, client, scratch, ""
    finally:
        if not started:
            if client is not None:
                client.close()
            if scratch is not None:
                shutil.rmtree(scratch, ignore_errors=True)


def run_server(entry, project, paths, timeout=240.0, settle=None, read_limit=READ_LIMIT,
               message_limit=MESSAGE_LIMIT):
    """Run one server of the table over `paths` and return its result (see the module's opening)."""
    project = Path(project).resolve()
    # Its files first: a server with nothing to check is idle whether or not it is installed, so an input that
    # holds none of its files is never reported as a server that could not run.
    mine = [(Path(path), language) for path in paths for language in [serves(entry, project, path)] if language]
    if not mine:
        return _result(entry, "idle")
    settle = entry["settle"] if settle is None else settle
    status, reason, client, scratch, stderr = start_server(entry, project, timeout=min(90.0, timeout),
                                                           read_limit=read_limit, message_limit=message_limit)
    if status != "ok":
        return _result(entry, status, reason, len(mine) if status == "failed" else 0, stderr=stderr)
    try:
        uris, unread = {}, []
        for path, language in mine:
            try:
                uris[client.open(path, language)] = path
            except OSError as exc:
                unread.append(f"{_relative(path, project)} ({exc.strerror or exc})")
        deadline = time.monotonic() + timeout
        answered_at = None
        while time.monotonic() < deadline and client.alive:
            if all(uri in client.diagnostics for uri in uris):
                if answered_at is None:
                    answered_at = time.monotonic()
                elif time.monotonic() - answered_at >= settle:
                    break
            time.sleep(0.05)
        collected = {uri: list(client.diagnostics.get(uri, [])) for uri in uris}
        unanswered = [_relative(uris[uri], project) for uri in uris if uri not in client.diagnostics]
        ended_early = not client.alive
        failure = client.failure
        stderr = _text(client.stderr)
    finally:
        client.close()
        if scratch is not None:
            shutil.rmtree(scratch, ignore_errors=True)
    rows, invalid = [], []
    for uri, diagnostics in collected.items():
        file = _relative(uris[uri], project)
        for item in diagnostics:
            try:
                rows.append(_row(entry["name"], file, item))
            except ValueError:
                if file not in invalid:
                    invalid.append(file)
    if failure:
        return _result(entry, "failed", failure, len(mine), rows, unanswered, unread, stderr)
    if invalid:
        return _result(entry, "failed", f"published a diagnostic that is not valid for {', '.join(invalid)}",
                       len(mine), rows, unanswered, unread, stderr)
    if ended_early and unanswered:
        return _result(entry, "failed", f"ended before answering {len(unanswered)} of {len(uris)} file(s)",
                       len(mine), rows, unanswered, unread, stderr)
    if unanswered or unread:
        parts = []
        if unanswered:
            parts.append(f"{len(unanswered)} file(s) not answered within {timeout:g} seconds: {', '.join(unanswered)}")
        if unread:
            parts.append(f"{len(unread)} file(s) could not be read: {', '.join(unread)}")
        return _result(entry, "partial", "; ".join(parts), len(mine), rows, unanswered, unread, stderr)
    return _result(entry, "ok", None, len(mine), rows, stderr=stderr)


def _row(server, file, item):
    """One diagnostic as a row of the report. Raises ValueError for one that is not a diagnostic: it was sent by a
    program outside this one, so its shape is checked, never assumed."""
    if not isinstance(item, dict):
        raise ValueError("not an object")
    span = item.get("range", {})
    start = span.get("start", {}) if isinstance(span, dict) else None
    if not isinstance(start, dict):
        raise ValueError("no start position")
    line, column = start.get("line", 0), start.get("character", 0)
    severity = item.get("severity")
    severity = 1 if severity is None else severity
    for number in (line, column, severity):
        if not isinstance(number, int) or isinstance(number, bool) or number < 0:
            raise ValueError("not a number")
    if severity not in SEVERITY:
        raise ValueError("unknown severity")
    code = item.get("code")
    return {"server": server, "file": file, "line": line + 1, "column": column + 1,
            "severity": SEVERITY[severity], "code": "" if code is None else str(code),
            "message": str(item.get("message") or "").replace("\n", " ").strip()}


def _relative(path, project):
    try:
        return Path(path).resolve().relative_to(project).as_posix()
    except ValueError:
        return Path(path).as_posix()


def _text(data):
    return data.decode("utf-8", errors="replace")
