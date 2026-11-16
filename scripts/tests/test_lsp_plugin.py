"""The Claude Code plugin generated from the server table: what an entry becomes, the folder as generated output,
how it follows the table, and what happens when it cannot be written."""
import contextlib
import importlib.util
import io
import json
import os
import pathlib
import sys
import tempfile
import types
import unittest
import unittest.mock

SCRIPTS = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(SCRIPTS / "lib"))
import lsp_plugin  # noqa: E402
import lsp_table  # noqa: E402

spec = importlib.util.spec_from_file_location("lsp_servers", SCRIPTS / "lsp-servers.py")
lsp_servers = importlib.util.module_from_spec(spec)
spec.loader.exec_module(lsp_servers)

# The keys of a server entry Claude Code documents (code.claude.com/docs/en/plugins/components, "LSP servers"),
# written here and not read from the generator: one unknown key makes Claude Code skip the whole file.
DOCUMENTED = {"command", "args", "extensionToLanguage", "transport", "env", "initializationOptions", "settings",
              "workspaceFolder", "startupTimeout", "shutdownTimeout", "requestTimeout", "restartOnCrash",
              "maxRestarts", "diagnostics"}


class Fixture(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.project = pathlib.Path(tmp.name).resolve() / "project"
        (self.project / ".conclave").mkdir(parents=True)
        self.folder = self.project / lsp_plugin.FOLDER
        self.table([])

    def table(self, entries):
        (self.project / lsp_table.TABLE).write_text(json.dumps({"schema_version": 1, "servers": entries}),
                                                    encoding="utf-8")
        return lsp_table.load_servers(self.project)

    def entry(self, name, command=None, extensions=None, **keys):
        return {"name": name, "command": command or [name], "extensions": extensions or {".txt": "plaintext"},
                "install": f"install {name}", **keys}

    def listed(self):
        return json.loads((self.folder / lsp_plugin.SERVERS).read_text(encoding="utf-8"))

    def run_main(self, *argv):
        out, err = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            code = lsp_servers.main(list(argv), root=self.project)
        return code, out.getvalue(), err.getvalue()

    def failing_replace(self, only=lsp_plugin.SERVERS):
        """lsp_plugin with the replacement of one of its files made to fail."""
        real = os.replace

        def replace(source, target):
            if pathlib.Path(target).name == pathlib.PurePosixPath(only).name:
                raise PermissionError(13, "Permission denied")
            return real(source, target)

        failing = types.SimpleNamespace(**{name: getattr(os, name) for name in dir(os) if not name.startswith("__")})
        failing.replace = replace
        return unittest.mock.patch.object(lsp_plugin, "os", failing)


class EntryTests(Fixture):
    def test_each_server_of_the_table_becomes_an_entry_of_documented_keys(self):
        (self.project / "api").mkdir()
        servers = self.table([
            self.entry("typescript", ["typescript-language-server", "--stdio"],
                       {".ts": "typescript", ".tsx": "typescriptreact"}),
            self.entry("styles", ["node_modules/.bin/style-server", "--stdio", "--strict"], {".css": "css"},
                       root="api", init={"lint": True, "storagePath": "${scratch}"},
                       settings={"css": {"validate": True, "cache": {"dir": "${scratch}/cache", "size": 3}}},
                       requires="package.json", severity="warning", settle=7, note="styles only")])
        files = lsp_plugin.plugin_files(servers)
        self.assertEqual(sorted(files), sorted(lsp_plugin.FILES))
        manifest = json.loads(files[lsp_plugin.MANIFEST])
        self.assertEqual((manifest["name"], sorted(manifest)), ("conclave-lsp",
                                                                ["author", "description", "name", "version"]))
        listed = json.loads(files[lsp_plugin.SERVERS])
        self.assertEqual(list(listed), ["typescript", "styles"])   # no wrapper, in the table's order
        self.assertEqual(listed["typescript"], {
            "command": "typescript-language-server", "args": ["--stdio"],
            "extensionToLanguage": {".ts": "typescript", ".tsx": "typescriptreact"}})
        self.assertEqual(listed["styles"], {
            "command": "${CLAUDE_PROJECT_DIR}/api/node_modules/.bin/style-server", "args": ["--stdio", "--strict"],
            "extensionToLanguage": {".css": "css"}, "workspaceFolder": "${CLAUDE_PROJECT_DIR}/api",
            # The `${scratch}` values are left out with their keys, at any depth; the rest is carried.
            "initializationOptions": {"lint": True},
            "settings": {"css": {"validate": True, "cache": {"size": 3}}}})
        for name, entry in listed.items():
            self.assertLessEqual(set(entry), DOCUMENTED, f"{name} has a key Claude Code does not document")
            self.assertLessEqual(set(entry), set(lsp_plugin.KEYS))
        for data in files.values():
            self.assertTrue(data.endswith(b"\n"))
            self.assertNotIn(b"${scratch}", data)

    def test_a_program_is_a_bare_name_an_absolute_path_as_it_is_or_a_relative_path_under_the_project(self):
        (self.project / "app").mkdir()
        cases = [("clangd", ".", "clangd"),
                 ("/opt/llvm/bin/clangd", ".", "/opt/llvm/bin/clangd"),
                 ("/opt/llvm/bin/clangd", "app", "/opt/llvm/bin/clangd"),
                 ("C:/Program Files/LLVM/bin/clangd.exe", ".", "C:/Program Files/LLVM/bin/clangd.exe"),
                 ("C:\\Program Files\\LLVM\\bin\\clangd.exe", "app", "C:\\Program Files\\LLVM\\bin\\clangd.exe"),
                 ("\\\\tools\\share\\clangd.exe", ".", "\\\\tools\\share\\clangd.exe"),
                 ("tools/clangd", ".", "${CLAUDE_PROJECT_DIR}/tools/clangd"),
                 ("./tools/clangd", "app", "${CLAUDE_PROJECT_DIR}/app/tools/clangd"),
                 ("vendor/bin/server", "app", "${CLAUDE_PROJECT_DIR}/app/vendor/bin/server")]
        for program, root, expected in cases:
            with self.subTest(program=program, root=root):
                entry = {"name": "x", "command": [program], "extensions": {".c": "c"}, "root": root, "init": {},
                         "settings": None}
                self.assertEqual(lsp_plugin.server_entry(entry)["command"], expected)

    def test_an_entry_with_nothing_to_carry_has_no_empty_key(self):
        (server,) = self.table([self.entry("plain", init={"storagePath": "${scratch}"},
                                           settings={"a": {"b": "${scratch}/x"}})])
        self.assertEqual(lsp_plugin.server_entry(server), {"command": "plain",
                                                           "extensionToLanguage": {".txt": "plaintext"}})

    def test_extensions_more_than_one_server_claims_are_named_with_the_server_that_gets_them(self):
        servers = self.table(lsp_table.resolve_preset(self.project, "laravel", "."))
        self.assertEqual(lsp_plugin.shared_extensions(servers),
                         [(".php", "laravel-lsp", ["intelephense", "tailwindcss"]),
                          (".blade.php", "laravel-lsp", ["tailwindcss"])])
        # The generator writes what the table says and drops nothing.
        listed = json.loads(lsp_plugin.plugin_files(servers)[lsp_plugin.SERVERS])
        self.assertEqual([name for name, entry in listed.items() if ".php" in entry["extensionToLanguage"]],
                         ["laravel-lsp", "intelephense", "tailwindcss"])
        self.assertEqual(lsp_plugin.shared_extensions(self.table([self.entry("a"), self.entry("b", None,
                                                                                               {".md": "md"})])), [])


class FolderTests(Fixture):
    def test_plugin_writes_the_folder_and_a_second_run_changes_no_byte(self):
        self.table([self.entry("alpha", ["alpha", "--stdio"]), self.entry("beta", None, {".md": "markdown"})])
        self.assertEqual(lsp_plugin.state(self.project, lsp_table.load_servers(self.project)), "absent")
        code, out, err = self.run_main("plugin")
        self.assertEqual((code, err), (0, ""))
        self.assertEqual(out, ".claude/lsp/conclave-lsp: alpha, beta\n"
                              "load it with: claude --plugin-dir .claude/lsp/conclave-lsp\n")
        self.assertEqual(list(self.listed()), ["alpha", "beta"])
        self.assertEqual(sorted(path.relative_to(self.folder).as_posix() for path in self.folder.rglob("*")
                                if path.is_file()), sorted(lsp_plugin.FILES))
        self.assertEqual(lsp_plugin.state(self.project, lsp_table.load_servers(self.project)), "current")
        before = {rel: ((self.folder / rel).read_bytes(), (self.folder / rel).stat().st_mtime_ns)
                  for rel in lsp_plugin.FILES}
        code, out, _err = self.run_main("plugin")
        self.assertEqual((code, out.splitlines()[0]), (0, ".claude/lsp/conclave-lsp: alpha, beta (unchanged)"))
        self.assertEqual({rel: ((self.folder / rel).read_bytes(), (self.folder / rel).stat().st_mtime_ns)
                          for rel in lsp_plugin.FILES}, before)

    def test_the_folder_is_generated_output_and_anything_else_in_it_is_removed(self):
        self.table([self.entry("alpha")])
        (self.folder / "commands").mkdir(parents=True)
        (self.folder / "commands/old.md").write_text("left by hand", encoding="utf-8")
        (self.folder / ".lsp.json.bak").write_text("{}", encoding="utf-8")
        (self.folder / lsp_plugin.SERVERS).write_text('{"edited": {}}', encoding="utf-8")
        self.assertEqual(lsp_plugin.state(self.project, lsp_table.load_servers(self.project)), "differs")
        self.assertEqual(self.run_main("plugin")[0], 0)
        self.assertEqual(sorted(path.relative_to(self.folder).as_posix() for path in self.folder.rglob("*")),
                         [".claude-plugin", ".claude-plugin/plugin.json", ".lsp.json"])
        self.assertEqual(list(self.listed()), ["alpha"])

    def test_an_empty_table_writes_a_plugin_with_no_server_and_says_so(self):
        code, out, _err = self.run_main("plugin")
        self.assertEqual(code, 0)
        self.assertEqual(out.splitlines()[0], ".claude/lsp/conclave-lsp: no server (the table holds none yet)")
        self.assertEqual(self.listed(), {})

    def test_shared_extensions_are_said_in_the_output(self):
        self.table(lsp_table.resolve_preset(self.project, "laravel", "."))
        _code, out, _err = self.run_main("plugin")
        self.assertIn("Claude Code gives each extension to one server, the first in the table:\n"
                      "  .php: laravel-lsp gets it; not intelephense, tailwindcss\n"
                      "  .blade.php: laravel-lsp gets it; not tailwindcss\n"
                      "  the order of the servers in .conclave/lsp.json decides; scripts/lsp-diagnostics.py runs every "
                      "server on its files\n", out)

    def test_a_table_that_does_not_load_is_refused_and_nothing_is_written(self):
        (self.project / lsp_table.TABLE).write_text('{"schema_version": 1, "servers": [{"name": "x"}]}',
                                                    encoding="utf-8")
        code, out, err = self.run_main("plugin")
        self.assertEqual((code, out), (2, ""))
        self.assertTrue(err.startswith("lsp-servers: .conclave/lsp.json: server 'x': missing key"), err)
        self.assertFalse(self.folder.exists())

    def test_settings_nested_deeper_than_the_table_allows_are_refused_and_never_a_crash(self):
        # Found by the review round: 600 nested objects loaded, then the generator ran out of stack.
        def nested(levels):
            value = 0
            for _ in range(levels):
                value = {"nested": value}
            return value

        for key in ("init", "settings"):
            with self.subTest(key=key):
                text = json.dumps({"schema_version": 1, "servers": [self.entry("deep", **{key: 1})]})
                text = text.replace(f'"{key}": 1', f'"{key}": ' + '{"nested":' * 600 + "0" + "}" * 600)
                (self.project / lsp_table.TABLE).write_text(text, encoding="utf-8")
                code, out, err = self.run_main("plugin")
                self.assertEqual((code, out), (2, ""))
                self.assertEqual(err, f"lsp-servers: .conclave/lsp.json: server 'deep': {key}: nested deeper "
                                      "than 32 levels\n")
                self.assertFalse(self.folder.exists())
        # The limit itself is allowed, lists count as levels, and what is allowed is carried whole.
        self.assertFalse(lsp_table._too_deep(nested(32)))
        self.assertTrue(lsp_table._too_deep(nested(33)))
        self.assertTrue(lsp_table._too_deep({"a": [[[0]]]}, limit=3))
        (server,) = self.table([self.entry("deep", init=nested(32))])
        self.assertEqual(lsp_plugin.server_entry(server)["initializationOptions"], nested(32))

    def test_a_write_that_fails_leaves_the_earlier_list_whole(self):
        self.table([self.entry("alpha")])
        self.run_main("plugin")
        before = (self.folder / lsp_plugin.SERVERS).read_bytes()
        self.table([self.entry("alpha"), self.entry("beta", None, {".md": "markdown"})])
        with self.failing_replace():
            code, out, err = self.run_main("plugin")
        self.assertEqual((code, out), (1, ""))
        self.assertEqual(err, "lsp-servers: .claude/lsp/conclave-lsp/.lsp.json: cannot be written (Permission denied)\n")
        self.assertEqual((self.folder / lsp_plugin.SERVERS).read_bytes(), before)
        self.assertEqual([path.name for path in self.folder.rglob("*.tmp")], [])
        self.assertEqual(self.run_main("plugin")[0], 0)
        self.assertEqual(list(self.listed()), ["alpha", "beta"])


class FollowTests(Fixture):
    def test_a_project_that_never_ran_plugin_gets_none_from_add_or_remove(self):
        self.assertEqual(self.run_main("add", "python")[0], 0)
        self.assertEqual(self.run_main("remove", "basedpyright")[0], 0)
        self.assertFalse((self.project / ".claude").exists())

    def test_add_and_remove_keep_a_generated_plugin_in_step_with_the_table(self):
        self.run_main("add", "python")
        self.run_main("plugin")
        self.assertEqual(list(self.listed()), ["basedpyright"])
        self.assertEqual(self.run_main("add", "cpp")[0], 0)
        self.assertEqual(list(self.listed()), ["basedpyright", "clangd"])
        self.assertEqual(self.run_main("remove", "basedpyright")[0], 0)
        self.assertEqual(list(self.listed()), ["clangd"])   # the removed server is gone from the plugin
        self.assertEqual(lsp_plugin.state(self.project, lsp_table.load_servers(self.project)), "current")
        # An add that adds nothing changes nothing.
        before = (self.folder / lsp_plugin.SERVERS).stat().st_mtime_ns
        self.assertEqual(self.run_main("add", "cpp")[0], 0)
        self.assertEqual((self.folder / lsp_plugin.SERVERS).stat().st_mtime_ns, before)

    def test_a_plugin_that_cannot_follow_leaves_the_table_changed_and_names_the_recovery(self):
        self.run_main("add", "python")
        self.run_main("plugin")
        for argv, names in ((("add", "cpp"), ["basedpyright", "clangd"]), (("remove", "basedpyright"), ["clangd"])):
            with self.subTest(command=argv[0]):
                before = (self.folder / lsp_plugin.SERVERS).read_bytes()
                with self.failing_replace():
                    code, _out, err = self.run_main(*argv)
                self.assertEqual(code, 1)
                self.assertEqual(err, "lsp-servers: .conclave/lsp.json was changed and the Claude Code plugin was not "
                                      "(.claude/lsp/conclave-lsp/.lsp.json: cannot be written (Permission denied)): "
                                      "bring it in step with python scripts/lsp-servers.py plugin\n")
                # The table is the list and keeps its change; the plugin's list is the earlier one, whole.
                self.assertEqual([entry["name"] for entry in lsp_table.load_servers(self.project)], names)
                self.assertEqual((self.folder / lsp_plugin.SERVERS).read_bytes(), before)
                self.assertEqual(lsp_plugin.state(self.project, lsp_table.load_servers(self.project)), "differs")
                # The recovery the line names.
                self.assertEqual(self.run_main("plugin")[0], 0)
                self.assertEqual(list(self.listed()), names)
                self.assertEqual(lsp_plugin.state(self.project, lsp_table.load_servers(self.project)), "current")


if __name__ == "__main__":
    unittest.main()
