"""toolchain-check.py: the pin file's rules, version comparison, a tool's check and its bounds, the command line.

The tools are stubs on a scratch PATH: a launcher script on POSIX and a .cmd launcher on Windows, which is how npm
and Composer install theirs. No real build tool is run.
"""
import contextlib
import importlib.util
import io
import json
import os
import pathlib
import stat
import subprocess
import sys
import tempfile
import time
import unittest
import unittest.mock

SCRIPT = pathlib.Path(__file__).resolve().parents[1] / "toolchain-check.py"
spec = importlib.util.spec_from_file_location("toolchain_check", SCRIPT)
tc = importlib.util.module_from_spec(spec)
spec.loader.exec_module(tc)

COMMAND = {"command": "ninja", "version_args": ["--version"], "version_pattern": r"^(\d+(?:\.\d+)*)",
           "min_version": "1.10"}
TOOL = {"name": "ninja", "commands": [COMMAND], "install": "apt install ninja-build"}


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
        self.bin = self.dir / "bin"
        self.bin.mkdir()
        # Only the stubs are on PATH: a tool the host really has must not answer for one the test left out.
        env = {"PATH": str(self.bin)}
        if os.name == "nt":
            env["PATHEXT"] = ".CMD;.EXE"
        patch = unittest.mock.patch.dict(os.environ, env)
        patch.start()
        self.addCleanup(patch.stop)

    def pins(self, document):
        path = self.dir / "pins.json"
        path.write_text(document if isinstance(document, str) else json.dumps(document), encoding="utf-8")
        return path

    def stub(self, name, body):
        """A tool on the scratch PATH whose behaviour is the Python `body`; `{dir}` in it is the scratch folder."""
        code = self.dir / f"{name}-stub.py"
        code.write_text(body.replace("{dir}", self.dir.as_posix()), encoding="utf-8")
        if os.name == "nt":
            launcher = self.bin / f"{name}.cmd"
            launcher.write_text(f'@"{sys.executable}" "{code}" %*\r\n', encoding="utf-8")
        else:
            launcher = self.bin / name
            launcher.write_text(f'#!/bin/sh\nexec "{sys.executable}" "{code}" "$@"\n', encoding="utf-8")
            launcher.chmod(0o755)
        return launcher

    def prints(self, name, text, stream="stdout", code=0):
        return self.stub(name, f"import sys\nsys.{stream}.write({text!r})\nsys.exit({code})\n")

    def tool(self, name="ninja", commands=None, **extra):
        commands = commands or [dict(COMMAND, command=name)]
        return dict({"name": name, "commands": commands, "install": f"install {name} by hand", "optional": False},
                    **extra)


class LoadPinsTests(Fixture):
    def test_a_pin_file_loads_as_its_tools_in_order(self):
        cache = {"name": "compiler-cache", "optional": True, "install": "apt install ccache", "commands": [
            dict(COMMAND, command="ccache", version_pattern=r"ccache version ([0-9.]+)", min_version="4.0"),
            dict(COMMAND, command="sccache", version_pattern=r"sccache ([0-9.]+)", min_version="0.5")]}
        loaded = tc.load_pins(self.pins({"schema_version": 1, "tools": [TOOL, cache]}))
        self.assertEqual([tool["name"] for tool in loaded], ["ninja", "compiler-cache"])
        self.assertEqual(loaded[0], dict(TOOL, optional=False))  # optional is false when left out
        self.assertEqual(loaded[1]["optional"], True)
        self.assertEqual([c["min_version"] for c in loaded[1]["commands"]], ["4.0", "0.5"])
        self.assertEqual(tc.load_pins(self.pins({"schema_version": 1, "tools": []})), [])

    def test_a_file_that_cannot_be_read_as_a_pin_file_is_refused_naming_it(self):
        path = self.dir / "pins.json"
        with self.assertRaises(tc.PinError) as caught:
            tc.load_pins(path)  # missing
        self.assertTrue(str(caught.exception).startswith(f"{path.as_posix()}: cannot be read"), caught.exception)
        denied = PermissionError(13, "Permission denied")
        with unittest.mock.patch.object(pathlib.Path, "read_bytes", side_effect=denied):
            with self.assertRaises(tc.PinError) as caught:
                tc.load_pins(path)
        self.assertIn("cannot be read (Permission denied)", str(caught.exception))
        cases = {"not UTF-8": b"\xff\xfe{}", "not JSON": b"{", "an object": b"null", "an object ": b"[]",
                 "unknown key 'ccache'": json.dumps({"schema_version": 1, "tools": [], "ccache": {}}).encode(),
                 "missing key 'tools'": b'{"schema_version": 1}',
                 "schema_version: must be 1": b'{"schema_version": 2, "tools": []}',
                 "schema_version: must be 1 ": b'{"schema_version": true, "tools": []}',
                 "tools: a list": b'{"schema_version": 1, "tools": {}}',
                 "tool 1: a tool is an object": b'{"schema_version": 1, "tools": ["ninja"]}'}
        for expected, data in cases.items():
            with self.subTest(case=expected):
                path.write_bytes(data)
                with self.assertRaises(tc.PinError) as caught:
                    tc.load_pins(path)
                self.assertTrue(str(caught.exception).startswith(path.as_posix() + ": "), caught.exception)
                self.assertIn(expected.strip(), str(caught.exception))

    def test_each_fault_in_a_tool_is_refused_naming_the_tool_and_the_key(self):
        def tool(**changes):
            return dict(TOOL, **changes)

        def command(**changes):
            return tool(commands=[dict(COMMAND, **changes)])

        without = {key: value for key, value in COMMAND.items() if key != "min_version"}
        cases = [
            (tool(name="Ninja"), "tool 1: name:"),
            (tool(name=7), "tool 1: name:"),
            (tool(extra=1), "tool 'ninja': unknown key 'extra'"),
            ({key: value for key, value in TOOL.items() if key != "install"}, "tool 'ninja': missing key 'install'"),
            (tool(commands=[]), "tool 'ninja': commands:"),
            (tool(commands="ninja"), "tool 'ninja': commands:"),
            (tool(commands=["ninja"]), "tool 'ninja': a command is an object"),
            (command(command="./ninja"), "tool 'ninja': command: a bare program name"),
            (command(command="bin/ninja"), "tool 'ninja': command: a bare program name"),
            (command(command="C:\\tools\\ninja.exe"), "tool 'ninja': command: a bare program name"),
            (tool(commands=[dict(COMMAND, extra=1)]), "tool 'ninja': command 'ninja': unknown key 'extra'"),
            (tool(commands=[without]), "tool 'ninja': command 'ninja': missing key 'min_version'"),
            (command(version_args="--version"), "command 'ninja': version_args: a list of strings"),
            (command(version_args=[1]), "command 'ninja': version_args: a list of strings"),
            (command(version_args=["--version & calc"]), "command 'ninja': version_args:"),
            (command(version_args=["%PATH%"]), "command 'ninja': version_args:"),
            (command(version_args=[""]), "command 'ninja': version_args:"),
            (command(version_pattern="("), "command 'ninja': version_pattern: does not compile"),
            (command(version_pattern="version"), "command 'ninja': version_pattern: needs exactly one group, has 0"),
            (command(version_pattern="(v)(.*)"), "command 'ninja': version_pattern: needs exactly one group, has 2"),
            (command(version_pattern=None), "command 'ninja': version_pattern:"),
            (command(min_version="1.x"), "command 'ninja': min_version:"),
            (command(min_version="v1.2"), "command 'ninja': min_version:"),
            (command(min_version=1.2), "command 'ninja': min_version:"),
            # Found by the round of D04 T02 section 1: inputs the loader took and the check then choked on.
            (command(min_version="9" * 4301), "command 'ninja': min_version:"),
            (command(min_version=".".join(["1"] * 9)), "command 'ninja': min_version:"),
            (command(version_pattern="(?:" * 1000 + "(1)" + ")" * 1000), "command 'ninja': version_pattern:"),
            (command(version_pattern="(1)" + "a" * 600), "command 'ninja': version_pattern: longer than"),
            (tool(install="one\ntwo"), "tool 'ninja': install: a one-line hint"),
            (tool(install=""), "tool 'ninja': install: a one-line hint"),
            (tool(optional="yes"), "tool 'ninja': optional: true or false"),
            (tool(optional=1), "tool 'ninja': optional: true or false"),
        ]
        for entry, expected in cases:
            with self.subTest(expected=expected, entry=str(entry)[:60]):
                path = self.pins({"schema_version": 1, "tools": [entry]})
                with self.assertRaises(tc.PinError) as caught:
                    tc.load_pins(path)
                self.assertTrue(str(caught.exception).startswith(path.as_posix() + ": "), caught.exception)
                self.assertIn(expected, str(caught.exception))
        path = self.pins({"schema_version": 1, "tools": [TOOL, TOOL]})
        with self.assertRaises(tc.PinError) as caught:
            tc.load_pins(path)
        self.assertIn("tool 'ninja': name: used by two tools", str(caught.exception))


class VersionTests(unittest.TestCase):
    def test_versions_compare_number_by_number(self):
        self.assertEqual(tc.compare_versions("1.10", "1.9"), 1)  # as strings, 1.10 sorts before 1.9
        self.assertEqual(tc.compare_versions("1.9", "1.10"), -1)
        self.assertEqual(tc.compare_versions("1.2", "1.2.0"), 0)  # a missing trailing part is zero
        self.assertEqual(tc.compare_versions("1.2.0", "1.2"), 0)
        self.assertEqual(tc.compare_versions("1.2.1", "1.2"), 1)
        self.assertEqual(tc.compare_versions("2", "2.0.1"), -1)
        self.assertEqual(tc.compare_versions("10.0", "9.99.99"), 1)
        self.assertEqual(tc.compare_versions("19", "19"), 0)

    def test_only_dotted_numbers_are_a_version(self):
        self.assertEqual(tc.parse_version("19.1.7"), (19, 1, 7))
        for text in ("unknown", "", "1.", ".1", "1..2", "1.2-rc1", "v1.2", " 1.2", None, 12):
            with self.subTest(text=text):
                self.assertIsNone(tc.parse_version(text))
        with self.assertRaises(ValueError):
            tc.compare_versions("unknown", "1.0")


class CheckToolTests(Fixture):
    def test_an_installed_tool_new_enough_is_ok_with_its_version_and_path(self):
        launcher = self.prints("ninja", "1.11.1\n")
        result = tc.check_tool(self.tool())
        self.assertEqual(result, {"name": "ninja", "status": "ok", "detail": result["command"], "version": "1.11.1",
                                  "command": result["command"], "optional": False})
        self.assertTrue(os.path.samefile(result["command"], launcher))
        self.prints("ninja", "1.10\n")
        self.assertEqual(tc.check_tool(self.tool())["status"], "ok")  # the pinned version itself is new enough

    def test_a_version_printed_on_standard_error_is_read(self):
        self.prints("ninja", "ninja 1.12.0\n", stream="stderr")
        result = tc.check_tool(self.tool(commands=[dict(COMMAND, version_pattern=r"ninja ([0-9.]+)")]))
        self.assertEqual((result["status"], result["version"]), ("ok", "1.12.0"))

    def test_the_arguments_of_the_pin_reach_the_tool(self):
        self.stub("ninja", "import sys\nprint('1.11' if sys.argv[1:] == ['--version', '-q'] else 'usage')\n")
        result = tc.check_tool(self.tool(commands=[dict(COMMAND, version_args=["--version", "-q"])]))
        self.assertEqual((result["status"], result["version"]), ("ok", "1.11"))

    def test_each_command_is_held_to_its_own_minimum(self):
        commands = [dict(COMMAND, command="ccache", version_pattern=r"ccache version ([0-9.]+)", min_version="4.0"),
                    dict(COMMAND, command="sccache", version_pattern=r"sccache ([0-9.]+)", min_version="0.5")]
        pin = self.tool("compiler-cache", commands)
        self.prints("sccache", "sccache 0.8.1\n")
        result = tc.check_tool(pin)  # 0.8.1 is older than ccache's 4.0 and newer than sccache's 0.5
        self.assertEqual((result["status"], result["version"]), ("ok", "0.8.1"))
        self.assertIn("sccache", os.path.basename(result["command"]))
        # The first command found is the one checked: an old ccache is reported, sccache is not tried in its place.
        self.prints("ccache", "ccache version 3.7.7\n")
        result = tc.check_tool(pin)
        self.assertEqual((result["status"], result["version"]), ("setup_required", "3.7.7"))
        self.assertIn("is 3.7.7, older than the pinned 4.0. Install: install compiler-cache by hand", result["detail"])
        self.assertIn("ccache", os.path.basename(result["command"]))

    def test_a_tool_that_is_not_installed_names_its_commands_and_the_hint(self):
        commands = [dict(COMMAND, command="mold"), dict(COMMAND, command="ld.lld")]
        result = tc.check_tool(self.tool("linker", commands, optional=True))
        self.assertEqual(result, {"name": "linker", "status": "setup_required", "version": None, "command": None,
                                  "optional": True,
                                  "detail": "not installed (mold, ld.lld not on PATH). "
                                            "Install: install linker by hand"})

    def test_a_version_below_the_pin_names_both_versions(self):
        self.prints("ninja", "1.9.0\n")
        result = tc.check_tool(self.tool())
        self.assertEqual((result["status"], result["version"]), ("setup_required", "1.9.0"))
        self.assertIn("is 1.9.0, older than the pinned 1.10. Install: install ninja by hand", result["detail"])

    def test_output_the_pattern_cannot_read_is_not_a_version(self):
        self.prints("ninja", "ninja, a small build system\n")
        result = tc.check_tool(self.tool())
        self.assertEqual((result["status"], result["version"]), ("setup_required", None))
        self.assertIn("does not match its output", result["detail"])
        self.prints("ninja", "version unknown\n")
        for pattern in (r"version (\w+)", r"version(?: (\d+))? unknown"):  # text, and a group that took no part
            with self.subTest(pattern=pattern):
                result = tc.check_tool(self.tool(commands=[dict(COMMAND, version_pattern=pattern)]))
                self.assertEqual((result["status"], result["version"]), ("setup_required", None))
                self.assertIn("which is not a version", result["detail"])

    def test_a_tool_that_exits_non_zero_is_not_ok_whatever_it_printed(self):
        for text in ("", "1.11.1\n"):
            with self.subTest(printed=text):
                self.prints("ninja", text, code=3)
                result = tc.check_tool(self.tool())
                self.assertEqual((result["status"], result["version"]), ("setup_required", None))
                self.assertTrue(result["detail"].endswith("--version exited 3"), result["detail"])

    def test_a_file_on_path_that_cannot_be_started_names_the_systems_reason(self):
        if os.name == "nt":
            (self.bin / "ninja.exe").write_bytes(b"not a program")
        else:
            broken = self.bin / "ninja"
            broken.write_text("#!/nonexistent/interpreter\n", encoding="utf-8")
            broken.chmod(broken.stat().st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)
        result = tc.check_tool(self.tool())
        self.assertEqual(result["status"], "setup_required")
        self.assertIn("could not be started: ", result["detail"])
        self.assertGreater(len(result["detail"].split("could not be started: ")[1]), 3)


CHILD = ("import subprocess, sys, time\n"
         "child = subprocess.Popen([sys.executable, '-c', 'import time; time.sleep(120)'])\n"
         "open(r'{dir}/pids', 'w').write(f'{os.getpid()} {child.pid}')\n")


class BoundTests(Fixture):
    def pids(self):
        deadline = time.monotonic() + 10
        path = self.dir / "pids"
        while not (path.is_file() and len(path.read_text().split()) == 2):
            self.assertLess(time.monotonic(), deadline, "the stub never wrote its process ids")
            time.sleep(0.05)
        return [int(pid) for pid in path.read_text().split()]

    def test_a_tool_that_never_answers_is_stopped_with_everything_it_started(self):
        self.stub("ninja", "import os\n" + CHILD + "time.sleep(120)\n")
        started = time.monotonic()
        result = tc.check_tool(self.tool(), time_limit=3)
        elapsed = time.monotonic() - started
        self.assertEqual(result["status"], "setup_required")
        self.assertIn("did not answer within 3 seconds and was stopped", result["detail"])
        self.assertLess(elapsed, 13, "the check outlived its limit")
        for pid in self.pids():
            self.assertTrue(gone(pid), f"process {pid} outlived the check")

    def test_a_tool_that_floods_its_output_is_stopped_at_the_cap_with_everything_it_started(self):
        # The stub's own bound: 64 MiB, far past the cap and nowhere near a disk.
        self.stub("ninja", "import os, sys\n" + CHILD
                  + "for _ in range(65536):\n    sys.stdout.write('x' * 1023 + '\\n')\n    sys.stdout.flush()\n"
                  + "time.sleep(120)\n")
        started = time.monotonic()
        result = tc.check_tool(self.tool(), time_limit=60, output_cap=4096)
        elapsed = time.monotonic() - started
        self.assertEqual(result["status"], "setup_required")
        self.assertIn("printed more than 4096 bytes and was stopped", result["detail"])
        self.assertLess(elapsed, 30, "the check read to the end of the flood, or waited out the limit")
        for pid in self.pids():
            self.assertTrue(gone(pid), f"process {pid} outlived the check")

    def test_what_is_kept_of_the_output_stops_at_the_cap(self):
        self.stub("ninja", "import sys\nsys.stdout.write('y' * 100000)\n")
        ran = tc.run_bounded([str(self.bin / ("ninja.cmd" if os.name == "nt" else "ninja"))], 30, 1000)
        self.assertEqual((ran["stopped"], len(ran["output"])), ("output", 1000))

    def test_a_child_that_outlives_its_launcher_and_holds_the_output_is_ended_at_the_limit(self):
        # Found by the completion review of D04 T02 section 1: the launcher exits, a child it started keeps the
        # output open, and nothing names the child any more.
        self.stub("ninja", "import os, subprocess, sys, time\n"
                           "child = subprocess.Popen([sys.executable, '-c', 'import time; time.sleep(120)'],\n"
                           "                         stdout=sys.stdout, stderr=sys.stderr)\n"
                           "open(r'{dir}/pids', 'w').write(f'{os.getpid()} {child.pid}')\n"
                           "print('1.11.1')\n")
        started = time.monotonic()
        result = tc.check_tool(self.tool(), time_limit=3)
        self.assertLess(time.monotonic() - started, 13, "the check outlived its limit")
        self.assertEqual(result["status"], "setup_required")
        self.assertIn("did not answer within 3 seconds and was stopped", result["detail"])
        for pid in self.pids():
            self.assertTrue(gone(pid), f"process {pid} outlived the check")

    def test_a_silent_child_left_by_a_tool_that_answered_is_ended_too(self):
        # Same review: the tool prints its version and exits 0, leaving a child that holds nothing open.
        self.stub("ninja", "import os, subprocess, sys\n"
                           "child = subprocess.Popen([sys.executable, '-c', 'import time; time.sleep(120)'],\n"
                           "                         stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL,\n"
                           "                         stderr=subprocess.DEVNULL)\n"
                           "open(r'{dir}/pids', 'w').write(f'{os.getpid()} {child.pid}')\n"
                           "print('1.11.1')\n")
        result = tc.check_tool(self.tool(), time_limit=30)
        self.assertEqual((result["status"], result["version"]), ("ok", "1.11.1"))
        for pid in self.pids():
            self.assertTrue(gone(pid), f"process {pid} outlived the check")

    @unittest.skipUnless(os.name == "nt", "on POSIX the session exists before the program is executed")
    def test_a_child_started_at_once_cannot_be_made_before_the_tool_is_contained(self):
        # Found by the reconciliation of D04 T02 section 1: on Windows a tool put in its job after it started could
        # already have a child outside the job. Here the job is made late on purpose: the tool must not have run.
        real = tc._windows_job

        def late(proc):
            time.sleep(1.5)
            return real(proc)

        self.stub("ninja", "import os, subprocess, sys\n"
                           "child = subprocess.Popen([sys.executable, '-c', 'import time; time.sleep(120)'],\n"
                           "                         stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL,\n"
                           "                         stderr=subprocess.DEVNULL)\n"
                           "open(r'{dir}/pids', 'w').write(f'{os.getpid()} {child.pid}')\n"
                           "print('1.11.1')\n")
        with unittest.mock.patch.object(tc, "_windows_job", late):
            started = time.monotonic()
            result = tc.check_tool(self.tool(), time_limit=30)
        self.assertGreater(time.monotonic() - started, 1.4)
        self.assertEqual((result["status"], result["version"]), ("ok", "1.11.1"))
        for pid in self.pids():
            self.assertTrue(gone(pid), f"process {pid} was made before the job and outlived the check")

    RAN_MARK = "open(r'{dir}/ran', 'w').close()\nprint('1.11.1')\n"

    @unittest.skipUnless(os.name == "nt", "the job object is how Windows contains a tool")
    def test_a_tool_that_cannot_be_contained_is_never_run(self):
        # Found by the second reconciliation of D04 T02 section 1: with no job the tool was resumed all the same.
        self.stub("ninja", self.RAN_MARK)
        started = []
        real_resume = tc._windows_resume
        for name, patches in (("no job", {"_windows_job": lambda proc: started.append(proc.pid)}),
                              ("no resume", {"_windows_resume": lambda proc: started.append(proc.pid) or False})):
            with self.subTest(case=name):
                with contextlib.ExitStack() as stack:
                    for attribute, replacement in patches.items():
                        stack.enter_context(unittest.mock.patch.object(tc, attribute, replacement))
                    result = tc.check_tool(self.tool(), time_limit=30)
                self.assertEqual(result["status"], "setup_required")
                self.assertIn("could not be started: Windows did not let the program be", result["detail"])
                self.assertIn("so it was not run", result["detail"])
                self.assertFalse((self.dir / "ran").exists(), "the tool ran outside a job")
                self.assertTrue(gone(started[-1]), "the suspended process was left behind")
        self.assertIs(tc._windows_resume, real_resume)
        self.assertEqual(tc.check_tool(self.tool())["status"], "ok")  # and it runs once it can be contained
        self.assertTrue((self.dir / "ran").exists())

    @unittest.skipUnless(os.name == "nt", "the job object is how Windows contains a tool")
    def test_an_interrupt_while_the_tool_is_being_contained_leaves_nothing(self):
        # Same reconciliation: the job and the resume ran outside the cleanup.
        self.stub("ninja", self.RAN_MARK)
        started = []

        def interrupted(proc):
            started.append(proc.pid)
            raise KeyboardInterrupt

        for attribute in ("_windows_job", "_windows_resume"):
            with self.subTest(interrupted=attribute):
                with unittest.mock.patch.object(tc, attribute, interrupted):
                    with self.assertRaises(KeyboardInterrupt):
                        tc.check_tool(self.tool(), time_limit=30)
                self.assertTrue(gone(started[-1]), "the process outlived the interrupted check")
                self.assertFalse((self.dir / "ran").exists())

    def test_a_pattern_that_never_finishes_is_stopped_and_the_next_tool_is_checked(self):
        # Found by the round of D04 T02 section 1: the pattern ran outside every limit. `(?:a+)+([0-9])$` on a line
        # of a's with no digit backtracks for hours.
        self.prints("ninja", "a" * 64 + "!\n")
        self.prints("clangd", "clangd version 19.1.7\n")
        slow = self.tool(commands=[dict(COMMAND, version_pattern=r"(?:a+)+([0-9])$")])
        fine = self.tool("clangd", [dict(COMMAND, command="clangd", version_pattern=r"version ([0-9.]+)",
                                         min_version="15")])
        started = time.monotonic()
        with unittest.mock.patch.object(tc, "PATTERN_LIMIT", 2.0):
            results = tc.check_tools([slow, fine])
        self.assertLess(time.monotonic() - started, 30, "the pattern ran past its limit")
        self.assertEqual([result["status"] for result in results], ["setup_required", "ok"])
        self.assertIn("did not finish reading its output within 2 seconds", results[0]["detail"])

    def test_an_interrupted_check_leaves_no_tool_running(self):
        # Same round: Ctrl+C while a tool is being waited on skipped the cleanup, and the tool has its own session.
        self.stub("ninja", "import os\n" + CHILD + "time.sleep(120)\n")
        real = time.monotonic
        calls = []

        def interrupted():
            calls.append(1)
            if len(calls) > 3 and (self.dir / "pids").is_file() and len((self.dir / "pids").read_text().split()) == 2:
                raise KeyboardInterrupt
            return real()

        with unittest.mock.patch.object(tc.time, "monotonic", interrupted):
            with self.assertRaises(KeyboardInterrupt):
                tc.check_tool(self.tool(), time_limit=60)
        for pid in self.pids():
            self.assertTrue(gone(pid), f"process {pid} outlived the interrupted check")

    def test_a_capture_too_long_to_be_a_version_is_not_one(self):
        self.prints("ninja", "9" * 5000 + "\n")
        result = tc.check_tool(self.tool(commands=[dict(COMMAND, version_pattern=r"^(\d+)")]))
        self.assertEqual((result["status"], result["version"]), ("setup_required", None))
        self.assertIn("which is not a version", result["detail"])
        self.assertLess(len(result["detail"]), 400)


class CommandLineTests(Fixture):
    def run_script(self, *argv):
        return subprocess.run([sys.executable, "-B", str(SCRIPT), *argv], capture_output=True, text=True, timeout=120,
                              creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))

    def document(self, *tools):
        return self.pins({"schema_version": 1, "tools": list(tools)})

    def test_every_tool_is_reported_and_a_missing_required_one_exits_3(self):
        self.prints("clangd", "clangd version 19.1.7\n")
        pins = self.document(
            {"name": "ninja", "commands": [COMMAND], "install": "apt install ninja-build"},
            {"name": "clangd", "install": "apt install clangd", "commands": [
                dict(COMMAND, command="clangd", version_pattern=r"clangd version ([0-9.]+)", min_version="15")]})
        ran = self.run_script("--pins", pins, "--json")
        self.assertEqual(ran.returncode, 3, ran.stderr)
        report = json.loads(ran.stdout)
        self.assertEqual(report["pins"], pins.as_posix())
        self.assertEqual(report["required_not_ok"], 1)
        self.assertEqual([(tool["name"], tool["status"]) for tool in report["tools"]],
                         [("ninja", "setup_required"), ("clangd", "ok")])  # the tool after the failure is checked
        self.assertEqual(set(report["tools"][1]), {"name", "status", "detail", "version", "command", "optional"})
        self.assertEqual(report["tools"][1]["version"], "19.1.7")
        text = self.run_script("--pins", pins)
        self.assertEqual(text.returncode, 3)
        lines = text.stdout.splitlines()
        self.assertEqual(len(lines), 2)
        self.assertTrue(lines[0].startswith("ninja  setup_required  not installed (ninja not on PATH)"), lines[0])
        self.assertTrue(lines[1].startswith("clangd  ok  "), lines[1])

    def test_a_missing_optional_tool_is_shown_and_exits_0(self):
        pins = self.document({"name": "ninja", "commands": [COMMAND], "install": "apt install ninja-build",
                              "optional": True})
        ran = self.run_script("--pins", pins, "--json")
        self.assertEqual(ran.returncode, 0, ran.stderr)
        report = json.loads(ran.stdout)
        self.assertEqual((report["required_not_ok"], report["tools"][0]["status"]), (0, "setup_required"))

    def test_the_text_line_says_optional_for_an_optional_tool_that_is_not_ok(self):
        # Found 2026-11-15 on a real project: a missing optional tool was printed `setup_required`, the word of a
        # required one, and read as setup the project needs.
        self.prints("old", "1.2\n")
        self.prints("fine", "1.12.1\n")
        pins = self.document(
            {"name": "missing-optional", "commands": [dict(COMMAND, command="nowhere")], "install": "install it",
             "optional": True},
            {"name": "old-optional", "commands": [dict(COMMAND, command="old")], "install": "install it",
             "optional": True},
            {"name": "fine-optional", "commands": [dict(COMMAND, command="fine")], "install": "install it",
             "optional": True},
            {"name": "fine-required", "commands": [dict(COMMAND, command="fine")], "install": "install it"})
        text = self.run_script("--pins", pins)
        self.assertEqual(text.returncode, 0, text.stderr)   # only optional tools are not ok
        words = [line.split("  ")[:2] for line in text.stdout.splitlines()]
        self.assertEqual(words, [["missing-optional", "optional"], ["old-optional", "optional"],
                                 ["fine-optional", "ok"], ["fine-required", "ok"]])
        self.assertIn("not installed", text.stdout.splitlines()[0])
        self.assertIn("1.2", text.stdout.splitlines()[1])
        # The JSON document of the same pins keeps the two facts apart, as before.
        report = json.loads(self.run_script("--pins", pins, "--json").stdout)
        self.assertEqual([(tool["status"], tool["optional"]) for tool in report["tools"]],
                         [("setup_required", True), ("setup_required", True), ("ok", True), ("ok", False)])
        self.assertEqual(report["required_not_ok"], 0)
        # A required tool that is not ok keeps its word, beside an optional one.
        pins = self.document(
            {"name": "missing-required", "commands": [dict(COMMAND, command="nowhere")], "install": "install it"},
            {"name": "missing-optional", "commands": [dict(COMMAND, command="nowhere")], "install": "install it",
             "optional": True})
        text = self.run_script("--pins", pins)
        self.assertEqual(text.returncode, 3)
        self.assertEqual([line.split("  ")[:2] for line in text.stdout.splitlines()],
                         [["missing-required", "setup_required"], ["missing-optional", "optional"]])

    def test_a_pin_file_that_does_not_load_is_one_line_and_exit_2(self):
        for name, data in (("malformed", '{"schema_version": 1, "tools": [{"name": "x"}]}'), ("missing", None)):
            with self.subTest(case=name):
                path = self.dir / f"{name}.json"
                if data is not None:
                    path.write_text(data, encoding="utf-8")
                ran = self.run_script("--pins", path, "--json")
                self.assertEqual((ran.returncode, ran.stdout), (2, ""))
                self.assertEqual(len(ran.stderr.splitlines()), 1, ran.stderr)
                self.assertTrue(ran.stderr.startswith(f"toolchain-check: {path.as_posix()}: "), ran.stderr)

    def snapshot(self):
        return {path.relative_to(self.dir).as_posix(): path.read_bytes()
                for path in self.dir.rglob("*") if path.is_file()}

    def test_the_check_writes_nothing(self):
        self.prints("ninja", "1.11.1\n")
        pins = self.document({"name": "ninja", "commands": [COMMAND], "install": "apt install ninja-build"})
        before = self.snapshot()
        self.assertEqual(self.run_script("--pins", pins).returncode, 0)
        self.assertEqual(self.snapshot(), before)


class SearchFolderTests(Fixture):
    """D04 T02 section 6: a project's own tool folders (`vendor/bin`, `node_modules/.bin`), and a lookup that never
    reads the working directory."""

    def local_tool(self, folder, name, text):
        """A tool in a folder of the project, as Composer and npm install them: a launcher beside its script."""
        folder.mkdir(parents=True, exist_ok=True)
        code = folder / f"{name}-stub.py"
        code.write_text(f"import sys\nsys.stdout.write({text!r})\n", encoding="utf-8")
        if os.name == "nt":
            launcher = folder / f"{name}.bat"
            launcher.write_text(f'@"{sys.executable}" "{code}" %*\r\n', encoding="utf-8")
        else:
            launcher = folder / name
            launcher.write_text(f'#!/bin/sh\nexec "{sys.executable}" "{code}" "$@"\n', encoding="utf-8")
            launcher.chmod(0o755)
        return launcher

    def setUp(self):
        super().setUp()
        if os.name == "nt":
            os.environ["PATHEXT"] = ".CMD;.EXE;.BAT"  # restored with PATH by the fixture's patch
        self.project = self.dir / "project"
        self.project.mkdir()

    def document(self, **top):
        return {"schema_version": 1, "tools": [self.tool("phpstan", [dict(COMMAND, command="phpstan",
                                                                         version_pattern=r"Tool ([0-9.]+)",
                                                                         min_version="1.10")])], **top}

    def test_a_pin_file_names_the_apps_folder_and_its_tool_folders(self):
        loaded = tc.load_pin_file(self.pins(self.document(root="platform", search=["vendor/bin", "node_modules/.bin"])))
        self.assertEqual((loaded["root"], loaded["search"]), ("platform", ["vendor/bin", "node_modules/.bin"]))
        self.assertEqual([tool["name"] for tool in loaded["tools"]], ["phpstan"])
        plain = tc.load_pin_file(self.pins(self.document()))
        self.assertEqual((plain["root"], plain["search"]), (".", []))
        self.assertEqual(tc.load_pins(self.pins(self.document(search=["vendor/bin"]))), loaded["tools"])
        self.assertEqual(tc.search_folders(self.project, loaded),
                         [self.project / "platform/vendor/bin", self.project / "platform/node_modules/.bin"])

    def test_a_root_or_a_search_folder_outside_the_project_is_refused(self):
        cases = [({"root": "/srv/app"}, "root:"), ({"root": "../app"}, "root:"), ({"root": "C:/app"}, "root:"),
                 ({"root": 7}, "root:"), ({"root": "app\\src"}, "root:"),
                 ({"search": "vendor/bin"}, "search: a list"), ({"search": ["/usr/bin"]}, "search: a folder"),
                 ({"search": ["../bin"]}, "search: a folder"), ({"search": [3]}, "search: a folder"),
                 ({"search": ["."]}, "search: a folder"), ({"extra": 1}, "unknown key 'extra'")]
        for top, expected in cases:
            with self.subTest(top=top):
                path = self.pins(self.document(**top))
                with self.assertRaises(tc.PinError) as caught:
                    tc.load_pin_file(path)
                self.assertTrue(str(caught.exception).startswith(path.as_posix() + ": " + expected), caught.exception)

    def test_a_tool_is_found_in_the_projects_folder_before_path(self):
        pin_file = tc.load_pin_file(self.pins(self.document(search=["vendor/bin"])))
        folders = tc.search_folders(self.project, pin_file)
        (pin,) = pin_file["tools"]
        nowhere = tc.check_tool(pin, folders=folders)
        self.assertEqual(nowhere["status"], "setup_required")
        self.assertIn("not installed (phpstan not in the project's tool folders or on PATH). Install:",
                      nowhere["detail"])
        self.prints("phpstan", "PHPStan - PHP Static Analysis Tool 1.11.0\n")  # on PATH
        on_path = tc.check_tool(pin, folders=folders)
        self.assertEqual((on_path["status"], on_path["version"]), ("ok", "1.11.0"))
        local = self.local_tool(self.project / "vendor/bin", "phpstan", "PHPStan - PHP Static Analysis Tool 2.3.5\n")
        found = tc.check_tool(pin, folders=folders)  # the project's own copy wins
        self.assertEqual((found["status"], found["version"]), ("ok", "2.3.5"))
        self.assertTrue(os.path.samefile(found["command"], local))

    def test_a_nested_apps_tools_are_found_from_an_unrelated_working_directory(self):
        pins = self.pins(self.document(root="platform", search=["vendor/bin"]))
        self.local_tool(self.project / "platform/vendor/bin", "phpstan", "PHPStan - PHP Static Analysis Tool 2.3.5\n")
        elsewhere = self.dir / "elsewhere"
        elsewhere.mkdir()
        ran = subprocess.run([sys.executable, "-B", str(SCRIPT), "--pins", str(pins), "--root", str(self.project),
                              "--json"], capture_output=True, text=True, timeout=120, cwd=elsewhere,
                             creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
        self.assertEqual(ran.returncode, 0, ran.stderr)
        self.assertEqual(json.loads(ran.stdout)["tools"][0]["version"], "2.3.5")
        # Without --root the folders are read under the working directory, which holds no such tool.
        ran = subprocess.run([sys.executable, "-B", str(SCRIPT), "--pins", str(pins), "--json"], capture_output=True,
                             text=True, timeout=120, cwd=elsewhere,
                             creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
        self.assertEqual((ran.returncode, json.loads(ran.stdout)["tools"][0]["status"]), (3, "setup_required"))

    def test_a_file_in_the_working_directory_named_like_a_pinned_tool_is_never_run(self):
        # Found while adding the search folders: the standard lookup tries the working directory first on Windows,
        # so a file there named like a pinned tool was started as that tool.
        marker = self.dir / "ran"
        here = self.dir / "here"
        here.mkdir()
        code = here / "stub.py"
        code.write_text(f"open(r'{marker.as_posix()}', 'w').close()\nprint('1.11.1')\n", encoding="utf-8")
        if os.name == "nt":
            (here / "ninja.cmd").write_text(f'@"{sys.executable}" "{code}" %*\r\n', encoding="utf-8")
        else:
            (here / "ninja").write_text(f'#!/bin/sh\nexec "{sys.executable}" "{code}" "$@"\n', encoding="utf-8")
            (here / "ninja").chmod(0o755)
        held = os.getcwd()
        os.chdir(here)
        try:
            self.assertIsNone(tc.find_command("ninja"))
            result = tc.check_tool(self.tool())
        finally:
            os.chdir(held)
        self.assertEqual(result["status"], "setup_required")
        self.assertIn("not installed", result["detail"])
        self.assertFalse(marker.exists(), "a file in the working directory was run as the pinned tool")

    @unittest.skipIf(os.name == "nt", "Windows has no execute permission to withhold")
    def test_a_file_the_system_will_not_execute_is_not_the_tool(self):
        local = self.local_tool(self.project / "vendor/bin", "phpstan", "PHPStan - PHP Static Analysis Tool 2.3.5\n")
        local.chmod(0o644)
        self.assertIsNone(tc.find_command("phpstan", [self.project / "vendor/bin"]))


class CompdbKeyTests(Fixture):
    """D04 T02 section 3: the pin file's `compdb`, which compdb-check.py and the doctor read."""

    def document(self, **top):
        return {"schema_version": 1, "tools": [self.tool()], **top}

    def test_a_pin_file_without_compdb_has_the_defaults_and_one_with_it_still_checks_its_tools(self):
        plain = tc.load_pin_file(self.pins(self.document()))
        self.assertEqual(plain["compdb"], {"path": "compile_commands.json", "build_files": None, "regenerate": []})
        self.assertEqual(tc.load_pin_file(self.pins(self.document(compdb={})))["compdb"], plain["compdb"])
        compdb = {"path": "build/compile_commands.json", "build_files": ["CMakeLists.txt", "src/flags.cmake"],
                  "regenerate": ["cmake", "-S", ".", "-B", "build"]}
        path = self.pins(self.document(compdb=compdb))
        loaded = tc.load_pin_file(path)
        self.assertEqual(loaded["compdb"], compdb)
        self.assertEqual(loaded["tools"], plain["tools"])
        self.prints("ninja", "1.11.1\n")
        (tool,) = tc.check_tools(tc.load_pins(path))
        self.assertEqual((tool["name"], tool["status"], tool["version"]), ("ninja", "ok", "1.11.1"))

    def test_a_compdb_the_format_does_not_allow_is_refused_naming_the_file_and_the_key(self):
        cases = [("build", "an object with the optional keys path, build_files, regenerate"),
                 ({"run": ["make"]}, "unknown key 'run'"),
                 ({"path": "../compile_commands.json"}, "path: a file inside the project"),
                 ({"path": "/srv/compile_commands.json"}, "path: a file inside the project"),
                 ({"path": "C:/build/compile_commands.json"}, "path: a file inside the project"),
                 ({"path": "build\\compile_commands.json"}, "path: a file inside the project"),
                 ({"path": "."}, "path: a file inside the project"),
                 ({"path": 3}, "path: a file inside the project"),
                 ({"build_files": []}, "build_files: a list of at least one file"),
                 ({"build_files": "Makefile"}, "build_files: a list of at least one file"),
                 ({"build_files": ["Makefile", "../flags.mk"]},
                  "build_files: a file inside the project, not '../flags.mk'"),
                 ({"build_files": [7]}, "build_files: a file inside the project, not 7"),
                 ({"regenerate": []}, "regenerate: a command as a list of at least one argument"),
                 ({"regenerate": "make compdb"}, "regenerate: a command as a list of at least one argument"),
                 ({"regenerate": ["make", 3]}, "regenerate: each argument is a one-line string, not 3"),
                 ({"regenerate": ["make", ""]}, "regenerate: each argument is a one-line string, not ''"),
                 ({"regenerate": ["make\nrm"]}, "regenerate: each argument is a one-line string")]
        for compdb, expected in cases:
            with self.subTest(compdb=compdb):
                path = self.pins(self.document(compdb=compdb))
                with self.assertRaises(tc.PinError) as caught:
                    tc.load_pin_file(path)
                self.assertTrue(str(caught.exception).startswith(f"{path.as_posix()}: compdb: {expected}"),
                                caught.exception)


class InProcessTests(Fixture):
    def test_main_prints_the_same_report_in_process(self):
        self.prints("ninja", "1.11.1\n")
        pins = self.pins({"schema_version": 1, "tools": [TOOL]})
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            code = tc.main(["--pins", str(pins), "--json"])
        self.assertEqual(code, 0)
        self.assertEqual(json.loads(out.getvalue())["tools"][0]["status"], "ok")


if __name__ == "__main__":
    unittest.main()
