# Agent brief (shared by Kiro and Antigravity)

Project: Unified Engineering Workbench. Source of truth: the assignment PDF, summarised in
`.kiro/specs/workbench/requirements.md`. Read `.kiro/steering/*.md` before changing code.

Hard rules
1. All backend logic, numerics, file parsing/validation and concurrency are C++17+ in `src/core`
   (and transport in `src/server`). The UI only renders and collects input.
2. `src/core` must not include UI, HTTP, Win32 or filesystem-path code.
3. Work one phase at a time (`docs/IMPLEMENTATION_PLAN.md`). Do not start a phase until the
   previous phase's acceptance checks pass and are recorded.
4. Never claim a feature works unless a test or a logged manual check shows it.
5. Add no dependency without an entry in `third_party/VERSIONS.md` (version, licence, reason).
6. Use relative paths only. No credentials, no personal data, no machine-specific paths.
7. Append to `docs/AI_USE_LOG.md` after each phase. Never fabricate log entries.
8. Simulated hardware behaviour must be labelled SIMULATED everywhere it is shown.
