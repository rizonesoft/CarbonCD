#!/usr/bin/env python3
"""An MCP server that gives an agent the project's language servers, read-only.

    python scripts/lsp-mcp-bridge.py [--root <project>] [--timeout <seconds>]

Codex and Antigravity have no language server plugin; they reach the servers of `.conclave/lsp.json` through this
bridge, registered as an MCP server (docs/language-servers.md says how, for each agent). It speaks MCP over
standard input and output, one JSON-RPC message per line, and offers four tools:

    diagnostics   file                  what every server that serves the file reports for it
    definition    file, line, column    where the symbol at that place is defined
    references    file, line, column    where it is used
    hover         file, line, column    what the server says of it

`file` is a path inside the project; `line` and `column` count from 1, a column in characters, in what is asked
and in what is answered, so a place the bridge returns can be given back to it. A file goes to the servers that serve it
(its suffix, under the server's `root`). `diagnostics` asks every one of them; the other three ask them in the
table's order until one gives an answer that is not empty, and name what each earlier one did.

Each server is started on the first call that needs it and kept running: a file that changed since it was last
sent is sent again as a change, and a server that ended is started again by the next call. A server holds at most
64 open documents (the one used longest ago is closed for the next), and one that stops reading is ended.

The bridge is read-only by an allow-list: the only requests it can send a server are `initialize`, `shutdown`,
`textDocument/hover`, `textDocument/definition` and `textDocument/references`. Nothing that edits can be asked
of it, and a server that asks for an edit is told it was not made.

Every server is ended, with whatever it started, when the bridge ends: at the end of its input, on a signal, on
an error, also while a server is being started or a call is waiting. A call the agent cancels
(`notifications/cancelled`) stops waiting at once and is not answered. One message may be 4 MiB and 256 may
wait to be handled; more is refused or made to wait, never held. Standard library only.
"""
import argparse
import collections
import errno
import hashlib
import json
import os
import queue
import shutil
import signal
import sys
import threading
import time
from pathlib import Path
from urllib.parse import unquote, urlparse

sys.path.insert(0, str(Path(__file__).resolve().parent / "lib"))
import lsp_client  # noqa: E402
import lsp_table  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
NAME = "conclave-lsp"
VERSIONS = ("2025-06-18", "2025-03-26", "2024-11-05")   # the MCP versions this speaks, the latest first
TIMEOUT = 60.0                      # seconds one call may wait on a server
OPEN_DOCUMENTS = 64                 # documents kept open in one server
OUTBOX_LIMIT = 64 * 1024 * 1024     # bytes that may wait to be written to one server
MESSAGE_LIMIT = 4 * 1024 * 1024     # bytes one MCP message may be
WAITING_LIMIT = 256                 # MCP messages that may wait to be handled
OVERSIZE = object()                 # stands for a message that was longer than MESSAGE_LIMIT
READ_ONLY_REQUESTS = frozenset({"initialize", "shutdown", "textDocument/hover", "textDocument/definition",
                                "textDocument/references"})
POSITION_TOOLS = {"definition": "textDocument/definition", "references": "textDocument/references",
                  "hover": "textDocument/hover"}

_FILE = {"type": "string", "description": "A file of the project, as a path from the project root."}
_PLACE = {"file": _FILE,
          "line": {"type": "integer", "minimum": 1, "description": "The line, counted from 1."},
          "column": {"type": "integer", "minimum": 1, "description": "The column, counted from 1."}}


def _tool(name, description, properties):
    return {"name": name, "description": description,
            "inputSchema": {"type": "object", "properties": properties, "required": list(properties),
                            "additionalProperties": False}}


TOOLS = [
    _tool("diagnostics", "The diagnostics every language server of the project reports for one file, with each "
                         "server's outcome. Read-only.", {"file": _FILE}),
    _tool("definition", "Where the symbol at a place in a file is defined, as file:line:column. Read-only.", _PLACE),
    _tool("references", "Where the symbol at a place in a file is used, as file:line:column lines. Read-only.",
          _PLACE),
    _tool("hover", "What the language server says of the symbol at a place in a file (its type, its "
                   "documentation). Read-only.", _PLACE),
]


class InvalidParams(Exception):
    """Arguments that do not fit a tool's schema, or a tool that does not exist: JSON-RPC error -32602."""


class ToolError(Exception):
    """A call that could not be answered: a tool result with `isError`."""


class InputEnded(Exception):
    """A call that would reach a language server after the bridge's input has ended. No server is started or
    asked for it: the call is answered with INPUT_ENDED."""


INPUT_ENDED = ("The input ended before this call was answered, so it was not run to its end: a client keeps the "
               "bridge's input open until it has read the answer.")


class Forbidden(Exception):
    """An LSP request that is not on the read-only allow-list. It is never written to a server."""


class Halt:
    """What ends a wait: the bridge being told to stop, the agent cancelling the call in hand, or the input
    having ended (nobody is left to send what a call might still need, and perhaps nobody to read its answer)."""

    def __init__(self, stop):
        self.stop = stop
        self.cancel = threading.Event()
        self.ended = threading.Event()

    def is_set(self):
        return self.stop.is_set() or self.cancel.is_set() or self.ended.is_set()


def characters(line, units):
    """How many characters of `line` come before the UTF-16 offset `units`, which is how LSP counts."""
    return len(line.encode("utf-16-le")[:units * 2].decode("utf-16-le", errors="ignore"))


class Session:
    """One running server and the documents the bridge has open in it."""

    def __init__(self, entry, client, scratch):
        self.entry = entry
        self.client = client
        self.scratch = scratch
        self.documents = collections.OrderedDict()   # uri -> {"version", "hash", "since", "answered"}

    def close(self):
        try:
            self.client.close()
        finally:
            if self.scratch is not None:
                shutil.rmtree(self.scratch, ignore_errors=True)


class Bridge:
    def __init__(self, root, timeout=TIMEOUT, stop=None, open_documents=OPEN_DOCUMENTS, outbox_limit=OUTBOX_LIMIT):
        self.root = Path(root).resolve()
        self.timeout = timeout
        self.stop = stop or threading.Event()
        self.halt = Halt(self.stop)
        self.unread = False   # set when a reply could not be written: nothing more is written to the output
        self.reached = False  # whether the call in hand has reached a server
        self.open_documents = open_documents
        self.outbox_limit = outbox_limit
        self.sessions = {}

    def close(self):
        """End every server, each with whatever it started."""
        sessions, self.sessions = list(self.sessions.values()), {}
        for session in sessions:
            session.close()

    # ---------------------------------------------------------------- servers

    def serving(self, path):
        """The table's servers that serve a file, in the table's order: [(entry, language id)]."""
        try:
            servers = lsp_table.load_servers(self.root)
        except lsp_table.TableError as exc:
            raise ToolError(str(exc)) from exc
        if not servers:
            raise ToolError(f"{lsp_table.TABLE} holds no server: add a stack's with "
                            "python scripts/lsp-servers.py add <preset>")
        return [(entry, language) for entry in servers
                for language in [lsp_client.serves(entry, self.root, path)] if language]

    def session(self, entry):
        """The running session of a server, started when there is none, or its entry changed, or it ended:
        (session, None), or (None, why it is not running). Every call that needs a server comes through here
        first: once the input has ended none is started or asked (InputEnded), and the call is marked as one that
        reached a server, so `call` knows its result may have been cut short."""
        if self.halt.ended.is_set():
            raise InputEnded()
        self.reached = True
        name = entry["name"]
        session = self.sessions.get(name)
        if session is not None and (session.entry != entry or not session.client.alive or session.client.failure):
            del self.sessions[name]
            session.close()
            session = None
        if session is None:
            status, reason, client, scratch, _stderr = lsp_client.start_server(
                entry, self.root, timeout=min(90.0, self.timeout), stop=self.halt, read_limit=None,
                outbox_limit=self.outbox_limit, keep=set())
            if status != "ok":
                return None, f"{status}: {reason}"
            session = self.sessions[name] = Session(entry, client, scratch)
        return session, None

    def drop(self, session):
        """End a session that failed during a call; the next call starts the server again."""
        self.sessions.pop(session.entry["name"], None)
        session.close()

    def lsp_request(self, session, method, params):
        """Send a server one request of the read-only allow-list and return its response message (None when it
        did not answer). Any other method is refused here, before anything is written to the server."""
        if method not in READ_ONLY_REQUESTS:
            raise Forbidden(f"{method} is not a read-only request: this bridge sends only "
                            f"{', '.join(sorted(READ_ONLY_REQUESTS))}")
        return session.client.request(method, params, timeout=self.timeout, stop=self.halt)

    def sync(self, session, path, language, text):
        """Make the server hold the file's text as it is now: opened the first time, sent as a change when it
        differs from what was last sent. Returns the document's URI and the bridge's record of it."""
        client = session.client
        uri = path.as_uri()
        digest = hashlib.sha256(text.encode("utf-8", errors="replace")).hexdigest()
        document = session.documents.get(uri)
        if document is None:
            while len(session.documents) >= self.open_documents:
                oldest, _record = session.documents.popitem(last=False)   # the one used longest ago
                client.keep.discard(oldest)
                client.close_document(oldest)
            client.keep.add(uri)
            document = session.documents[uri] = {"version": 1, "hash": digest, "since": client.sequence,
                                                 "answered": False}
            client.notify("textDocument/didOpen", {"textDocument": {"uri": uri, "languageId": language, "version": 1,
                                                                    "text": text}})
        elif document["hash"] != digest:
            # Whatever was published for the old text is not an answer for the new one.
            document.update(version=document["version"] + 1, hash=digest, since=client.sequence, answered=False)
            client.change(uri, text, document["version"])
        session.documents.move_to_end(uri)
        return uri, document

    def fresh(self, session, uri, document):
        """The order number of a publication for the document's present text, or None: one that arrived after
        the text was sent and, where the server gives versions, carries that version."""
        published = session.client.published.get(uri)
        if published is None or published[0] <= document["since"]:
            return None
        if published[1] is not None and published[1] != document["version"]:
            return None
        return published[0]

    def await_diagnostics(self, session, uri, document):
        """The diagnostics for the document's present text, or None when the server published none in time.
        The first publication is waited for; the server is then listened to for its `settle` and the last taken,
        so a late answer for an older text is overtaken by the one for this."""
        client = session.client
        if not document["answered"]:
            deadline = time.monotonic() + self.timeout
            settle = session.entry["settle"]
            answered_at = None
            while time.monotonic() < deadline and client.alive and not self.halt.is_set():
                if self.fresh(session, uri, document) is not None:
                    if answered_at is None:
                        answered_at = time.monotonic()
                    elif time.monotonic() - answered_at >= settle:
                        break
                time.sleep(0.02)
            if answered_at is None:
                return None
            document["answered"] = True
        return list(client.diagnostics.get(uri, [])) if self.fresh(session, uri, document) is not None else None

    # ---------------------------------------------------------------- tools

    def file(self, arguments):
        """The file a call names, as a path inside the project, and its text."""
        name = arguments["file"]
        path = (self.root / name).resolve()
        try:
            path.relative_to(self.root)
        except ValueError:
            raise ToolError(f"{name} is outside the project") from None
        if not path.is_file():
            raise ToolError(f"{name} is not a file of the project")
        try:
            return path, path.read_text(encoding="utf-8", errors="replace")
        except OSError as exc:
            raise ToolError(f"{name} cannot be read ({exc.strerror or exc})") from exc

    def shown(self, path):
        try:
            return Path(path).resolve().relative_to(self.root).as_posix()
        except ValueError:
            return Path(path).as_posix()

    def diagnostics(self, arguments):
        path, text = self.file(arguments)
        file = self.shown(path)
        serving = self.serving(path)
        if not serving:
            return f"No server of {lsp_table.TABLE} serves {file}.", False
        lines, checked = [], 0
        for entry, language in serving:
            name = entry["name"]
            session, why = self.session(entry)
            if session is None:
                lines.append(f"{name}: {why}")
                continue
            uri, document = self.sync(session, path, language, text)
            found = self.await_diagnostics(session, uri, document)
            ended = session.client.failure or (None if session.client.alive else "ended before answering")
            if ended:
                self.drop(session)   # the next call starts it again
            if found is None:
                # An answer that came before the server ended is an answer; without one, why it ended is said.
                lines.append(f"{name}: failed: {ended}" if ended
                             else f"{name}: not answered within {self.timeout:g} seconds")
                continue
            try:
                rows = [lsp_client._row(name, file, item) for item in found]
                source = text.splitlines()
                for row in rows:   # the server's column is in UTF-16 units: shown in characters
                    if row["line"] <= len(source):
                        row["column"] = characters(source[row["line"] - 1], row["column"] - 1) + 1
            except ValueError:
                lines.append(f"{name}: failed: published a diagnostic that is not valid for {file}")
                continue
            checked += 1
            lines.append(f"{name}: {len(rows)} diagnostic(s)" if rows else f"{name}: clean")
            for row in sorted(rows, key=lambda row: (row["line"], row["column"])):
                code = f" [{row['code']}]" if row["code"] else ""
                lines.append(f"  {row['file']}:{row['line']}:{row['column']}  {row['severity']}  {row['message']}{code}")
        return "\n".join(lines), checked == 0

    def position(self, tool, arguments):
        path, text = self.file(arguments)
        file = self.shown(path)
        serving = self.serving(path)
        if not serving:
            return f"No server of {lsp_table.TABLE} serves {file}.", False
        line, column = arguments["line"], arguments["column"]
        source = text.splitlines()
        # LSP counts a column in UTF-16 units; a place past the end of the file is sent as it was given.
        character = len(source[line - 1][:column - 1].encode("utf-16-le")) // 2 if line <= len(source) \
            else column - 1
        place = {"line": line - 1, "character": character}
        method = POSITION_TOOLS[tool]
        outcomes, asked = [], 0
        for entry, language in serving:
            name = entry["name"]
            session, why = self.session(entry)
            if session is None:
                outcomes.append(f"{name}: {why}")
                continue
            uri, _document = self.sync(session, path, language, text)
            params = {"textDocument": {"uri": uri}, "position": place}
            if tool == "references":
                params["context"] = {"includeDeclaration": True}
            response = self.lsp_request(session, method, params)
            ended = session.client.failure or (None if session.client.alive else "ended before answering")
            if ended:
                self.drop(session)   # the next call starts it again
            if response is None:
                # An answer that came before the server ended is an answer; without one, why it ended is said.
                outcomes.append(f"{name}: failed: {ended}" if ended
                                else f"{name}: not answered within {self.timeout:g} seconds")
                continue
            if "error" in response:
                error = response["error"] if isinstance(response["error"], dict) else {}
                if error.get("code") == -32601:
                    outcomes.append(f"{name}: does not support {method}")
                else:
                    outcomes.append(f"{name}: answered with an error: {str(error.get('message') or response['error'])[:300]}")
                continue
            asked += 1
            answer = self.hover_text(response.get("result")) if tool == "hover" \
                else self.places(response.get("result"))
            if answer:
                return "\n".join([f"{name}:", *[f"  {item}" for item in answer],
                                  *[f"({outcome})" for outcome in outcomes]]), False
            outcomes.append(f"{name}: no {tool} at {file}:{line}:{column}")
        return "\n".join([f"No {tool} found at {file}:{line}:{column}.", *outcomes]), asked == 0

    def places(self, result):
        """A definition or references result as `file:line:column` lines, whatever form the server chose
        (a Location, a list of them, or LocationLinks)."""
        items = result if isinstance(result, list) else [result] if isinstance(result, dict) else []
        found = []
        for item in items:
            if not isinstance(item, dict):
                continue
            uri = item.get("uri") or item.get("targetUri")
            span = item.get("range") or item.get("targetSelectionRange") or item.get("targetRange")
            start = span.get("start") if isinstance(span, dict) else None
            if not isinstance(uri, str) or not isinstance(start, dict):
                continue
            line, character = start.get("line"), start.get("character")
            if type(line) is not int or type(character) is not int:
                continue
            parsed = urlparse(uri)
            where = unquote(parsed.path)
            if parsed.scheme == "file":
                # file:///C:/x on Windows carries the drive after a slash.
                where = where[1:] if len(where) > 2 and where[0] == "/" and where[2] == ":" else where
                # The server counts the column in UTF-16 units; it is shown in characters, as it is asked.
                source = self.lines(where)
                if line < len(source):
                    character = characters(source[line], character)
                where = self.shown(where)
            place = f"{where}:{line + 1}:{character + 1}"
            if place not in found:
                found.append(place)
        return found

    def lines(self, path):
        """The lines of a file a server pointed at, or none when it cannot be read."""
        try:
            return Path(path).read_text(encoding="utf-8", errors="replace").splitlines()
        except (OSError, ValueError):
            return []

    def hover_text(self, result):
        """A hover result as lines of text: MarkupContent, a marked string, or a list of them."""
        contents = result.get("contents") if isinstance(result, dict) else None
        parts = contents if isinstance(contents, list) else [contents]
        lines = []
        for part in parts:
            value = part.get("value") if isinstance(part, dict) else part
            if isinstance(value, str) and value.strip():
                lines.extend(value.strip().splitlines())
        return lines

    def call(self, name, arguments):
        """One tool call: (text, whether it is an error). Raises InvalidParams for a tool that does not exist or
        arguments that do not fit its schema. What needs no server is answered whenever it is asked (arguments
        that do not fit, a file outside the project, a file no server serves). A call that needs one and was not
        answered when the input ended, because it had not reached a server yet or was cut short while it waited
        on one, is answered INPUT_ENDED: the same answer either way."""
        schema = next((tool["inputSchema"] for tool in TOOLS if tool["name"] == name), None)
        if schema is None:
            raise InvalidParams(f"Unknown tool: {name}")
        if not isinstance(arguments, dict):
            raise InvalidParams(f"{name}: arguments must be an object")
        for key in arguments:
            if key not in schema["properties"]:
                raise InvalidParams(f"{name}: unknown argument {key!r}")
        for key, rule in schema["properties"].items():
            value = arguments.get(key)
            if rule["type"] == "string" and (not isinstance(value, str) or not value):
                raise InvalidParams(f"{name}: {key} must be a path")
            if rule["type"] == "integer" and (type(value) is not int or value < 1):
                raise InvalidParams(f"{name}: {key} must be a whole number from 1")
        self.reached = False
        try:
            result = self.diagnostics(arguments) if name == "diagnostics" else self.position(name, arguments)
        except ToolError as exc:
            result = str(exc), True
        except InputEnded:
            return INPUT_ENDED, True
        if self.reached and self.halt.ended.is_set():
            return INPUT_ENDED, True   # the wait on a server was cut: what it holds is not a whole answer
        return result


# ------------------------------------------------------------------ MCP

def _error(number, code, message):
    return {"jsonrpc": "2.0", "id": number, "error": {"code": code, "message": message}}


def handle(bridge, line):
    """The reply to one line of input, or None when it asks for none (a notification)."""
    if line is OVERSIZE:
        return _error(None, -32600, f"Invalid request: a message may be {MESSAGE_LIMIT} bytes")
    try:
        message = json.loads(line)
    except (ValueError, RecursionError):
        return _error(None, -32700, "Parse error: a message is one line of JSON")
    if not isinstance(message, dict) or not isinstance(message.get("method"), str):
        if isinstance(message, dict) and "id" in message and "method" not in message:
            return None   # a response to something this server never asked
        return _error(message.get("id") if isinstance(message, dict) else None, -32600, "Invalid request")
    method, params = message["method"], message.get("params")
    if "id" not in message:
        return None   # a notification (notifications/initialized, notifications/cancelled): nothing is replied
    number = message["id"]
    params = params if isinstance(params, dict) else {}
    if method == "initialize":
        asked = params.get("protocolVersion")
        return {"jsonrpc": "2.0", "id": number, "result": {
            "protocolVersion": asked if asked in VERSIONS else VERSIONS[0],
            "capabilities": {"tools": {"listChanged": False}},
            "serverInfo": {"name": NAME, "version": "1.0.0"},
            "instructions": "Read-only access to the project's language servers (.conclave/lsp.json): diagnostics "
                            "for a file, and the definition, references and hover of a symbol."}}
    if method == "ping":
        return {"jsonrpc": "2.0", "id": number, "result": {}}
    if method == "tools/list":
        return {"jsonrpc": "2.0", "id": number, "result": {"tools": TOOLS}}
    if method == "tools/call":
        try:
            text, failed = bridge.call(params.get("name"), params.get("arguments", {}))
        except InvalidParams as exc:
            return _error(number, -32602, str(exc))
        return {"jsonrpc": "2.0", "id": number,
                "result": {"content": [{"type": "text", "text": text}], "isError": failed}}
    return _error(number, -32601, f"Method not found: {method}")


def reader_gone(exc):
    """Whether an error from writing the output means that its reader has gone (a closed pipe), and not that the
    output itself failed (a full disk, an I/O error). Windows reports a pipe closed at the other end as an
    invalid argument as well as a broken pipe."""
    if isinstance(exc, (BrokenPipeError, ConnectionError)):
        return True
    return os.name == "nt" and isinstance(exc, OSError) and exc.errno == errno.EINVAL


def serve(bridge, source, sink):
    """Answer MCP messages from `source` on `sink` until the input ends, the bridge is told to stop, or an error
    nobody expected: 0, or 1 for such an error. Input is read on a thread of its own, so the end of input and a
    signal are seen while a call is waiting on a server. The servers are closed on every way out.

    The end of input ends the bridge once what arrived before it is answered, in order: whatever needs no server
    as always, and a `tools/call` that needs one and was not answered by then (not yet at a server, or cut short
    while it waited on one) with a tool error that says so, which `Bridge.call` decides. No server is started or
    asked after the input has ended. A signal
    ends the bridge at once, with nothing more answered, and so does an output nobody reads any more. An output
    that fails for any other reason (a full disk, an I/O error) is an answer lost: it is said on stderr, with 1."""
    inbox = queue.Queue(maxsize=WAITING_LIMIT)
    in_hand = {"id": None}   # the request being handled, for a cancellation that names it
    ended = bridge.halt.ended

    def say(reply):
        """Write one reply: None when it is written, else the exit code to end with: 0 when nobody reads the
        output any more (the agent has gone), 1 when the output failed, which is said on stderr."""
        try:
            sink.write((json.dumps(reply) + "\n").encode("utf-8"))
            sink.flush()
        except ValueError:   # the output was closed in this process
            bridge.unread = True
            return 0
        except OSError as exc:
            bridge.unread = True   # either way nothing more can be written to it
            if reader_gone(exc):
                return 0
            print(f"lsp-mcp-bridge: stopped: the output could not be written ({exc.strerror or exc}), so an answer "
                  "was lost", file=sys.stderr)
            return 1
        return None

    def cancels(line):
        """Whether a line is the agent cancelling the request in hand. It is read here, on the input's thread,
        because the call it cancels is what the main thread is busy with."""
        if line is OVERSIZE or b"notifications/cancelled" not in line:
            return False
        try:
            message = json.loads(line)
            params = message.get("params")
            return (message.get("method") == "notifications/cancelled" and in_hand["id"] is not None
                    and params.get("requestId") == in_hand["id"])
        except (ValueError, AttributeError, RecursionError):
            return False

    def read():
        try:
            for line in source:
                if cancels(line):
                    bridge.halt.cancel.set()
                    continue
                while not bridge.stop.is_set():
                    try:
                        inbox.put(line, timeout=0.2)   # full: the agent is made to wait, nothing piles up
                        break
                    except queue.Full:
                        continue
        except (OSError, ValueError):
            pass
        finally:
            ended.set()   # after every line was handed over: what arrived is answered, a call in hand is cut

    threading.Thread(target=read, daemon=True).start()
    try:
        while True:
            if bridge.stop.is_set():
                return 0   # a signal: what was still waiting is not answered
            try:
                line = inbox.get(timeout=0.2)
            except queue.Empty:
                if not ended.is_set():
                    continue
                try:
                    line = inbox.get_nowait()   # handed over between the wait and the look at `ended`
                except queue.Empty:
                    return 0   # the input has ended and everything that arrived is answered
            if line is not OVERSIZE and not line.strip():
                continue
            number = None
            if line is not OVERSIZE:
                try:
                    number = json.loads(line).get("id")
                except (ValueError, AttributeError, RecursionError):
                    number = None
            bridge.halt.cancel.clear()
            in_hand["id"] = number
            try:
                reply = handle(bridge, line)   # a call that needs a server after the input ended says so itself
            except Exception as exc:   # what state the servers are in is unknown: say so once, and end with them closed
                say(_error(number, -32603, f"Internal error: {type(exc).__name__}: {exc}"))   # 1 either way
                print(f"lsp-mcp-bridge: stopped by an error: {type(exc).__name__}: {exc}", file=sys.stderr)
                return 1
            in_hand["id"] = None
            # A call the agent cancelled is not answered (it has stopped listening for it), and nothing is after a
            # signal. The end of input silences nothing: whoever sent the message may still be reading.
            if reply is not None and not bridge.stop.is_set() and not bridge.halt.cancel.is_set():
                code = say(reply)
                if code is not None:
                    return code   # nobody reads the output (0), or it failed and an answer was lost (1)
    finally:
        bridge.stop.set()
        bridge.close()


def input_lines(descriptor, held=None):
    """The lines arriving on a file descriptor, read from it directly, a line longer than MESSAGE_LIMIT as
    OVERSIZE with none of it kept. The bridge ends while a thread is still
    waiting for input (a signal, an error); a buffered reader holds a lock for as long as it waits, which the
    interpreter cannot take back at exit, so standard input is read without one. `held`, a list, is given the
    most bytes this ever held at once (for the test of that bound)."""
    pending, oversize = b"", False
    while True:
        chunk = os.read(descriptor, 65536)
        if not chunk:
            break
        pending += chunk
        if held is not None:
            held[:] = [max([len(pending), *held])]
        while b"\n" in pending:
            line, pending = pending.split(b"\n", 1)
            yield OVERSIZE if oversize or len(line) > MESSAGE_LIMIT else line
            oversize = False
        if len(pending) > MESSAGE_LIMIT:
            # A message with no end in sight: nothing more of it is held, and its line is refused when it ends.
            pending, oversize = b"", True
    if oversize:
        yield OVERSIZE
    elif pending.strip():
        yield pending


def build_parser():
    """The script's argument parser: what `main` parses with, and what a documented command must fit."""
    parser = argparse.ArgumentParser(prog="lsp-mcp-bridge.py",
                                     description="An MCP server giving read-only access to the project's language "
                                                 "servers.")
    parser.add_argument("--root", help="the project (default: the folder above scripts/)")
    parser.add_argument("--timeout", type=float, default=TIMEOUT,
                        help=f"seconds one call may wait on a server (default: {TIMEOUT:g})")
    return parser


def main(argv=None):
    args = build_parser().parse_args(argv)
    stop = threading.Event()
    for name in ("SIGTERM", "SIGINT", "SIGBREAK"):
        if hasattr(signal, name):
            signal.signal(getattr(signal, name), lambda _number, _frame: stop.set())
    bridge = Bridge(args.root or ROOT, timeout=args.timeout, stop=stop)
    code = serve(bridge, input_lines(sys.stdin.fileno()), sys.stdout.buffer)
    if bridge.unread:
        # The output takes nothing more: point it at nothing, so the interpreter's own flush at exit does not
        # fail a second time and change the exit code.
        try:
            os.dup2(os.open(os.devnull, os.O_WRONLY), sys.stdout.fileno())
        except OSError:
            pass
    return code


if __name__ == "__main__":
    sys.exit(main())
