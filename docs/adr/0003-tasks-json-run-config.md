# ADR 0003: `tasks.json` as the editor run configuration

**Status:** Accepted (Antigravity compatibility unverified)  
**Date:** 2026-11-01

## Context
The assignment requires a one-click run configuration. The two candidate editors are VS Code
and Antigravity IDE. Both support `.vscode/tasks.json` in their documented feature sets.

## Decision
Use `.vscode/tasks.json` with two tasks:
- **Package (Release):** runs `tools/package.ps1` to build and assemble `dist/`.
- **Run Workbench:** depends on Package; launches `dist/Launch.exe` directly.

No debugger extension is required. The launcher needs neither editor nor internet at run time.

## Consequences
- **Positive:** Standard format understood by both VS Code and Antigravity.
- **Positive:** No custom extension required.
- **Negative:** Packaging step is slow (~30 s) on first run; subsequent runs reuse incremental build.
- **Risk:** Antigravity extension compatibility with `tasks.json` has **NOT** been verified. Manual
  verification is required; see `docs/MANUAL_TESTS.md`.