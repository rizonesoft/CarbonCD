"""compdb-check.py: each status of a compile database, which file is inspected, the build files it is compared
with, its output and exit codes, and that it never starts a process."""
import contextlib
import importlib.util
import io
import json
import os
import pathlib
import subprocess
import sys
import tempfile
import unittest
import unittest.mock

SCRIPT = pathlib.Path(__file__).resolve().parents[1] / "compdb-check.py"
spec = importlib.util.spec_from_file_location("compdb_check", SCRIPT)
cc = importlib.util.module_from_spec(spec)
spec.loader.exec_module(cc)

THEN = 1_700_000_000_000_000_000   # the time every fixture file is given, in nanoseconds
LATER = THEN + 5_000_000_000


class Fixture(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.dir = pathlib.Path(tmp.name).resolve()
        self.project = self.dir / "project"
        self.elsewhere = self.dir / "elsewhere"   # a working directory that is not the project
        for folder in (self.project / ".conclave", self.elsewhere):
            folder.mkdir(parents=True)
        self.pins()
        cwd = os.getcwd()
        os.chdir(self.elsewhere)
        self.addCleanup(os.chdir, cwd)

    def pins(self, **top):
        """The project's pin file: no tool, and whatever top-level keys are given (`root`, `compdb`)."""
        path = self.project / ".conclave/toolchain-cpp.json"
        path.write_text(json.dumps({"schema_version": 1, "tools": [], **top}), encoding="utf-8")
        return path

    def file(self, rel, text="int main(void) { return 0; }\n", when=THEN):
        path = self.project / rel
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")
        os.utime(path, ns=(when, when))
        return path

    def entry(self, file, directory=None, **keys):
        entry = {"directory": str(self.project) if directory is None else directory, "file": file}
        entry.update(keys or {"command": f"cc -c {file}"})
        return entry

    def database(self, entries=None, rel="compile_commands.json", when=THEN):
        """A compile database of two entries, one with `command` and one with `arguments`, whose sources exist."""
        if entries is None:
            self.file("src/a.c")
            self.file("src/b.c")
            entries = [self.entry("src/a.c"), self.entry("src/b.c", arguments=["cc", "-c", "src/b.c"])]
        text = entries if isinstance(entries, str) else json.dumps(entries)
        return self.file(rel, text, when)

    def run_main(self, *argv, root=True):
        out, err = io.StringIO(), io.StringIO()
        argv = [*(["--root", str(self.project)] if root else []), *[str(arg) for arg in argv]]
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            code = cc.main(argv)
        return code, out.getvalue(), err.getvalue()

    def both(self, *argv):
        """The check as text and as JSON: (exit, status, detail, the JSON document). The two forms must agree."""
        code, text, err = self.run_main(*argv)
        json_code, document, _err = self.run_main(*argv, "--json")
        report = json.loads(document)
        self.assertEqual((json_code, err), (code, ""))
        first = text.splitlines()[0]
        self.assertEqual(first, f"compile database: {report['status']}: {report['detail']}")
        self.assertEqual(set(report), {"path", "status", "detail", "entries", "newer", "gone", "unlisted",
                                       "regenerate"})
        self.assertEqual(code, 0 if report["status"] == "current" else 1)
        return code, report["status"], report["detail"], report


class StatusTests(Fixture):
    def test_a_valid_database_is_current_with_its_entry_count(self):
        self.database()
        code, status, detail, report = self.both()
        self.assertEqual((code, status, detail), (0, "current", "compile_commands.json, 2 entries"))
        self.assertEqual(report, {"path": "compile_commands.json", "status": "current", "detail": detail, "entries": 2,
                                  "newer": [], "gone": [], "unlisted": [], "regenerate": []})
        self.file("one.c")
        self.database([self.entry("one.c")])
        self.assertEqual(self.both()[2], "compile_commands.json, 1 entry")

    def test_no_file_at_the_path_is_missing(self):
        code, status, detail, report = self.both()
        self.assertEqual((code, status, detail), (1, "missing", "compile_commands.json does not exist"))
        self.assertIsNone(report["entries"])

    def test_a_database_that_cannot_be_read_or_parsed_is_invalid_and_never_a_traceback(self):
        (self.project / "compile_commands.json").mkdir()
        code, status, detail, _report = self.both()
        self.assertEqual((code, status), (1, "invalid"))
        self.assertTrue(detail.startswith("compile_commands.json cannot be read ("), detail)
        (self.project / "compile_commands.json").rmdir()
        cases = [("not json {", "compile_commands.json is not JSON ("),
                 ('{"file": "a.c"}', "compile_commands.json is not a JSON list of entries"),
                 ("[]", "compile_commands.json holds no entry, so it describes no build"),
                 ("[" * 100000, "compile_commands.json is not JSON (")]
        for text, expected in cases:
            with self.subTest(text=text[:20]):
                self.database(text)
                code, status, detail, _report = self.both()
                self.assertEqual((code, status), (1, "invalid"))
                self.assertTrue(detail.startswith(expected), detail)
        (self.project / "compile_commands.json").write_bytes(b"[\xff\xfe]")
        self.assertTrue(self.both()[2].startswith("compile_commands.json is not UTF-8 ("))

    def test_an_entry_that_is_not_one_is_invalid_and_named_by_its_number(self):
        self.file("a.c")
        good = self.entry("a.c")
        cases = [("text", "not an object"),
                 ({"directory": str(self.project), "command": "cc -c a.c"}, "no `file`"),
                 ({"file": "a.c", "command": "cc -c a.c"}, "no `directory`"),
                 (self.entry("a.c", directory=7), "`directory` is not a non-empty string"),
                 (self.entry("a.c", directory=""), "`directory` is not a non-empty string"),
                 (self.entry(""), "`file` is not a non-empty string"),
                 (self.entry(["a.c"]), "`file` is not a non-empty string"),
                 ({"directory": str(self.project), "file": "a.c"}, "neither `command` nor `arguments`"),
                 (self.entry("a.c", command=""), "`command` is not a command line"),
                 (self.entry("a.c", command="   "), "`command` is not a command line"),
                 (self.entry("a.c", command=["cc"]), "`command` is not a command line"),
                 (self.entry("a.c", arguments=[]), "`arguments` is not a list of strings that starts with the compiler"),
                 (self.entry("a.c", arguments=[""]), "`arguments` is not a list of strings that starts with the compiler"),
                 (self.entry("a.c", arguments=["cc", 3]),
                  "`arguments` is not a list of strings that starts with the compiler"),
                 (self.entry("a.c", arguments="cc -c a.c"),
                  "`arguments` is not a list of strings that starts with the compiler"),
                 (self.entry("a.c", command="cc -c a.c", arguments=[]),
                  "`arguments` is not a list of strings that starts with the compiler")]
        for bad, fault in cases:
            with self.subTest(fault=fault, entry=bad):
                self.database([good, good, bad, good])
                code, status, detail, report = self.both()
                self.assertEqual((code, status, detail), (1, "invalid", f"compile_commands.json: entry 3: {fault}"))
                self.assertIsNone(report["entries"])
        self.assertIsNone(cc.entry_fault(self.entry("a.c", command="cc -c a.c", arguments=["cc", "-c", "a.c"])))


class WhichFileTests(Fixture):
    def test_the_database_read_is_the_one_under_the_pin_files_root_and_path(self):
        self.pins(root="engine", compdb={"path": "build/compile_commands.json"})
        self.file("engine/src/a.c")
        self.database([self.entry("src/a.c", directory=str(self.project / "engine"))] * 3,
                      rel="engine/build/compile_commands.json")
        # Decoys: at the project root, in the app's folder, and in the working directory.
        self.database("not json", rel="compile_commands.json")
        self.database("[]", rel="engine/compile_commands.json")
        (self.elsewhere / "compile_commands.json").write_text("{}", encoding="utf-8")
        code, status, detail, report = self.both()
        self.assertEqual((code, status, detail), (0, "current", "engine/build/compile_commands.json, 3 entries"))
        self.assertEqual(report["path"], "engine/build/compile_commands.json")

    def test_path_overrides_the_pin_file_and_is_read_from_the_project_root(self):
        self.pins(root="engine", compdb={"path": "build/compile_commands.json"})
        self.file("engine/src/a.c")
        self.database([self.entry("src/a.c", directory=str(self.project / "engine"))],
                      rel="engine/build/compile_commands.json")
        self.database("[]", rel="out/other.json")
        code, status, detail, _report = self.both("--path", "out/other.json")
        self.assertEqual((code, status, detail), (1, "invalid", "out/other.json holds no entry, so it describes no build"))
        self.assertEqual(self.both("--path", "out/absent.json")[1:3], ("missing", "out/absent.json does not exist"))

    def test_without_root_the_project_is_the_working_directory_and_pins_names_another_pin_file(self):
        self.database()
        os.chdir(self.project)
        self.assertEqual(self.run_main(root=False)[:2], (0, "compile database: current: compile_commands.json, 2 entries\n"))
        other = self.dir / "other-pins.json"
        other.write_text(json.dumps({"schema_version": 1, "tools": [], "compdb": {"path": "build/db.json"}}),
                         encoding="utf-8")
        self.assertEqual(self.run_main("--pins", other)[:2], (1, "compile database: missing: build/db.json does not exist\n"))

    def test_the_script_runs_as_a_program_from_an_installed_layout(self):
        scripts = self.project / "scripts"
        scripts.mkdir()
        for name in ("compdb-check.py", "toolchain-check.py"):
            (scripts / name).write_bytes((SCRIPT.parent / name).read_bytes())
        self.database()
        ran = subprocess.run([sys.executable, "-B", str(scripts / "compdb-check.py"), "--json"], capture_output=True,
                             text=True, timeout=120, cwd=self.project,
                             creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
        self.assertEqual(ran.returncode, 0, ran.stderr)
        self.assertEqual(json.loads(ran.stdout)["entries"], 2)


class FreshnessTests(Fixture):
    def test_a_usual_build_file_newer_than_the_database_makes_it_stale_and_is_named(self):
        self.database()
        self.file("Makefile", "all:\n", when=THEN)
        self.file("CMakeLists.txt", "project(x)\n", when=THEN - 1)
        self.assertEqual(self.both()[1], "current")   # older, or of the same time
        self.file("Makefile", "all:\n", when=LATER)
        code, status, detail, report = self.both()
        self.assertEqual((code, status, detail), (1, "stale", "compile_commands.json: older than Makefile"))
        self.assertEqual(report["newer"], ["Makefile"])
        self.assertEqual(report["entries"], 2)

    def test_a_build_file_beside_a_database_in_a_build_folder_is_compared_too(self):
        self.pins(compdb={"path": "build/compile_commands.json"})
        self.database(rel="build/compile_commands.json")
        self.assertEqual(self.both()[1], "current")
        self.file("build/build.ninja", "rule cc\n", when=LATER)
        self.assertEqual(self.both()[1:3], ("stale", "build/compile_commands.json: older than build/build.ninja"))

    def test_the_pin_files_build_files_replace_the_usual_ones(self):
        self.pins(compdb={"build_files": ["src/CMakeLists.txt", "flags.mk"]})
        self.database()
        self.file("src/CMakeLists.txt", "add_library(x)\n")
        self.file("flags.mk", "CFLAGS=-O2\n")
        self.file("Makefile", "all:\n", when=LATER)   # no longer one of the build files
        self.assertEqual(self.both()[1], "current")
        self.file("flags.mk", "CFLAGS=-O3\n", when=LATER)
        self.assertEqual(self.both()[1:3], ("stale", "compile_commands.json: older than flags.mk"))
        self.file("src/CMakeLists.txt", "add_library(y)\n", when=LATER)
        self.assertEqual(self.both()[3]["newer"], ["src/CMakeLists.txt", "flags.mk"])

    def test_the_pin_files_build_files_are_read_under_its_root(self):
        self.pins(root="engine", compdb={"build_files": ["flags.mk"]})
        self.file("engine/a.c")
        self.database([self.entry("a.c", directory=str(self.project / "engine"))], rel="engine/compile_commands.json")
        self.file("flags.mk", "decoy\n", when=LATER)
        self.file("engine/flags.mk", "CFLAGS=\n")
        self.assertEqual(self.both()[1], "current")
        self.file("engine/flags.mk", "CFLAGS=-g\n", when=LATER)
        self.assertEqual(self.both()[2], "engine/compile_commands.json: older than engine/flags.mk")

    def test_build_file_on_the_command_line_replaces_both(self):
        self.pins(compdb={"build_files": ["flags.mk"]})
        self.database()
        self.file("flags.mk", "CFLAGS=\n", when=LATER)
        self.file("Makefile", "all:\n", when=LATER)
        self.file("config/toolchain.cmake", "set(x 1)\n")
        self.file("config/options.cmake", "set(y 1)\n")
        argv = ("--build-file", "config/toolchain.cmake", "--build-file", "config/options.cmake")
        self.assertEqual(self.both(*argv)[1], "current")
        self.file("config/options.cmake", "set(y 2)\n", when=LATER)
        self.assertEqual(self.both(*argv)[2], "compile_commands.json: older than config/options.cmake")

    def test_a_named_build_file_that_is_absent_makes_the_freshness_unknown(self):
        self.database()
        code, status, detail, report = self.both("--build-file", "absent.mk")
        self.assertEqual((code, status), (1, "unknown"))
        self.assertEqual(detail, "compile_commands.json: a build file named by --build-file cannot be read, so its "
                                 "freshness cannot be told: absent.mk (it does not exist)")
        self.assertEqual((report["entries"], report["newer"]), (2, []))
        self.pins(compdb={"build_files": ["Makefile", "gone.mk"]})
        self.file("Makefile", "all:\n")
        self.assertEqual(self.both()[2], "compile_commands.json: a build file named by the pin file cannot be read, "
                                         "so its freshness cannot be told: gone.mk (it does not exist)")
        # What is certain sets the status, and what could not be read is still named.
        self.file("Makefile", "all:\n", when=LATER)
        self.assertEqual(self.both()[1:3], ("stale", "compile_commands.json: older than Makefile; a build file named "
                                                     "by the pin file cannot be read: gone.mk (it does not exist)"))
        # A usual build file the project does not have is no fault.
        self.pins()
        (self.project / "Makefile").unlink()
        self.assertEqual(self.both()[1], "current")

    def test_a_usual_build_file_whose_time_cannot_be_read_makes_the_freshness_unknown(self):
        # Found by the review round: only an absent usual build file may be passed over. One the system will not
        # give the time of (a link into a folder the user cannot enter) says nothing about freshness.
        self.database()
        self.file("Makefile", "all:\n")
        real = pathlib.Path.stat

        def stat(path, *args, **kwargs):
            if path.name == "Makefile":
                raise PermissionError(13, "Permission denied")
            return real(path, *args, **kwargs)

        with unittest.mock.patch.object(pathlib.Path, "stat", stat):
            code, status, detail, _report = self.both()
        self.assertEqual((code, status), (1, "unknown"))
        self.assertEqual(detail, "compile_commands.json: a build file of the project cannot be read, so its "
                                 "freshness cannot be told: Makefile (Permission denied)")

    def test_a_source_file_the_system_will_not_show_makes_the_freshness_unknown_and_never_raises(self):
        # Found by the completion review: `Path.is_file` raises PermissionError, which left the check as a
        # traceback with no JSON document.
        self.file("open.c")
        self.database([self.entry("open.c"), self.entry("locked/a.c"), self.entry("locked/b.c"),
                       self.entry("locked/a.c")])
        real = pathlib.Path.stat

        def stat(path, *args, **kwargs):
            if path.parent.name == "locked":
                raise PermissionError(13, "Permission denied")
            return real(path, *args, **kwargs)

        with unittest.mock.patch.object(pathlib.Path, "stat", stat):
            code, status, detail, report = self.both()
            self.assertEqual((code, status), (1, "unknown"))
            self.assertEqual(detail, "compile_commands.json: 2 source file(s) it names cannot be inspected, so its "
                                     "freshness cannot be told: locked/a.c (Permission denied), locked/b.c "
                                     "(Permission denied)")
            self.assertEqual((report["entries"], report["gone"]), (4, []))
            # Found by the reconciliation: what is certain sets the status, and a file that could not be
            # inspected is still named with its reason, beside a deleted source and beside a newer build file.
            self.database([self.entry("open.c"), self.entry("locked/a.c"), self.entry("deleted.c")])
            hidden = "1 source file(s) it names cannot be inspected: locked/a.c (Permission denied)"
            self.assertEqual(self.both()[1:3], ("stale", "compile_commands.json: 1 source file(s) it names no "
                                                         f"longer exist: deleted.c; {hidden}"))
            self.database([self.entry("open.c"), self.entry("locked/a.c")])
            self.file("Makefile", "all:\n", when=LATER)
            self.assertEqual(self.both()[1:3], ("stale", f"compile_commands.json: older than Makefile; {hidden}"))
            (self.project / "Makefile").unlink()
        # A folder where a source file is named is not that file, and a name no system can hold is gone.
        (self.project / "folder.c").mkdir()
        self.database([self.entry("open.c"), self.entry("folder.c"), self.entry("nul\x00byte.c")])
        self.assertEqual(self.both()[3]["gone"], ["folder.c", "nul\x00byte.c"])

    def test_a_database_larger_than_the_check_reads_is_unknown_and_never_loaded(self):
        # Found by the review round: the database is the project's file, of any size.
        self.database()
        pin_file = cc._pins_module().load_pin_file(self.project / ".conclave/toolchain-cpp.json")
        size = (self.project / "compile_commands.json").stat().st_size
        self.assertEqual(cc.check_database(self.project, pin_file, read_limit=size)["status"], "current")
        read = []

        class Counting:
            """The file, noting how many bytes each read asks for."""

            def __init__(self, *args, **kwargs):
                self.handle = open(*args, **kwargs)

            def __enter__(self):
                return self

            def __exit__(self, *exc):
                self.handle.close()

            def read(self, count=-1):
                read.append(count)
                return self.handle.read(count)

        with unittest.mock.patch.object(cc, "open", Counting, create=True):
            result = cc.check_database(self.project, pin_file, read_limit=size - 1)
        self.assertEqual((result["status"], result["entries"]), ("unknown", None))
        self.assertEqual(result["detail"], f"compile_commands.json is larger than the {size - 1} bytes this check "
                                           "reads, so it was not checked")
        self.assertEqual(read, [size], "the check asked for more than its limit and one byte")
        self.assertEqual(cc.READ_LIMIT, 256 * 1024 * 1024)

    def test_a_source_file_that_no_longer_exists_makes_it_stale_and_is_named(self):
        self.file("src/kept.c")
        self.database([self.entry("src/kept.c"), self.entry("src/deleted.c"), self.entry("src/deleted.c")])
        code, status, detail, report = self.both()
        self.assertEqual((code, status), (1, "stale"))
        self.assertEqual(detail, "compile_commands.json: 1 source file(s) it names no longer exist: src/deleted.c")
        self.assertEqual(report["gone"], ["src/deleted.c"])

    def test_a_relative_file_is_read_from_its_directory_and_a_relative_directory_from_the_databases_folder(self):
        self.pins(compdb={"path": "build/compile_commands.json"})
        self.file("src/a.c")
        self.file("build/gen/b.c")
        entries = [self.entry("a.c", directory=str(self.project / "src")),   # an absolute directory
                   self.entry("b.c", directory="gen"),                       # relative to build/
                   self.entry("../src/a.c", directory="."),
                   self.entry(str(self.project / "src/a.c"), directory="nowhere")]   # an absolute file
        self.database(entries, rel="build/compile_commands.json")
        self.assertEqual(self.both()[1:3], ("current", "build/compile_commands.json, 4 entries"))
        # The same names read from the project root would not be found.
        self.database([self.entry("b.c", directory="src")], rel="build/compile_commands.json")
        self.assertEqual(self.both()[2], "build/compile_commands.json: 1 source file(s) it names no longer exist: "
                                         "build/src/b.c")

    def test_at_most_twenty_deleted_files_are_named_and_the_rest_counted(self):
        self.database([self.entry(f"src/gone{number:02}.c") for number in range(25)])
        code, status, detail, report = self.both()
        self.assertEqual((code, status), (1, "stale"))
        names = [f"src/gone{number:02}.c" for number in range(20)]
        self.assertEqual(detail, "compile_commands.json: 25 source file(s) it names no longer exist: "
                                 + ", ".join(names) + " and 5 more")
        self.assertEqual(report["gone"], names)

    def test_a_newer_build_file_and_a_deleted_source_are_both_reported(self):
        self.file("a.c")
        self.database([self.entry("a.c"), self.entry("b.c")])
        self.file("Makefile", "all:\n", when=LATER)
        self.assertEqual(self.both()[2], "compile_commands.json: older than Makefile; 1 source file(s) it names no "
                                         "longer exist: b.c")


class CommandLineTests(Fixture):
    def test_a_source_asked_for_must_have_an_entry_whatever_the_build_files_say(self):
        # A build that finds its sources by itself: a new source changes no build file.
        self.file("Makefile", "SRC := $(wildcard src/*.c)\n", when=THEN - 10)
        self.database()
        self.file("src/new.c")
        code, status, detail, report = self.both()
        self.assertEqual((code, status, report["unlisted"]), (0, "current", []))
        code, status, detail, report = self.both("--source", "src/new.c")
        self.assertEqual((code, status), (1, "stale"))
        self.assertEqual(detail, "compile_commands.json: 1 source file(s) asked for have no entry in it: src/new.c")
        self.assertEqual(report["unlisted"], ["src/new.c"])
        # One that is listed and one that is not, each asked for once however often it is named.
        code, status, detail, report = self.both("--source", "src/a.c", "--source", "src/new.c", "--source",
                                                 "src/new.c")
        self.assertEqual((status, report["unlisted"]), ("stale", ["src/new.c"]))
        # Regenerated: the database has its entry.
        self.database([self.entry("src/a.c"), self.entry("src/b.c"), self.entry("src/new.c")])
        self.assertEqual(self.both("--source", "src/new.c")[:2], (0, "current"))
        self.assertEqual(self.both("--source", "src/./new.c", "--source", "src/../src/a.c")[:2], (0, "current"))

    def test_a_source_is_matched_through_the_entrys_directory(self):
        self.file("src/a.c")
        self.file("lib/b.c")
        self.database([self.entry("a.c", directory=str(self.project / "src")),
                       self.entry("../lib/b.c", directory="src")])
        self.assertEqual(self.both("--source", "src/a.c", "--source", "lib/b.c")[:2], (0, "current"))
        code, status, detail, report = self.both("--source", "a.c")
        self.assertEqual((status, report["unlisted"]), ("stale", ["a.c"]))
        # It is one more certain fault beside the others, and each is named.
        self.file("Makefile", when=THEN + 10**9)
        detail = self.both("--source", "lib/c.c")[2]
        self.assertEqual(detail, "compile_commands.json: older than Makefile; 1 source file(s) asked for have no "
                                 "entry in it: lib/c.c")

    def test_every_source_asked_for_without_an_entry_is_named_however_many(self):
        # Found by the completion review: the 21st and later were counted, not named.
        self.database()
        names = [f"src/new{number:02d}.c" for number in range(1, 24)]
        for name in names:
            self.file(name)
        arguments = [part for name in names for part in ("--source", name)]
        code, status, detail, report = self.both(*arguments)
        self.assertEqual((code, status), (1, "stale"))
        self.assertEqual(report["unlisted"], names)
        self.assertEqual(detail, "compile_commands.json: 23 source file(s) asked for have no entry in it: "
                                 + ", ".join(names))
        self.assertNotIn("more", detail)

    def test_a_source_reached_through_a_link_is_the_same_file(self):
        # Found by the review round: a build that names its sources through a linked folder.
        self.file("src/a.c")
        try:
            os.symlink(self.project / "src", self.project / "src-link", target_is_directory=True)
        except (OSError, NotImplementedError) as exc:
            self.skipTest(f"this system will not make a link here ({exc})")
        self.database([self.entry("src-link/a.c")])
        self.assertEqual(self.both("--source", "src/a.c")[:2], (0, "current"))
        self.assertEqual(self.both("--source", "src-link/a.c")[:2], (0, "current"))
        self.database([self.entry("src/a.c")])
        self.assertEqual(self.both("--source", "src-link/a.c")[:2], (0, "current"))
        self.file("src/b.c")
        code, status, _detail, report = self.both("--source", "src-link/b.c")
        self.assertEqual((status, report["unlisted"]), ("stale", ["src-link/b.c"]))

    def test_a_source_outside_the_project_is_bad_usage(self):
        self.database()
        for source in ("../elsewhere/x.c", str(self.elsewhere / "x.c"), "."):
            with self.subTest(source=source):
                out, err = io.StringIO(), io.StringIO()
                with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err), \
                        self.assertRaises(SystemExit) as caught:
                    cc.main(["--root", str(self.project), "--source", source])
                self.assertEqual((caught.exception.code, out.getvalue()), (2, ""))
                self.assertIn("is not a file inside the project", err.getvalue())

    def test_a_pin_file_that_does_not_load_is_refused_in_both_forms(self):
        self.database()
        (self.project / ".conclave/toolchain-cpp.json").write_text(
            json.dumps({"schema_version": 1, "tools": [], "compdb": {"path": "../db.json"}}), encoding="utf-8")
        where = (self.project / ".conclave/toolchain-cpp.json").as_posix()
        expected = f"{where}: compdb: path: a file inside the project, not '../db.json'"
        code, out, err = self.run_main()
        self.assertEqual((code, out, err), (2, "", f"compdb-check: {expected}\n"))
        code, out, err = self.run_main("--json")
        self.assertEqual((code, err), (2, f"compdb-check: {expected}\n"))
        self.assertEqual(json.loads(out), {"path": None, "status": "refused", "detail": expected, "entries": None,
                                           "newer": [], "gone": [], "unlisted": [], "regenerate": []})
        (self.project / ".conclave/toolchain-cpp.json").unlink()
        code, out, err = self.run_main()
        self.assertEqual((code, out), (2, ""))
        self.assertTrue(err.startswith(f"compdb-check: {where}: cannot be read ("), err)

    def test_the_regenerate_command_is_printed_for_a_database_that_is_not_current_and_nothing_is_run(self):
        self.pins(compdb={"regenerate": ["cmake", "-S", ".", "-B", "build dir"]})

        def started(*args, **kwargs):
            raise AssertionError(f"the check started a process: {args!r}")

        names = ["system", "posix_spawn", "posix_spawnp", "startfile", "popen",
                 *[name for name in dir(os) if name.startswith(("spawn", "exec"))]]
        with contextlib.ExitStack() as stack:
            stack.enter_context(unittest.mock.patch.object(subprocess, "Popen", started))
            for name in names:
                if hasattr(os, name):
                    stack.enter_context(unittest.mock.patch.object(os, name, started))
            line = ("regenerate with (PowerShell): cmake -S '.' -B 'build dir'\n" if os.name == "nt"
                    else "regenerate with (sh): cmake -S . -B 'build dir'\n")
            # Missing.
            code, out, _err = self.run_main()
            self.assertEqual((code, out), (1, "compile database: missing: compile_commands.json does not exist\n" + line))
            # Stale.
            self.database()
            self.file("Makefile", "all:\n", when=LATER)
            code, out, _err = self.run_main()
            self.assertEqual((code, out), (1, "compile database: stale: compile_commands.json: older than Makefile\n"
                                           + line))
            report = json.loads(self.run_main("--json")[1])
            self.assertEqual(report["regenerate"], ["cmake", "-S", ".", "-B", "build dir"])
            # Current: the command is not offered.
            (self.project / "Makefile").unlink()
            code, out, _err = self.run_main()
            self.assertEqual((code, out), (0, "compile database: current: compile_commands.json, 2 entries\n"))
        # The database was not written either.
        self.assertEqual((self.project / "compile_commands.json").stat().st_mtime_ns, THEN)

    def test_the_regenerate_command_is_quoted_for_the_shell_it_names(self):
        # Found by the review round: sh quoting pasted into PowerShell is a parser error for a quoted program.
        program = "C:/Program Files/CMake/bin/cmake.exe"
        arguments = [program, "-S", ".", "-B", "build", "-DVERSION=1.2", "it's"]
        self.assertEqual(cc.shell_line(arguments, windows=True),
                         ("PowerShell", "& 'C:/Program Files/CMake/bin/cmake.exe' -S '.' -B build "
                                        "'-DVERSION=1.2' 'it''s'"))
        self.assertEqual(cc.shell_line(arguments, windows=False),
                         ("sh", "'C:/Program Files/CMake/bin/cmake.exe' -S . -B build -DVERSION=1.2 'it'\"'\"'s'"))
        self.assertEqual(cc.shell_line(["make", "compdb"], windows=True), ("PowerShell", "make compdb"))
        self.assertEqual(cc.shell_line(["tools\\gen", "build/out"], windows=True),
                         ("PowerShell", "tools\\gen build/out"))
        self.assertEqual(cc.shell_line(["tools\\gen.exe"], windows=True), ("PowerShell", "& 'tools\\gen.exe'"))
        stale = {"status": "stale", "regenerate": ["make", "compdb"]}
        self.assertEqual(cc.regenerate_line(stale, windows=True), "regenerate with (PowerShell): make compdb")
        self.assertEqual(cc.regenerate_line(stale, windows=False), "regenerate with (sh): make compdb")
        self.assertIsNone(cc.regenerate_line({"status": "current", "regenerate": ["make"]}))
        self.assertIsNone(cc.regenerate_line({"status": "stale", "regenerate": []}))


if __name__ == "__main__":
    unittest.main()
