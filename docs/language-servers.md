# Language servers

The project's language servers are listed in one table, `.conclave/lsp.json` (`python scripts/lsp-servers.py list`). Three things read it:

- `python scripts/lsp-diagnostics.py` runs the servers over files and has an exit code. It is the gate.
- `python scripts/lsp-servers.py plugin` generates a Claude Code plugin, so Claude Code runs the servers while it edits.
- `scripts/lsp-mcp-bridge.py` is an MCP server that gives any agent the servers, read-only. Codex and Antigravity have no language server plugin, so this is how they reach them.

## Setting up

1. Add the servers of the project's stack to the table. A preset is a stack's servers (`cpp`, `python`, `php`, `web`, `laravel`), and `list` shows the table and the presets.

```text
python scripts/lsp-servers.py add python
python scripts/lsp-servers.py list
```

2. Install the servers. Nothing here installs one: each entry of the table carries the line that installs its server, `list` shows the table, and `python forge.py doctor <project> lsp` (run from the SDK) says which are missing. A server that is not installed is reported as `SKIPPED`, never as a pass.

3. For Claude Code, generate the plugin and load it. The plugin is generated from the table; `claude --plugin-dir` loads it for a session, and the `CLAUDE_CODE_PLUGIN_DIRS` environment variable does the same where the command line cannot be changed.

```text
python scripts/lsp-servers.py plugin
claude --plugin-dir .claude/lsp/conclave-lsp
```

Once the plugin exists, `add` and `remove` keep it in step with the table. After the table was edited by hand, the plugin is behind it (the doctor's `claude-plugin` check says `unexpected`): run `python scripts/lsp-servers.py plugin` again.

4. For Codex and Antigravity, register the bridge (below).

To take a server out of the table later (it is not part of setting up):

```text
python scripts/lsp-servers.py remove basedpyright
```

## Checking a change

Run the servers on the files a change touched. Give one input. `--changed` is every file the change touched: staged, not yet staged and new. `--staged` and `--worktree` each see one part of that, a commit's files are `--commit`, and paths name files directly. When the run says no file was in the input, nothing was checked.

```text
python scripts/lsp-diagnostics.py --all --changed
python scripts/lsp-diagnostics.py --all --staged
python scripts/lsp-diagnostics.py --all --worktree
python scripts/lsp-diagnostics.py --all --commit HEAD
python scripts/lsp-diagnostics.py --server basedpyright src/app.py
python scripts/lsp-diagnostics.py --all --staged --json
```

A file goes only to the servers that serve it. Each server ends with one outcome, and the exit code follows from them. A check that did not finish is never a pass.

| Run | The client prints | Exit |
| --- | --- | --- |
| Every server finished, no diagnostic at or above its threshold | the diagnostics, if any | 0 |
| A diagnostic at or above a server's threshold | the diagnostic | 1 |
| One server not installed, another finished clean | `SKIPPED` for the one | 0 |
| No requested server installed | `SKIPPED` for each | 3 |
| A server could not finish, another finished clean | `FAILED` with the reason | 1 |
| A server answered only some of its files | `PARTIAL` naming the files | 1 |
| Bad usage, an unknown server, a table that does not load | one line saying which | 2 |

`SKIPPED` means the server is not installed, or a file it requires is absent: nothing was checked by it. `FAILED` means it could not be run to the end (it would not start, refused, ended early or was stopped at a limit). `PARTIAL` means it ran and left some file unanswered or unread. Each server has its own threshold in the table (`severity`); `--severity` sets one for the run.

## The MCP bridge

The bridge offers four tools, each read-only. `file` is a path from the project root; `line` and `column` count from 1, a column in characters, in what is asked and in what is answered, so a place the bridge returns can be given back to it.

| Tool | Arguments | Answer |
| --- | --- | --- |
| `diagnostics` | `file` | What every server that serves the file reports, with each server's outcome |
| `definition` | `file`, `line`, `column` | Where the symbol is defined, as `file:line:column` |
| `references` | `file`, `line`, `column` | Where the symbol is used, one `file:line:column` a line |
| `hover` | `file`, `line`, `column` | What the server says of the symbol |

It starts a server on the first call that needs it and keeps it running, sends a file again when it has changed, and ends every server when the agent ends the bridge. A call the agent cancels stops waiting at once. It cannot rename, format or apply an edit: the only requests it sends a server are `initialize`, `shutdown`, `textDocument/hover`, `textDocument/definition` and `textDocument/references`. It installs no server: `python forge.py doctor <project> lsp` says which are missing.

To try the bridge by hand, send it one request. The same line works in `sh` and in PowerShell:

```text
echo '{"jsonrpc":"2.0","id":1,"method":"tools/list"}' | python scripts/lsp-mcp-bridge.py
```

It prints one line of JSON that names the four tools, and exits 0. The end of its input ends the bridge once it has answered what arrived before it, so a request sent this way is answered. A `tools/call` that needs a language server and is not answered when the input ends is answered that the input ended before it was answered, so it was not run to its end, and a server it had started is ended: such a call needs the input kept open until its answer has been read, which is how an agent holds it. A call answered without a server (arguments that do not fit, a file outside the project or not in it, a file no server serves) gets its usual answer this way too. When the reader of its output has gone, the bridge ends quietly with 0; when its output fails for another reason (a full disk), it says so on stderr and ends with 1.

## Registering the bridge

Each entry below runs `python scripts/lsp-mcp-bridge.py`. The path is relative, so it is read from the folder the agent is started in: start the agent at the project root, or give the script's full path. Where `python` is not Python 3 on the machine, name `python3`.

Each registration was confirmed on 2026-11-15 on Windows 11 by a real session of that agent, which found the tools, called `diagnostics` and got a stub server's diagnostic back (the SDK's `tests/test_lsp_kit.py`, run with `CONCLAVE_LIVE_AGENTS=1`).

### Codex

Confirmed with Codex 0.169.0. From `codex mcp add --help`: `codex mcp add [OPTIONS] <NAME> (--url <URL> | -- <COMMAND>...)`.

```text
codex mcp add conclave-lsp -- python scripts/lsp-mcp-bridge.py
```

It writes this table to `~/.codex/config.toml`, the user's configuration (seen by running the command against a scratch `CODEX_HOME`):

```toml
[mcp_servers.conclave-lsp]
command = "python"
args = ["scripts/lsp-mcp-bridge.py"]
```

In an interactive session Codex asks before the first call of a tool. A run with no one to ask (`codex exec`) refuses the call ("MCP tool call requires approval, but approval policy is never") unless the server's tools are approved in the configuration. The tools are read-only, so approving them is safe:

```toml
[mcp_servers.conclave-lsp]
command = "python"
args = ["scripts/lsp-mcp-bridge.py"]
default_tools_approval_mode = "approve"
```

`codex mcp get conclave-lsp` shows the entry and `codex mcp remove conclave-lsp` removes it.

### Antigravity

Confirmed with Antigravity 1.5.9 (`agy`). From `agy mcp add --help`: `agy mcp add [flags] <name> <commandOrUrl> [args...]`, with flags before the name.

```text
agy mcp add conclave-lsp python scripts/lsp-mcp-bridge.py
```

The entry goes to the user's configuration (`~/.gemini/config/mcp_config.json` on the machine this was confirmed on). `agy mcp list` shows it and `agy mcp remove conclave-lsp` removes it. Its session-only `--settings` option does not take MCP servers (`unknown setting "mcpServers"`), so there is no registration for one run.

### Claude Code

Claude Code has the plugin (`python scripts/lsp-servers.py plugin`), which gives it the servers natively, and needs the bridge only where the plugin is not used. Confirmed with Claude Code 2.1.325, from a project `.mcp.json` or a file given with `--mcp-config`:

```json
{
  "mcpServers": {
    "conclave-lsp": {
      "command": "python",
      "args": ["scripts/lsp-mcp-bridge.py"]
    }
  }
}
```

A project that already has an `.mcp.json` adds the `conclave-lsp` entry to its `mcpServers`. `python forge.py doctor <project> lsp` reports `mcp-bridge` as `ok` when it finds one there.
