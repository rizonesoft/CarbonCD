#!/usr/bin/env python3
"""The project's language server table, `.conclave/lsp.json`: list it, add a stack's servers, remove one.

    python scripts/lsp-servers.py list
    python scripts/lsp-servers.py add <preset> [--root <folder>]
    python scripts/lsp-servers.py remove <name>
    python scripts/lsp-servers.py plugin

`add` appends the servers of a preset (cpp, python, php, web, laravel) and never changes an entry the project
already has by that name. A preset for a framework finds the app's folder (the Laravel preset looks for `artisan`
at the project root, in `platform/`, then in the other first-level folders); `--root` names it instead. The
command installs no server: `python forge.py doctor <project> lsp` says which are missing and how to install each.

`plugin` writes the Claude Code plugin for the table's servers (`.claude/lsp/conclave-lsp/`, loaded with
`claude --plugin-dir .claude/lsp/conclave-lsp`), so Claude Code runs them live while it edits. The folder is
generated output: rewritten whole, never merged. Once a project has it, `add` and `remove` keep it in step with
the table; a project that never ran `plugin` gets none.

Exit 0 on success, 1 when the table or the plugin cannot be written, 2 for a table that does not load or a name
that is not known. Standard library only.
"""
import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent / "lib"))
import lsp_plugin  # noqa: E402
import lsp_table  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]


def suffixes(entry):
    return " ".join(entry["extensions"])


def command_list(root):
    servers = lsp_table.load_servers(root)
    presets = lsp_table.load_presets()
    if servers:
        print(f"Servers in {lsp_table.TABLE}:")
        for entry in servers:
            where = "" if entry["root"] == "." else f"  (in {entry['root']}/)"
            print(f"  {entry['name']}  {entry['command'][0]}  {suffixes(entry)}{where}")
    else:
        print(f"{lsp_table.TABLE} holds no server. Add a stack's servers with: "
              "python scripts/lsp-servers.py add <preset>")
    print("Presets:")
    for name, body in presets["presets"].items():
        print(f"  {name}  {', '.join(entry['name'] for entry in body['servers'])}")
    return 0


class PluginBehind(Exception):
    """The table was changed and the plugin could not be regenerated: the table keeps its change."""


def follow(root):
    """After the table changed: bring a plugin the project has generated in step with it."""
    try:
        lsp_plugin.refresh(root, lsp_table.load_servers(root))
    except lsp_plugin.PluginWriteError as exc:
        raise PluginBehind(f"{lsp_table.TABLE} was changed and the Claude Code plugin was not ({exc}): bring it "
                           f"in step with {lsp_plugin.COMMAND}") from exc


def shared_lines(servers):
    """What to say of extensions more than one server claims: Claude Code gives each to one server."""
    lines = [f"  {extension}: {first} gets it; not {', '.join(others)}"
             for extension, first, others in lsp_plugin.shared_extensions(servers)]
    if lines:
        lines.insert(0, "Claude Code gives each extension to one server, the first in the table:")
        lines.append(f"  the order of the servers in {lsp_table.TABLE} decides; scripts/lsp-diagnostics.py "
                     "runs every server on its files")
    return lines


def command_plugin(root):
    servers = lsp_table.load_servers(root)
    changed = lsp_plugin.write_plugin(root, servers)
    names = ", ".join(entry["name"] for entry in servers) or "no server (the table holds none yet)"
    print(f"{lsp_plugin.FOLDER}: {names}" + ("" if changed else " (unchanged)"))
    for line in shared_lines(servers):
        print(line)
    print(f"load it with: {lsp_plugin.LOAD}")
    return 0


def command_add(root, preset, app_root):
    servers = lsp_table.load_servers(root)
    have = {entry["name"] for entry in servers}
    entries = lsp_table.resolve_preset(root, preset, app_root)
    added = [entry for entry in entries if entry["name"] not in have]
    kept = [entry["name"] for entry in entries if entry["name"] in have]
    if added:
        lsp_table.write_servers(root, servers + added)
    print(f"added: {', '.join(entry['name'] for entry in added) or 'nothing'}")
    if kept:
        print(f"kept as the project has them: {', '.join(kept)}")
    if added:
        follow(root)
    return 0


def command_remove(root, name):
    servers = lsp_table.load_servers(root)
    if name not in {entry["name"] for entry in servers}:
        known = ", ".join(entry["name"] for entry in servers) or "none"
        raise lsp_table.TableError(f"no server named {name!r} in {lsp_table.TABLE} (it holds: {known})")
    lsp_table.write_servers(root, [entry for entry in servers if entry["name"] != name])
    print(f"removed: {name}")
    follow(root)
    return 0


def build_parser():
    """The script's argument parser: what `main` parses with, and what a documented command must fit."""
    parser = argparse.ArgumentParser(description="List and change the project's language server table.")
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("list", help="print the table's servers and the presets on offer")
    add = commands.add_parser("add", help="append a preset's servers to the table")
    add.add_argument("preset")
    add.add_argument("--root", dest="app_root", help="the app's folder inside the project, instead of looking for it")
    remove = commands.add_parser("remove", help="delete one server from the table")
    remove.add_argument("name")
    commands.add_parser("plugin", help="write the Claude Code plugin for the table's servers")
    return parser


def main(argv=None, root=None):
    args = build_parser().parse_args(argv)
    root = Path(root or ROOT)
    try:
        if args.command == "list":
            return command_list(root)
        if args.command == "add":
            return command_add(root, args.preset, args.app_root)
        if args.command == "plugin":
            return command_plugin(root)
        return command_remove(root, args.name)
    except (lsp_plugin.PluginWriteError, PluginBehind) as exc:
        print(f"lsp-servers: {exc}", file=sys.stderr)
        return 1
    except lsp_table.TableWriteError as exc:
        print(f"lsp-servers: {exc}", file=sys.stderr)
        return 1
    except lsp_table.TableError as exc:
        print(f"lsp-servers: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
