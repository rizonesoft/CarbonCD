"""The Claude Code plugin generated from the server table, so the table stays the only list of servers.

Claude Code runs language servers live from a plugin: a folder holding `.claude-plugin/plugin.json` and, at its
root, `.lsp.json`, which maps each server's name straight to its entry. `plugin_files` builds both from the loaded
table (`lsp_table.load_servers`); `write_plugin` writes the folder whole. It is generated output: never edited,
never merged, rewritten by `python scripts/lsp-servers.py plugin`.

Only keys Claude Code documents are written, because one unknown key makes it skip the whole file:

    command               the entry's program: a bare name or an absolute path as it is, a relative path under
                          ${CLAUDE_PROJECT_DIR} and the server's root
    args                  the rest of the entry's command, when there is any
    extensionToLanguage   the entry's extensions
    workspaceFolder       ${CLAUDE_PROJECT_DIR}/<root>, when the root is not the project's
    initializationOptions the entry's init, when not empty
    settings              the entry's settings, when not empty

Claude Code resolves variables in `command`, `args` and `workspaceFolder` only, so a string in `init` or `settings`
that holds `${scratch}` is left out with its key. What Claude Code has no key for (`requires`, `severity`,
`settle`, `note`, `install`) is not written. Standard library only.
"""
import json
import os
import tempfile
from pathlib import Path, PurePosixPath, PureWindowsPath

FOLDER = ".claude/lsp/conclave-lsp"
NAME = "conclave-lsp"
MANIFEST = ".claude-plugin/plugin.json"
SERVERS = ".lsp.json"
FILES = (MANIFEST, SERVERS)   # written in this order: the manifest never changes with the table
PROJECT = "${CLAUDE_PROJECT_DIR}"
SCRATCH = "${scratch}"
KEYS = ("command", "args", "extensionToLanguage", "workspaceFolder", "initializationOptions", "settings")
COMMAND = "python scripts/lsp-servers.py plugin"
LOAD = f"claude --plugin-dir {FOLDER}"


class PluginWriteError(Exception):
    """The plugin could not be written: the message names the file and why."""


def _is_absolute(program):
    """Whether a program is named by an absolute path of either system: `/usr/bin/x`, `C:/x`, `C:\\x`, `\\\\host\\x`."""
    return PurePosixPath(program).is_absolute() or PureWindowsPath(program).is_absolute()


def _carried(value):
    """`value` without what holds `${scratch}`: a string that holds it is dropped with its key (or from its list),
    at any depth. Returns (the value, whether anything of it is left to write)."""
    if isinstance(value, str):
        return value, SCRATCH not in value
    if isinstance(value, dict):
        kept = {key: carried for key, (carried, keep) in ((key, _carried(item)) for key, item in value.items())
                if keep}
        return kept, bool(kept) or not value
    if isinstance(value, list):
        kept = [carried for carried, keep in (_carried(item) for item in value) if keep]
        return kept, bool(kept) or not value
    return value, True


def server_entry(entry):
    """One server of the table as Claude Code's `.lsp.json` takes it."""
    program = entry["command"][0]
    root = entry["root"]
    under = PROJECT if root == "." else f"{PROJECT}/{root}"
    if ("/" in program or "\\" in program) and not _is_absolute(program):
        program = f"{under}/{PurePosixPath(program.replace(chr(92), '/')).as_posix()}"
    out = {"command": program}
    if entry["command"][1:]:
        out["args"] = list(entry["command"][1:])
    out["extensionToLanguage"] = dict(entry["extensions"])
    if root != ".":
        out["workspaceFolder"] = under
    for key, source in (("initializationOptions", entry["init"]), ("settings", entry["settings"])):
        carried, keep = _carried(source) if source else (None, False)
        if keep and carried:
            out[key] = carried
    return out


def plugin_files(servers):
    """The plugin's files for the loaded table, as {path inside the plugin folder: bytes}."""
    manifest = {"name": NAME, "version": "1.0.0",
                "description": "Language servers of this project, generated from .conclave/lsp.json. "
                               f"Do not edit: run `{COMMAND}`.",
                "author": {"name": "scripts/lsp-servers.py"}}
    listed = {entry["name"]: server_entry(entry) for entry in servers}
    return {MANIFEST: (json.dumps(manifest, indent=2) + "\n").encode("utf-8"),
            SERVERS: (json.dumps(listed, indent=2) + "\n").encode("utf-8")}


def shared_extensions(servers):
    """The extensions more than one server claims, in the table's order: [(extension, the server that gets it,
    the servers that do not)]. Claude Code gives each extension to one server, the first registered."""
    claimed = {}
    for entry in servers:
        for extension in entry["extensions"]:
            claimed.setdefault(extension, []).append(entry["name"])
    return [(extension, names[0], names[1:]) for extension, names in claimed.items() if len(names) > 1]


def state(root, servers):
    """`absent` when the project has no generated plugin, `current` when its two files are byte for byte what the
    table gives now, `differs` otherwise (a table changed since, or a file missing or edited)."""
    folder = Path(root) / FOLDER
    wanted = plugin_files(servers)
    found = {}
    for rel in FILES:
        try:
            found[rel] = (folder / rel).read_bytes()
        except OSError:
            found[rel] = None
    if all(data is None for data in found.values()):
        return "absent"
    return "current" if found == wanted else "differs"


def _replace(path, data):
    temporary = None
    try:
        path.parent.mkdir(parents=True, exist_ok=True)
        fd, temporary = tempfile.mkstemp(dir=path.parent, prefix=".plugin-", suffix=".tmp")
        with os.fdopen(fd, "wb") as out:
            out.write(data)
            out.flush()
            os.fsync(out.fileno())
        os.replace(temporary, path)
        temporary = None
    finally:
        if temporary is not None and os.path.exists(temporary):
            os.unlink(temporary)


def write_plugin(root, servers):
    """Write the plugin folder whole for the loaded table: each file through a temporary file and a replacement,
    then every other file of the folder removed. Returns the files that changed. The manifest does not depend on
    the table, so the server list changes in the one replacement of `.lsp.json`: a failure leaves it as it was or
    as the table says, never between. Raises PluginWriteError naming the file."""
    folder = Path(root) / FOLDER
    changed = []
    for rel, data in plugin_files(servers).items():
        path = folder / rel
        try:
            if path.is_file() and path.read_bytes() == data:
                continue
            _replace(path, data)
        except OSError as exc:
            raise PluginWriteError(f"{FOLDER}/{rel}: cannot be written ({exc.strerror or exc})") from exc
        changed.append(rel)
    # Generated output is the folder, not two files in it: whatever else an earlier run or a hand left goes.
    keep = {folder / rel for rel in FILES}
    try:
        for path in sorted(folder.rglob("*"), reverse=True):
            if path.is_dir() and not path.is_symlink():
                if not any(path.iterdir()):
                    path.rmdir()
            elif path not in keep:
                path.unlink()
                changed.append(path.relative_to(folder).as_posix())
    except OSError as exc:
        raise PluginWriteError(f"{FOLDER}: a stray file could not be removed ({exc.strerror or exc})") from exc
    return changed


def refresh(root, servers):
    """Bring a plugin the project has generated in step with the table. A project that never ran `plugin` has
    none, and gets none: returns None. Else the files that changed. Raises PluginWriteError."""
    if state(root, servers) == "absent":
        return None
    return write_plugin(root, servers)
