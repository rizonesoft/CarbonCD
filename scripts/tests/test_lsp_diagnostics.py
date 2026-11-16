"""The diagnostics client: one server's six results, what a server is told, which files it is sent, the limits and
the containment of its process tree, and lsp-diagnostics.py's input, output and exit codes.

The language servers are stubs: one Python program, written here, that speaks LSP over standard input and output
and behaves as its JSON configuration says. No real language server is run.
"""
import contextlib
import importlib.util
import io
import json
import os
import pathlib
import shutil
import subprocess
import sys
import tempfile
import time
import unittest
import unittest.mock

SCRIPTS = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(SCRIPTS / "lib"))
import lsp_client  # noqa: E402
import lsp_table  # noqa: E402

spec = importlib.util.spec_from_file_location("lsp_diagnostics", SCRIPTS / "lsp-diagnostics.py")
lsp_diagnostics = importlib.util.module_from_spec(spec)
spec.loader.exec_module(lsp_diagnostics)

# The stub language server. Its one argument is a JSON file saying how to behave; it logs what it receives.
STUB = r'''
import json, os, subprocess, sys, threading, time

config = json.load(open(sys.argv[1], encoding="utf-8"))
lock = threading.Lock()


def record(kind, value):
    if config.get("log"):
        with lock, open(config["log"], "a", encoding="utf-8") as log:
            log.write(json.dumps({"kind": kind, "value": value}) + "\n")


def send(message):
    body = json.dumps(message).encode("utf-8")
    with lock:
        sys.stdout.buffer.write(b"Content-Length: %d\r\n\r\n" % len(body) + body)
        sys.stdout.buffer.flush()


def read():
    length = None
    while True:
        line = sys.stdin.buffer.readline()
        if not line:
            return None
        line = line.strip()
        if not line:
            break
        if line.lower().startswith(b"content-length:"):
            length = int(line.split(b":", 1)[1])
    return json.loads(sys.stdin.buffer.read(length)) if length else {}


record("cwd", os.getcwd())
if config.get("child"):
    holds = config["child"] == "holds-pipe"
    child = subprocess.Popen([sys.executable, "-c", "import time; time.sleep(120)"], stdin=subprocess.DEVNULL,
                             stdout=sys.stdout if holds else subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    with open(config["pids"], "w", encoding="utf-8") as pids:
        pids.write(f"{os.getpid()} {child.pid}")
if config.get("exit_at_start"):
    sys.exit(0)
if config.get("flood"):
    # Bounded: a fixture that writes in a loop stops by itself.
    stream = sys.stdout.buffer if config["flood"] == "stdout" else sys.stderr.buffer
    for _ in range(config["flood_bytes"] // 1024):
        stream.write(b"x" * 1023 + b"\n")
        stream.flush()
    time.sleep(120)
if config.get("big_message"):
    sys.stdout.buffer.write(b"Content-Length: %d\r\n\r\n" % config["big_message"])
    sys.stdout.buffer.flush()
    time.sleep(120)


def diagnostics(text):
    found = []
    for number, line in enumerate(text.splitlines()):
        for mark, severity in (("E:", 1), ("W:", 2)):
            if mark in line:
                found.append({"range": {"start": {"line": number, "character": line.index(mark)},
                                        "end": {"line": number, "character": len(line)}},
                              "severity": severity, "code": "stub", "message": line.split(mark, 1)[1].strip()})
    return found


def publish(uri, items):
    send({"jsonrpc": "2.0", "method": "textDocument/publishDiagnostics", "params": {"uri": uri, "diagnostics": items}})


asked = 9000
while True:
    message = read()
    if message is None:
        break
    record("message", message)
    method = message.get("method")
    if method == "initialize":
        options = message["params"].get("initializationOptions") or {}
        if isinstance(options.get("storagePath"), str):
            record("scratch", {"path": options["storagePath"], "exists": os.path.isdir(options["storagePath"])})
        mode = config.get("init", "ok")
        if mode == "silent":
            continue
        if mode == "exit":
            sys.exit(0)
        if mode == "error":
            send({"jsonrpc": "2.0", "id": message["id"], "error": {"code": -32002, "message": "no workspace here"}})
            continue
        send({"jsonrpc": "2.0", "id": message["id"], "result": {"capabilities": {}}})
    elif method == "initialized":
        for section in config.get("ask_config", []):
            asked += 1
            send({"jsonrpc": "2.0", "id": asked, "method": "workspace/configuration",
                  "params": {"items": [{"section": section}]}})
        if config.get("after_init") == "exit":
            sys.exit(0)
        if config.get("stop_reading"):
            time.sleep(120)
    elif method == "textDocument/didOpen":
        document = message["params"]["textDocument"]
        if document["uri"].rsplit("/", 1)[-1] in config.get("skip", []):
            continue
        if "raw_params" in config:
            send({"jsonrpc": "2.0", "method": "textDocument/publishDiagnostics", "params": config["raw_params"]})
            continue
        found = config["raw"] if "raw" in config else diagnostics(document["text"])
        publish(document["uri"], found)
        late = config.get("late")
        if late:
            extra = found + [{"range": {"start": {"line": 0, "character": 0}, "end": {"line": 0, "character": 1}},
                              "severity": 1, "code": "late", "message": "found later"}]
            threading.Timer(late, publish, (document["uri"], extra)).start()
    elif method == "shutdown":
        send({"jsonrpc": "2.0", "id": message["id"], "result": None})
    elif method == "exit":
        break
'''


def alive(pid):
    if os.name == "nt":
        out = subprocess.run(["tasklist", "/FI", f"PID eq {pid}", "/NH"], capture_output=True, text=True,
                             creationflags=subprocess.CREATE_NO_WINDOW).stdout
        return str(pid) in out
    try:
        os.kill(pid, 0)
    except ProcessLookupError:
        return False
    except PermissionError:
        return True
    return True


def gone(pid, seconds=10):
    deadline = time.monotonic() + seconds
    while alive(pid):
        if time.monotonic() > deadline:
            return False
        time.sleep(0.1)
    return True


class Fixture(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.dir = pathlib.Path(tmp.name).resolve()
        self.project = self.dir / "project"
        (self.project / ".conclave").mkdir(parents=True)
        self.stub = self.dir / "stub-server.py"
        self.stub.write_text(STUB, encoding="utf-8")
        self.entries = []

    def server(self, name="stub", extensions=None, behaviour=None, **keys):
        """A table entry whose program is the stub, behaving as `behaviour` says; its log is `<name>.log`."""
        config = self.dir / f"{name}.json"
        behaviour = dict(behaviour or {}, log=str(self.dir / f"{name}.log"), pids=str(self.dir / f"{name}.pids"))
        config.write_text(json.dumps(behaviour), encoding="utf-8")
        entry = {"name": name, "command": [sys.executable, str(self.stub), str(config)],
                 "extensions": extensions or {".py": "python"}, "install": f"install {name} by hand", **keys}
        self.entries.append(entry)
        (self.project / lsp_table.TABLE).write_text(json.dumps({"schema_version": 1, "servers": self.entries}),
                                                    encoding="utf-8")
        return next(loaded for loaded in lsp_table.load_servers(self.project) if loaded["name"] == name)

    def file(self, rel, text="clean\n"):
        path = self.project / rel
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")
        return path

    def log(self, name="stub"):
        path = self.dir / f"{name}.log"
        if not path.is_file():
            return []
        return [json.loads(line) for line in path.read_text(encoding="utf-8").splitlines()]

    def messages(self, name="stub", method=None):
        found = [item["value"] for item in self.log(name) if item["kind"] == "message"]
        return [message for message in found if method is None or message.get("method") == method]

    def pids(self, name="stub"):
        path = self.dir / f"{name}.pids"
        deadline = time.monotonic() + 10
        while not (path.is_file() and len(path.read_text().split()) == 2):
            self.assertLess(time.monotonic(), deadline, "the stub never wrote its process ids")
            time.sleep(0.05)
        return [int(pid) for pid in path.read_text().split()]

    def run_server(self, entry, *paths, **options):
        options.setdefault("timeout", 20)
        options.setdefault("settle", 0.2)
        return lsp_client.run_server(entry, self.project, list(paths), **options)


class WhatAServerIsToldTests(Fixture):
    def test_a_server_is_started_in_its_root_with_its_init_and_settings_and_answers_are_read_back(self):
        entry = self.server(root="app", init={"mode": "strict"}, settings={"stub": {"lint": True, "level": 3}},
                            behaviour={"ask_config": ["stub", "stub.lint", "stub.absent", "other"]})
        path = self.file("app/src/main.py", "fine\nvalue = 1  # E: undefined name\nother  # W: unused\n")
        result = self.run_server(entry, path)
        self.assertEqual((result["status"], result["reason"], result["files"]), ("ok", None, 1))
        self.assertEqual(result["rows"], [
            {"server": "stub", "file": "app/src/main.py", "line": 2, "column": 14, "severity": "error",
             "code": "stub", "message": "undefined name"},
            {"server": "stub", "file": "app/src/main.py", "line": 3, "column": 10, "severity": "warning",
             "code": "stub", "message": "unused"}])
        (cwd,) = [item["value"] for item in self.log() if item["kind"] == "cwd"]
        self.assertTrue(os.path.samefile(cwd, self.project / "app"))
        (initialize,) = self.messages(method="initialize")
        root_uri = (self.project / "app").resolve().as_uri()
        self.assertEqual(initialize["params"]["rootUri"], root_uri)
        self.assertEqual(initialize["params"]["workspaceFolders"], [{"uri": root_uri, "name": "app"}])
        self.assertEqual(initialize["params"]["initializationOptions"], {"mode": "strict"})
        (changed,) = self.messages(method="workspace/didChangeConfiguration")
        self.assertEqual(changed["params"], {"settings": {"stub": {"lint": True, "level": 3}}})
        answers = {message["id"]: message["result"] for message in self.messages() if "result" in message}
        self.assertEqual([answers[number] for number in sorted(answers)],
                         [[{"lint": True, "level": 3}], [True], [None], [None]])
        (opened,) = self.messages(method="textDocument/didOpen")
        self.assertEqual(opened["params"]["textDocument"]["languageId"], "python")
        self.assertEqual(opened["params"]["textDocument"]["uri"], path.resolve().as_uri())
        self.assertEqual(opened["params"]["textDocument"]["text"], path.read_text(encoding="utf-8"))

    def test_a_file_goes_only_to_the_servers_that_serve_it(self):
        python = self.server("python")
        php = self.server("php", extensions={".php": "php", ".blade.php": "blade"}, root="app")
        files = [self.file("tool.py"), self.file("app/page.php"), self.file("app/view.blade.php"),
                 self.file("legacy/old.php")]
        self.assertEqual(self.run_server(python, *files)["files"], 1)
        self.assertEqual(self.run_server(php, *files)["files"], 2)
        sent = {message["params"]["textDocument"]["uri"].rsplit("/", 1)[-1]:
                message["params"]["textDocument"]["languageId"]
                for message in self.messages("php", "textDocument/didOpen")}
        # The longest suffix wins, and the file outside the server's root is not sent.
        self.assertEqual(sent, {"page.php": "php", "view.blade.php": "blade"})
        self.assertEqual([m["params"]["textDocument"]["uri"].rsplit("/", 1)[-1]
                          for m in self.messages("python", "textDocument/didOpen")], ["tool.py"])
        self.assertEqual(lsp_client.language_for(php, "a.blade.php"), "blade")
        self.assertEqual(lsp_client.language_for(php, "a.php"), "php")
        self.assertIsNone(lsp_client.language_for(php, "a.py"))
        self.assertIsNone(lsp_client.serves(php, self.project, self.project / "legacy/old.php"))

    def test_scratch_is_a_folder_outside_the_project_for_the_run_only(self):
        entry = self.server(init={"storagePath": "${scratch}", "clearCache": False},
                            settings={"cache": {"dir": "${scratch}/cache"}})
        self.assertEqual(self.run_server(entry, self.file("a.py"))["status"], "ok")
        (scratch,) = [item["value"] for item in self.log() if item["kind"] == "scratch"]
        self.assertTrue(scratch["exists"], "the folder did not exist while the server ran")
        folder = pathlib.Path(scratch["path"])
        self.assertFalse(folder.exists(), "the folder outlived the run")
        self.assertNotIn(self.project, [folder, *folder.parents])
        (changed,) = self.messages(method="workspace/didChangeConfiguration")
        self.assertEqual(pathlib.Path(changed["params"]["settings"]["cache"]["dir"]), folder / "cache")
        # Also when the server is ended at a limit.
        flooding = self.server("flooding", init={"storagePath": "${scratch}"}, behaviour={"init": "silent"})
        self.assertEqual(self.run_server(flooding, self.file("b.py"), timeout=2)["status"], "failed")
        (scratch,) = [item["value"] for item in self.log("flooding") if item["kind"] == "scratch"]
        self.assertTrue(scratch["exists"])
        self.assertFalse(pathlib.Path(scratch["path"]).exists())
        # An entry that names no scratch folder gets none.
        self.assertFalse(lsp_client.uses_scratch({"a": ["b", {"c": "d"}]}))
        self.assertEqual(lsp_client.fill_scratch({"a": ["${scratch}/x", 3, {"b": "${scratch}"}]}, "/tmp/s"),
                         {"a": ["/tmp/s/x", 3, {"b": "/tmp/s"}]})

    def test_a_later_set_of_diagnostics_is_collected_when_the_client_listens_long_enough(self):
        late = {"late": 1.5}
        patient = self.server("patient", behaviour=late, settle=4)
        hasty = self.server("hasty", behaviour=late, settle=1)
        path = self.file("a.py")
        # Each entry's own settle.
        self.assertEqual([row["code"] for row in self.run_server(patient, path, settle=None)["rows"]], ["late"])
        self.assertEqual(self.run_server(hasty, path, settle=None)["rows"], [])
        # And the override, either way.
        self.assertEqual(self.run_server(patient, path, settle=0.2)["rows"], [])
        self.assertEqual([row["code"] for row in self.run_server(hasty, path, settle=4)["rows"]], ["late"])


class SixResultsTests(Fixture):
    def test_a_server_that_is_not_installed_or_lacks_what_it_requires_is_not_run(self):
        missing = dict(self.server("absent"), command=["no-such-language-server-anywhere"])
        result = self.run_server(missing, self.file("a.py"))
        self.assertEqual((result["status"], result["reason"]),
                         ("missing", "not installed. Install: install absent by hand"))
        needy = self.server("needy", root="app", requires="artisan")
        path = self.file("app/a.py")
        result = self.run_server(needy, path)
        self.assertEqual((result["status"], result["reason"]), ("skipped", "requires app/artisan"))
        self.assertEqual(self.log("needy"), [], "a skipped server was started")
        self.file("app/artisan", "")
        self.assertEqual(self.run_server(needy, path)["status"], "ok")

    def test_no_file_of_its_kind_is_idle_and_starts_nothing(self):
        entry = self.server()
        result = self.run_server(entry, self.file("notes.txt"))
        self.assertEqual((result["status"], result["files"]), ("idle", 0))
        self.assertEqual(self.log(), [])
        # Also for a server that could not run: with nothing to check, that it is absent is not a failure.
        missing = dict(self.server("absent"), command=["no-such-language-server-anywhere"])
        needy = self.server("needy", requires="artisan")
        for unable in (missing, needy):
            self.assertEqual(self.run_server(unable, self.project / "notes.txt")["status"], "idle")
            self.assertEqual(self.run_server(unable)["status"], "idle")

    def test_a_diagnostic_that_is_not_one_makes_the_server_failed_and_never_crashes_the_run(self):
        path = self.file("a.py")
        start = {"line": 0, "character": 0}
        cases = {"null": [None], "text": ["wrong"], "range": [{"range": "nowhere", "message": "m"}],
                 "line": [{"range": {"start": {"line": "3", "character": 0}}, "message": "m"}],
                 "negative": [{"range": {"start": {"line": -1, "character": 0}}, "message": "m"}],
                 "severity": [{"range": {"start": start}, "severity": 9, "message": "m"}],
                 "mixed": [{"range": {"start": start}, "severity": 1, "message": "real"}, None]}
        for name, raw in cases.items():
            with self.subTest(diagnostic=name):
                result = self.run_server(self.server(f"bad-{name}", behaviour={"raw": raw}), path)
                self.assertEqual((result["status"], result["reason"]),
                                 ("failed", "published a diagnostic that is not valid for a.py"))
        self.assertEqual([row["message"] for row in result["rows"]], ["real"])  # what was valid is kept
        for name, behaviour in (("params", {"raw_params": ["not", "an", "object"]}),
                                ("list", {"raw": "not a list"})):
            with self.subTest(message=name):
                result = self.run_server(self.server(f"odd-{name}", behaviour=behaviour), path)
                self.assertEqual((result["status"], result["reason"]),
                                 ("failed", "sent a publishDiagnostics message that is not valid"))
        # A diagnostic with no severity or code is an error with no code; one with no range is at the file's start.
        plain = self.run_server(self.server("plain", behaviour={"raw": [{"message": "bare"}]}), path)
        self.assertEqual((plain["status"], plain["rows"]), ("ok", [
            {"server": "plain", "file": "a.py", "line": 1, "column": 1, "severity": "error", "code": "",
             "message": "bare"}]))

    def test_a_server_that_refuses_or_never_answers_initialize_has_failed(self):
        refusing = self.server("refusing", behaviour={"init": "error"})
        result = self.run_server(refusing, self.file("a.py"))
        self.assertEqual((result["status"], result["reason"]), ("failed", "refused initialize: no workspace here"))
        silent = self.server("silent", behaviour={"init": "silent"})
        result = self.run_server(silent, self.file("a.py"), timeout=2)
        self.assertEqual((result["status"], result["reason"]),
                         ("failed", "did not answer initialize within 2 seconds"))
        dying = self.server("dying", behaviour={"init": "exit"})
        result = self.run_server(dying, self.file("a.py"))
        self.assertEqual((result["status"], result["reason"]), ("failed", "ended before answering initialize"))

    def test_a_server_that_ends_before_answering_its_files_has_failed(self):
        entry = self.server(behaviour={"after_init": "exit"})
        result = self.run_server(entry, self.file("a.py"), self.file("b.py"))
        self.assertEqual((result["status"], result["reason"]), ("failed", "ended before answering 2 of 2 file(s)"))
        self.assertEqual(result["unanswered"], ["a.py", "b.py"])

    def test_a_file_that_is_not_answered_or_cannot_be_read_makes_the_server_partial_and_is_named(self):
        entry = self.server(behaviour={"skip": ["b.py"]})
        answered, unanswered = self.file("a.py", "x  # E: wrong\n"), self.file("b.py")
        result = self.run_server(entry, answered, unanswered, timeout=3)
        self.assertEqual(result["status"], "partial")
        self.assertEqual(result["reason"], "1 file(s) not answered within 3 seconds: b.py")
        self.assertEqual([row["file"] for row in result["rows"]], ["a.py"])  # what was answered is still reported
        self.assertEqual(result["unanswered"], ["b.py"])
        locked = self.file("locked.py")
        real = pathlib.Path.read_text

        def read_text(path, *args, **kwargs):
            if path.name == "locked.py":
                raise PermissionError(13, "Permission denied")
            return real(path, *args, **kwargs)

        reader = self.server("reader")
        with unittest.mock.patch.object(pathlib.Path, "read_text", read_text):
            result = self.run_server(reader, answered, locked)
        self.assertEqual(result["status"], "partial")
        self.assertEqual(result["reason"], "1 file(s) could not be read: locked.py (Permission denied)")
        self.assertEqual(result["unread"], ["locked.py (Permission denied)"])
        opened = [m["params"]["textDocument"]["uri"].rsplit("/", 1)[-1]
                  for m in self.messages("reader", "textDocument/didOpen")]
        self.assertEqual(opened, ["a.py"], "a file that could not be read was opened all the same")


class ContainmentTests(Fixture):
    def test_the_child_of_a_server_that_completed_normally_is_ended_with_it(self):
        entry = self.server(behaviour={"child": "silent"})
        self.assertEqual(self.run_server(entry, self.file("a.py"))["status"], "ok")
        for pid in self.pids():
            self.assertTrue(gone(pid), f"process {pid} outlived a clean run")

    def test_a_server_that_never_answers_is_ended_with_its_child_at_the_time_limit(self):
        entry = self.server(behaviour={"child": "silent", "skip": ["a.py"]})
        started = time.monotonic()
        result = self.run_server(entry, self.file("a.py"), timeout=3)
        self.assertLess(time.monotonic() - started, 20)
        self.assertEqual(result["status"], "partial")
        for pid in self.pids():
            self.assertTrue(gone(pid), f"process {pid} outlived the run")

    def test_a_server_that_stops_reading_cannot_hold_the_run_past_its_time_limit(self):
        entry = self.server(behaviour={"child": "silent", "stop_reading": True})
        big = self.file("big.py", "value = 1\n" * 800_000)  # 8 MB: more than a pipe holds
        started = time.monotonic()
        result = self.run_server(entry, big, timeout=3)
        self.assertLess(time.monotonic() - started, 15, "the write to a server that stopped reading held the run")
        self.assertEqual((result["status"], result["reason"]),
                         ("partial", "1 file(s) not answered within 3 seconds: big.py"))
        for pid in self.pids():
            self.assertTrue(gone(pid), f"process {pid} outlived the run")

    def test_a_server_that_floods_either_stream_is_stopped_at_the_limit_with_its_child(self):
        for stream in ("stdout", "stderr"):
            with self.subTest(stream=stream):
                entry = self.server(f"flood-{stream}", behaviour={"child": "silent", "flood": stream,
                                                                 "flood_bytes": 4 * 1024 * 1024})
                started = time.monotonic()
                result = self.run_server(entry, self.file(f"{stream}.py"), timeout=60, read_limit=64 * 1024)
                self.assertLess(time.monotonic() - started, 40, "the client read the flood to its end")
                self.assertEqual((result["status"], result["reason"]),
                                 ("failed", "was stopped: it wrote more than 65536 bytes"))
                for pid in self.pids(f"flood-{stream}"):
                    self.assertTrue(gone(pid), f"process {pid} outlived the run")

    def test_a_message_over_the_limit_stops_the_server(self):
        entry = self.server(behaviour={"child": "silent", "big_message": 5000})
        result = self.run_server(entry, self.file("a.py"), timeout=60, message_limit=4096)
        self.assertEqual((result["status"], result["reason"]),
                         ("failed", "was stopped: it announced a message of 5000 bytes, over the 4096 byte limit"))
        for pid in self.pids():
            self.assertTrue(gone(pid), f"process {pid} outlived the run")

    def test_a_child_that_outlives_its_launcher_and_holds_the_pipe_is_ended(self):
        entry = self.server(behaviour={"child": "holds-pipe", "exit_at_start": True})
        result = self.run_server(entry, self.file("a.py"), timeout=5)
        self.assertEqual(result["status"], "failed")
        for pid in self.pids():
            self.assertTrue(gone(pid), f"process {pid} outlived the run")

    def test_an_interrupt_during_a_run_leaves_no_process(self):
        entry = self.server(behaviour={"child": "silent", "skip": ["a.py"]})
        path = self.file("a.py")
        real = time.monotonic
        pids = self.dir / "stub.pids"

        def interrupted():
            if pids.is_file() and len(pids.read_text().split()) == 2 and self.messages(method="textDocument/didOpen"):
                raise KeyboardInterrupt
            return real()

        with unittest.mock.patch.object(lsp_client.time, "monotonic", interrupted):
            with self.assertRaises(KeyboardInterrupt):
                self.run_server(entry, path, timeout=60)
        for pid in self.pids():
            self.assertTrue(gone(pid), f"process {pid} outlived the interrupted run")

    @unittest.skipUnless(os.name == "nt", "the job object is how Windows contains a server")
    def test_a_server_that_cannot_be_contained_is_never_run(self):
        entry = self.server()
        path = self.file("a.py")
        started = []
        for attribute, replacement, reason in (
                ("_windows_job", lambda proc: started.append(proc.pid),
                 "could not be contained (Windows refused the job object), so it was not run"),
                ("_windows_resume", lambda proc: started.append(proc.pid) or False,
                 "could not be resumed inside its job object, so it was not run")):
            with self.subTest(refused=attribute):
                with unittest.mock.patch.object(lsp_client, attribute, replacement):
                    result = self.run_server(entry, path)
                self.assertEqual((result["status"], result["reason"]), ("failed", reason))
                self.assertEqual(self.log(), [], "the server ran outside a job")
                self.assertTrue(gone(started[-1]), "the suspended process was left behind")
        self.assertEqual(self.run_server(entry, path)["status"], "ok")


class CommandLine(Fixture):
    """The fixture plus lsp-diagnostics.py run in this process."""

    def run_main(self, *argv):
        out, err = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            try:
                code = lsp_diagnostics.main([str(arg) for arg in argv], root=self.project)
            except SystemExit as exc:
                code = exc.code
        return code, out.getvalue(), err.getvalue()

    def both_forms(self, *argv):
        """The run as text and as JSON: (exit, text, stderr, the parsed JSON document). The exits must agree."""
        code, text, err = self.run_main(*argv, "--settle", "0.2", "--timeout", "4")
        json_code, document, _err = self.run_main(*argv, "--settle", "0.2", "--timeout", "4", "--json")
        report = json.loads(document)
        self.assertEqual(json_code, code, "the exit differs between the text and the JSON form")
        self.assertEqual(report["exit"], code)
        return code, text, err, report

    def assert_refused(self, expected, *argv):
        """The run is refused in both forms: exit 2 and one line on standard error; nothing on standard output
        as text, and one JSON document carrying the refusal with `--json`."""
        for json_form in (False, True):
            with self.subTest(argv=argv, json=json_form):
                code, out, err = self.run_main(*argv, *(["--json"] if json_form else []))
                self.assertEqual(code, 2)
                self.assertEqual(len(err.splitlines()), 1, err)
                self.assertTrue(err.startswith(f"lsp-diagnostics: {expected}"), err)
                if not json_form:
                    self.assertEqual(out, "")
                    continue
                report = json.loads(out)
                self.assertEqual(report, {"servers": {}, "diagnostics": [], "failing": 0, "exit": 2,
                                          "error": err[len("lsp-diagnostics: "):].rstrip("\n")})


class CommandLineTests(CommandLine):
    def test_a_clean_run_and_a_failing_diagnostic(self):
        self.server(note="the stub's own note")
        clean, dirty = self.file("clean.py"), self.file("dirty.py", "x  # E: wrong here\n")
        code, text, err, report = self.both_forms("--all", clean.name)
        self.assertEqual((code, err), (0, ""))
        self.assertIn("stub           1 file(s), 0 diagnostic(s). the stub's own note", text)
        self.assertIn("lsp-diagnostics: 0 diagnostic(s) at or above threshold.", text)
        self.assertEqual(report, {"servers": {"stub": {"status": "ok", "reason": None, "files": 1, "unanswered": [],
                                                       "unread": []}},
                                  "diagnostics": [], "failing": 0, "exit": 0, "error": None})
        code, text, _err, report = self.both_forms("--server", "stub", dirty.name)
        self.assertEqual(code, 1)
        self.assertIn("dirty.py:1:6  error       wrong here [stub]", text)
        self.assertEqual((report["failing"], report["diagnostics"][0]["message"]), (1, "wrong here"))

    def test_a_servers_threshold_and_the_override(self):
        self.server("strict", severity="warning")
        self.server("lenient")
        path = self.file("a.py", "x  # W: only a warning\n")
        self.assertEqual(self.both_forms("--server", "strict", path.name)[0], 1)
        self.assertEqual(self.both_forms("--server", "lenient", path.name)[0], 0)
        self.assertEqual(self.both_forms("--server", "lenient", "--severity", "warning", path.name)[0], 1)
        self.assertEqual(self.both_forms("--server", "strict", "--severity", "error", path.name)[0], 0)

    def test_missing_skipped_failed_and_partial_servers_set_the_exit(self):
        self.server("good")
        self.entries.append({"name": "absent", "command": ["no-such-language-server-anywhere"],
                             "extensions": {".py": "python"}, "install": "npm install -g absent"})
        self.server("needy", requires="artisan")
        self.server("refusing", behaviour={"init": "error"})
        self.server("half", behaviour={"skip": ["b.py"]})
        a, b = self.file("a.py"), self.file("b.py")
        # One missing among two: the other's result decides.
        code, _text, err, report = self.both_forms("--server", "good", "--server", "absent", a.name)
        self.assertEqual(code, 0)
        self.assertIn("absent         SKIPPED -- not installed. Install: npm install -g absent", err)
        self.assertEqual(report["servers"]["absent"]["status"], "missing")
        # Nothing could check anything.
        code, _text, err, report = self.both_forms("--server", "absent", "--server", "needy", a.name)
        self.assertEqual(code, 3)
        self.assertIn("needy          SKIPPED -- requires artisan", err)
        self.assertEqual([entry["status"] for entry in report["servers"].values()], ["missing", "skipped"])
        code, _text, err, report = self.both_forms("--server", "refusing", a.name)
        self.assertEqual(code, 3)
        self.assertIn("refusing       FAILED -- refused initialize: no workspace here", err)
        # A check that did not finish is not a pass, even beside a clean server.
        code, _text, err, report = self.both_forms("--server", "good", "--server", "refusing", a.name)
        self.assertEqual((code, report["servers"]["refusing"]["status"]), (1, "failed"))
        code, _text, err, report = self.both_forms("--server", "good", "--server", "half", a.name, b.name)
        self.assertEqual((code, report["servers"]["half"]["status"]), (1, "partial"))
        self.assertIn("half           PARTIAL -- 1 file(s) not answered within 4 seconds: b.py", err)
        self.assertEqual(report["servers"]["half"]["unanswered"], ["b.py"])

    def test_no_file_of_any_servers_kind_is_a_pass_that_says_so(self):
        # Also when a server of the table is not installed: it had nothing to check.
        self.entries.append({"name": "absent", "command": ["no-such-language-server-anywhere"],
                             "extensions": {".py": "python"}, "install": "npm install -g absent"})
        self.server()
        code, text, err, report = self.both_forms("--all", self.file("notes.txt").name)
        self.assertEqual((code, err), (0, ""))
        self.assertIn("stub           no file of its kind in this input", text)
        self.assertEqual({name: entry["status"] for name, entry in report["servers"].items()},
                         {"absent": "idle", "stub": "idle"})
        code, text, err, report = self.both_forms("--all")
        self.assertEqual((code, err), (0, ""))
        self.assertIn("lsp-diagnostics: no file in the input.", text)
        self.assertEqual((report["servers"]["absent"]["status"], report["diagnostics"]), ("idle", []))

    def test_bad_usage_an_unknown_server_and_a_broken_or_empty_table_are_one_line_and_exit_2(self):
        (self.project / lsp_table.TABLE).write_text('{"schema_version": 1, "servers": []}', encoding="utf-8")
        self.assert_refused(".conclave/lsp.json holds no server: add a stack's with "
                            "python scripts/lsp-servers.py add <preset>\n", "--all")
        self.server()
        path = self.file("a.py")
        cases = [(("--server", "gopls", path.name), "no server named 'gopls' in .conclave/lsp.json (it holds: stub)"),
                 ((path.name,), "name the servers with --server <name>, or run every one with --all"),
                 (("--all", "--server", "stub", path.name), "name the servers with --server <name>"),
                 (("--all", "--staged", path.name), "give one input:"),
                 (("--all", "missing.py"), "missing.py is not a file in the project"),
                 (("--all", "../outside.py"), "../outside.py is outside the project")]
        for argv, expected in cases:
            self.assert_refused(expected, *argv)
        (self.project / lsp_table.TABLE).write_text('{"schema_version": 1, "servers": [{"name": "x"}]}',
                                                    encoding="utf-8")
        self.assert_refused(".conclave/lsp.json: server 'x': missing key", "--all", path.name)

    def test_the_script_runs_as_a_program_from_an_installed_layout(self):
        scripts = self.project / "scripts"
        (scripts / "lib").mkdir(parents=True)
        for rel in ("lsp-diagnostics.py", "lib/lsp_client.py", "lib/lsp_table.py", "lib/lsp_presets.json"):
            (scripts / rel).write_bytes((SCRIPTS / rel).read_bytes())
        self.server()
        self.file("src/a.py", "x  # E: wrong\n")
        ran = subprocess.run([sys.executable, "-B", str(scripts / "lsp-diagnostics.py"), "--all", "src/a.py",
                              "--settle", "0.2", "--json"], capture_output=True, text=True, timeout=120,
                             cwd=self.dir, creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
        self.assertEqual(ran.returncode, 1, ran.stderr)
        self.assertEqual(json.loads(ran.stdout)["diagnostics"][0]["file"], "src/a.py")


@unittest.skipUnless(shutil.which("git"), "needs git")
class GitInputTests(CommandLine):
    def setUp(self):
        super().setUp()
        env = unittest.mock.patch.dict(os.environ, {"GIT_CONFIG_GLOBAL": os.devnull, "GIT_CONFIG_NOSYSTEM": "1"})
        env.start()
        self.addCleanup(env.stop)
        for args in (["init", "-q"], ["config", "user.email", "t@example.com"], ["config", "user.name", "T"],
                     ["config", "core.autocrlf", "false"]):
            self.git(*args)
        self.server()
        self.file("kept.py", "x  # E: in the first commit\n")
        self.file("removed.py")
        self.git("add", "-A")
        self.git("commit", "-qm", "first")

    def git(self, *args):
        return subprocess.run(["git", "-C", str(self.project), *args], capture_output=True, text=True, check=True,
                              creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))

    def files(self, *argv):
        code, out, err = self.run_main("--all", *argv, "--settle", "0.2", "--json")
        self.assertIn(code, (0, 1), err)
        report = json.loads(out)
        return report["servers"]["stub"]["files"], sorted({row["file"] for row in report["diagnostics"]})

    def test_staged_worktree_and_commit_name_the_files_whatever_their_names(self):
        odd = ["naïve file.py", "café/日本.py"]
        for name in odd:
            self.file(name, "x  # E: odd name\n")
        self.file("notes.txt")
        (self.project / "removed.py").unlink()
        self.git("add", "-A")
        # Staged: both odd names, the text file left to no server, the deleted file left out.
        self.assertEqual(self.files("--staged"), (2, sorted(odd)))
        self.git("commit", "-qm", "second")
        self.assertEqual(self.files("--commit", "HEAD"), (2, sorted(odd)))
        self.assertEqual(self.files("--commit", "HEAD~1"), (1, ["kept.py"]))  # removed.py is gone from the tree
        self.assertEqual(self.files("--staged"), (0, []))
        # The working tree: a changed tracked file, and nothing for a file git does not track.
        self.file(odd[0], "x  # E: changed\n")
        self.file("untracked.py", "x  # E: new\n")
        self.assertEqual(self.files("--worktree"), (1, [odd[0]]))

    def test_changed_names_every_file_a_change_touched_staged_or_not_and_new(self):
        # Found by the round of D04 T01 section 7: --worktree leaves out what is staged and what is new, so a
        # check of "the change" with it could check nothing and pass.
        # A file that is committed and not touched: `t.py` is what is left of `kept.py` when the name a rename had
        # is cut as if it were an entry, so it is listed only if that name is not passed over.
        self.file("t.py", "x  # E: not part of the change\n")
        self.git("add", "t.py")
        self.git("commit", "-qm", "an untouched file")
        self.assertEqual(self.files("--changed"), (0, []))
        self.file("staged.py", "x  # E: staged\n")
        self.git("add", "staged.py")
        self.file("kept.py", "x  # E: changed and not staged\n")
        self.file("new file.py", "x  # E: new, not tracked\n")
        self.file("sub/caf\u00e9.py", "x  # E: new, in a new folder\n")
        (self.project / ".gitignore").write_text("ignored.py\n", encoding="utf-8")
        self.file("ignored.py", "x  # E: ignored\n")
        (self.project / "removed.py").unlink()
        self.git("mv", "kept.py", "moved.py")   # a rename: its new name, not the old one
        self.assertEqual(self.files("--changed"),
                         (4, ["moved.py", "new file.py", "staged.py", "sub/caf\u00e9.py"]))
        self.assertEqual(self.files("--worktree"), (1, ["moved.py"]))   # what the narrower forms see of it
        self.assertEqual(self.files("--staged")[0], 2)
        code, out, err = self.run_main("--all", "--changed", "--staged")
        self.assertEqual((code, out), (2, ""))
        self.assertIn("give one input: --changed, --staged, --worktree, --commit <sha>, or paths", err)

    def test_changed_works_in_a_repository_with_no_commit_yet(self):
        fresh = self.dir / "fresh"
        (fresh / ".conclave").mkdir(parents=True)
        (fresh / lsp_table.TABLE).write_bytes((self.project / lsp_table.TABLE).read_bytes())
        (fresh / "first.py").write_text("x  # E: before any commit\n", encoding="utf-8")
        subprocess.run(["git", "-C", str(fresh), "init", "-q"], check=True, capture_output=True,
                       creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
        out, err = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            code = lsp_diagnostics.main(["--all", "--changed", "--settle", "0.2", "--json"], root=fresh)
        self.assertEqual(code, 1, err.getvalue())
        self.assertEqual([row["file"] for row in json.loads(out.getvalue())["diagnostics"]], ["first.py"])

    def test_a_git_command_that_fails_is_refused_and_never_an_empty_input(self):
        self.assert_refused("git show failed: ", "--all", "--commit", "no-such-revision")
        shutil.rmtree(self.project / ".git", onerror=lambda function, path, _info: (os.chmod(path, 0o700),
                                                                                    function(path)))
        outside = unittest.mock.patch.dict(os.environ, {"GIT_CEILING_DIRECTORIES": str(self.dir)})
        with outside:
            self.assert_refused("git diff failed: ", "--all", "--staged")


if __name__ == "__main__":
    unittest.main()
