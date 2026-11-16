---
name: language-servers
description: Check a change with the project's language servers and look symbols up through them. Use before calling a code change done, and when you need the definition or the uses of a symbol.
---

# Language servers

The project's language servers are listed in one table, `.conclave/lsp.json` (`python scripts/lsp-servers.py list`). `docs/language-servers.md` has the setup and every command.

## When

- Before calling a change done: check the files the change touched.
- To find where a symbol is defined or used: ask a server, and do not rely on a text search alone.

## Check a change

Run the servers on the files the change touched, not on the whole project:

```text
python scripts/lsp-diagnostics.py --all --changed           # everything the change touched: staged, not staged, new
python scripts/lsp-diagnostics.py --all --commit <sha>      # a change that is already committed
```

`--changed` is the one to use for work in progress: `--staged` and `--worktree` each see only part of a change, and neither sees a new file that is not staged. If the run says no file was in the input, nothing was checked: name the files as paths.

Read the result as a check, not as advice:

- A diagnostic at or above a server's own threshold fails the check (exit 1). Each server's threshold is in the table, and some fail on warnings.
- A `FAILED` or `PARTIAL` server fails the check too: it did not finish, so it proved nothing.
- `SKIPPED` is not a pass. The server is not installed, or a file it requires is absent: say so in your report, with the install line it printed.
- Never edit a file only to silence a diagnostic: no suppression comment, no excluded path, no lowered threshold. Fix what it reports, or leave it and say why it stands.

## While you work

Codex reaches the same servers through the MCP bridge (`scripts/lsp-mcp-bridge.py`, registered once with `codex mcp add conclave-lsp -- python scripts/lsp-mcp-bridge.py`). Its tools are read-only: `diagnostics` for one file, and `definition`, `references` and `hover` for a place in a file (`file`, `line`, `column`, counted from 1). Use them while you work. This does not replace the check above, which runs every server of the table and has an exit code.
