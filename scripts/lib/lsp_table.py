"""The language server table, `.conclave/lsp.json`: the one list of a project's language servers.

    {"schema_version": 1,
     "servers": [{"name": "clangd", "command": ["clangd", "--background-index"],
                  "extensions": {".c": "c", ".h": "c", ".cpp": "cpp"},
                  "install": "apt install clangd"}]}

Everything that reaches a language server reads this table through `load_servers`: the diagnostics client, the
Claude Code plugin generator, the MCP bridge and the doctor. Presets (`lsp_presets.json`, beside this file) fill it
for a stack. Standard library only.
"""
import json
import os
import re
import tempfile
from pathlib import Path, PurePosixPath

TABLE = ".conclave/lsp.json"
PRESETS = Path(__file__).resolve().parent / "lsp_presets.json"
NAME = re.compile(r"[a-z0-9][a-z0-9-]*")
REQUIRED = ("command", "extensions", "install")
# Optional keys and the value the client uses when a server leaves one out.
DEFAULTS = {"root": ".", "requires": None, "init": {}, "settings": None, "severity": "error", "settle": 5,
            "note": None}
SEVERITIES = ("error", "warning")
SETTLE_RANGE = (1, 120)
NESTING = 32   # levels `init` and `settings` may nest: every tool that reads them walks them


class TableError(ValueError):
    """A server table, or the presets file, that cannot be used. The message names the file, the entry and the key."""


class TableWriteError(TableError):
    """The table could not be written: the system's reason, with the table left as it was."""


def _inside(value):
    """Whether a path given in the table stays inside the project: relative, with no `..` and no drive."""
    if not isinstance(value, str) or not value or "\\" in value:
        return False
    path = PurePosixPath(value)
    # `.` and `./` name the folder itself and have no parts: a root may be that, a file may not.
    return bool(path.parts) and not path.is_absolute() and ".." not in path.parts and ":" not in path.parts[0]


def _is_here(value):
    return isinstance(value, str) and bool(value) and "\\" not in value and not PurePosixPath(value).parts


def _too_deep(value, limit=NESTING):
    """Whether objects and lists nest deeper than `limit` in `value`. Walked with a list, not by calling itself:
    the depth is the thing being measured."""
    pending = [(value, 1)]
    while pending:
        item, depth = pending.pop()
        if isinstance(item, (dict, list)):
            if depth > limit:
                return True
            pending.extend((child, depth + 1) for child in (item.values() if isinstance(item, dict) else item))
    return False


def check_entry(where, name, entry):
    """One server entry, checked and with its defaults filled in: the dict every consumer reads."""
    where = f"{where}: server {name!r}"
    if not isinstance(entry, dict):
        raise TableError(f"{where}: a server is an object")
    unknown = sorted(set(entry) - set(REQUIRED) - set(DEFAULTS))
    if unknown:
        raise TableError(f"{where}: unknown key {unknown[0]!r}")
    missing = [key for key in REQUIRED if key not in entry]
    if missing:
        raise TableError(f"{where}: missing key {missing[0]!r}")
    command = entry["command"]
    if not isinstance(command, list) or not command or not all(isinstance(part, str) and part for part in command):
        raise TableError(f"{where}: command: a list of the program and its arguments, each a non-empty string")
    extensions = entry["extensions"]
    if not isinstance(extensions, dict) or not extensions:
        raise TableError(f"{where}: extensions: an object from a file suffix to its language id")
    for suffix, language in extensions.items():
        if not suffix.startswith(".") or len(suffix) < 2:
            raise TableError(f"{where}: extensions: a suffix starts with a dot, not {suffix!r}")
        if not isinstance(language, str) or not language:
            raise TableError(f"{where}: extensions: the language id of {suffix!r} is a non-empty string")
    install = entry["install"]
    if not isinstance(install, str) or not install.strip() or "\n" in install or "\r" in install:
        raise TableError(f"{where}: install: a one-line hint")
    checked = {"name": name, "command": list(command), "extensions": dict(extensions), "install": install}
    for key, default in DEFAULTS.items():
        value = entry.get(key, default)
        if key == "root":
            if not (_is_here(value) or _inside(value)):
                raise TableError(f"{where}: root: a folder inside the project, not {value!r}")
            value = "." if _is_here(value) else PurePosixPath(value).as_posix()
        if key == "requires" and value is not None and not _inside(value):
            raise TableError(f"{where}: requires: a file inside the project, not {value!r}")
        if key == "init" and not isinstance(value, dict):
            raise TableError(f"{where}: init: an object")
        if key == "settings" and value is not None and not isinstance(value, dict):
            raise TableError(f"{where}: settings: an object")
        if key in ("init", "settings") and _too_deep(value):
            raise TableError(f"{where}: {key}: nested deeper than {NESTING} levels")
        if key == "severity" and value not in SEVERITIES:
            raise TableError(f"{where}: severity: error or warning, not {value!r}")
        if key == "settle" and (isinstance(value, bool) or not isinstance(value, (int, float))
                                or not SETTLE_RANGE[0] <= value <= SETTLE_RANGE[1]):
            raise TableError(f"{where}: settle: a number of seconds from {SETTLE_RANGE[0]} to {SETTLE_RANGE[1]}")
        if key == "note" and value is not None and (not isinstance(value, str) or "\n" in value or "\r" in value):
            raise TableError(f"{where}: note: one line")
        checked[key] = value
    return checked


def find_program(root, entry):
    """The file a server's program is, as text, or None when it is not installed.

    A bare name is looked for on PATH and nowhere else: not in the working directory, which on Windows the
    standard lookup tries first and which has nothing to do with the project. A program named with a folder is
    read from the folder the server runs in (`root`), as the client starts it; an absolute path is read as it is.
    Either way it must be a file the system will execute."""
    program = entry["command"][0]
    windows = os.name == "nt"
    extensions = [ext for ext in os.environ.get("PATHEXT", ".COM;.EXE;.BAT;.CMD").split(os.pathsep) if ext] \
        if windows else []

    def runnable(path):
        names = [path]
        if windows and path.suffix.upper() not in {ext.upper() for ext in extensions}:
            names = [path.with_name(path.name + ext) for ext in extensions]
        for candidate in names:
            if candidate.is_file() and os.access(candidate, os.X_OK):
                return candidate
        return None

    if "/" in program or "\\" in program:
        path = Path(program)
        found = runnable(path if path.is_absolute() else Path(root) / entry["root"] / path)
        return found.as_posix() if found else None
    for folder in os.environ.get("PATH", "").split(os.pathsep):
        found = runnable(Path(folder) / program) if folder else None
        if found:
            return str(found)
    return None


def _read_json(path, where):
    try:
        return json.loads(Path(path).read_bytes().decode("utf-8"))
    except OSError as exc:
        raise TableError(f"{where}: cannot be read ({exc.strerror or exc})") from exc
    except UnicodeDecodeError as exc:
        raise TableError(f"{where}: not UTF-8 ({exc})") from exc
    except (ValueError, RecursionError) as exc:
        raise TableError(f"{where}: not JSON ({exc})") from exc


def _document(data, where, keys):
    if not isinstance(data, dict):
        raise TableError(f"{where}: an object with the keys {' and '.join(keys)}")
    unknown = sorted(set(data) - set(keys))
    if unknown:
        raise TableError(f"{where}: unknown key {unknown[0]!r}")
    missing = [key for key in keys if key not in data]
    if missing:
        raise TableError(f"{where}: missing key {missing[0]!r}")
    if type(data["schema_version"]) is not int or data["schema_version"] != 1:
        raise TableError(f"{where}: schema_version: must be 1")


def _name(where, number, name, seen):
    if not isinstance(name, str) or not NAME.fullmatch(name):
        raise TableError(f"{where}: server {number}: name: lowercase letters, digits and -, not {name!r}")
    if name in seen:
        raise TableError(f"{where}: server {name!r}: name: used by two servers")
    seen.add(name)


def load_servers(root):
    """The project's servers, checked, in the table's order. Raises TableError for a table that cannot be used."""
    data = _read_json(Path(root) / TABLE, TABLE)
    _document(data, TABLE, ("schema_version", "servers"))
    if not isinstance(data["servers"], list):
        raise TableError(f"{TABLE}: servers: a list of servers")
    servers, seen = [], set()
    for number, entry in enumerate(data["servers"], 1):
        if not isinstance(entry, dict):
            raise TableError(f"{TABLE}: server {number}: a server is an object")
        _name(TABLE, number, entry.get("name"), seen)
        servers.append(check_entry(TABLE, entry["name"], {k: v for k, v in entry.items() if k != "name"}))
    return servers


def load_presets(path=None):
    """The presets file, checked: {"servers": {name: entry}, "presets": {preset: {"servers": [...], "root_marker"}}},
    each preset's servers already resolved to full entries (the catalogue entry with the preset's overrides)."""
    path = Path(path or PRESETS)
    where = path.name
    data = _read_json(path, where)
    _document(data, where, ("schema_version", "servers", "presets"))
    if not isinstance(data["servers"], dict) or not isinstance(data["presets"], dict):
        raise TableError(f"{where}: servers and presets are objects")
    catalogue, seen = {}, set()
    for number, (name, entry) in enumerate(data["servers"].items(), 1):
        _name(where, number, name, seen)
        catalogue[name] = check_entry(where, name, entry)
    presets = {}
    for preset, body in data["presets"].items():
        at = f"{where}: preset {preset!r}"
        if not isinstance(preset, str) or not NAME.fullmatch(preset):
            raise TableError(f"{at}: a preset name is lowercase letters, digits and -")
        if not isinstance(body, dict) or set(body) - {"servers", "root_marker"} or "servers" not in body:
            raise TableError(f"{at}: an object with servers and, optionally, root_marker")
        marker = body.get("root_marker")
        if marker is not None and not _inside(marker):
            raise TableError(f"{at}: root_marker: a file name inside the project")
        if not isinstance(body["servers"], list) or not body["servers"]:
            raise TableError(f"{at}: servers: a list of at least one server")
        resolved, named = [], set()
        for item in body["servers"]:
            overrides = {}
            if isinstance(item, dict):
                overrides = {key: value for key, value in item.items() if key != "server"}
                item = item.get("server")
            if not isinstance(item, str) or item not in catalogue:
                raise TableError(f"{at}: names a server the catalogue lacks: {item!r}")
            if "name" in overrides:
                raise TableError(f"{at}: server {item!r}: a preset cannot rename a server")
            if item in named:
                raise TableError(f"{at}: server {item!r}: listed twice")
            named.add(item)
            raw = {key: value for key, value in data["servers"][item].items()}
            raw.update(overrides)
            resolved.append(check_entry(at, item, raw))
        presets[preset] = {"servers": resolved, "root_marker": marker}
    return {"servers": catalogue, "presets": presets}


def find_root(root, marker):
    """The folder of the project that holds `marker`: the project root, then `platform/`, then the first other
    immediate child in name order. Returned relative to the project (`.` for the root), or None."""
    root = Path(root)
    if (root / marker).is_file():
        return "."
    if (root / "platform" / marker).is_file():
        return "platform"
    for child in sorted(path for path in root.iterdir() if path.is_dir() and not path.is_symlink()):
        if (child / marker).is_file():
            return child.name
    return None


def resolve_preset(root, preset, app_root=None, presets=None):
    """The entries `add <preset>` writes: the preset's servers with `root` set to the folder its `root_marker`
    names, or to `app_root` when that is given. Raises TableError for an unknown preset or a marker not found."""
    presets = presets or load_presets()
    if preset not in presets["presets"]:
        known = ", ".join(sorted(presets["presets"]))
        raise TableError(f"unknown preset {preset!r}: the presets are {known}")
    body = presets["presets"][preset]
    folder = app_root
    if folder is not None:
        folder = PurePosixPath(str(folder).replace("\\", "/")).as_posix()
        if not (folder == "." or _inside(folder)) or not (Path(root) / folder).is_dir():
            raise TableError(f"--root: a folder inside the project, not {app_root!r}")
    elif body["root_marker"]:
        folder = find_root(root, body["root_marker"])
        if folder is None:
            raise TableError(f"preset {preset!r}: no folder of the project holds {body['root_marker']}; "
                             "name the app's folder with --root")
    return [dict(entry, root=folder) if folder is not None else dict(entry) for entry in body["servers"]]


def stored(entry):
    """An entry as the table stores it: its name, its required keys and only the optional keys that are not the
    default, so a table reads as what the project chose."""
    out = {"name": entry["name"], **{key: entry[key] for key in REQUIRED}}
    out.update({key: entry[key] for key, default in DEFAULTS.items() if entry.get(key, default) != default})
    return out


def write_servers(root, servers):
    """Write the table: a temporary file in its folder, flushed, then replaced. Returns False, writing nothing, when
    the table already holds exactly this. Raises TableWriteError naming the table when the write fails; the table
    is then as it was and no temporary file is left."""
    path = Path(root) / TABLE
    text = json.dumps({"schema_version": 1, "servers": [stored(entry) for entry in servers]}, indent=2) + "\n"
    data = text.encode("utf-8")
    try:
        if path.is_file() and path.read_bytes() == data:
            return False
    except OSError:
        pass  # unreadable now: the write below reports what is wrong with it
    temporary = None
    try:
        path.parent.mkdir(parents=True, exist_ok=True)
        fd, temporary = tempfile.mkstemp(dir=path.parent, prefix=".lsp-", suffix=".tmp")
        with os.fdopen(fd, "wb") as out:
            out.write(data)
            out.flush()
            os.fsync(out.fileno())
        os.replace(temporary, path)
        temporary = None
    except OSError as exc:
        raise TableWriteError(f"{TABLE}: cannot be written ({exc.strerror or exc})") from exc
    finally:
        if temporary is not None and os.path.exists(temporary):
            os.unlink(temporary)
    return True
