# Tooling setup (Antigravity + Kiro)

Verified from official or primary pages on 2026-11-01 unless marked **unverified**.

## IDEs
| Tool | Get it | Notes |
|---|---|---|
| Google Antigravity | https://antigravity.google/download | Several downloads exist (desktop client, standalone IDE, CLI). Use the standalone IDE for the VS Code-style workspace. Confirm `tasks.json` runs there (**unverified**). |
| Kiro | https://kiro.dev (docs: https://kiro.dev/docs/getting-started/installation) | Windows supported. Specs = requirements/design/tasks. Steering in `.kiro/steering/`. |

## Toolchain (Windows 11 x64, PowerShell; run `winget search <name>` if an ID differs)
```
winget install Microsoft.VisualStudio.2022.BuildTools --override "--wait --passive --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
winget install Kitware.CMake
winget install Ninja-build.Ninja
winget install OpenJS.NodeJS.LTS
winget install Git.Git
```
Check: `cmake --version` (>= 3.24), `node -v` (>= 18), `git --version`. Open a new terminal after installs.

## MCP servers (only these two)
Node.js >= 18 is required by both.

Context7 (docs lookup): `npx -y @upstash/context7-mcp@latest` , repo https://github.com/upstash/context7
Playwright MCP (UI checks against the dev server): `npx @playwright/mcp@latest`, repo https://github.com/microsoft/playwright-mcp

Shared JSON shape (see `docs/mcp.config.example.json`):
- Antigravity: agent panel "..." menu, MCP Servers, View raw config, edit `mcp_config.json`.
- Kiro: Kiro panel, MCP; workspace file `.kiro/settings/mcp.json` (**unverified path**, confirm at https://kiro.dev/docs/features/mcp-servers/).
- Windows clients sometimes need `"command": "cmd", "args": ["/c", "npx", ...]`.

## Skills
- Antigravity: workspace skills in `.agent/skills/` (the CLI reportedly uses `.agents/skills/`; **verify** which your build reads).
- Install `frontend-design` from https://github.com/anthropics/skills (check its licence in the repo first).
- Kiro: paste the skill's guidance into `.kiro/steering/` (direct SKILL.md support **unverified**).
- Do not install bulk skill marketplaces. Project-specific steering files already live in `.kiro/steering/`.

## Division of labour
Kiro: specs, steering, C++ core. Antigravity: frontend and browser verification.
Never let both agents edit the same files at the same time.
