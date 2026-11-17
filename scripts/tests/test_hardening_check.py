"""hardening-check.py: a project's declared hardening held to its compile commands (D04 T03 section 1) and to its
built binaries (section 2): how each entry's flags are read, which entries are the project's own, what the PE and ELF
readers report for real linker output, what is unknown, the output and the exit codes."""
import contextlib
import gzip
import importlib.util
import io
import json
import os
import pathlib
import struct
import subprocess
import sys
import tempfile
import unittest
import unittest.mock

SCRIPTS = pathlib.Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("hardening_check", SCRIPTS / "hardening-check.py")
hc = importlib.util.module_from_spec(spec)
spec.loader.exec_module(hc)

THEN = 1_700_000_000_000_000_000   # the time every fixture file is given, in nanoseconds
LATER = THEN + 5_000_000_000
STL = "_MSVC_STL_HARDENING=1"


class Fixture(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.dir = pathlib.Path(tmp.name).resolve()
        self.project = self.dir / "project"
        (self.project / ".conclave").mkdir(parents=True)
        cwd = os.getcwd()
        os.chdir(self.dir)
        self.addCleanup(os.chdir, cwd)

    def pins(self, hardening=None, **top):
        document = {"schema_version": 1, "tools": [], **top}
        if hardening is not None:
            document["hardening"] = hardening
        path = self.project / ".conclave/toolchain-cpp.json"
        path.write_text(json.dumps(document), encoding="utf-8")
        return path

    def file(self, rel, text="int main(void) { return 0; }\n", when=THEN, data=None):
        path = self.project / rel
        path.parent.mkdir(parents=True, exist_ok=True)
        if data is not None:
            path.write_bytes(data)
        else:
            path.write_text(text, encoding="utf-8")
        os.utime(path, ns=(when, when))
        return path

    def entry(self, source, flags, compiler="c++", command=False):
        """An entry compiling `source` (created) with `flags`, as `arguments` or as one `command` string."""
        self.file(source)
        argv = [compiler, *flags, "-c", source]
        entry = {"directory": str(self.project), "file": source}
        entry.update({"command": " ".join(argv)} if command else {"arguments": argv})
        return entry

    def database(self, entries, when=THEN):
        return self.file("compile_commands.json", entries if isinstance(entries, str) else json.dumps(entries), when)

    def run_main(self, *argv):
        out, err = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            code = hc.main(["--root", str(self.project), *[str(arg) for arg in argv]])
        return code, out.getvalue(), err.getvalue()

    def both(self):
        """The check as text and as JSON: (exit, status, the text, the JSON document). The two must agree."""
        code, text, err = self.run_main()
        json_code, document, _err = self.run_main("--json")
        whole = json.loads(document)
        self.assertEqual(set(whole), {"policy", "status", "compile", "image"})
        report = dict(whole["compile"], policy=whole["policy"], image=whole["image"])
        self.assertEqual((json_code, err), (code, ""))
        self.assertEqual(text.splitlines()[0], f"hardening:compile: {report['status']}: {report['detail']}")
        self.assertIn(f"hardening:image: {whole['image']['status']}: {whole['image']['detail']}", text.splitlines())
        self.assertEqual(code, {"ok": 0, "none": 0, "unknown": 1, "missing": 3}[whole["status"]])
        if whole["image"]["status"] == "none":
            self.assertEqual(whole["status"], report["status"])
        return code, report["status"], text, report

    def policy(self, *required, **keys):
        self.pins({"compile": list(required), **keys})


class PolicyTests(Fixture):
    def test_a_project_with_no_policy_gets_no_finding(self):
        self.pins()
        code, status, text, report = self.both()
        self.assertEqual((code, status, report["policy"]), (0, "none", None))
        self.assertIn("no policy declared", text)
        self.assertEqual(report["requirements"], [])

    def test_json_prints_the_loaded_policy(self):
        self.policy(STL, "/guard:cf", exclude=["third_party"])
        self.database([self.entry("src/a.cpp", [f"/D{STL}", "/guard:cf"], compiler="cl.exe")])
        code, status, _text, report = self.both()
        self.assertEqual((code, status), (0, "ok"))
        self.assertEqual(report["policy"], {"compile": [STL, "/guard:cf"], "sources": ["."], "exclude": ["third_party"],
                                            "binaries": [], "image": [], "test_binaries": []})

    def test_a_policy_that_does_not_load_exits_2_with_the_loaders_refusal(self):
        path = self.pins({"compile": [STL, 7]})
        code, out, err = self.run_main()
        self.assertEqual((code, out), (2, ""))
        self.assertEqual(err.strip(), f"hardening-check: {path.as_posix()}: hardening: compile: each entry is a "
                                      "definition or option on one line without spaces around it, not 7")
        code, out, _err = self.run_main("--json")
        self.assertEqual((code, json.loads(out)["status"]), (2, "refused"))


class MatchTests(Fixture):
    def test_every_entry_hardened_passes_with_one_line_per_requirement(self):
        self.policy(STL, "/guard:cf")
        self.database([self.entry(f"src/{name}.cpp", [f"/D{STL}", "/guard:cf"], compiler="cl.exe")
                       for name in ("a", "b", "c")])
        code, status, text, report = self.both()
        self.assertEqual((code, status), (0, "ok"))
        self.assertEqual(report["checked"], 3)
        self.assertEqual(text.splitlines()[1:3], [f"  ok       {STL}", "  ok       /guard:cf"])

    def test_one_unhardened_first_party_source_exits_3_naming_the_requirement_and_the_source(self):
        # The other entries carry it, so a search over the whole file would pass.
        self.policy(STL)
        entries = [self.entry("src/a.cpp", [f"/D{STL}"], compiler="cl.exe"),
                   self.entry("src/b.cpp", [], compiler="cl.exe"),
                   self.entry("src/c.cpp", [f"/D{STL}"], compiler="cl.exe")]
        self.database(entries)
        code, status, text, report = self.both()
        self.assertEqual((code, status), (3, "missing"))
        self.assertEqual(report["requirements"], [{"requirement": STL, "status": "missing", "lacking": 1,
                                                   "first": "src/b.cpp"}])
        self.assertIn(f"  missing  {STL}: on 1 entry, first src/b.cpp", text)

    def test_the_last_occurrence_on_a_command_wins(self):
        pairs = [("_GLIBCXX_ASSERTIONS", "g++", ["-D_GLIBCXX_ASSERTIONS"], ["-U_GLIBCXX_ASSERTIONS"]),
                 ("_GLIBCXX_ASSERTIONS", "g++", ["-D", "_GLIBCXX_ASSERTIONS"], ["-U", "_GLIBCXX_ASSERTIONS"]),
                 ("_MSVC_STL_HARDENING", "cl.exe", ["/D_MSVC_STL_HARDENING=1"], ["/U_MSVC_STL_HARDENING"]),
                 ("_MSVC_STL_HARDENING", "cl.exe", ["/D", "_MSVC_STL_HARDENING=1"], ["/U", "_MSVC_STL_HARDENING"]),
                 (STL, "cl.exe", [f"/D{STL}"], ["/D_MSVC_STL_HARDENING=0"]),
                 ("-fstack-protector-strong", "g++", ["-fstack-protector-strong"], ["-fno-stack-protector"]),
                 ("-fstack-protector-strong", "clang++", ["-fstack-protector-strong"], ["-fstack-protector"]),
                 ("-fsanitize=address", "clang++", ["-fsanitize=address"], ["-fno-sanitize=address"]),
                 ("-fcf-protection=full", "gcc", ["-fcf-protection"], ["-fcf-protection=none"]),
                 ("-fcf-protection", "gcc", ["-fcf-protection=full"], ["-fno-cf-protection"]),
                 ("/guard:cf", "cl.exe", ["/guard:cf"], ["/guard:cf-"]),
                 ("-ftrivial-auto-var-init=zero", "clang++", ["-ftrivial-auto-var-init=zero"],
                  ["-ftrivial-auto-var-init=uninitialized"])]
        for requirement, compiler, on, off in pairs:
            for flags, expected in ((on + off, 3), (off + on, 0)):
                with self.subTest(requirement=requirement, flags=flags):
                    self.policy(requirement)
                    self.database([self.entry("src/a.cpp", flags, compiler=compiler)])
                    self.assertEqual(self.both()[0], expected)

    def test_only_what_the_compiler_itself_reads_counts(self):
        # Round 1 of the review: what the linker, the assembler or an option's own value takes is not a flag.
        cases = [("/guard:cf", "cl.exe", ["/c", "src/a.cpp", "/link", "/guard:cf"], 3),
                 ("/guard:cf", "cl.exe", ["/guard:cf", "/c", "src/a.cpp", "/link", "/guard:cf-"], 0),
                 ("_GLIBCXX_ASSERTIONS", "clang++", ["-Xlinker", "-D_GLIBCXX_ASSERTIONS"], 3),
                 ("_GLIBCXX_ASSERTIONS", "clang++", ["-Xclang", "-D_GLIBCXX_ASSERTIONS"], 3),
                 ("-fstack-protector-strong", "g++", ["-Wl,-fstack-protector-strong"], 3),
                 ("_GLIBCXX_ASSERTIONS", "g++", ["-o", "-D_GLIBCXX_ASSERTIONS"], 3),
                 ("_GLIBCXX_ASSERTIONS", "g++", ["-I", "inc", "-D", "_GLIBCXX_ASSERTIONS"], 0)]
        for requirement, compiler, flags, expected in cases:
            with self.subTest(requirement=requirement, flags=flags):
                self.policy(requirement)
                self.database([self.entry("src/a.cpp", flags, compiler=compiler)])
                self.assertEqual(self.both()[0], expected)

    def test_option_families_as_the_compilers_read_them(self):
        cases = [("-fstack-protector-strong", ["-fstack-protector-all"], 0),   # a stronger level meets it
                 ("-fstack-protector-strong", ["-fstack-protector"], 3),
                 ("-fstack-protector", ["-fstack-protector-strong"], 0),
                 ("-fstack-protector", ["-fstack-protector-explicit"], 3),
                 ("-fsanitize=address", ["-fsanitize=address", "-fsanitize=undefined"], 0),   # values add up
                 ("-fsanitize=address,undefined", ["-fsanitize=undefined", "-fsanitize=address"], 0),
                 ("-fsanitize=address,undefined", ["-fsanitize=address,undefined", "-fno-sanitize=undefined"], 3),
                 ("-ftrivial-auto-var-init=zero", ["-ftrivial-auto-var-init=zero", "-O2"], 0)]
        for requirement, flags, expected in cases:
            with self.subTest(requirement=requirement, flags=flags):
                self.policy(requirement)
                self.database([self.entry("src/a.cpp", flags, compiler="clang++")])
                self.assertEqual(self.both()[0], expected)

    def test_each_spelling_of_a_definition_and_an_option(self):
        cases = [("_GLIBCXX_ASSERTIONS", "g++", ["-D_GLIBCXX_ASSERTIONS=1"], 0),     # a bare name: any value
                 ("_GLIBCXX_ASSERTIONS", "g++", ["-D", "_GLIBCXX_ASSERTIONS"], 0),
                 ("_FORTIFY_SOURCE=3", "g++", ["-D_FORTIFY_SOURCE=2"], 3),            # a value: that value only
                 ("_FORTIFY_SOURCE=3", "g++", ["-D_FORTIFY_SOURCE=3"], 0),
                 (STL, "g++", ["-D_MSVC_STL_HARDENING"], 0),                          # -DNAME is NAME=1
                 (STL, "cl.exe", ["/D_MSVC_STL_HARDENING#1"], 0),                     # cl's # for =
                 (STL, "cl.exe", ["-D_MSVC_STL_HARDENING=1"], 0),
                 (STL, "g++", ["/D_MSVC_STL_HARDENING=1"], 3),                        # not a / reader
                 (STL, "C:/VS/bin/CL.EXE", ["/D_MSVC_STL_HARDENING=1"], 0),
                 (STL, "clang-cl", ["/D", "_MSVC_STL_HARDENING=1"], 0),
                 (STL, "sccache", ["cl", "/D_MSVC_STL_HARDENING=1"], 0),                # after a launcher
                 ("/guard:cf", "cl.exe", ["-guard:cf"], 0),                           # cl's - spelling
                 ("-fstack-protector-strong", "g++", ["-fstack-protector-strong=1"], 3),
                 (STL, "cl.exe", ["/D_MSVC_STL_HARDENING_EXTRA=1"], 3)]
        for requirement, compiler, flags, expected in cases:
            with self.subTest(requirement=requirement, compiler=compiler, flags=flags):
                self.policy(requirement)
                self.database([self.entry("src/a.cpp", flags, compiler=compiler)])
                self.assertEqual(self.both()[0], expected)

    def test_a_command_string_is_split_as_its_system_splits_it(self):
        self.policy("_GLIBCXX_ASSERTIONS", "-DNAME=a b")
        self.database([self.entry("src/a.cpp", ["-D_GLIBCXX_ASSERTIONS", '"-DNAME=a b"'], command=True)])
        self.assertEqual(self.both()[0], 0)
        self.assertEqual(hc.split_windows(r'cl "/DNAME=a b" /DQ=\"x\" a\\b.c "C:\dir\\" ""'),
                         ["cl", "/DNAME=a b", '/DQ="x"', r"a\\b.c", "C:\\dir\\", ""])
        self.assertEqual(hc.split_windows('cl "say ""hi"""'), ["cl", 'say "hi"'])
        with self.assertRaises(hc.Unreadable):
            hc.split_windows('cl "/DNAME=1')

    def test_a_windows_directory_splits_by_windows_rules(self):
        self.assertTrue(hc.WINDOWS_DIRECTORY.search("C:/work/build"))
        self.assertTrue(hc.WINDOWS_DIRECTORY.search(r"build\x64"))
        self.assertFalse(hc.WINDOWS_DIRECTORY.search("/home/me/build"))
        if os.name == "nt":
            # A real Windows entry: a backslash in a command is no escape.
            self.policy("_GLIBCXX_ASSERTIONS")
            self.file("src/a.cpp")
            self.database([{"directory": str(self.project), "file": "src/a.cpp",
                            "command": r'g++ "-I C:\inc\\" -D_GLIBCXX_ASSERTIONS -c src\a.cpp'}])
            self.assertEqual(self.both()[0], 0)


class ScopeTests(Fixture):
    def test_an_unhardened_entry_under_an_excluded_folder_is_counted_and_not_checked(self):
        self.policy("_GLIBCXX_ASSERTIONS", exclude=["src/vendor"])
        self.database([self.entry("src/a.cpp", ["-D_GLIBCXX_ASSERTIONS"]), self.entry("src/vendor/z.cpp", [])])
        code, status, _text, report = self.both()
        self.assertEqual((code, status, report["checked"], report["excluded"]), (0, "ok", 1, 1))
        self.assertIn("1 excluded", report["detail"])

    def test_entries_outside_the_sources_are_counted_and_not_checked(self):
        self.policy("_GLIBCXX_ASSERTIONS", sources=["src"])
        self.database([self.entry("src/a.cpp", ["-D_GLIBCXX_ASSERTIONS"]), self.entry("tools/gen.cpp", []),
                       self.entry("srcgen/b.cpp", [])])   # a sibling whose name starts like the folder
        code, status, _text, report = self.both()
        self.assertEqual((code, status, report["checked"], report["outside"]), (0, "ok", 1, 2))
        self.assertIn("2 outside the sources", report["detail"])

    def test_the_sources_are_under_the_pin_files_root(self):
        self.pins({"compile": ["_GLIBCXX_ASSERTIONS"], "sources": ["src"]}, root="engine")
        self.file("engine/src/a.cpp")
        self.file("engine/compile_commands.json", json.dumps([
            {"directory": str(self.project / "engine"), "file": "src/a.cpp", "arguments": ["g++", "-c", "src/a.cpp"]}]))
        code, status, text, _report = self.both()
        self.assertEqual((code, status), (3, "missing"))
        self.assertIn("first engine/src/a.cpp", text)

    def test_a_policy_no_entry_falls_under_is_unknown(self):
        self.policy("_GLIBCXX_ASSERTIONS", sources=["src"], exclude=["src"])
        self.database([self.entry("src/a.cpp", [])])
        code, status, _text, report = self.both()
        self.assertEqual((code, status), (1, "unknown"))
        self.assertIn("no entry is under the policy's sources (src), so nothing was checked", report["detail"])


class UnknownTests(Fixture):
    def test_a_database_that_is_missing_or_invalid_is_unknown(self):
        self.policy("_GLIBCXX_ASSERTIONS")
        code, status, _text, report = self.both()
        self.assertEqual((code, status), (1, "unknown"))
        self.assertIn("the compile database is missing", report["detail"])
        self.database("[{]")
        code, status, _text, report = self.both()
        self.assertEqual((code, status), (1, "unknown"))
        self.assertIn("the compile database is invalid", report["detail"])

    def test_a_database_larger_than_the_read_limit_is_unknown(self):
        self.policy("_GLIBCXX_ASSERTIONS")
        self.database([self.entry("src/a.cpp", ["-D_GLIBCXX_ASSERTIONS"])])
        pin_file = hc._sibling("toolchain-check.py", "tc").load_pin_file(self.project / hc.PINS)
        compdb = hc._sibling("compdb-check.py", "cc")
        result = hc.check_compile(self.project, pin_file, compdb, read_limit=16)
        self.assertEqual(result["status"], "unknown")
        self.assertIn("is larger than the 16 bytes this check reads", result["detail"])
        self.assertEqual(compdb.READ_LIMIT, 256 * 1024 * 1024)   # the default it reads up to

    def test_a_hardened_database_older_than_a_build_file_is_unknown_with_the_regenerate_line(self):
        self.pins({"compile": ["_GLIBCXX_ASSERTIONS"]}, compdb={"regenerate": ["cmake", "-B", "build"]})
        self.database([self.entry("src/a.cpp", ["-D_GLIBCXX_ASSERTIONS"])])
        self.file("CMakeLists.txt", "add_compile_definitions()\n", when=LATER)
        code, status, text, report = self.both()
        self.assertEqual((code, status), (1, "unknown"))
        self.assertIn("the compile database is stale", report["detail"])
        self.assertIn("older than CMakeLists.txt", report["detail"])
        self.assertRegex(text.splitlines()[1], r"^regenerate with \((PowerShell|sh)\): cmake -B build$")


class ResponseFileTests(Fixture):
    def policy_with(self, rsp_text=None, rsp_data=None, flags=("@flags.rsp",)):
        self.policy("_GLIBCXX_ASSERTIONS", "-DNAME=a b")
        if rsp_text is not None or rsp_data is not None:
            self.file("flags.rsp", rsp_text or "", data=rsp_data)
        self.database([self.entry("src/a.cpp", list(flags))])

    def test_a_requirement_only_in_a_quoted_response_file_passes(self):
        self.policy_with('-D_GLIBCXX_ASSERTIONS\n"-DNAME=a b"\n')
        self.assertEqual(self.both()[:2], (0, "ok"))

    def test_a_utf16_response_file_with_a_byte_order_mark_passes(self):
        self.policy_with(rsp_data='-D_GLIBCXX_ASSERTIONS "-DNAME=a b"'.encode("utf-16"))
        self.assertEqual(self.both()[:2], (0, "ok"))

    def test_a_flag_after_the_response_file_still_wins(self):
        self.policy_with('-D_GLIBCXX_ASSERTIONS "-DNAME=a b"', flags=("@flags.rsp", "-U_GLIBCXX_ASSERTIONS"))
        self.assertEqual(self.both()[:2], (3, "missing"))

    def test_a_response_file_that_cannot_be_read_faithfully_is_unknown(self):
        cases = [(None, None, "response file flags.rsp of src/a.cpp cannot be read"),
                 ("-D_GLIBCXX_ASSERTIONS " * 10, None, "is larger than the 64 bytes this check reads"),
                 (None, b"-D\xff\xfe\xfa", "is neither UTF-8 nor UTF-16 with a byte order mark"),
                 ('-D_GLIBCXX_ASSERTIONS "-DNAME=a b', None, "response file flags.rsp of src/a.cpp: "),
                 ('-D_GLIBCXX_ASSERTIONS "-DNAME=a b" @inner.rsp', None, "names a response file itself (@inner.rsp)")]
        for text, data, expected in cases:
            with self.subTest(expected=expected):
                (self.project / "flags.rsp").unlink(missing_ok=True)
                self.file("inner.rsp", "-U_GLIBCXX_ASSERTIONS\n")
                with unittest.mock.patch.object(hc, "RESPONSE_LIMIT", 64):
                    self.policy_with(text, data)
                    code, status, _text, report = self.both()
                self.assertEqual((code, status), (1, "unknown"))
                self.assertIn(expected, report["detail"])
                self.assertTrue(report["detail"].endswith("so its flags cannot be told"), report["detail"])

    def test_an_unreadable_entry_keeps_a_confirmed_finding_on_another(self):
        # Found by D04 T03 section 3's round: one unreadable entry made the whole check unknown, so a source
        # compiled without a requirement passed --strict.
        self.policy("_GLIBCXX_ASSERTIONS")
        self.database([self.entry("src/a.cpp", []), self.entry("src/b.cpp", ["@build/missing.rsp"]),
                       self.entry("src/c.cpp", ["@build/gone.rsp"])])
        code, status, text, report = self.both()
        self.assertEqual((code, status, report["unreadable"]), (3, "missing", 2))
        self.assertIn("  missing  _GLIBCXX_ASSERTIONS: on 1 entry, first src/a.cpp", text)
        self.assertIn("; 2 entries cannot be read; first: response file build/missing.rsp of src/b.cpp cannot be read",
                      report["detail"])
        # With every readable entry hardened, the unreadable one leaves the answer unknown.
        self.database([self.entry("src/a.cpp", ["-D_GLIBCXX_ASSERTIONS"]),
                       self.entry("src/b.cpp", ["@build/missing.rsp"])])
        code, status, _text, report = self.both()
        self.assertEqual((code, status, report["unreadable"]), (1, "unknown", 1))
        self.assertTrue(report["detail"].endswith("so its flags cannot be told"), report["detail"])

    def test_an_unbalanced_quote_in_a_command_is_unknown(self):
        self.policy("_GLIBCXX_ASSERTIONS")
        self.file("src/a.cpp")
        self.database([{"directory": str(self.project), "file": "src/a.cpp",
                        "command": 'g++ "-D_GLIBCXX_ASSERTIONS -c src/a.cpp'}])
        code, status, _text, report = self.both()
        self.assertEqual((code, status), (1, "unknown"))
        self.assertIn("the command of src/a.cpp: ", report["detail"])


FIXTURES = SCRIPTS / "tests" / "fixtures"
# What dumpbin and readelf reported for each fixture (fixtures/README.md), as the readers name it.
PE_EXPECTED = {
    "pe-hardened.exe.bin": ({"DYNAMIC_BASE": True, "HIGH_ENTROPY_VA": True, "NX_COMPAT": True, "GUARD_CF": True,
                             "CET_COMPAT": True}, 10),
    "pe-plain.exe.bin": ({"DYNAMIC_BASE": False, "HIGH_ENTROPY_VA": False, "NX_COMPAT": False, "GUARD_CF": False,
                          "CET_COMPAT": False}, 0),
    "pe-asan.exe.bin": ({"DYNAMIC_BASE": True, "HIGH_ENTROPY_VA": True, "NX_COMPAT": True, "GUARD_CF": False,
                         "CET_COMPAT": False}, 0),
    "pe32-hardened.exe.bin": ({"DYNAMIC_BASE": True, "HIGH_ENTROPY_VA": "not applicable", "NX_COMPAT": True,
                               "GUARD_CF": True, "CET_COMPAT": False}, 9),
    "pe-delay.exe.bin": ({"DYNAMIC_BASE": True, "HIGH_ENTROPY_VA": True, "NX_COMPAT": True, "GUARD_CF": False,
                          "CET_COMPAT": False}, 0),
    "pe32-delay.exe.bin": ({"DYNAMIC_BASE": True, "HIGH_ENTROPY_VA": "not applicable", "NX_COMPAT": True,
                            "GUARD_CF": False, "CET_COMPAT": False}, 0),
}
CRT = ["KERNEL32.dll", "VCRUNTIME140.dll"]
ELF_EXPECTED = {
    "elf-hardened.bin": ({"PIE": True, "RELRO": True, "BIND_NOW": True, "NX_STACK": True, "STACK_PROTECTOR": True},
                         ["libc.so.6"]),
    "elf-plain.bin": ({"PIE": False, "RELRO": False, "BIND_NOW": False, "NX_STACK": False, "STACK_PROTECTOR": False},
                      ["libc.so.6"]),
    "elf-asan.bin": ({"PIE": True, "RELRO": True, "BIND_NOW": True, "NX_STACK": True, "STACK_PROTECTOR": True},
                     ["libasan.so.8", "libc.so.6"]),
    "elf-ubsan-static.bin.gz": ({"PIE": True, "RELRO": True, "BIND_NOW": True, "NX_STACK": True,
                                 "STACK_PROTECTOR": True}, ["libm.so.6", "libgcc_s.so.1", "libc.so.6"]),
    "elf-static.bin": ({"PIE": False, "RELRO": False, "BIND_NOW": False, "NX_STACK": True, "STACK_PROTECTOR": None},
                       []),
}


def fixture_bytes(name):
    data = (FIXTURES / name).read_bytes()
    return gzip.decompress(data) if name.endswith(".gz") else data


class ImageFixture(Fixture):
    def binary(self, name, rel=None, data=None):
        """A fixture (or `data`) placed in the project at `rel` (default: build/<its name without .bin>)."""
        rel = rel or "build/" + name.removesuffix(".gz").removesuffix(".bin")
        path = self.project / rel
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(fixture_bytes(name) if data is None else data)
        return path

    def image_policy(self, binaries, image=None, test_binaries=None, **keys):
        policy = {"binaries": list(binaries), **keys}
        if image is not None:
            policy["image"] = list(image)
        if test_binaries is not None:
            policy["test_binaries"] = list(test_binaries)
        self.pins(policy)

    def image(self):
        """(exit, the image result, the text) from the command line, text and JSON agreeing."""
        code, text, _err = self.run_main()
        json_code, document, _err = self.run_main("--json")
        whole = json.loads(document)
        self.assertEqual(json_code, code)
        self.assertIn(f"hardening:image: {whole['image']['status']}: {whole['image']['detail']}", text.splitlines())
        for entry in whole["image"]["binaries"]:
            self.assertIn(f"  {entry['status']:<8} {entry['detail']}", text.splitlines())
        return code, whole["image"], text


class ReaderTests(ImageFixture):
    def test_the_pe_reader_reports_what_dumpbin_reported(self):
        for name, (properties, guarded) in PE_EXPECTED.items():
            with self.subTest(name=name):
                found = hc.read_pe(hc.Image(fixture_bytes(name), False))
                self.assertEqual({key: found[key] for key in properties}, properties)
                self.assertEqual(found["guard_functions"], guarded)
                self.assertEqual(found["libraries"][:0], [])
                for dll in CRT:
                    self.assertIn(dll, found["libraries"])
        self.assertIn("clang_rt.asan_dynamic-x86_64.dll", hc.read_pe(hc.Image(fixture_bytes("pe-asan.exe.bin"),
                                                                              False))["libraries"])
        delay = hc.read_pe(hc.Image(fixture_bytes("pe-delay.exe.bin"), False))["libraries"]
        self.assertEqual(delay[-1], "USER32.dll")   # the delay import table is read after the import table
        self.assertNotIn("USER32.dll", delay[:-1])

    def test_the_elf_reader_reports_what_readelf_reported(self):
        for name, (properties, needed) in ELF_EXPECTED.items():
            with self.subTest(name=name):
                found = hc.read_elf(hc.Image(fixture_bytes(name), False))
                self.assertEqual({key: found[key] for key in properties}, properties)
                self.assertEqual(found["libraries"], needed)

    def test_the_two_readers_name_exactly_the_properties_the_pin_file_takes(self):
        tc = hc._sibling("toolchain-check.py", "tc")
        self.assertEqual(tc.IMAGE_PROPERTIES, hc.PE_PROPERTIES + hc.ELF_PROPERTIES)


class ImagePolicyTests(ImageFixture):
    def test_each_binary_a_pattern_matches_is_checked_and_only_the_failing_one_named(self):
        self.binary("pe-hardened.exe.bin", "build/app-hardened.exe")
        self.binary("pe-plain.exe.bin", "build/app-plain.exe")
        self.image_policy(["build/app-*.exe"], ["GUARD_CF"])
        code, image, text = self.image()
        self.assertEqual((code, image["status"]), (3, "missing"))
        self.assertEqual([(entry["path"], entry["status"]) for entry in image["binaries"]],
                         [("build/app-hardened.exe", "ok"), ("build/app-plain.exe", "missing")])
        self.assertIn("build/app-plain.exe (PE) lacks GUARD_CF", text)
        self.assertNotIn("build/app-hardened.exe (PE) lacks", text)

    def test_every_property_of_a_hardened_binary_passes(self):
        self.binary("pe-hardened.exe.bin")
        self.binary("elf-hardened.bin")
        self.image_policy(["build/pe-hardened.exe", "build/elf-hardened"], list(hc.PE_PROPERTIES + hc.ELF_PROPERTIES))
        code, image, _text = self.image()
        self.assertEqual((code, image["status"]), (0, "ok"))
        pe, elf = image["binaries"]
        self.assertEqual(set(pe["properties"].values()), {"ok", "not applicable"})
        self.assertEqual(pe["properties"]["PIE"], "not applicable")
        self.assertEqual(elf["properties"]["GUARD_CF"], "not applicable")

    def test_a_pattern_that_names_no_file_is_unknown(self):
        self.binary("pe-hardened.exe.bin")
        self.image_policy(["build/pe-hardened.exe", "build/missing-*.exe", "out/app.exe"], ["GUARD_CF"])
        code, image, text = self.image()
        self.assertEqual((code, image["status"], image["unmatched"]), (1, "unknown", ["build/missing-*.exe",
                                                                                      "out/app.exe"]))
        self.assertIn("  unknown  out/app.exe names no file", text)
        # The summary counts the patterns; each is named once, on its own line.
        self.assertTrue(image["detail"].endswith("; 2 pattern(s) name no file"), image["detail"])
        self.assertEqual(text.count("out/app.exe"), 1)

    def test_high_entropy_does_not_apply_to_a_32_bit_image(self):
        self.binary("pe32-hardened.exe.bin")
        self.image_policy(["build/pe32-hardened.exe"], ["GUARD_CF", "HIGH_ENTROPY_VA"])
        code, image, _text = self.image()
        self.assertEqual((code, image["binaries"][0]["properties"]), (0, {"GUARD_CF": "ok",
                                                                          "HIGH_ENTROPY_VA": "not applicable"}))

    def test_a_binary_none_of_whose_required_properties_applies_is_unknown(self):
        self.binary("elf-hardened.bin")
        self.image_policy(["build/elf-hardened"], ["GUARD_CF"])
        code, image, _text = self.image()
        self.assertEqual((code, image["binaries"][0]["status"]), (1, "unknown"))
        self.assertIn("none of the required properties (GUARD_CF) applies", image["binaries"][0]["detail"])

    def test_a_stack_protector_that_cannot_be_told_is_unknown(self):
        self.binary("elf-static.bin")
        self.image_policy(["build/elf-static"], ["STACK_PROTECTOR", "NX_STACK"])
        code, image, _text = self.image()
        self.assertEqual((code, image["binaries"][0]["properties"]), (1, {"STACK_PROTECTOR": "unknown",
                                                                          "NX_STACK": "ok"}))

    def test_binaries_alone_are_still_refused_a_sanitizer_runtime(self):
        self.binary("pe-hardened.exe.bin")
        self.image_policy(["build/pe-hardened.exe"])
        self.assertEqual(self.image()[:2][0], 0)


class CaseTests(ImageFixture):
    def test_patterns_match_names_by_the_systems_rule_for_case(self):
        # Round 1 of the review: on Windows `*.exe` missed SANITIZED.EXE, so a sanitizer build escaped the check.
        self.binary("pe-hardened.exe.bin", "build/release.exe")
        self.binary("pe-asan.exe.bin", "build/SANITIZED.EXE")
        self.image_policy(["build/*.exe"], ["DYNAMIC_BASE"])
        code, image, _text = self.image()
        names = [entry["path"] for entry in image["binaries"]]
        if os.name == "nt":
            self.assertEqual((code, names), (3, ["build/SANITIZED.EXE", "build/release.exe"]))
            self.image_policy(["build/*.exe"], ["DYNAMIC_BASE"], test_binaries=["BUILD/sanitized.exe"])
            self.assertEqual(self.image()[0], 0)
        else:
            self.assertEqual((code, names), (0, ["build/release.exe"]))
        self.assertEqual(hc.fnmatch.fnmatch("SANITIZED.EXE", "*.exe"), os.path.normcase("A") == "a")

    def test_the_help_states_the_detection_limit(self):
        out = io.StringIO()
        with contextlib.redirect_stdout(out), self.assertRaises(SystemExit):
            hc.main(["--help"])
        text = " ".join(out.getvalue().split())
        self.assertIn("Limit: a runtime linked in without that string in the file is not found.", text)
        self.assertIn("by the system's rule for case", text)


class SanitizerTests(ImageFixture):
    def test_a_sanitizer_runtime_is_refused_by_its_library_or_its_option_variable(self):
        cases = [("pe-asan.exe.bin", "clang_rt.asan_dynamic-x86_64.dll"), ("elf-asan.bin", "libasan.so.8"),
                 ("elf-ubsan-static.bin.gz", "UBSAN_OPTIONS")]
        for name, evidence in cases:
            with self.subTest(name=name):
                path = self.binary(name)
                self.image_policy([path.relative_to(self.project).as_posix()])
                code, image, text = self.image()
                self.assertEqual((code, image["binaries"][0]["status"]), (3, "missing"))
                self.assertEqual(image["binaries"][0]["sanitizers"], [evidence])
                self.assertIn(f"carries a sanitizer runtime ({evidence})", text)
        for name in ("pe-hardened.exe.bin", "pe-delay.exe.bin", "elf-hardened.bin", "elf-plain.bin",
                     "elf-static.bin", "pe32-hardened.exe.bin", "pe-plain.exe.bin"):
            with self.subTest(clean=name):
                found = hc.inspect_binary(self.binary(name), name, [], False)
                self.assertEqual((found["status"], found["sanitizers"]), ("ok", []))

    def test_a_test_binary_may_carry_one_and_is_still_held_to_image(self):
        self.binary("pe-asan.exe.bin", "build/unit_test.exe")
        self.image_policy(["build/*.exe"], ["GUARD_CF"], test_binaries=["build/*_test.exe"])
        code, image, text = self.image()
        self.assertEqual((code, image["binaries"][0]["missing"]), (3, ["GUARD_CF"]))
        self.assertNotIn("carries a sanitizer runtime", text)
        self.image_policy(["build/*.exe"], ["DYNAMIC_BASE", "NX_COMPAT"], test_binaries=["build/*_test.exe"])
        code, image, _text = self.image()
        self.assertEqual((code, image["status"]), (0, "ok"))
        self.assertIn("its sanitizer runtime is allowed", image["binaries"][0]["detail"])
        self.image_policy(["build/*.exe"], ["DYNAMIC_BASE"], test_binaries=["other/*_test.exe"])
        self.assertEqual(self.image()[0], 3)   # a pattern under another folder lifts nothing


class BoundTests(ImageFixture):
    def assert_one_line(self, data, rel, expected_status, needle):
        path = self.binary("pe-hardened.exe.bin", rel, data=data) if data is not None else self.project / rel
        self.image_policy([rel], ["GUARD_CF"] if rel.endswith(".exe") else ["PIE"])
        code, image, text = self.image()
        (entry,) = image["binaries"]
        self.assertEqual((code, entry["status"]), (1, expected_status), entry["detail"])
        self.assertIn(needle, entry["detail"])
        self.assertEqual(len([line for line in text.splitlines() if line.startswith("  ")]), 1)
        return path

    @staticmethod
    def contents_end(data):
        """Where the last range of file contents the image declares ends, read here without the reader: the
        sections' raw data of a PE image, the segments' file contents of an ELF one."""
        if data[:4] == b"\x7fELF":
            phoff, phentsize, phnum = (struct.unpack_from(f, data, o)[0] for f, o in (("<Q", 32), ("<H", 54),
                                                                                   ("<H", 56)))
            return max(struct.unpack_from("<Q", data, phoff + phentsize * n + 8)[0]
                       + struct.unpack_from("<Q", data, phoff + phentsize * n + 32)[0] for n in range(phnum))
        lfanew = struct.unpack_from("<I", data, 0x3C)[0]
        count, optional_size = struct.unpack_from("<H", data, lfanew + 6)[0], struct.unpack_from("<H", data,
                                                                                                  lfanew + 20)[0]
        table = lfanew + 24 + optional_size
        return max(struct.unpack_from("<I", data, table + 40 * n + 20)[0]
                   + struct.unpack_from("<I", data, table + 40 * n + 16)[0] for n in range(count))

    def test_a_truncated_copy_of_any_fixture_is_refused_and_never_raises(self):
        # Round 1 of the review: a copy cut short (an interrupted artifact upload) passed while its headers held.
        for name in list(PE_EXPECTED) + list(ELF_EXPECTED):
            data = fixture_bytes(name)
            needed = self.contents_end(data)
            self.assertLessEqual(needed, len(data), name)
            with self.subTest(name=name):
                path = self.project / "cut.bin"
                for end in list(range(0, needed, 256)) + [needed - 1]:
                    path.write_bytes(data[:end])
                    found = hc.inspect_binary(path, "cut.bin", ["GUARD_CF", "PIE", "NX_STACK"], False)
                    self.assertEqual(found["status"], "invalid", f"{name} cut at {end}: {found['detail']}")
                path.write_bytes(data[:needed])
                self.assertNotEqual(hc.inspect_binary(path, "cut.bin", ["NX_STACK"], False)["status"], "invalid")

    def test_the_static_fixture_cut_to_1024_bytes_is_invalid(self):
        self.binary("elf-static.bin", data=fixture_bytes("elf-static.bin")[:1024])
        self.image_policy(["build/elf-static"], ["NX_STACK"])
        code, image, _text = self.image()
        self.assertEqual((code, image["binaries"][0]["status"]), (1, "invalid"))
        self.assertIn("lies past the end of the file", image["binaries"][0]["detail"])

    def test_offsets_past_the_end_inside_valid_images_are_invalid_naming_the_offset(self):
        pe = bytearray(fixture_bytes("pe-hardened.exe.bin"))
        lfanew = struct.unpack_from("<I", pe, 0x3C)[0]
        optional = lfanew + 24
        cases = [("e_lfanew", 0x3C, "<I", 0x7FFFFF00, "past the end"),
                 ("section count", lfanew + 6, "<H", 0xFFFF, "past the end"),
                 ("load configuration address", optional + 112 + 8 * 10, "<I", 0x7FFF0000, "in no section")]
        for what, at, fmt, value, needle in cases:
            with self.subTest(what=what):
                data = bytearray(pe)
                struct.pack_into(fmt, data, at, value)
                self.assert_one_line(bytes(data), "build/app.exe", "invalid", needle)
        elf = bytearray(fixture_bytes("elf-hardened.bin"))
        data = bytearray(elf)
        struct.pack_into("<Q", data, 32, 0x7FFFFFFF0000)   # e_phoff
        self.assert_one_line(bytes(data), "build/app", "invalid", "past the end")
        found = hc.read_elf(hc.Image(bytes(elf), False))
        self.assertEqual(found["libraries"], ["libc.so.6"])
        # A needed library's name moved past the end of the string table and the file.
        data = bytearray(elf)
        phoff, phentsize, phnum = (struct.unpack_from(f, elf, o)[0] for f, o in (("<Q", 32), ("<H", 54), ("<H", 56)))
        for number in range(phnum):
            at = phoff + phentsize * number
            if struct.unpack_from("<I", elf, at)[0] == hc.PT_DYNAMIC:
                dynamic = struct.unpack_from("<Q", elf, at + 8)[0]
        for number in range(64):
            tag = struct.unpack_from("<q", elf, dynamic + 16 * number)[0]
            if tag == hc.DT_NEEDED:
                struct.pack_into("<Q", data, dynamic + 16 * number + 8, 0x7FFFFFF0)
                break
        self.assert_one_line(bytes(data), "build/app", "invalid", "past the end")

    def test_files_that_are_no_image_are_one_line_each(self):
        self.assert_one_line(bytes(4096), "build/zeros.exe", "invalid", "neither a PE nor an ELF image")
        self.assert_one_line(b"#!/bin/sh\necho not a binary\n", "build/script.exe", "invalid", "neither a PE")
        (self.project / "build/folder.exe").mkdir(parents=True)
        self.assert_one_line(None, "build/folder.exe", "unknown", "is a folder, not a binary")

    def test_a_file_that_cannot_be_read_is_unknown_with_the_reason(self):
        path = self.binary("pe-hardened.exe.bin")
        real = open

        def refusing(file, *args, **kwargs):
            if pathlib.Path(file) == path:
                raise PermissionError(13, "Permission denied")
            return real(file, *args, **kwargs)

        with unittest.mock.patch("builtins.open", refusing):
            found = hc.inspect_binary(path, "build/pe-hardened.exe", ["GUARD_CF"], False)
        self.assertEqual((found["status"], found["detail"]), ("unknown", "build/pe-hardened.exe cannot be read "
                                                                         "(Permission denied)"))

    def test_a_binary_larger_than_the_read_limit_is_unknown_saying_the_limit(self):
        path = self.binary("elf-ubsan-static.bin.gz")
        found = hc.inspect_binary(path, "build/elf-ubsan-static", ["PIE"], False, limit=4096)
        self.assertEqual(found["status"], "unknown", found["detail"])
        self.assertIn("larger than the 4096 bytes this check reads", found["detail"])
        self.assertEqual(hc.IMAGE_LIMIT, 64 * 1024 * 1024)


class CombinedTests(ImageFixture):
    def test_a_missing_compile_requirement_and_an_unknown_image_exit_3(self):
        self.pins({"compile": ["_GLIBCXX_ASSERTIONS"], "binaries": ["build/none.exe"], "image": ["GUARD_CF"]})
        self.database([self.entry("src/a.cpp", [])])
        code, image, text = self.image()
        self.assertEqual((code, image["status"]), (3, "unknown"))
        self.assertTrue(text.startswith("hardening:compile: missing: "))

    def test_an_unknown_compile_database_does_not_stop_the_image_check(self):
        self.binary("pe-hardened.exe.bin")
        self.pins({"compile": ["_GLIBCXX_ASSERTIONS"], "binaries": ["build/pe-hardened.exe"], "image": ["GUARD_CF"]})
        code, image, text = self.image()
        self.assertEqual((code, image["status"]), (1, "ok"))
        self.assertTrue(text.startswith("hardening:compile: unknown: the compile database is missing"))
        _code, document, _err = self.run_main("--json")
        whole = json.loads(document)
        self.assertEqual((whole["status"], whole["compile"]["status"], whole["image"]["status"]),
                         ("unknown", "unknown", "ok"))
        self.assertEqual(whole["image"]["binaries"][0]["properties"], {"GUARD_CF": "ok"})
        self.assertEqual(whole["image"]["binaries"][0]["format"], "pe")

    def test_a_compile_only_and_an_image_only_policy(self):
        self.policy("_GLIBCXX_ASSERTIONS")
        self.database([self.entry("src/a.cpp", ["-D_GLIBCXX_ASSERTIONS"])])
        code, image, _text = self.image()
        self.assertEqual((code, image["status"], image["detail"]), (0, "none", "no binaries declared"))
        self.binary("elf-hardened.bin")
        self.image_policy(["build/elf-hardened"], ["PIE", "BIND_NOW"])
        code, image, text = self.image()
        self.assertEqual((code, image["status"]), (0, "ok"))
        self.assertTrue(text.startswith("hardening:compile: none: no compile requirement declared"))

    def test_no_policy_prints_none_for_both(self):
        self.pins()
        code, text, _err = self.run_main()
        self.assertEqual(code, 0)
        self.assertEqual(text.splitlines(), ["hardening:compile: none: no policy declared: add a `hardening` key to "
                                             "the pin file (docs/toolchain-cpp.md, Memory safety)",
                                             "hardening:image: none: no policy declared"])


class EditedImageTests(ImageFixture):
    """Real fixtures with one field changed, for what the toolchains here do not produce (the writer's own review
    of section 2)."""

    def pe_directory(self, data, index):
        lfanew = struct.unpack_from("<I", data, 0x3C)[0]
        optional = lfanew + 24
        plus = struct.unpack_from("<H", data, optional)[0] == 0x20B
        return struct.unpack_from("<II", data, optional + (112 if plus else 96) + 8 * index)

    def pe_offset(self, data, rva):
        lfanew = struct.unpack_from("<I", data, 0x3C)[0]
        count, optional_size = struct.unpack_from("<H", data, lfanew + 6)[0], struct.unpack_from("<H", data,
                                                                                                  lfanew + 20)[0]
        table = lfanew + 24 + optional_size
        for number in range(count):
            size, address, _raw_size, raw = struct.unpack_from("<IIII", data, table + 40 * number + 8)
            if address <= rva < address + size:
                return raw + rva - address
        raise AssertionError(f"no section holds {rva:#x}")

    def test_a_debug_entry_whose_data_is_not_in_the_file_is_not_read_from_offset_0(self):
        data = bytearray(fixture_bytes("pe-hardened.exe.bin"))
        rva, size = self.pe_directory(data, 6)
        at = self.pe_offset(data, rva)
        for number in range(size // 28):
            entry = at + 28 * number
            if struct.unpack_from("<I", data, entry + 12)[0] == hc.DEBUG_EX_DLLCHARACTERISTICS:
                struct.pack_into("<I", data, entry + 24, 0)
        # Offset 0 holds "MZ", whose low bit is set: read there, the binary would pass as CET compatible.
        self.assertEqual(hc.read_pe(hc.Image(bytes(data), False))["CET_COMPAT"], False)

    def test_a_delay_import_of_the_first_format_names_its_dll_by_address(self):
        # The first delay import format, of 32-bit images built before Visual C++ 7, holds addresses.
        data = bytearray(fixture_bytes("pe32-delay.exe.bin"))
        self.assertEqual(hc.read_pe(hc.Image(bytes(data), False))["libraries"][-1], "USER32.dll")
        rva, _size = self.pe_directory(data, 13)
        at = self.pe_offset(data, rva)
        lfanew = struct.unpack_from("<I", data, 0x3C)[0]
        image_base = struct.unpack_from("<I", data, lfanew + 24 + 28)[0]
        self.assertEqual(image_base, 0x400000)
        name_rva = struct.unpack_from("<I", data, at + 4)[0]
        struct.pack_into("<I", data, at, 0)                       # attributes: the address-based format
        struct.pack_into("<I", data, at + 4, name_rva + image_base)
        self.assertEqual(hc.read_pe(hc.Image(bytes(data), False))["libraries"][-1], "USER32.dll")

    def test_needed_libraries_without_a_string_table_are_never_a_pass(self):
        # The completion review of section 2: with DT_STRTAB gone, the AddressSanitizer build passed with no
        # library and so no sanitizer named.
        data = bytearray(fixture_bytes("elf-asan.bin"))
        phoff, phentsize, phnum = (struct.unpack_from(f, data, o)[0] for f, o in (("<Q", 32), ("<H", 54), ("<H", 56)))
        dynamic = next(struct.unpack_from("<Q", data, phoff + phentsize * n + 8)[0] for n in range(phnum)
                       if struct.unpack_from("<I", data, phoff + phentsize * n)[0] == hc.PT_DYNAMIC)
        for number in range(64):
            if struct.unpack_from("<q", data, dynamic + 16 * number)[0] == hc.DT_STRTAB:
                struct.pack_into("<q", data, dynamic + 16 * number, 0x6FFFFE00)   # a tag no reader knows
                break
        self.binary("elf-asan.bin", "build/app", data=bytes(data))
        self.image_policy(["build/app"], ["NX_STACK"])
        code, image, _text = self.image()
        entry = image["binaries"][0]
        self.assertEqual((code, entry["status"]), (1, "invalid"))
        self.assertIn("names needed libraries but no dynamic string table", entry["detail"])

    def test_a_string_table_without_its_size_leaves_the_stack_protector_unknown(self):
        data = bytearray(fixture_bytes("elf-hardened.bin"))
        phoff, phentsize, phnum = (struct.unpack_from(f, data, o)[0] for f, o in (("<Q", 32), ("<H", 54), ("<H", 56)))
        dynamic = next(struct.unpack_from("<Q", data, phoff + phentsize * n + 8)[0] for n in range(phnum)
                       if struct.unpack_from("<I", data, phoff + phentsize * n)[0] == hc.PT_DYNAMIC)
        for number in range(64):
            if struct.unpack_from("<q", data, dynamic + 16 * number)[0] == hc.DT_STRSZ:
                struct.pack_into("<q", data, dynamic + 16 * number, 0x6FFFFE00)   # a tag no reader knows
                break
        found = hc.read_elf(hc.Image(bytes(data), False))
        self.assertIsNone(found["STACK_PROTECTOR"])
        self.assertEqual(found["libraries"], ["libc.so.6"])


class ReadOnlyTests(Fixture):
    def test_the_check_starts_no_process_and_writes_nothing(self):
        self.policy("_GLIBCXX_ASSERTIONS")
        self.database([self.entry("src/a.cpp", ["-D_GLIBCXX_ASSERTIONS"])])
        before = {path: path.read_bytes() for path in self.project.rglob("*") if path.is_file()}
        with unittest.mock.patch.object(subprocess, "Popen", side_effect=AssertionError("a process was started")):
            self.assertEqual(self.both()[0], 0)
        self.assertEqual({path: path.read_bytes() for path in self.project.rglob("*") if path.is_file()}, before)

    def test_the_script_runs_as_a_program(self):
        self.policy("_GLIBCXX_ASSERTIONS")
        self.database([self.entry("src/a.cpp", [])])
        done = subprocess.run([sys.executable, str(SCRIPTS / "hardening-check.py"), "--root", str(self.project)],
                              capture_output=True, text=True, timeout=60)
        self.assertEqual(done.returncode, 3, done.stderr)
        self.assertTrue(done.stdout.startswith("hardening:compile: missing: "), done.stdout)


if __name__ == "__main__":
    unittest.main()
