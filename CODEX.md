# CODEX.md

Follow [AGENTS.md](./AGENTS.md). This file holds only what is specific to OpenAI Codex (`codex`).

- Skills live in `.codex/skills/<name>/` (with `agents/openai.yaml` metadata) and are mirrored as real files in `.agents/skills/<name>/` and `.claude/skills/<name>/`. Change all three together.
- Use `.codex/skills/local-tool-safety` before local tool, Windows/WSL, connector, mail, cloud, SQL, or outbound work.
- Rules live in `.codex/rules/`.
- Subagents: Codex has no project subagent definitions (Claude uses `.claude/agents/*.md` and Antigravity uses native `Orchestrator (Carbon)` with built-in subagents); run mechanical subtasks in the main session.

## Role

Claude Code and Antigravity are the writers. Codex is a read-only reviewer: inspect, test and report findings, but do not implement changes or launch writer campaigns unless the owner explicitly authorizes a role change.
