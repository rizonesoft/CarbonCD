# Starts CarbonCD's unattended run: an interactive agent session in this checkout, in this window, with every
# permission prompt off and its goal set to finishing todo/implementation-plan.md. The run starts only when the
# operator starts it, and ends when its window closes, it crashes or the machine restarts: nothing restarts it.
#
#   pwsh scripts/start-run.ps1                    check the machine, then start the run in Claude Code (the latest Opus)
#   pwsh scripts/start-run.ps1 -Runner agy        start it in Antigravity on Orchestrator (Carbon)
#   pwsh scripts/start-run.ps1 -Check             run only the preflight: one PASS, WARN or FAIL line each; exit 1 on a FAIL
#   pwsh scripts/start-run.ps1 -InstallShortcut   put "CarbonCD Claude campaign" and "CarbonCD Carbon campaign" on the desktop
#   pwsh scripts/start-run.ps1 -DryRun            print what would run or be written, and change nothing
#   pwsh scripts/start-run.ps1 -Model sonnet      start on another model
#   pwsh scripts/start-run.ps1 -Effort high       start the Claude runner at another effort (default medium)
#   pwsh scripts/start-run.ps1 -TakeOver          take over a held run guard without being asked
#
# The run is Windows-native because the build is: MSVC and MFC do not run in WSL, so the Conclave unattended kit
# (tmux, Linux and WSL) is not used here. This file follows the Conclave-SDK's own scripts/start-run.ps1.
#
# The goal is a Claude Code /goal, checked after every turn: the plan finished through the process-plan skill. That
# skill starts the run guard itself (scripts/campaign_guard.py and a heartbeat inside the session), so this file
# starts no guard. Two sessions must never write to one tree: when a guard from an earlier session is held, this file
# names that session and waits for Enter (or takes -TakeOver), and only then may the new session take the run over.
#
# The session signs in with the long-lived token in the user variable CLAUDE_CODE_OAUTH_TOKEN when one is stored, and
# with the claude.ai sign-in otherwise. The token is never printed, and this file never creates or changes one.
param(
  [ValidateSet('claude', 'agy')]
  [string]$Runner = 'claude',
  [switch]$Check,
  [switch]$InstallShortcut,
  [switch]$DryRun,
  [switch]$TakeOver,
  [string]$ShortcutDir = [Environment]::GetFolderPath('Desktop'),
  [string]$Model = '',
  [ValidateSet('low', 'medium', 'high', 'xhigh', 'max')]
  [string]$Effort = 'medium',
  [string]$IconDir = (Join-Path $env:LOCALAPPDATA 'CarbonCD'),
  [string]$UserEnvKey = 'HKCU:\Environment',
  [string]$Goal = ''
)
$ErrorActionPreference = 'Stop'
# The automations kit marks every process a scheduled task runs: a run starts only when its operator starts it.
if ($env:CONCLAVE_AUTOMATION -and -not $InstallShortcut -and -not $Check) {
  "start refused: it came from the scheduled task $env:CONCLAVE_AUTOMATION, and a run starts only when its operator starts it"
  exit 2
}
$repo = Split-Path $PSScriptRoot
$tokenName = 'CLAUDE_CODE_OAUTH_TOKEN'
if (-not $Model) { $Model = if ($Runner -eq 'agy') { 'gemini-pro-orchestrator' } else { 'opus' } }
$GuardFile = Join-Path $repo 'build\claude-campaign-guard.json'

$shortcuts = @(
  @{ Runner = 'claude'; Name = 'CarbonCD Claude campaign'; Icon = 'carboncd-campaign-claude.ico'
     Description = 'Start the CarbonCD run in Claude Code (the latest Opus)' },
  @{ Runner = 'agy'; Name = 'CarbonCD Carbon campaign'; Icon = 'carboncd-campaign-carbon.ico'
     Description = 'Start the CarbonCD run in Google Antigravity on Orchestrator (Carbon)' }
)
$title = ($shortcuts | Where-Object { $_.Runner -eq $Runner }).Name

function Find-Tool([string]$name) {
  (Get-Command $name -CommandType Application -ErrorAction Ignore | Select-Object -First 1).Source
}

# The preflight: what a run needs before it can ship a section. FAIL refuses the start; WARN says what will park.
function Test-Run {
  $fails = 0
  function Line([string]$state, [string]$name, [string]$detail) { '{0,-4} {1,-18} {2}' -f $state, $name, $detail }
  function Quiet([scriptblock]$command) { $out = & $command 2>&1 | Out-String; @{ Ok = ($LASTEXITCODE -eq 0); Text = $out.Trim() } }
  Push-Location -LiteralPath $repo
  try {
    foreach ($tool in 'git', 'python', 'cmake', 'ninja', 'rg', 'gh', 'bosun') {
      $found = Find-Tool $tool
      if ($found) { Line 'PASS' "tool:$tool" $found } else { $fails++; Line 'FAIL' "tool:$tool" 'not on PATH' }
    }
    $agent = if ($Runner -eq 'agy') { 'agy' } else { 'claude' }
    $found = (Get-Command $agent -ErrorAction Ignore | Select-Object -First 1).Source
    if ($found) { Line 'PASS' "agent:$agent" $found } else { $fails++; Line 'FAIL' "agent:$agent" 'not on PATH' }
    $found = (Get-Command codex -ErrorAction Ignore | Select-Object -First 1).Source
    if ($found) { Line 'PASS' 'reviewer:codex' $found } else { $fails++; Line 'FAIL' 'reviewer:codex' 'not on PATH: no section can be reviewed or stamped' }

    $r = Quiet { python scripts/review-dispatch.py --check-pin }
    if ($r.Ok) { Line 'PASS' 'reviewer-pin' '.conclave/reviewers.toml loads' } else { $fails++; Line 'FAIL' 'reviewer-pin' $r.Text }
    $r = Quiet { python scripts/todo-graph.py validate }
    if ($r.Ok) { Line 'PASS' 'plan-valid' $r.Text } else { $fails++; Line 'FAIL' 'plan-valid' $r.Text }
    $r = Quiet { python scripts/todo-graph.py plan --check }
    if ($r.Ok) { Line 'PASS' 'plan-current' 'todo/implementation-plan.md matches the TODO files' } else { $fails++; Line 'FAIL' 'plan-current' 'run python scripts/todo-graph.py plan --sync' }
    $r = Quiet { python scripts/campaign_guard.py --self-test }
    if ($r.Ok) { Line 'PASS' 'run-guard' 'the Stop hooks answer' } else { $fails++; Line 'FAIL' 'run-guard' ($r.Text -split "`n")[-1] }
    $r = Quiet { python design/build.py --no-raster }
    $dirty = @(git status --porcelain -- design src/res)
    if ($r.Ok -and -not $dirty) { Line 'PASS' 'design-assets' 'design/build.py reproduces the committed masters' }
    else { $fails++; Line 'FAIL' 'design-assets' 'design/build.py changes tracked files, or failed: commit or fix the design first' }

    $branch = (git rev-parse --abbrev-ref HEAD).Trim()
    if ($branch -eq 'master') { Line 'PASS' 'branch' 'master' } else { $fails++; Line 'FAIL' 'branch' "on $branch; the run commits on master" }
    $changes = @(git status --porcelain)
    if (-not $changes) { Line 'PASS' 'clean-tree' 'nothing uncommitted' } else { $fails++; Line 'FAIL' 'clean-tree' "$($changes.Count) uncommitted path(s): commit or remove them first" }
    $hooks = (git config core.hooksPath)
    if ($hooks -eq 'scripts/hooks') { Line 'PASS' 'pre-commit-hook' 'core.hooksPath is scripts/hooks' } else { $fails++; Line 'FAIL' 'pre-commit-hook' 'run: git config core.hooksPath scripts/hooks' }
    $r = Quiet { gh auth status }
    if ($r.Ok) { Line 'PASS' 'github-sign-in' 'gh is signed in (the run pushes to origin and reads CI)' } else { Line 'WARN' 'github-sign-in' 'gh is not signed in: pushes and the CI sections will fail' }

    $stats = (python scripts/todo-graph.py query stats | ConvertFrom-Json)
    if ($stats.ready -gt 0) { Line 'PASS' 'ready-sections' "$($stats.ready) ready, $($stats.verified) of $($stats.total) verified" }
    elseif ($stats.verified -eq $stats.total) { Line 'WARN' 'ready-sections' 'the plan is finished' }
    else { $fails++; Line 'FAIL' 'ready-sections' 'no section is ready: every open row waits on a Need or a dependency' }

    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    $vs = if (Test-Path -LiteralPath $vswhere) { & $vswhere -latest -prerelease -products * -property installationPath } else { $null }
    if (-not $vs) { $fails++; Line 'FAIL' 'compiler' 'no Visual Studio with the C++ tools was found' }
    else {
      Line 'PASS' 'compiler' $vs
      $mfc = @(Get-ChildItem -Path (Join-Path $vs 'VC\Tools\MSVC\*\atlmfc\lib\x64\nafxcw.lib') -ErrorAction Ignore)
      if ($mfc) { Line 'PASS' 'mfc' $mfc[0].FullName }
      else { Line 'WARN' 'mfc' 'the MFC component is missing: the core library, the engine and the tests can be built, and every section that builds the app parks (D00 T01 section 1 has the install command)' }
    }
    $free = [math]::Floor((Get-PSDrive -Name ($repo.Substring(0, 1))).Free / 1GB)
    if ($free -ge 20) { Line 'PASS' 'disk' "$free GB free" } else { $fails++; Line 'FAIL' 'disk' "$free GB free; a run needs 20" }
  } finally { Pop-Location }
  $fails
}

if ($Check) {
  $result = @(Test-Run)
  $result[0..($result.Count - 2)]
  if ($result[-1] -gt 0) { "preflight: $($result[-1]) FAIL"; exit 1 }
  'preflight: ready'
  exit 0
}

if ($InstallShortcut) {
  $pwsh = (Get-Process -Id $PID).Path
  $brand = Join-Path $repo 'src\res\cdm.ico'
  $renderer = Join-Path $repo 'scripts\campaign-icon.py'
  if ($DryRun) {
    foreach ($s in $shortcuts) {
      "would write: $(Join-Path $ShortcutDir "$($s.Name).lnk")"
      "  target: $pwsh -NoLogo -NoExit -NoProfile -File `"$PSCommandPath`" -Runner $($s.Runner)"
      "  icon: $(Join-Path $IconDir $s.Icon)"
    }
    exit 0
  }
  $python = Find-Tool 'python'
  if (-not $python) { 'python is not on PATH: install Python 3, then write the shortcuts again'; exit 1 }
  New-Item -ItemType Directory -Force $IconDir | Out-Null
  foreach ($s in $shortcuts) {
    $icon = Join-Path $IconDir $s.Icon
    & $python $renderer badge $brand $s.Runner $icon
    if ($LASTEXITCODE -ne 0) { "the icon for $($s.Name) could not be rendered"; exit 1 }
    $link = Join-Path $ShortcutDir "$($s.Name).lnk"
    $shortcut = (New-Object -ComObject WScript.Shell).CreateShortcut($link)
    $shortcut.TargetPath = $pwsh
    $shortcut.Arguments = "-NoLogo -NoExit -NoProfile -File `"$PSCommandPath`" -Runner $($s.Runner)"
    $shortcut.WorkingDirectory = $repo
    $shortcut.Description = $s.Description
    $shortcut.IconLocation = "$icon,0"
    $shortcut.Save()
    "shortcut written: $link"
  }
  exit 0
}

if (-not $Goal) {
  $Goal = (@'
Finish the CarbonCD 2 implementation plan (todo/implementation-plan.md) 100% complete and aim for excellence: a 64-bit Unicode disc imaging tool for CDs and DVDs that works end to end and is a pleasure to use. Act autonomously under the Operating directive in AGENTS.md: decide and act instead of asking, and heal what breaks at its owning layer. Work it with the process-plan skill, as the prompt `process plan` does, without stopping between sections or phases. Every interface section is built to design/DESIGN.md and design/SPEC.md and matches its reference render in design/mockups 1:1; change the design first when it must change, never the code alone. Never ask the operator a question: take the industry-standard option and record the decision, and what reversing it would cost, in the run file. Never touch a real optical drive: every disc task is proven on the simulated drive, and real-drive checks are phase 99. The goal is met only when `python scripts/todo-graph.py query stats` shows verified equal to total, or every row still open is in phase 99 or waits on a Need this session cannot meet and the run file carries its PARKED line and park record.
'@).Trim()
}

# Who holds the run, read before anything starts. The guard's own field is session_id (scripts/campaign_guard.py).
$held = $null
if (Test-Path -LiteralPath $GuardFile) {
  $held = try { [string](Get-Content -LiteralPath $GuardFile -Raw | ConvertFrom-Json).session_id } catch { '' }
  if ($held -notmatch '^[0-9A-Za-z-]{8,64}$') { $held = '' }
}
if ($null -eq $held) {
  $Goal += ' No run guard was held when this session was launched: if process-plan finds a guard owned by another session, another run is live, so start nothing, never use --handover, and end the turn.'
} elseif ($held) {
  $Goal += " A run guard held by session $held was acknowledged by the operator at launch: take the run over from that session only, with ``acquire --handover resumed-from-launcher``. If the guard names any other session, another run is live: start nothing and end the turn."
} else {
  $Goal += ' A run guard file was present at launch but named no session: do not take it over. Report what the guard file holds and end the turn.'
}
# One line with no double quote: it is passed to the agent as a single argument. Claude Code allows 4,000 characters.
if ($Goal.Contains('"') -or $Goal.Contains("`n")) { 'the goal must be one line with no double quote'; exit 1 }
if ($Goal.Length -gt 4000) { "the goal is $($Goal.Length) characters; Claude Code allows 4,000"; exit 1 }

if ($Runner -eq 'agy') {
  $agent = 'agy'
  $arguments = @("--model=$Model", '--dangerously-skip-permissions', "--prompt-interactive=$Goal")
} else {
  $agent = 'claude'
  $session = [guid]::NewGuid().ToString()
  $arguments = @('--session-id', $session, '--dangerously-skip-permissions', '--model', $Model, '--effort', $Effort, '--name', $title, "/goal $Goal")
}
if ($DryRun) {
  "in $repo"
  if ($null -ne $held) { "a run guard is held by session $(if ($held) { $held } else { '(the guard file names none)' }): a real start asks before taking it over" }
  "$agent $($arguments[0..($arguments.Count - 2)] -join ' ') `"$($arguments[-1])`""
  exit 0
}

$result = @(Test-Run)
$result[0..($result.Count - 2)]
if ($result[-1] -gt 0) { "start refused: $($result[-1]) preflight FAIL"; exit 1 }
$program = (Get-Command $agent -ErrorAction Ignore | Select-Object -First 1).Source

if ($null -ne $held) {
  "a run guard is held by session $(if ($held) { $held } else { '(the guard file names none)' })"
  'If that session is still working in another window, close this one now.'
  if (-not $TakeOver) { [void](Read-Host 'Press Enter to start a new session that takes the run over') }
}

# The sign-in (Claude Code's; Antigravity signs in itself). Each line says where it came from, never the token itself.
if ($agent -eq 'agy') {
  'sign-in: Antigravity uses its own Google sign-in'
} elseif ([Environment]::GetEnvironmentVariable($tokenName)) {
  'sign-in: the long-lived token this window already has'
} else {
  $token = if (Test-Path -LiteralPath $UserEnvKey) { (Get-Item -LiteralPath $UserEnvKey).GetValue($tokenName, $null) }
  if ($token) {
    [Environment]::SetEnvironmentVariable($tokenName, $token)
    $token = $null
    'sign-in: the long-lived token from your user variables'
  } else {
    'sign-in: no long-lived token is stored, so the session uses the claude.ai sign-in'
  }
}

try { $Host.UI.RawUI.WindowTitle = $title } catch { Write-Verbose "window title not set: $($_.Exception.Message)" }
Set-Location -LiteralPath $repo
& $program @arguments
