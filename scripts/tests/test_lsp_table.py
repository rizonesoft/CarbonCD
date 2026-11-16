"""The language server table: its rules, the presets, how a preset finds its app, the write, and lsp-servers.py."""
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
import unittest
import unittest.mock

SCRIPTS = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(SCRIPTS / "lib"))
import lsp_table  # noqa: E402

spec = importlib.util.spec_from_file_location("lsp_servers", SCRIPTS / "lsp-servers.py")
lsp_servers = importlib.util.module_from_spec(spec)
spec.loader.exec_module(lsp_servers)

TABLE = lsp_table.TABLE
CLANGD = {"name": "clangd", "command": ["clangd", "--background-index"],
          "extensions": {".c": "c", ".h": "c", ".cpp": "cpp"}, "install": "apt install clangd"}


# Every preset, resolved, in full: each value the kit promises a stack. The Laravel values are the ones the
# Laravel kit's client held for its three servers (its SERVERS table, read 2026-11-15),
# so a changed value fails here before it changes a project's diagnostics.
EXPECTED_PRESETS = {'cpp': [{'name': 'clangd',
          'command': ['clangd'],
          'extensions': {'.c': 'c',
                         '.h': 'c',
                         '.cc': 'cpp',
                         '.cpp': 'cpp',
                         '.cxx': 'cpp',
                         '.hh': 'cpp',
                         '.hpp': 'cpp',
                         '.hxx': 'cpp'},
          'install': 'apt install clangd (Windows: winget install LLVM.LLVM)',
          'root': '.',
          'requires': None,
          'init': {},
          'settings': None,
          'severity': 'error',
          'settle': 5,
          'note': 'reads compile_commands.json from the project root; without one it guesses the flags'}],
 'python': [{'name': 'basedpyright',
             'command': ['basedpyright-langserver', '--stdio'],
             'extensions': {'.py': 'python', '.pyi': 'python'},
             'install': 'pipx install basedpyright (or: uv tool install basedpyright)',
             'root': '.',
             'requires': None,
             'init': {},
             'settings': None,
             'severity': 'error',
             'settle': 5,
             'note': None}],
 'php': [{'name': 'intelephense',
          'command': ['intelephense', '--stdio'],
          'extensions': {'.php': 'php'},
          'install': 'npm install -g intelephense',
          'root': '.',
          'requires': None,
          'init': {},
          'settings': None,
          'severity': 'error',
          'settle': 10,
          'note': None}],
 'web': [{'name': 'typescript',
          'command': ['typescript-language-server', '--stdio'],
          'extensions': {'.ts': 'typescript',
                         '.tsx': 'typescriptreact',
                         '.js': 'javascript',
                         '.jsx': 'javascriptreact',
                         '.mjs': 'javascript',
                         '.cjs': 'javascript'},
          'install': 'npm install -g typescript-language-server typescript',
          'root': '.',
          'requires': None,
          'init': {},
          'settings': None,
          'severity': 'error',
          'settle': 5,
          'note': None},
         {'name': 'tailwindcss',
          'command': ['tailwindcss-language-server', '--stdio'],
          'extensions': {'.css': 'css',
                         '.html': 'html',
                         '.jsx': 'javascriptreact',
                         '.tsx': 'typescriptreact',
                         '.vue': 'vue'},
          'install': 'npm install -g @tailwindcss/language-server',
          'root': '.',
          'requires': None,
          'init': {},
          'settings': None,
          'severity': 'warning',
          'settle': 5,
          'note': 'its lint rules report at warning: two utilities setting one property is its main finding'}],
 'laravel': [{'name': 'laravel-lsp',
              'command': ['laravel-lsp'],
              'extensions': {'.php': 'php', '.blade.php': 'blade'},
              'install': 'composer global require laravel/lsp',
              'root': '.',
              'requires': 'artisan',
              'init': {'phpEnvironment': 'local', 'phpCommand': ['php'], 'pestGenerateDocBlocks': False},
              'settings': None,
              'severity': 'warning',
              'settle': 8,
              'note': 'string-keyed edges: route, config, view, env, middleware, policy; reports them at '
                      'warning'},
             {'name': 'intelephense',
              'command': ['intelephense', '--stdio'],
              'extensions': {'.php': 'php'},
              'install': 'npm install -g intelephense',
              'root': '.',
              'requires': 'composer.json',
              'init': {'storagePath': '${scratch}', 'clearCache': False},
              'settings': {'intelephense': {'files': {'maxSize': 3000000,
                                                      'exclude': ['**/node_modules/**',
                                                                  '**/bower_components/**',
                                                                  '**/vendor/**/{Tests,tests}/**',
                                                                  '**/.git/**']},
                                            'diagnostics': {'enable': True,
                                                            'typeErrors': False,
                                                            'argumentCount': True,
                                                            'undefinedTypes': True,
                                                            'undefinedFunctions': True,
                                                            'undefinedConstants': True,
                                                            'undefinedClassConstants': True,
                                                            'undefinedMethods': True,
                                                            'undefinedProperties': False,
                                                            'undefinedVariables': True,
                                                            'unusedSymbols': True,
                                                            'unexpectedTokens': True,
                                                            'duplicateSymbols': True,
                                                            'deprecated': True}}},
              'severity': 'error',
              'settle': 10,
              'note': 'on demand: Larastan and Pint own most of what it finds in a Laravel app'},
             {'name': 'tailwindcss',
              'command': ['tailwindcss-language-server', '--stdio'],
              'extensions': {'.blade.php': 'blade', '.css': 'css', '.php': 'php'},
              'install': 'npm install -g @tailwindcss/language-server',
              'root': '.',
              'requires': 'resources/css/app.css',
              'init': {'userLanguages': {'blade': 'html'}},
              'settings': {'tailwindCSS': {'validate': True,
                                           'lint': {'cssConflict': 'warning',
                                                    'invalidApply': 'error',
                                                    'invalidScreen': 'error',
                                                    'invalidVariant': 'error',
                                                    'invalidConfigPath': 'error',
                                                    'invalidTailwindDirective': 'error',
                                                    'recommendedVariantOrder': 'warning',
                                                    'suggestCanonicalClasses': 'ignore'},
                                           'includeLanguages': {'blade': 'html', 'php': 'html'},
                                           'emmetCompletions': False,
                                           'showPixelEquivalents': True},
                           'editor': {'tabSize': 4}},
              'severity': 'warning',
              'settle': 6,
              'note': 'its lint rules report at warning: two utilities setting one property is its main '
                      'finding'}]}


class Fixture(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.root = pathlib.Path(tmp.name).resolve()
        (self.root / ".conclave").mkdir()
        self.table = self.root / TABLE
        self.write({"schema_version": 1, "servers": []})

    def write(self, document):
        data = document if isinstance(document, bytes) else json.dumps(document).encode("utf-8")
        self.table.write_bytes(data)
        return self.table

    def servers(self, *entries):
        return self.write({"schema_version": 1, "servers": list(entries)})

    def refusal(self):
        with self.assertRaises(lsp_table.TableError) as caught:
            lsp_table.load_servers(self.root)
        self.assertTrue(str(caught.exception).startswith(TABLE + ": "), caught.exception)
        return str(caught.exception)


class LoadServersTests(Fixture):
    def test_a_table_loads_with_its_defaults_filled_in(self):
        self.assertEqual(lsp_table.load_servers(self.root), [])
        full = dict(CLANGD, name="laravel-lsp", root="platform", requires="artisan", init={"a": 1},
                    settings={"b": {"c": 2}}, severity="warning", settle=8.5, note="one line")
        self.servers(CLANGD, full)
        first, second = lsp_table.load_servers(self.root)
        self.assertEqual(first, dict(CLANGD, root=".", requires=None, init={}, settings=None, severity="error",
                                     settle=5, note=None))
        self.assertEqual(second, full)

    def test_a_file_that_is_not_a_table_is_refused_naming_it(self):
        self.table.unlink()
        self.assertIn("cannot be read", self.refusal())
        self.write(b"\xff\xfe{}")
        self.assertIn("not UTF-8", self.refusal())
        denied = PermissionError(13, "Permission denied")
        with unittest.mock.patch.object(pathlib.Path, "read_bytes", side_effect=denied):
            self.assertIn("cannot be read (Permission denied)", self.refusal())
        cases = {b"{": "not JSON", b"null": "an object with the keys", b"[]": "an object with the keys",
                 b'{"schema_version": 1}': "missing key 'servers'",
                 b'{"schema_version": 1, "servers": [], "extra": 1}': "unknown key 'extra'",
                 b'{"schema_version": 2, "servers": []}': "schema_version: must be 1",
                 b'{"schema_version": true, "servers": []}': "schema_version: must be 1",
                 b'{"schema_version": 1, "servers": {}}': "servers: a list",
                 b'{"schema_version": 1, "servers": ["clangd"]}': "server 1: a server is an object"}
        for data, expected in cases.items():
            with self.subTest(data=data):
                self.write(data)
                self.assertIn(expected, self.refusal())

    def test_each_fault_in_an_entry_is_refused_naming_the_entry_and_the_key(self):
        def entry(**changes):
            return dict(CLANGD, **changes)

        without = {key: value for key, value in CLANGD.items() if key != "install"}
        cases = [
            (entry(name="Clangd"), "server 1: name:"),
            (entry(name=""), "server 1: name:"),
            ({key: value for key, value in CLANGD.items() if key != "name"}, "server 1: name:"),
            (entry(extra=1), "server 'clangd': unknown key 'extra'"),
            (entry(language="c"), "server 'clangd': unknown key 'language'"),
            (without, "server 'clangd': missing key 'install'"),
            (entry(command="clangd --background-index"), "server 'clangd': command:"),
            (entry(command=[]), "server 'clangd': command:"),
            (entry(command=["clangd", ""]), "server 'clangd': command:"),
            (entry(command=["clangd", 4]), "server 'clangd': command:"),
            (entry(extensions=[".c", ".h"]), "server 'clangd': extensions:"),
            (entry(extensions={}), "server 'clangd': extensions:"),
            (entry(extensions={"c": "c"}), "server 'clangd': extensions: a suffix starts with a dot"),
            (entry(extensions={".": "c"}), "server 'clangd': extensions: a suffix starts with a dot"),
            (entry(extensions={".c": ""}), "server 'clangd': extensions: the language id of '.c'"),
            (entry(extensions={".c": 3}), "server 'clangd': extensions: the language id of '.c'"),
            (entry(install="one\ntwo"), "server 'clangd': install: a one-line hint"),
            (entry(install=" "), "server 'clangd': install: a one-line hint"),
            (entry(root="/srv/app"), "server 'clangd': root:"),
            (entry(root="../elsewhere"), "server 'clangd': root:"),
            (entry(root="C:/app"), "server 'clangd': root:"),
            (entry(root="app\\src"), "server 'clangd': root:"),
            (entry(requires="/etc/passwd"), "server 'clangd': requires:"),
            (entry(requires="a/../../b"), "server 'clangd': requires:"),
            (entry(severity="hint"), "server 'clangd': severity: error or warning"),
            (entry(settle="5"), "server 'clangd': settle:"),
            (entry(settle=True), "server 'clangd': settle:"),
            (entry(settle=0), "server 'clangd': settle:"),
            (entry(settle=121), "server 'clangd': settle:"),
            (entry(init=[]), "server 'clangd': init: an object"),
            (entry(settings="x"), "server 'clangd': settings: an object"),
            (entry(note="a\nb"), "server 'clangd': note: one line"),
        ]
        for bad, expected in cases:
            with self.subTest(expected=expected, entry=str(bad)[:50]):
                self.servers(bad)
                self.assertIn(expected, self.refusal())
        self.servers(CLANGD, CLANGD)
        self.assertIn("server 'clangd': name: used by two servers", self.refusal())


class RootAndProgramTests(Fixture):
    """Found by the round of D04 T01 section 2."""

    def test_a_root_that_names_the_project_itself_is_the_project(self):
        for value in (".", "./"):
            with self.subTest(root=value):
                self.servers(dict(CLANGD, root=value))
                self.assertEqual(lsp_table.load_servers(self.root)[0]["root"], ".")
        self.servers(dict(CLANGD, root="./app/"))
        self.assertEqual(lsp_table.load_servers(self.root)[0]["root"], "app")
        for value in (".", "./"):  # a file cannot be the folder itself
            with self.subTest(requires=value):
                self.servers(dict(CLANGD, requires=value))
                self.assertIn("server 'clangd': requires:", self.refusal())

    def program(self, folder, name, executable=True):
        folder.mkdir(parents=True, exist_ok=True)
        path = folder / (f"{name}.cmd" if os.name == "nt" else name)
        path.write_text("@exit /b 0\r\n" if os.name == "nt" else "#!/bin/sh\nexit 0\n", encoding="utf-8")
        path.chmod(0o755 if executable else 0o644)
        return path

    def on_path(self, *folders):
        env = {"PATH": os.pathsep.join(str(folder) for folder in folders)}
        if os.name == "nt":
            env["PATHEXT"] = ".CMD;.EXE"
        patch = unittest.mock.patch.dict(os.environ, env)
        patch.start()
        self.addCleanup(patch.stop)

    def entry(self, **changes):
        self.servers(dict(CLANGD, **changes))
        return lsp_table.load_servers(self.root)[0]

    def test_a_bare_name_is_found_on_path_and_never_in_the_working_directory(self):
        elsewhere = self.root / "elsewhere"
        self.program(elsewhere, "clangd")
        self.on_path(self.root / "empty-bin")
        entry = self.entry()
        held = os.getcwd()
        os.chdir(elsewhere)  # a file of that name here has nothing to do with the project
        try:
            self.assertIsNone(lsp_table.find_program(self.root, entry))
        finally:
            os.chdir(held)
        found = self.program(self.root / "empty-bin", "clangd")
        self.assertTrue(os.path.samefile(lsp_table.find_program(self.root, entry), found))

    def test_a_program_named_by_a_path_is_read_from_the_servers_folder_not_the_working_directory(self):
        self.on_path(self.root / "empty-bin")
        elsewhere = self.root / "elsewhere"
        self.program(elsewhere / "bin", "server")
        entry = self.entry(command=["./bin/server"], root="app")
        (self.root / "app").mkdir()
        held = os.getcwd()
        os.chdir(elsewhere)  # holds bin/server; the project's app/ folder does not
        try:
            self.assertIsNone(lsp_table.find_program(self.root, entry))
            found = self.program(self.root / "app/bin", "server")
            self.assertTrue(os.path.samefile(lsp_table.find_program(self.root, entry), found))
        finally:
            os.chdir(held)
        absolute = self.program(self.root / "tools", "abs-server")
        entry = self.entry(command=[absolute.as_posix()])
        self.assertTrue(os.path.samefile(lsp_table.find_program(self.root, entry), absolute))

    @unittest.skipIf(os.name == "nt", "Windows has no execute permission to withhold: a .cmd file runs")
    def test_a_file_the_system_will_not_execute_is_not_an_installed_program(self):
        self.on_path(self.root / "bin")
        self.program(self.root / "bin", "clangd", executable=False)
        self.assertIsNone(lsp_table.find_program(self.root, self.entry()))
        self.program(self.root / "app/bin", "server", executable=False)
        self.assertIsNone(lsp_table.find_program(self.root, self.entry(command=["bin/server"], root="app")))
        (self.root / "app/bin/server").chmod(0o755)
        self.assertIsNotNone(lsp_table.find_program(self.root, self.entry(command=["bin/server"], root="app")))


class PresetTests(Fixture):
    def presets(self, document):
        path = self.root / "presets.json"
        path.write_text(json.dumps(document), encoding="utf-8")
        return path

    def test_every_preset_resolves_to_exactly_the_configuration_the_kit_promises(self):
        # Found by the completion review of D04 T01 section 2: asserting chosen values let a changed one through.
        # Every key of every resolved server is compared, the general presets as well as Laravel's.
        resolved = {name: body["servers"] for name, body in lsp_table.load_presets()["presets"].items()}
        self.assertEqual(list(resolved), list(EXPECTED_PRESETS))
        for preset, servers in EXPECTED_PRESETS.items():
            for expected, found in zip(servers, resolved[preset]):
                with self.subTest(preset=preset, server=expected["name"]):
                    self.assertEqual(found, expected)
            self.assertEqual(len(resolved[preset]), len(servers))

    def test_one_changed_value_in_a_preset_fails_the_comparison(self):
        # The comparison itself can fail: a scratch copy of the presets file with one nested value changed.
        document = json.loads(lsp_table.PRESETS.read_text(encoding="utf-8"))
        laravel = document["presets"]["laravel"]["servers"]
        overrides = {item["server"]: item for item in laravel if isinstance(item, dict)}
        overrides["tailwindcss"]["settings"]["tailwindCSS"]["includeLanguages"] = {}
        overrides["intelephense"]["settings"]["intelephense"]["diagnostics"]["undefinedVariables"] = False
        document["servers"]["typescript"]["extensions"][".ts"] = "javascript"
        changed = lsp_table.load_presets(self.presets(document))["presets"]
        self.assertNotEqual(changed["laravel"]["servers"][1], EXPECTED_PRESETS["laravel"][1])
        self.assertNotEqual(changed["laravel"]["servers"][2], EXPECTED_PRESETS["laravel"][2])
        self.assertNotEqual(changed["web"]["servers"][0], EXPECTED_PRESETS["web"][0])
        self.assertEqual(changed["laravel"]["servers"][0], EXPECTED_PRESETS["laravel"][0])

    def test_the_five_presets_resolve_to_their_servers(self):
        loaded = lsp_table.load_presets()
        names = {name: [entry["name"] for entry in body["servers"]] for name, body in loaded["presets"].items()}
        self.assertEqual(names,
                         {"cpp": ["clangd"], "python": ["basedpyright"], "php": ["intelephense"],
                          "web": ["typescript", "tailwindcss"],
                          "laravel": ["laravel-lsp", "intelephense", "tailwindcss"]})
        self.assertEqual(sorted(loaded["servers"]), ["basedpyright", "clangd", "intelephense", "laravel-lsp",
                                                     "tailwindcss", "typescript"])
        self.assertEqual({name: body["root_marker"] for name, body in loaded["presets"].items()},
                         {"cpp": None, "python": None, "php": None, "web": None, "laravel": "artisan"})

    def test_a_general_preset_carries_nothing_that_belongs_to_laravel(self):
        loaded = lsp_table.load_presets()["presets"]
        typescript, tailwind = loaded["web"]["servers"]
        self.assertEqual(typescript["command"], ["typescript-language-server", "--stdio"])
        self.assertEqual(tailwind["command"], ["tailwindcss-language-server", "--stdio"])
        self.assertEqual((tailwind["requires"], tailwind["init"], tailwind["settings"]), (None, {}, None))
        self.assertNotIn(".blade.php", tailwind["extensions"])
        self.assertEqual(tailwind["extensions"][".css"], "css")
        (intelephense,) = loaded["php"]["servers"]
        self.assertEqual((intelephense["requires"], intelephense["settings"], intelephense["severity"]),
                         (None, None, "error"))
        self.assertEqual(intelephense["extensions"], {".php": "php"})
        (clangd,) = loaded["cpp"]["servers"]
        self.assertEqual((clangd["extensions"][".c"], clangd["extensions"][".cpp"]), ("c", "cpp"))
        (pyright,) = loaded["python"]["servers"]
        self.assertEqual(pyright["command"], ["basedpyright-langserver", "--stdio"])

    def test_the_laravel_preset_carries_what_the_laravel_client_held_for_its_servers(self):
        laravel, intelephense, tailwind = lsp_table.load_presets()["presets"]["laravel"]["servers"]
        self.assertEqual((laravel["command"], laravel["requires"], laravel["severity"], laravel["settle"]),
                         (["laravel-lsp"], "artisan", "warning", 8))
        self.assertEqual(laravel["init"], {"phpEnvironment": "local", "phpCommand": ["php"],
                                           "pestGenerateDocBlocks": False})
        self.assertEqual(laravel["extensions"], {".php": "php", ".blade.php": "blade"})
        self.assertEqual((intelephense["requires"], intelephense["severity"], intelephense["settle"]),
                         ("composer.json", "error", 10))
        # Its index goes to a folder the client makes outside the repository: the placeholder says so.
        self.assertEqual(intelephense["init"], {"storagePath": "${scratch}", "clearCache": False})
        diagnostics = intelephense["settings"]["intelephense"]["diagnostics"]
        self.assertEqual((diagnostics["typeErrors"], diagnostics["undefinedFunctions"]), (False, True))
        self.assertEqual((tailwind["requires"], tailwind["severity"], tailwind["settle"]),
                         ("resources/css/app.css", "warning", 6))
        self.assertEqual(tailwind["init"], {"userLanguages": {"blade": "html"}})
        self.assertEqual(tailwind["settings"]["tailwindCSS"]["lint"]["cssConflict"], "warning")
        self.assertEqual(tailwind["extensions"], {".blade.php": "blade", ".css": "css", ".php": "php"})

    def test_a_presets_file_with_a_fault_is_refused(self):
        entry = {key: value for key, value in CLANGD.items() if key != "name"}
        good = {"schema_version": 1, "servers": {"clangd": entry}, "presets": {"cpp": {"servers": ["clangd"]}}}
        self.assertEqual(list(lsp_table.load_presets(self.presets(good))["presets"]), ["cpp"])
        cases = [
            (dict(good, presets={"cpp": {"servers": ["gopls"]}}), "names a server the catalogue lacks: 'gopls'"),
            (dict(good, presets={"cpp": {"servers": [{"server": "clangd", "name": "other"}]}}), "cannot rename"),
            (dict(good, presets={"cpp": {"servers": [{"server": "clangd", "extra": 1}]}}), "unknown key 'extra'"),
            (dict(good, presets={"cpp": {"servers": ["clangd", "clangd"]}}), "listed twice"),
            (dict(good, presets={"cpp": {"servers": []}}), "servers: a list of at least one server"),
            (dict(good, presets={"cpp": {"servers": ["clangd"], "extra": 1}}), "an object with servers"),
            (dict(good, presets={"cpp": {"servers": ["clangd"], "root_marker": "../artisan"}}), "root_marker:"),
            (dict(good, servers={"clangd": dict(entry, extra=1)}), "server 'clangd': unknown key 'extra'"),
            (dict(good, servers={"Clangd": entry}), "name:"),
            (dict(good, extra=1), "unknown key 'extra'"),
        ]
        for document, expected in cases:
            with self.subTest(expected=expected):
                with self.assertRaises(lsp_table.TableError) as caught:
                    lsp_table.load_presets(self.presets(document))
                self.assertIn(expected, str(caught.exception))
                self.assertTrue(str(caught.exception).startswith("presets.json: "), caught.exception)

    def test_an_override_is_checked_like_any_entry_and_replaces_the_catalogue_value(self):
        entry = {key: value for key, value in CLANGD.items() if key != "name"}
        document = {"schema_version": 1, "servers": {"clangd": entry},
                    "presets": {"cpp": {"servers": [{"server": "clangd", "settle": 30, "requires": "build.ninja"}]}}}
        (resolved,) = lsp_table.load_presets(self.presets(document))["presets"]["cpp"]["servers"]
        self.assertEqual((resolved["settle"], resolved["requires"], resolved["command"]),
                         (30, "build.ninja", CLANGD["command"]))
        document["presets"]["cpp"]["servers"][0]["settle"] = 500
        with self.assertRaises(lsp_table.TableError) as caught:
            lsp_table.load_presets(self.presets(document))
        self.assertIn("preset 'cpp': server 'clangd': settle:", str(caught.exception))


class RootTests(Fixture):
    def app(self, folder):
        base = self.root if folder == "." else self.root / folder
        (base / "resources/css").mkdir(parents=True)
        for rel in ("artisan", "composer.json", "resources/css/app.css"):
            (base / rel).write_text("", encoding="utf-8")

    def test_the_laravel_preset_finds_the_app_and_the_written_table_names_files_that_exist(self):
        for folder in (".", "platform", "backend"):
            with self.subTest(app=folder):
                for leftover in ("artisan", "platform", "backend"):
                    path = self.root / leftover
                    if path.is_dir():
                        shutil.rmtree(path)
                    elif path.exists():
                        path.unlink()
                self.write({"schema_version": 1, "servers": []})
                (self.root / "aaa-first-child").mkdir(exist_ok=True)  # an earlier child with no artisan is passed
                self.app(folder)
                self.assertEqual(lsp_servers.main(["add", "laravel"], root=self.root), 0)
                loaded = lsp_table.load_servers(self.root)
                self.assertEqual([entry["name"] for entry in loaded], ["laravel-lsp", "intelephense", "tailwindcss"])
                for entry in loaded:
                    self.assertEqual(entry["root"], folder)
                    self.assertTrue((self.root / entry["root"]).is_dir())
                    self.assertTrue((self.root / entry["root"] / entry["requires"]).is_file(), entry["requires"])
                stored = json.loads(self.table.read_text(encoding="utf-8"))["servers"]
                self.assertEqual("root" in stored[0], folder != ".")  # the default is not written

    def test_platform_is_found_before_any_other_child(self):
        self.app("aaa")
        self.app("platform")
        self.assertEqual(lsp_table.find_root(self.root, "artisan"), "platform")
        self.app(".")
        self.assertEqual(lsp_table.find_root(self.root, "artisan"), ".")

    def test_a_project_with_no_app_is_refused_and_nothing_is_written(self):
        before = self.table.read_bytes()
        with self.assertRaises(lsp_table.TableError) as caught:
            lsp_table.resolve_preset(self.root, "laravel")
        self.assertIn("no folder of the project holds artisan", str(caught.exception))
        self.assertIn("--root", str(caught.exception))
        self.assertEqual(lsp_servers.main(["add", "laravel"], root=self.root), 2)
        self.assertEqual(self.table.read_bytes(), before)

    def test_the_root_option_wins_over_discovery_and_must_be_inside_the_project(self):
        self.app("platform")
        self.app("other")
        entries = lsp_table.resolve_preset(self.root, "laravel", "other")
        self.assertEqual({entry["root"] for entry in entries}, {"other"})
        self.assertEqual({entry["root"] for entry in lsp_table.resolve_preset(self.root, "cpp", "other")}, {"other"})
        self.assertEqual({entry["root"] for entry in lsp_table.resolve_preset(self.root, "cpp")}, {"."})
        for bad in ("../outside", "/abs", "missing-folder"):
            with self.subTest(root=bad):
                with self.assertRaises(lsp_table.TableError) as caught:
                    lsp_table.resolve_preset(self.root, "laravel", bad)
                self.assertIn("--root: a folder inside the project", str(caught.exception))


class WriteTests(Fixture):
    def test_the_table_is_written_as_indented_json_and_only_when_it_changes(self):
        (entry,) = lsp_table.resolve_preset(self.root, "cpp")
        self.assertTrue(lsp_table.write_servers(self.root, [entry]))
        text = self.table.read_text(encoding="utf-8")
        self.assertTrue(text.endswith("}\n"))
        self.assertIn('\n  "servers": [\n', text)
        self.assertEqual(lsp_table.load_servers(self.root), [entry])
        os.utime(self.table, (1_000_000_000, 1_000_000_000))
        self.assertFalse(lsp_table.write_servers(self.root, [entry]))
        self.assertEqual(self.table.stat().st_mtime, 1_000_000_000)
        self.assertEqual(sorted(path.name for path in self.table.parent.iterdir()), ["lsp.json"])

    def test_a_failed_write_leaves_the_table_and_no_temporary_file(self):
        (entry,) = lsp_table.resolve_preset(self.root, "cpp")
        before = self.table.read_bytes()
        full, denied = OSError(28, "No space left on device"), PermissionError(13, "Access is denied")
        real_fdopen = os.fdopen

        class DiskFull:
            """The temporary file as a full disk leaves it: opened, and refusing the bytes."""

            def __init__(self, *args, **kwargs):
                self.file = real_fdopen(*args, **kwargs)

            def __enter__(self):
                return self

            def __exit__(self, *exc):
                self.file.close()

            def write(self, data):
                raise full

        failures = {
            "the write": (unittest.mock.patch.object(lsp_table.os, "fdopen", DiskFull), "No space left on device"),
            "the flush to disk": (unittest.mock.patch.object(lsp_table.os, "fsync", side_effect=full),
                                  "No space left on device"),
            "the replacement": (unittest.mock.patch.object(lsp_table.os, "replace", side_effect=denied),
                                "Access is denied"),
        }
        for name, (failure, reason) in failures.items():
            with self.subTest(failed=name):
                out, err = io.StringIO(), io.StringIO()
                with failure:
                    with self.assertRaises(lsp_table.TableWriteError) as caught:
                        lsp_table.write_servers(self.root, [entry])
                    self.assertEqual(str(caught.exception), f"{TABLE}: cannot be written ({reason})")
                    with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
                        code = lsp_servers.main(["add", "cpp"], root=self.root)
                # One line, naming the table and the system's reason; nothing claimed on standard output.
                self.assertEqual((code, out.getvalue()), (1, ""))
                self.assertEqual(err.getvalue(), f"lsp-servers: {TABLE}: cannot be written ({reason})\n")
                self.assertEqual(self.table.read_bytes(), before)
                self.assertEqual(sorted(path.name for path in self.table.parent.iterdir()), ["lsp.json"])
        # And with nothing failing, the same command writes the table.
        self.assertEqual(lsp_servers.main(["add", "cpp"], root=self.root), 0)
        self.assertEqual([server["name"] for server in lsp_table.load_servers(self.root)], ["clangd"])


class CommandLineTests(Fixture):
    """lsp-servers.py run as a program, from a copy laid out as the kit installs it (scripts/ beside .conclave/)."""

    def setUp(self):
        super().setUp()
        scripts = self.root / "scripts"
        (scripts / "lib").mkdir(parents=True)
        for rel in ("lsp-servers.py", "lib/lsp_table.py", "lib/lsp_plugin.py", "lib/lsp_presets.json"):
            (scripts / rel).write_bytes((SCRIPTS / rel).read_bytes())

    def run_script(self, *argv):
        return subprocess.run([sys.executable, "-B", str(self.root / "scripts/lsp-servers.py"), *argv],
                              capture_output=True, text=True, timeout=60, cwd=self.root,
                              creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))

    def names(self):
        return [entry["name"] for entry in json.loads(self.table.read_text(encoding="utf-8"))["servers"]]

    def test_list_shows_the_table_and_the_presets_and_writes_nothing(self):
        before = self.table.read_bytes()
        ran = self.run_script("list")
        self.assertEqual(ran.returncode, 0, ran.stderr)
        lines = ran.stdout.splitlines()
        self.assertTrue(lines[0].startswith(f"{TABLE} holds no server."), lines[0])
        self.assertIn("python scripts/lsp-servers.py add <preset>", lines[0])
        self.assertEqual(lines[1:], ["Presets:", "  cpp  clangd", "  python  basedpyright", "  php  intelephense",
                                     "  web  typescript, tailwindcss",
                                     "  laravel  laravel-lsp, intelephense, tailwindcss"])
        self.assertEqual(self.table.read_bytes(), before)
        self.assertEqual(self.run_script("add", "cpp").returncode, 0)
        populated = self.table.read_bytes()
        lines = self.run_script("list").stdout.splitlines()
        self.assertEqual(lines[0], f"Servers in {TABLE}:")
        self.assertTrue(lines[1].startswith("  clangd  clangd  .c .h "), lines[1])
        self.assertEqual(lines[2], "Presets:")
        self.assertEqual(self.table.read_bytes(), populated)

    def test_add_says_what_it_added_and_what_it_kept(self):
        ran = self.run_script("add", "web")
        self.assertEqual((ran.returncode, ran.stdout), (0, "added: typescript, tailwindcss\n"))
        self.assertEqual(self.names(), ["typescript", "tailwindcss"])
        os.utime(self.table, (1_000_000_000, 1_000_000_000))
        before = self.table.read_bytes()
        again = self.run_script("add", "web")
        self.assertEqual((again.returncode, again.stdout),
                         (0, "added: nothing\nkept as the project has them: typescript, tailwindcss\n"))
        self.assertEqual((self.table.read_bytes(), self.table.stat().st_mtime), (before, 1_000_000_000))

    def test_a_projects_edited_entry_is_kept_and_only_new_servers_are_added(self):
        self.assertEqual(self.run_script("add", "php").returncode, 0)
        document = json.loads(self.table.read_text(encoding="utf-8"))
        document["servers"][0]["command"] = ["intelephense", "--stdio", "--the-projects-own-flag"]
        document["servers"][0]["settle"] = 42
        self.write(document)
        for rel in ("artisan", "composer.json"):
            (self.root / rel).write_text("", encoding="utf-8")
        ran = self.run_script("add", "laravel")
        self.assertEqual((ran.returncode, ran.stdout),
                         (0, "added: laravel-lsp, tailwindcss\nkept as the project has them: intelephense\n"))
        loaded = {entry["name"]: entry for entry in lsp_table.load_servers(self.root)}
        self.assertEqual(list(loaded), ["intelephense", "laravel-lsp", "tailwindcss"])
        self.assertEqual(loaded["intelephense"]["command"], ["intelephense", "--stdio", "--the-projects-own-flag"])
        self.assertEqual(loaded["intelephense"]["settle"], 42)
        self.assertIsNone(loaded["intelephense"]["settings"])  # Laravel's settings did not replace the project's

    def test_remove_deletes_one_server(self):
        self.run_script("add", "web")
        ran = self.run_script("remove", "typescript")
        self.assertEqual((ran.returncode, ran.stdout), (0, "removed: typescript\n"))
        self.assertEqual(self.names(), ["tailwindcss"])

    def test_an_unknown_name_or_a_broken_table_is_one_line_and_exit_2_with_nothing_written(self):
        self.run_script("add", "cpp")
        before = self.table.read_bytes()
        refused = ((("add", "rust"), "unknown preset 'rust': the presets are cpp, laravel, php, python, web"),
                   (("remove", "gopls"), "no server named 'gopls' in .conclave/lsp.json (it holds: clangd)"),
                   (("add", "cpp", "--root", "../out"), "--root: a folder inside the project"))
        for argv, expected in refused:
            with self.subTest(argv=argv):
                ran = self.run_script(*argv)
                self.assertEqual((ran.returncode, ran.stdout), (2, ""))
                self.assertEqual(len(ran.stderr.splitlines()), 1, ran.stderr)
                self.assertIn(f"lsp-servers: {expected}", ran.stderr)
                self.assertEqual(self.table.read_bytes(), before)
        self.write(b'{"schema_version": 1, "servers": [{"name": "x"}]}')
        broken = self.table.read_bytes()
        for argv in (("list",), ("add", "cpp"), ("remove", "x")):
            with self.subTest(argv=argv, table="broken"):
                ran = self.run_script(*argv)
                self.assertEqual((ran.returncode, ran.stdout), (2, ""))
                self.assertTrue(ran.stderr.startswith(f"lsp-servers: {TABLE}: server 'x': missing key"), ran.stderr)
                self.assertEqual(self.table.read_bytes(), broken)


if __name__ == "__main__":
    unittest.main()
