"""cc-cache.py and cache-stats.py: the wrappers and the statement that activates them, run in the shells they are
for; opting out and stale wrappers; a failed setup; and the cache's statistics, read, reset and judged.

The compilers and the caches are stubs on a scratch PATH. A stub cache logs how it was called and runs the
compiler it was given, and keeps its counters in a file, printed in the form the real cache prints them.
"""
import contextlib
import importlib.util
import io
import json
import os
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
import unittest.mock

SCRIPTS = pathlib.Path(__file__).resolve().parents[1]
# The project's document, installed beside scripts/ (in the kit: templates/docs/), so these tests read the
# same file from the kit and from an installed copy.
DOCUMENT = SCRIPTS.parent / "docs/toolchain-cpp.md"


def load(name):
    spec = importlib.util.spec_from_file_location(name.replace("-", "_").replace(".py", ""), SCRIPTS / name)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


cc_cache = load("cc-cache.py")
cache_stats = load("cache-stats.py")
WINDOWS = os.name == "nt"
POWERSHELL = shutil.which("pwsh") or shutil.which("powershell")   # found before the tests narrow PATH

# Captured 2026-11-15 after one miss and two hits. ccache 4.12.3 `--print-stats` (the counters the check reads,
# among its 80) and sccache 0.13.0 `--show-stats --stats-format json` (its beginning, as printed).
CCACHE_STATS = "autoconf_test\t0\ncache_miss\t{misses}\ncache_size_kibibyte\t8\ndirect_cache_hit\t{hits}\n" \
               "direct_cache_miss\t1\nfiles_in_cache\t2\nlocal_storage_hit\t2\npreprocessed_cache_hit\t0\n" \
               "preprocessed_cache_miss\t1\n"
SCCACHE_STATS = '{{"stats":{{"compile_requests":3,"requests_unsupported_compiler":0,"requests_not_compile":0,' \
                '"requests_not_cacheable":0,"requests_executed":3,"cache_errors":{{"counts":{{}},"adv_counts":{{}}}},' \
                '"cache_hits":{{"counts":{hits},"adv_counts":{{"c [gcc]":2}}}},' \
                '"cache_misses":{{"counts":{misses},"adv_counts":{{"c [gcc]":1}}}},"cache_timeouts":0,' \
                '"cache_read_errors":0,"non_cacheable_compilations":0,"forced_recaches":0,"cache_write_errors":0,' \
                '"cache_writes":1}},"cache_location":"Local disk","max_cache_size":10737418240,"version":"0.13.0"}}\n'

COMPILER = r'''
import json, sys
with open(r"{log}", "a", encoding="utf-8") as log:
    log.write(json.dumps({{"compiler": r"{name}", "argv": sys.argv[1:]}}) + "\n")
sys.exit(7 if "--fail" in sys.argv else 0)
'''

CACHE = r'''
import json, os, subprocess, sys
state = r"{state}"
kind = r"{kind}"
arguments = sys.argv[1:]


def counters():
    if os.path.isfile(state):
        return json.load(open(state, encoding="utf-8"))
    return {{"hits": 2, "misses": 1}}


if arguments[:1] == ["--zero-stats"]:
    if {resets}:
        json.dump({{"hits": 0, "misses": 0}}, open(state, "w", encoding="utf-8"))
    sys.exit({zero_exit})
if arguments[:1] in (["--print-stats"], ["--show-stats"]):
    now = counters()
    if kind == "sccache":
        hits = json.dumps({{"C/C++": now["hits"]}} if now["hits"] else {{}})
        misses = json.dumps({{"C/C++": now["misses"]}} if now["misses"] else {{}})
        sys.stdout.write(r"""{sccache}""".format(hits=hits, misses=misses))
    else:
        sys.stdout.write("""{ccache}""".format(hits=now["hits"], misses=now["misses"]))
    sys.exit(0)
with open(r"{log}", "a", encoding="utf-8") as log:
    log.write(json.dumps({{"cache": kind, "argv": arguments}}) + "\n")
# Bounded: a wrapper that reached itself would have this stub start itself for ever.
depth = int(os.environ.get("CC_CACHE_STUB_DEPTH", "0"))
if depth >= 3:
    sys.exit(97)
sys.exit(subprocess.call(arguments, env=dict(os.environ, CC_CACHE_STUB_DEPTH=str(depth + 1))))
'''


class Fixture(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        # A space and an apostrophe in every path the wrappers and the statement must quote.
        self.dir = pathlib.Path(tmp.name).resolve() / "it's here"
        self.bin = self.dir / "bin dir"
        self.project = self.dir / "project"
        self.home = self.dir / "cache home"
        for folder in (self.bin, self.project, self.home):
            folder.mkdir(parents=True)
        self.log = self.dir / "calls.log"
        scripts = self.project / "scripts"
        scripts.mkdir()
        for name in ("cc-cache.py", "cache-stats.py", "toolchain-check.py"):
            (scripts / name).write_bytes((SCRIPTS / name).read_bytes())
        env = {"PATH": str(self.bin), "XDG_CACHE_HOME": str(self.home), "LOCALAPPDATA": str(self.home)}
        if WINDOWS:
            env["PATHEXT"] = ".CMD;.EXE"
        patch = unittest.mock.patch.dict(os.environ, env)
        patch.start()
        self.addCleanup(patch.stop)
        os.environ.pop("BUILD_NO_CCACHE", None)
        self.folder = cc_cache.wrapper_folder(self.project)

    def program(self, name, body, folder=None):
        """A program on the scratch PATH whose behaviour is the Python `body`."""
        folder = folder or self.bin
        code = folder / f"{name}-stub.py"
        code.write_text(body, encoding="utf-8")
        if WINDOWS:
            launcher = folder / f"{name}.cmd"
            launcher.write_text(f'@"{sys.executable}" "{code}" %*\r\n', encoding="utf-8")
        else:
            launcher = folder / name
            launcher.write_text(f'#!/bin/sh\nexec "{sys.executable}" "{code}" "$@"\n', encoding="utf-8")
            launcher.chmod(0o755)
        return launcher

    def compiler(self, name="cc", folder=None):
        return self.program(name, COMPILER.format(log=self.log, name=name), folder)

    def cache(self, kind="ccache", resets=True, zero_exit=0):
        return self.program(kind, CACHE.format(state=self.dir / f"{kind}.state", kind=kind, log=self.log,
                                               resets=resets, zero_exit=zero_exit, ccache=CCACHE_STATS,
                                               sccache=SCCACHE_STATS))

    def python_on_path(self):
        """`python` and `python3` on the scratch PATH, as a build script names them."""
        if WINDOWS:
            # The interpreter's own folder: a batch file must find a program, since calling a `.cmd` launcher
            # without `call` would end it.
            os.environ["PATH"] = os.environ["PATH"] + os.pathsep + str(pathlib.Path(sys.executable).parent)
            return
        for name in ("python", "python3"):
            launcher = self.bin / name
            launcher.write_text(f'#!/bin/sh\nexec "{sys.executable}" "$@"\n', encoding="utf-8")
            launcher.chmod(0o755)

    def calls(self):
        if not self.log.is_file():
            return []
        return [json.loads(line) for line in self.log.read_text(encoding="utf-8").splitlines()]

    def setup(self, *argv):
        out, err = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            code = cc_cache.main([str(arg) for arg in argv], root=self.project)
        return code, out.getvalue(), err.getvalue()

    def activated(self):
        """The environment of a shell after it evaluated the statement: the wrapper folder first on PATH."""
        return dict(os.environ, PATH=str(self.folder) + os.pathsep + os.environ["PATH"])

    def compile(self, name="cc", *arguments, env=None):
        """Run a compiler by name, as a build does: (exit code, the calls logged since)."""
        before = len(self.calls())
        found = shutil.which(name, path=(env or self.activated())["PATH"])
        self.assertIsNotNone(found, f"{name} is not on PATH")
        ran = subprocess.run([found, *arguments], env=env or self.activated(), capture_output=True, text=True,
                             timeout=120, creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
        return ran.returncode, self.calls()[before:]

    def holds(self, path, name="cc"):
        """Whether a wrapper names a path: quoted for sh on POSIX; on Windows as it is, whatever the letter case
        of its extension (the lookup adds the extension as PATHEXT spells it)."""
        text = self.wrapper(name).read_text(encoding="utf-8")
        if WINDOWS:
            return str(path).lower() in text.lower()
        return cc_cache._sh(path) in text

    def wrapper(self, name="cc"):
        return self.folder / (name + ".cmd" if WINDOWS else name)


class WrapperTests(Fixture):
    def test_a_compiler_run_by_name_goes_through_the_cache_with_the_real_compilers_path(self):
        real = self.compiler()
        cache = self.cache()
        code, out, err = self.setup("cc")
        self.assertEqual(code, 0, err)
        self.assertEqual(out.strip(), cc_cache.statement(self.folder, "powershell" if WINDOWS else "sh"))
        self.assertEqual(err, f"cc-cache: ccache in front of cc ({self.folder})\n")
        self.assertTrue(self.wrapper().is_file())
        code, calls = self.compile("cc", "-c", "a b.c", "-o", "a.o")
        self.assertEqual(code, 0)
        self.assertEqual(calls[0]["cache"], "ccache")
        # The cache is given the real compiler's full path, never its name: the name is the wrapper.
        given, *arguments = calls[0]["argv"]
        self.assertTrue(os.path.samefile(given, real))
        self.assertEqual(arguments, ["-c", "a b.c", "-o", "a.o"])
        self.assertEqual(calls[1], {"compiler": "cc", "argv": ["-c", "a b.c", "-o", "a.o"]})
        self.assertEqual(self.compile("cc", "--fail")[0], 7)   # the compiler's exit code comes through
        self.assertEqual(cc_cache.recorded_choice(self.project)[0], "ccache")
        self.assertTrue(os.path.samefile(cc_cache.recorded_choice(self.project)[1], cache))

    def test_the_statement_names_the_folder_in_each_shells_form(self):
        folder = pathlib.Path("/home/o'neil/cache dir") if not WINDOWS else pathlib.Path(r"C:\Users\o'neil\cache dir")
        self.assertEqual(cc_cache.statement(folder, "sh"), f"export PATH='{str(folder).replace(chr(39), chr(39) + chr(34) + chr(39) + chr(34) + chr(39))}':\"$PATH\"")
        self.assertEqual(cc_cache.statement(folder, "powershell"),
                         f"$env:PATH = '{str(folder).replace(chr(39), chr(39) * 2)}' + [IO.Path]::PathSeparator + $env:PATH")
        self.assertEqual(cc_cache.statement(folder, "cmd"), f'set "PATH={folder};%PATH%"')
        self.assertEqual(cc_cache.comment("off", "cmd"), "rem off")
        self.assertEqual(cc_cache.comment("off", "sh"), "# off")
        for unsafe in ('C:\\a"b', "C:\\100%\\x"):
            with self.assertRaises(cc_cache.Refused):
                cc_cache.statement(unsafe, "cmd")
            with self.assertRaises(cc_cache.Refused):
                cc_cache.wrapper_text("C:\\ccache.exe", unsafe, windows=True)

    def test_a_name_that_is_not_a_bare_name_is_refused(self):
        self.cache()
        for name in ("../cc", "cc;rm", "a b", "/usr/bin/cc"):
            with self.subTest(name=name):
                code, out, err = self.setup(name)
                self.assertEqual((code, out), (2, ""))
                self.assertIn("is not a compiler's name", err)
        self.assertFalse(self.folder.exists())

    def test_a_second_run_with_the_folder_on_path_still_finds_the_real_compiler(self):
        real = self.compiler()
        self.cache()
        self.setup("cc")
        before = (self.wrapper().read_bytes(), self.wrapper().stat().st_mtime_ns)
        with unittest.mock.patch.dict(os.environ, {"PATH": self.activated()["PATH"]}):
            code, out, _err = self.setup("cc")
            self.assertEqual(code, 0)
            self.assertTrue(os.path.samefile(cc_cache.find("cc", skip=[self.folder]), real))
        self.assertEqual((self.wrapper().read_bytes(), self.wrapper().stat().st_mtime_ns), before)
        self.assertTrue(self.holds(real), "the wrapper does not name the real compiler")
        code, calls = self.compile("cc", "x.c")
        self.assertEqual((code, len(calls)), (0, 2), "the wrapper reached itself")

    def test_a_wrapper_is_rewritten_only_when_it_would_change(self):
        self.compiler()
        self.cache()
        self.setup("cc")
        os.utime(self.wrapper(), ns=(10**18, 10**18))
        self.setup("cc")
        self.assertEqual(self.wrapper().stat().st_mtime_ns, 10**18)
        # The real compiler moved: the wrapper follows it.
        elsewhere = self.dir / "other bin"
        elsewhere.mkdir()
        moved = self.compiler(folder=elsewhere)
        (self.bin / ("cc.cmd" if WINDOWS else "cc")).unlink()
        with unittest.mock.patch.dict(os.environ, {"PATH": os.environ["PATH"] + os.pathsep + str(elsewhere)}):
            self.setup("cc")
        self.assertNotEqual(self.wrapper().stat().st_mtime_ns, 10**18)
        self.assertTrue(self.holds(moved), "the wrapper does not name the compiler where it now is")
        self.assertEqual([path.name for path in self.folder.iterdir() if path.name.endswith(".tmp")], [])

    @unittest.skipIf(WINDOWS, "a compiler that is a link to another is how POSIX installs clang++")
    def test_a_compiler_that_is_a_link_is_run_by_its_own_name(self):
        # Found by the review round: the link was resolved, so clang++ ran as clang and lost its C++ mode.
        real = self.compiler("clang")
        (self.bin / "clang++").symlink_to(real)
        self.cache()
        self.assertEqual(self.setup("clang++")[0], 0)
        code, calls = self.compile("clang++", "main.cpp")
        self.assertEqual(code, 0)
        self.assertEqual(calls[0]["argv"][0], str(self.bin / "clang++"))
        self.assertEqual(cc_cache.find("clang++"), self.bin / "clang++")

    @unittest.skipUnless(WINDOWS, "an extension in a compiler's name is a Windows matter")
    def test_a_compiler_named_with_its_extension_is_refused_and_not_left_uncached_in_silence(self):
        # Found by the review round: clang.exe got a wrapper named clang.exe.cmd, which no shell runs.
        self.compiler("clang")
        self.cache()
        for name in ("clang.exe", "clang.CMD"):
            code, out, err = self.setup(name)
            self.assertEqual((code, out), (2, ""))
            self.assertEqual(err, f"cc-cache: name the compiler without its extension: clang, not {name}\n")
        self.assertFalse(self.folder.exists())
        self.assertEqual(self.setup("clang")[0], 0)

    def test_two_setups_of_one_project_take_turns(self):
        # Found by the review round: one setup's cleanup removed the file another was still writing.
        for name in ("cc", "c++", "clang"):
            self.compiler(name)
        self.cache()
        sets = [["cc"], ["cc", "c++"], ["clang"], ["c++", "clang"], ["cc", "c++", "clang"], ["c++"]] * 2
        script = str(self.project / "scripts/cc-cache.py")
        running = [subprocess.Popen([sys.executable, "-B", script, "--shell", "sh", *names],
                                    stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
                                    creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0)) for names in sets]
        results = [(proc.communicate(timeout=120), proc.returncode) for proc in running]
        for (out, err), code in results:
            self.assertEqual(code, 0, err)
            self.assertTrue(out.startswith("export PATH="), out)
        # The folder is one setup's decision, whole: its wrappers and the choice, with nothing half-written.
        left = sorted(path.name for path in self.folder.iterdir())
        allowed = [sorted([cc_cache.CHOICE, *[self.wrapper(name).name for name in names]]) for names in sets]
        self.assertIn(left, allowed)
        self.assertEqual([path.name for path in self.folder.parent.iterdir() if path.name.endswith(".lock")], [])
        # A lock another setup holds is waited for, then reported; one a dead setup left is taken over.
        lock = self.folder.parent / (self.folder.name + ".lock")
        lock.write_text("", encoding="utf-8")
        with unittest.mock.patch.object(cc_cache, "LOCK_WAIT", 0.3):
            code, out, err = self.setup("cc")
        self.assertEqual((code, out), (1, ""))
        self.assertEqual(err, f"cc-cache: another setup has held {lock} for 0.3 seconds\n")
        old = lock.stat().st_mtime - cc_cache.LOCK_STALE - 5
        os.utime(lock, (old, old))
        self.assertEqual(self.setup("cc")[0], 0)
        self.assertFalse(lock.exists())

    def test_a_compiler_that_is_not_installed_gets_no_wrapper_and_the_others_get_theirs(self):
        self.compiler()
        self.cache()
        code, out, err = self.setup("cc", "c++")
        self.assertEqual(code, 0)
        self.assertIn("cc-cache: c++ is not installed: no wrapper for it\n", err)
        self.assertIn("ccache in front of cc (", err)
        self.assertTrue(self.wrapper("cc").is_file())
        self.assertFalse(self.wrapper("c++").exists())
        self.assertTrue(out.strip())


class OptOutTests(Fixture):
    def test_no_cache_or_the_opt_out_writes_nothing_and_prints_only_a_comment(self):
        self.compiler()
        code, out, err = self.setup("--shell", "sh", "cc")
        self.assertEqual((code, out), (0, "# cc-cache: off: no compiler cache is installed (ccache or sccache)\n"))
        self.assertEqual(err, "cc-cache: off: no compiler cache is installed (ccache or sccache)\n")
        self.assertFalse(self.folder.exists())
        self.cache()
        with unittest.mock.patch.dict(os.environ, {"BUILD_NO_CCACHE": "1"}):
            for shell, mark in (("sh", "#"), ("powershell", "#"), ("cmd", "rem")):
                code, out, err = self.setup("--shell", shell, "cc")
                self.assertEqual((code, out), (0, f"{mark} cc-cache: off: BUILD_NO_CCACHE is set\n"))
        self.assertFalse(self.folder.exists())
        code, out, err = self.setup("--cache", "sccache", "cc")
        self.assertEqual((code, err), (0, "cc-cache: off: sccache is not installed\n"))
        with unittest.mock.patch.dict(os.environ, {"BUILD_NO_CCACHE": ""}):   # set to nothing is not set
            self.assertEqual(self.setup("cc")[1].strip(), cc_cache.statement(self.folder, "powershell" if WINDOWS
                                                                             else "sh"))

    def test_opting_out_after_activation_reaches_the_real_compiler_in_the_same_shell(self):
        self.compiler()
        self.cache()
        self.setup("cc")
        shell = self.activated()   # the folder stays first on this shell's PATH from here on
        self.assertEqual([call.get("cache") for call in self.compile("cc", "a.c", env=shell)[1]], ["ccache", None])
        # Only the variable set, nothing rerun: the wrapper stands aside by itself.
        code, calls = self.compile("cc", "a.c", "--fail", env=dict(shell, BUILD_NO_CCACHE="1"))
        self.assertEqual((code, calls), (7, [{"compiler": "cc", "argv": ["a.c", "--fail"]}]))
        # Rerun with the opt-out: the wrappers are gone and the folder on PATH holds nothing.
        with unittest.mock.patch.dict(os.environ, {"BUILD_NO_CCACHE": "1", "PATH": shell["PATH"]}):
            self.assertEqual(self.setup("cc")[0], 0)
        self.assertEqual(list(self.folder.iterdir()), [])
        code, calls = self.compile("cc", "a b.c", env=shell)
        self.assertEqual((code, calls), (0, [{"compiler": "cc", "argv": ["a b.c"]}]))

    def test_a_cache_removed_after_activation_leaves_a_compiler_that_still_runs(self):
        self.compiler()
        cache = self.cache()
        self.setup("cc")
        cache.unlink()
        code, calls = self.compile("cc", "a.c", "--fail")
        self.assertEqual((code, calls), (7, [{"compiler": "cc", "argv": ["a.c", "--fail"]}]))
        # And the next run, finding no cache, removes the wrapper.
        with unittest.mock.patch.dict(os.environ, {"PATH": self.activated()["PATH"]}):
            self.assertEqual(self.setup("cc")[2], "cc-cache: off: no compiler cache is installed (ccache or sccache)\n")
        self.assertEqual(list(self.folder.iterdir()), [])

    def test_wrappers_of_compilers_no_longer_named_or_installed_are_removed(self):
        self.compiler("cc")
        gxx = self.compiler("c++")
        self.cache()
        self.setup("cc", "c++")
        self.assertEqual(sorted(path.name for path in self.folder.iterdir()),
                         sorted([cc_cache.CHOICE, self.wrapper("cc").name, self.wrapper("c++").name]))
        self.setup("cc")   # c++ is no longer named
        self.assertFalse(self.wrapper("c++").exists())
        self.setup("cc", "c++")
        gxx.unlink()       # c++ is no longer installed
        self.setup("cc", "c++")
        self.assertFalse(self.wrapper("c++").exists())
        self.assertTrue(self.wrapper("cc").is_file())
        (self.bin / ("cc.cmd" if WINDOWS else "cc")).unlink()
        code, out, err = self.setup("--shell", "sh", "cc", "c++")
        self.assertEqual((code, out), (0, "# cc-cache: off: none of the compilers named is installed\n"))
        self.assertEqual(list(self.folder.iterdir()), [])


class ShellTests(Fixture):
    """The printed statement evaluated by the shell it is for, and the compiler then run by name in that shell."""

    def expect_cached(self, calls, cache="ccache"):
        self.assertEqual(calls[0]["cache"], cache)
        self.assertEqual(calls[0]["argv"][1:], ["-c", "a b.c", "-DNAME=it's"])
        self.assertEqual(calls[1]["argv"], ["-c", "a b.c", "-DNAME=it's"])

    def run_shell(self, argv, script=None):
        self.compiler()
        self.cache()
        ran = subprocess.run(argv, input=script, cwd=self.project, capture_output=True, text=True, timeout=180,
                             creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
        return ran.returncode, ran.stdout, ran.stderr

    @unittest.skipIf(WINDOWS, "sh is the POSIX shell")
    def test_sh_evaluates_the_statement_and_runs_the_compiler_through_the_cache(self):
        script = (f'setup=$("{sys.executable}" scripts/cc-cache.py --shell sh cc) || exit 1\n'
                  'eval "$setup"\n'
                  'cc -c "a b.c" "-DNAME=it\'s"\n'
                  'cc --fail\n'
                  'echo "code=$?"\n')
        code, out, err = self.run_shell(["/bin/sh", "-s"], script)
        self.assertEqual((code, out.strip()), (0, "code=7"), err)
        self.expect_cached(self.calls())

    @unittest.skipUnless(POWERSHELL, "needs PowerShell")
    def test_powershell_evaluates_the_statement_and_runs_the_compiler_through_the_cache(self):
        shell = POWERSHELL
        script = (f"$setup = & '{sys.executable}' scripts/cc-cache.py --shell powershell cc\n"
                  "if ($LASTEXITCODE) { exit 1 }\n"
                  "$setup | Invoke-Expression\n"
                  "cc -c 'a b.c' '-DNAME=it''s'\n"
                  "cc --fail\n"
                  "Write-Output \"code=$LASTEXITCODE\"\n")
        path = self.dir / "build.ps1"
        path.write_text(script, encoding="utf-8")
        code, out, err = self.run_shell([shell, "-NoProfile", "-NonInteractive", "-File", str(path)])
        self.assertEqual((code, out.strip().splitlines()[-1]), (0, "code=7"), err)
        self.expect_cached(self.calls())

    @unittest.skipUnless(WINDOWS, "cmd is the Windows shell")
    def test_cmd_evaluates_the_statement_and_runs_the_compiler_through_the_cache(self):
        script = ("@echo off\r\n"
                  'set "CC_CACHE_SETUP=%TEMP%\\cc-cache-setup-%RANDOM%.cmd"\r\n'
                  f'"{sys.executable}" scripts\\cc-cache.py --shell cmd cc > "%CC_CACHE_SETUP%" || exit /b 1\r\n'
                  'call "%CC_CACHE_SETUP%"\r\n'
                  'del "%CC_CACHE_SETUP%"\r\n'
                  'call cc -c "a b.c" "-DNAME=it\'s"\r\n'
                  "call cc --fail\r\n"
                  "echo code=%errorlevel%\r\n")
        path = self.dir / "build.cmd"
        path.write_text(script, encoding="utf-8")
        env_cmd = os.environ.get("COMSPEC", "cmd.exe")
        code, out, err = self.run_shell([env_cmd, "/d", "/c", str(path)])
        self.assertEqual((code, out.strip().splitlines()[-1]), (0, "code=7"), err)
        self.expect_cached(self.calls())


class DocumentedLinesTests(Fixture):
    """The build-script lines the project's document gives, run as they are written."""

    def block(self, language):
        blocks = dict(re.findall(r"```(\w+)\n(.*?)```", self.section(), flags=re.DOTALL))
        self.assertIn(language, blocks, f"the document has no {language} block for the compiler cache")
        return blocks[language]

    def section(self):
        text = DOCUMENT.read_text(encoding="utf-8")
        return text.split("## The compiler cache\n", 1)[1].split("\n## ", 1)[0]

    def test_the_statistics_lines_read_the_cache_the_setup_chose(self):
        self.compiler("cc")
        self.cache("sccache")
        self.assertEqual(self.setup("cc")[0], 0)
        self.assertEqual(self.compile("cc", "-c", "x.c")[1][0]["cache"], "sccache")
        # On PATH only now, and the first by preference: the statistics still read what the setup chose.
        self.cache("ccache")
        lines = [line.split()[2:] for line in self.section().splitlines()
                 if line.startswith("python scripts/cache-stats.py")]
        self.assertEqual(lines, [["--zero"], ["--min-hit-rate", "60"], ["--json"]])

        def run(argv):
            ran = subprocess.run([sys.executable, "-B", "scripts/cache-stats.py", *argv], cwd=self.project,
                                 capture_output=True, text=True, timeout=120,
                                 creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
            return ran.returncode, ran.stdout, ran.stderr

        code, out, err = run(lines[1])   # the stub's captured figures: two hits, one miss
        self.assertEqual((code, err), (0, ""))
        self.assertTrue(out.startswith("sccache (") and out.endswith("hit rate 66.67%\n"), out)
        code, out, err = run(lines[0])
        self.assertEqual((code, err), (0, ""))
        self.assertTrue(out.startswith("sccache (") and out.endswith("statistics reset\n"), out)
        # After a reset and no compile: the document says exit 3.
        code, out, err = run(lines[1])
        self.assertEqual((code, err), (3, "cache-stats: no compile went through the cache\n"))
        code, out, err = run(lines[2])
        self.assertEqual(code, 0, err)
        self.assertEqual((json.loads(out)["cache"], json.loads(out)["hits"]), ("sccache", 0))

    def unwritable(self):
        """A file where the wrapper folder's parent should be: the folder cannot be made."""
        (self.home / "conclave").write_text("not a folder", encoding="utf-8")

    @unittest.skipIf(WINDOWS, "sh is the POSIX shell")
    def test_the_sh_lines_stop_the_build_when_the_setup_fails_and_activate_when_it_does_not(self):
        self.python_on_path()
        self.compiler("cc")
        self.compiler("c++")
        self.cache()
        lines = self.block("sh")
        self.assertIn("|| exit 1", lines)
        script = lines + 'cc -c x.c\necho "built"\n'
        ran = subprocess.run(["/bin/sh", "-s"], input=script, cwd=self.project, capture_output=True, text=True,
                             timeout=180)
        self.assertEqual((ran.returncode, ran.stdout.strip()), (0, "built"), ran.stderr)
        self.assertEqual(self.calls()[0]["cache"], "ccache")
        shutil.rmtree(self.home / "conclave")
        self.unwritable()
        ran = subprocess.run(["/bin/sh", "-s"], input=script, cwd=self.project, capture_output=True, text=True,
                             timeout=180)
        self.assertNotEqual(ran.returncode, 0)
        self.assertNotIn("built", ran.stdout)
        self.assertIn("cannot be written", ran.stderr)

    @unittest.skipUnless(POWERSHELL, "needs PowerShell")
    def test_the_powershell_lines_stop_the_build_when_the_setup_fails_and_activate_when_it_does_not(self):
        shell = POWERSHELL
        self.python_on_path()
        self.compiler("clang")
        self.cache()
        lines = self.block("powershell")
        self.assertIn("$LASTEXITCODE", lines)
        path = self.dir / "build.ps1"
        path.write_text(lines + "clang -c x.c\nWrite-Output 'built'\n", encoding="utf-8")
        argv = [shell, "-NoProfile", "-NonInteractive", "-File", str(path)]
        ran = subprocess.run(argv, cwd=self.project, capture_output=True, text=True, timeout=180,
                             creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
        self.assertEqual((ran.returncode, ran.stdout.strip().splitlines()[-1]), (0, "built"), ran.stderr)
        self.assertEqual(self.calls()[0]["cache"], "ccache")
        shutil.rmtree(self.home / "conclave")
        self.unwritable()
        ran = subprocess.run(argv, cwd=self.project, capture_output=True, text=True, timeout=180,
                             creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
        self.assertNotEqual(ran.returncode, 0)
        self.assertNotIn("built", ran.stdout)

    @unittest.skipUnless(WINDOWS, "cmd is the Windows shell")
    def test_the_batch_lines_stop_the_build_when_the_setup_fails_and_activate_when_it_does_not(self):
        self.python_on_path()
        self.compiler("clang")
        self.cache()
        lines = self.block("bat")
        self.assertIn("|| exit /b 1", lines)
        path = self.dir / "build.cmd"
        path.write_text("@echo off\r\n" + lines.replace("\n", "\r\n") + "call clang -c x.c\r\necho built\r\n",
                        encoding="utf-8")
        argv = [os.environ.get("COMSPEC", "cmd.exe"), "/d", "/c", str(path)]
        ran = subprocess.run(argv, cwd=self.project, capture_output=True, text=True, timeout=180,
                             creationflags=subprocess.CREATE_NO_WINDOW)
        self.assertEqual((ran.returncode, ran.stdout.strip().splitlines()[-1]), (0, "built"), ran.stderr)
        self.assertEqual(self.calls()[0]["cache"], "ccache")
        shutil.rmtree(self.home / "conclave")
        self.unwritable()
        ran = subprocess.run(argv, cwd=self.project, capture_output=True, text=True, timeout=180,
                             creationflags=subprocess.CREATE_NO_WINDOW)
        self.assertNotEqual(ran.returncode, 0)
        self.assertNotIn("built", ran.stdout)

    def test_a_wrapper_that_cannot_be_written_is_exit_1_with_one_line_and_nothing_to_evaluate(self):
        self.compiler()
        self.cache()
        self.setup("cc")
        before = self.wrapper().read_bytes()
        elsewhere = self.dir / "other bin"
        elsewhere.mkdir()
        self.compiler(folder=elsewhere)
        (self.bin / ("cc.cmd" if WINDOWS else "cc")).unlink()

        def replace(*args):
            raise PermissionError(13, "Permission denied")

        with unittest.mock.patch.dict(os.environ, {"PATH": os.environ["PATH"] + os.pathsep + str(elsewhere)}), \
                unittest.mock.patch.object(cc_cache.os, "replace", replace):
            code, out, err = self.setup("cc")
        self.assertEqual((code, out), (1, ""))
        self.assertEqual(err, f"cc-cache: {self.folder}: cannot be written (Permission denied)\n")
        self.assertEqual(self.wrapper().read_bytes(), before)   # the earlier wrapper, whole
        self.assertEqual([path.name for path in self.folder.iterdir() if path.name.endswith(".tmp")], [])


class StatsTests(Fixture):
    def stats(self, *argv, runner=None):
        out, err = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            try:
                code = cache_stats.main([str(arg) for arg in argv], root=self.project, runner=runner)
            except SystemExit as exc:
                code = exc.code
        return code, out.getvalue(), err.getvalue()

    def test_the_captured_statistics_of_each_cache_give_two_hits_one_miss_and_the_rate(self):
        for kind in ("ccache", "sccache"):
            with self.subTest(cache=kind):
                self.cache(kind)
                code, out, err = self.stats("--cache", kind)
                self.assertEqual((code, err), (0, ""))
                self.assertEqual(out, f"{kind} (named by --cache): 2 hit(s), 1 miss(es), hit rate 66.67%\n")
                self.assertEqual(self.stats("--cache", kind, "--min-hit-rate", "60")[0], 0)
                code, _out, err = self.stats("--cache", kind, "--min-hit-rate", "70")
                self.assertEqual((code, err), (3, "cache-stats: the hit rate 66.67% is below 70%\n"))
                code, out, _err = self.stats("--cache", kind, "--json")
                self.assertEqual(json.loads(out)["hit_rate"], 66.67)
                self.assertEqual((json.loads(out)["hits"], json.loads(out)["misses"]), (2, 1))

    def test_zero_resets_the_cache_and_reads_it_back_and_no_compile_fails_the_threshold(self):
        for kind in ("ccache", "sccache"):
            with self.subTest(cache=kind):
                self.cache(kind)
                code, out, err = self.stats("--cache", kind, "--zero")
                self.assertEqual((code, out, err), (0, f"{kind} (named by --cache): statistics reset\n", ""))
                code, out, _err = self.stats("--cache", kind)
                self.assertEqual((code, out), (0, f"{kind} (named by --cache): no compile went through the cache\n"))
                code, _out, err = self.stats("--cache", kind, "--min-hit-rate", "1")
                self.assertEqual((code, err), (3, "cache-stats: no compile went through the cache\n"))

    def test_a_reset_that_changes_nothing_or_fails_is_refused(self):
        self.cache("ccache", resets=False)
        code, out, err = self.stats("--cache", "ccache", "--zero")
        self.assertEqual((code, out), (2, ""))
        self.assertEqual(err, "cache-stats: ccache --zero-stats did not reset the statistics: they still show 2 "
                              "hit(s) and 1 miss(es)\n")
        self.cache("sccache", zero_exit=4)
        code, _out, err = self.stats("--cache", "sccache", "--zero")
        self.assertEqual((code, err), (2, "cache-stats: sccache --zero-stats exited 4\n"))

    def test_the_cache_the_wrappers_run_is_the_one_read_reset_and_judged(self):
        self.compiler()
        self.cache("ccache")
        self.cache("sccache")
        (self.dir / "ccache.state").write_text(json.dumps({"hits": 9, "misses": 1}), encoding="utf-8")
        # No wrappers yet: the first on PATH.
        self.assertEqual(self.stats()[1], "ccache (the first on PATH): 9 hit(s), 1 miss(es), hit rate 90.00%\n")
        self.assertEqual(self.setup("--cache", "sccache", "cc")[0], 0)
        self.assertEqual(self.stats()[1], "sccache (the cache this project's wrappers run): 2 hit(s), 1 miss(es), "
                                          "hit rate 66.67%\n")
        self.assertEqual(self.stats("--min-hit-rate", "80")[0], 3)             # sccache's 66.67, not ccache's 90
        self.assertEqual(self.stats("--cache", "ccache", "--min-hit-rate", "80")[0], 0)
        self.assertEqual(self.stats("--zero")[0], 0)
        self.assertEqual(json.loads((self.dir / "sccache.state").read_text()), {"hits": 0, "misses": 0})
        self.assertEqual(json.loads((self.dir / "ccache.state").read_text()), {"hits": 9, "misses": 1})

    def test_each_way_the_cache_can_fail_has_its_own_line_and_exit_2(self):
        code, out, err = self.stats()
        self.assertEqual((code, out, err), (2, "", "cache-stats: no compiler cache is installed (ccache or sccache)\n"))
        self.assertEqual(self.stats("--cache", "sccache")[2], "cache-stats: sccache is not installed\n")
        self.cache("ccache")
        outcomes = [({"output": "", "exit": None, "stopped": None, "error": "Access is denied"},
                     "ccache --print-stats could not be started (Access is denied)"),
                    ({"output": "", "exit": None, "stopped": "time", "error": None},
                     "ccache --print-stats was stopped: it ran past the time limit"),
                    ({"output": "x" * 10, "exit": None, "stopped": "output", "error": None},
                     "ccache --print-stats was stopped: it printed more than the limit"),
                    ({"output": "boom", "exit": 1, "stopped": None, "error": None}, "ccache --print-stats exited 1"),
                    ({"output": "Cacheable calls: 3 / 3 (100.0%)\n", "exit": 0, "stopped": None, "error": None},
                     "ccache --print-stats printed what is not its statistics (no direct_cache_hit, "
                     "preprocessed_cache_hit, cache_miss)")]
        for result, line in outcomes:
            with self.subTest(line=line):
                code, out, err = self.stats("--cache", "ccache", runner=lambda argv, result=result: result)
                self.assertEqual((code, out, err), (2, "", f"cache-stats: {line}\n"))
        self.cache("sccache")
        for output in ("Compile requests 3\n", '{"stats": {"cache_hits": {"counts": {"C": "two"}}, '
                                               '"cache_misses": {"counts": {}}}}', '{"stats": []}'):
            with self.subTest(output=output[:20]):
                result = {"output": output, "exit": 0, "stopped": None, "error": None}
                code, _out, err = self.stats("--cache", "sccache", runner=lambda argv, result=result: result)
                self.assertEqual(code, 2)
                self.assertIn("sccache --show-stats --stats-format json printed what is not its statistics", err)
        # Bad usage is exit 2 too.
        self.assertEqual(self.stats("--zero", "--min-hit-rate", "50")[0], 2)
        self.assertEqual(self.stats("--min-hit-rate", "150")[0], 2)

    def test_the_real_statistics_as_captured_are_read(self):
        # ccache 4.12.3 and sccache 0.13.0, 2026-11-15, one miss and two hits each.
        ccache = {"output": CCACHE_STATS.format(hits=2, misses=1), "exit": 0, "stopped": None, "error": None}
        self.assertEqual(cache_stats.read("ccache", "ccache", lambda argv: ccache), (2, 1))
        text = SCCACHE_STATS.format(hits='{"C/C++":2}', misses='{"C/C++":1}')
        self.assertEqual(json.loads(text)["stats"]["cache_hits"]["adv_counts"], {"c [gcc]": 2})
        sccache = {"output": "sccache: warning on standard error\n" + text, "exit": 0, "stopped": None,
                   "error": None}
        self.assertEqual(cache_stats.read("sccache", "sccache", lambda argv: sccache), (2, 1))
        self.assertEqual(cache_stats.kind_of("C:/tools/sccache.exe"), "sccache")
        self.assertEqual(cache_stats.kind_of("/usr/bin/ccache"), "ccache")


if __name__ == "__main__":
    unittest.main()
