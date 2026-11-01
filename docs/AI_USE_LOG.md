# AI-Use Log

> **MANDATORY ASSESSMENT REQUIREMENT (Requirement Z5):**  
> Every entry in this log must represent genuine interaction. Never fabricate log entries, prompts, or outcomes.  
> **Candidate Action Required:** At least one incorrect or unsuitable AI suggestion must be recorded, or a deliberate challenge test and its outcome. Candidate review placeholders (`_TODO (candidate)_`) must be confirmed and filled by the candidate.

---

## Log Index

| # | Date | Tool / Model | Phase | Status |
|:---:|:---:|:---|:---:|:---|
| **001** | 2026-11-01 | Claude (Anthropic chat) | 0 | Recorded (Candidate edits pending) |
| **002** | 2026-11-01 | Kiro (automated agent) | 0 | Recorded (Candidate edits pending) |
| **003** | 2026-11-01 | Kiro (automated agent) | 1–3 | Recorded (Candidate edits pending) |
| **004** | 2026-11-01 | Antigravity agent | 4 | Recorded & Verified |
| **005** | 2026-11-01 | Antigravity agent | 5 | Recorded & Verified |
| **006** | 2026-11-01 | Antigravity agent | 6 | Recorded & Verified |
| **007** | 2026-11-01 | Antigravity agent | 7 | Recorded & Verified |
| **008** | 2026-11-01 | Antigravity agent | 8 | Recorded & Verified |
| **009** | 2026-11-01 | Antigravity agent | 9 | Recorded (Candidate sign-off pending) |

---

## 001 - Planning and Phase 0 Scaffold (2026-11-01)
- **Tool:** Claude in the Anthropic chat app (model name as shown in the app).
- **Prompt (representative):** "Analyse the attached assignment, recommend tools and open-source skills with install instructions, produce a phase-by-phase plan; do not write application code until the plan is reviewed."
- **Output used:** Architecture proposal, tool list, phase plan, FEA test oracles, and the Phase 0 scaffold (CMake, vendored Catch2, steering files, spec drafts, docs templates).
- **My edits:** _TODO (candidate): list what you changed or rejected._
- **Validation done so far:** Configure + build + smoke test on Linux/GCC 13 passed. FEA oracle values (midpoint error = $125 h^2$) were reproduced with a throwaway script. Tool facts were checked against web pages on 2026-11-01. MSVC build: `cmake --preset vs2022`, `cmake --build --preset vs2022-release`, and `ctest --preset vs2022-release` all passed on Windows 11 x64 (MSVC 14.44, MSBuild 17.14, CMake 3.31.6).
- **Incorrect / unsuitable suggestions found:** _TODO (candidate): record candidate verification or challenge test._
- **Challenge test (if needed):** _TODO (candidate)_

---

## 002 - Phase 0 Windows MSVC Verification (2026-11-01)
- **Tool:** Kiro automated agent.
- **Prompt (representative):** "Run configure/build/ctest on Windows 11 with MSVC (--preset vs2022) and record the result."
- **Output used:** Located `cmake.exe` and `ctest.exe` in VS BuildTools 2022, ran all three CMake commands via a batch script, read the output log, and updated `IMPLEMENTATION_PLAN.md`.
- **Commands run (exact):** `cmake --preset vs2022`; `cmake --build --preset vs2022-release`; `ctest --preset vs2022-release`.
- **Result:** Configure EXIT=0; build EXIT=0 (`catch2_amalgamated.lib`, `workbench_core.lib`, `core_tests.exe`); ctest EXIT=0 (1/1 passed, 0.03s).
- **My edits:** _TODO (candidate): record any corrections you made._
- **Validation done:** MSVC build verified - unit-tested (1/1 `core_tests` pass).
- **Incorrect / unsuitable suggestions found:** _None in this session._

---

## 003 - Phases 1-3 Implementation (2026-11-01)
- **Tool:** Kiro automated agent.
- **Phases covered:** Phase 1 (server skeleton, Win32 host, packaging), Phase 2 (FEM core, job manager), Phase 3 (STL/CSV parsers, sample generator, limits).
- **Commands run:** `cmake` configure/build/ctest (all targets), `gen_samples.exe`, `time_parse.exe` on stress files.
- **Evidence:** BUILD_EXIT=0; 5/5 test suites pass (192 assertions); real parse timing in `docs/LIMITS.md`; sample files validated by `gen_samples`.
- **What was generated:** `server.cpp`, `server.hpp`, `token.cpp/hpp`, `job_manager.cpp/hpp`, `atomic_write.cpp/hpp`, `fem_routes.cpp/hpp`, `file_routes.cpp/hpp`, `file_store.cpp/hpp`, `fem1d.cpp/hpp`, `geometry_io.cpp/hpp`, `status.hpp`, tests (`test_server`, `test_fem1d`, `test_jobs`, `test_geometry_io`), `gen_samples.cpp`, `time_parse.cpp`, `workbench_cli.cpp`, `src/host/main.cpp`, `Launch.manifest`, docs (`API.md`, `FEA_NOTES.md`, `LIMITS.md`, `MANUAL_TESTS.md`, `EDITOR_RUN.md`, 3 ADRs), tools (`package.ps1`, `verify_package.ps1`).
- **My edits:** _TODO (candidate): list what you changed or rejected._
- **Validation done:** MSVC unit-tested (5 test suites, 192 assertions).
- **Incorrect / unsuitable suggestions found:** _TODO (candidate)_

---

## 004 - Phase 4 UI Implementation & Verification (2026-11-01)
- **Tool:** Antigravity agent.
- **Phases covered:** Phase 4 (App shell, design system tokens, Geometry 3D view with OrbitControls, Inferno point cloud, Thermal FEA view with Plotly convergence charts, Vitest and Playwright test suites).
- **Commands run:** `npm run build`, `npm test`, `node test/run_verification.mjs`.
- **Evidence:** Vitest 11/11 tests pass; strict TypeScript build passes; Playwright end-to-end passes with 11 screenshots saved to `docs/img/ui/`; 0 CSP violations against strict loopback policy.
- **Incorrect / unsuitable suggestions found:** Httplib wildcard static file routing registered before API routes caused 404 responses on API endpoints; corrected by registering all API routes before static file handlers in `server.cpp`.

---

## 005 - Phase 5 Batch Solver, Benchmark Harness & Performance View (2026-11-01)
- **Tool:** Antigravity agent.
- **Phases covered:** Phase 5A (reproducible SplitMix64 generator, bits-to-double mapping, bounded pool, exact agreement check $\le 10^{-8}\,^\circ\text{C}$, hardware detection, benchmark harness, calibration helper, steady_clock timing, CSV/JSON atomic exports), Phase 5B (PerformanceView, batch form with inline validation errors from API, Calibrate button, Run Benchmark with progress/cancel, hardware card, non-Release warning, results table with all 5 raw reps, speedup/efficiency chart, Honest Notes panel with single-CPU disclosure, launch-to-ready shown separately, export CSV/JSON).
- **Commands run:** CTest `batch_tests`, `node tests/test_server_batch.mjs`, Vitest 12/12, `npm run build`, `node test/verify_phase5b.mjs`.
- **Evidence:** CTest 5/5 test suites pass (100%); Vitest 12/12 pass; Playwright verified all 6 scenarios with screenshots `12_` through `17_` in `docs/img/ui/`; 0 CSP violations.
- **Incorrect / unsuitable suggestions found:** In JSX string interpolation within `PerformanceView.tsx`, the expression `$O(N)$ with $N \approx 10{-}80$` caused a syntax error because `{-}` was parsed as an incomplete JSX subtraction expression. Corrected to clean text `[10, 80]`.

---

## 006 - Phase 6 Embedded Mocks & Hardware View (2026-11-01)
- **Tool:** Antigravity agent.
- **Phases covered:** Phase 6A (C++ `IDeviceAdapter` hierarchy, `McuAdapter`, `FpgaAdapter`, `JtagAdapter`, exponential backoff retry policy, fault injection, fake clock, device HTTP routes), Phase 6B (Hardware view, 3 device cards with state badges and capabilities, connect/disconnect controls, telemetry polling, persistent SIMULATED banners and badges, timeout simulation control, recovery event timeline with API timestamps, typed actionable error handling on illegal actions).
- **Commands run:** CTest `device_tests`, `npm test`, `npm run build`, `node test/verify_phase6b.mjs`.
- **Evidence:** CTest 6/6 test suites pass (100%); Vitest 12/12 pass; Playwright verified all scenarios with screenshots `18_` through `21_` in `docs/img/ui/`; 0 CSP violations.
- **Incorrect / unsuitable suggestions found:** Windows SDK header `<windows.h>` defines `#define DeviceCapabilities DeviceCapabilitiesA`. When included via `httplib.h` in `device_routes.cpp`, it caused an unresolved external symbol error `DeviceCapabilitiesA` during linking. Resolved by adding `#undef DeviceCapabilities` after Windows header inclusion.

---

## 007 - Phase 7 Backpropagation & Learning Demo (2026-11-01)
- **Tool:** Antigravity agent.
- **Phases covered:** Phase 7 (C++ backprop core without autodiff, forward pass, provisional analytical gradients, central-difference numerical gradients with $\epsilon = 10^{-6}$, gradient check with tolerance $\le 10^{-5}$, simultaneous update step, 101-step training loop, full-precision locale-independent CSV generation, CLI command `workbench_cli backprop --out results/`, HTTP routes `POST /api/jobs {kind: "backprop"}` and `GET /api/jobs/:id/export/backprop.csv`, Performance view "Learning demo" sub-tab with Plotly loss plot and gradient check table, `docs/BACKPROP_NOTES.md`).
- **Commands run:** `backprop_tests.exe`, `workbench_cli.exe backprop --out results`, `npm test`, `npm run build`, `node test/verify_phase7.mjs`.
- **Evidence:** `backprop_tests.exe` (149 assertions in 7 test cases pass 100%); CLI generated `results/backprop.csv` (102 rows); Playwright verified all scenarios with screenshots `22_`, `23_`, and `24_` in `docs/img/ui/`; 0 CSP violations.
- **Incorrect / unsuitable suggestions found:** In Plotly TypeScript definitions, passing array values for marker sizes (`steps.map(...)`) and colors produced TS2322 type errors because `Plotly.ScatterMarker.size` was typed as a scalar `number`. Resolved by casting via `as unknown as number` and `as unknown as string` to satisfy strict typing while maintaining per-point styling at runtime.

---

## 008 - Phase 8 Hardening, Sanitizers & Accessibility (2026-11-01)
- **Tool:** Antigravity agent.
- **Phases covered:** Phase 8 (Traceability matrix audit against all 39 requirements, negative & fuzzing tests in C++, AddressSanitizer build and execution on MSVC, code readability & units review, single-instance mutex implementation in `main.cpp`, Axe-Core automated WCAG AA accessibility audit with contrast fixes, package verification enhancements in `tools/verify_package.ps1`, real technical boundaries in `docs/KNOWN_LIMITATIONS.md`).
- **Commands run:** `cmake` ASan build (`/fsanitize=address /EHsc /Zi`), CTest ASan execution (91 cases, 1,068 assertions), CTest Release execution (7/7 passed), `node test/audit_accessibility_and_ui.mjs`, `powershell .\tools\verify_package.ps1`.
- **Evidence:** MSVC ASan zero-error clean run; Axe-core 0 accessibility violations across all views; package verification passed (260 files, 140.9 MB dist).
- **Incorrect / unsuitable suggestions found:** Initial design tokens used dark cyan (`#0284c7`) as `--accent-default` in dark mode, which had a contrast ratio of only 4.23:1 against `#18191e` panels, failing WCAG AA (requires 4.5:1). Additionally, `<input>` tags lacked explicit background color token mapping in `tailwind.config.js`. Corrected by setting `--accent-default: #38bdf8` in `:root` (yielding $>8:1$ contrast) and adding `input: 'var(--bg-input)'`.

---

## 009 - Phase 9 Final Submission Package & Documentation (2026-11-01)
- **Tool:** Antigravity agent.
- **Phases covered:** Phase 9 (Final `README.md` with one-click steps, architecture diagram, schemas, and limitations; 6-page compact technical report `docs/REPORT.md`; 3–5 minute demo video script and fallback screenshot walkthrough `docs/DEMO_SCRIPT.md`; statement of ownership and attribution `docs/OWNERSHIP.md`; consolidated AI use log `docs/AI_USE_LOG.md`; git evidence generation script `tools/make_git_evidence.ps1`; submission checklist `docs/SUBMISSION_CHECKLIST.md`; final ZIP packaging and self-assessment rubric).
- **Commands run:** `workbench_cli fem --out results`, `test_server_batch.mjs`, `powershell .\tools\package.ps1`, `powershell .\tools\verify_package.ps1`.
- **Evidence:** Real convergence numbers recorded in `REPORT.md`; real hardware and scaling benchmarks measured and recorded in `REPORT.md` and `PERFORMANCE_NOTES.md`; `tools/package.ps1` and `tools/verify_package.ps1` execute cleanly with 0 absolute paths.
- **My edits:** _TODO (candidate): review and confirm candidate-owned sections (candidate ID, effort hours, handwritten derivation review, and debugging insights)._
