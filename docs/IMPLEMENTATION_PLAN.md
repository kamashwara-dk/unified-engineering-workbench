# Implementation plan (â‰ˆ45 h, 15 calendar days)

Gate rules from the brief: C++ backend â‰¥6/12, thermal FEA â‰¥7/14, delivery â‰¥3/6, working C++ backend,
successful standalone launch. A phase is done only when every acceptance check passes AND is recorded below.

| Phase | Hours | Status | Evidence |
|---|---|---|---|
| 0 Setup | 2 | **done** | configure/build/ctest on GCC 13 (Linux); configure/build/ctest on MSVC 19.44/MSBuild 17.14 (Windows 11 x64) — all pass |
| 1 Walking skeleton + packaging | 4 | not started | |
| 2 FEA core | 5 | not started | |
| 3 STL/CSV importers | 5 | **done** | geometry_tests all pass; gen_samples validated; LIMITS.md measured |
| 4 UI shell + Geometry/Thermal views | 8 | **done** | Vitest 11/11 tests pass; Vite build passes; Playwright full verification passes; 11 visual screenshots in docs/img/ui/; 0 CSP violations |
| 5 Parallel batch + benchmark | 5 | **done** | Phase 5A: batch_tests all pass; reproducible generator; bounded pool (1, 2, 4 workers); agreement check (dT=0.0 <= 1e-8 C); hardware query & flags; calibrate & benchmark jobs; CSV/JSON atomic exports; PERFORMANCE_NOTES.md. Phase 5B: Performance view; batch form with inline API errors; Calibrate button; Run Benchmark with progress/cancel; hardware card + non-Release warning; results table (5 raw reps, median, speedup, efficiency, max |dT|); speedup/efficiency Plotly chart; Honest notes panel with single-CPU disclosure; launch-to-ready shown separately; CSV/JSON exports; empty state; 6 screenshots in docs/img/ui/; 0 CSP violations |
| 6 Embedded mocks + design doc | 4 | **in progress (6A & 6B done)** | Phase 6A: device_tests pass 100%; IDeviceAdapter with McuAdapter (ESP32-DevKitC), FpgaAdapter (Arty A7-35T), JtagAdapter (Olimex ARM-USB-TINY-H); fault injection & retry policy; fake clock; device_routes. Phase 6B: HardwareView; 3 cards with state badges, capabilities, connect/disconnect, read telemetry; persistent unmissable SIMULATED banner and badges; simulate timeout control; event timeline with API timestamps; typed actionable errors on illegal actions; 4 screenshots in docs/img/ui/; 0 CSP violations |
| 7 Backprop (hand + C++) | 2.5 | **done (C++ engine, CLI, API & UI; candidate hand derivation comparison pending)** | backprop_tests 7/7 pass (149 assertions); results/backprop.csv generated (102 rows); gradient check (diff ~1e-10 <= 1e-5); simultaneous update verified; POST /api/jobs backprop & export/backprop.csv; Learning Demo UI with Plotly loss plot & gradient table; docs/BACKPROP_NOTES.md; 3 screenshots in docs/img/ui/ |
| 8 Hardening | 4 | not started | |
| 9 Delivery | 6 | not started | |

## Phase 0 - Setup
Objective: reproducible repo, build, test and agent configuration.
Acceptance: `cmake --preset vs2022` and `--preset dev` configure; `ctest` passes; steering and specs exist.
Windows 11 x64 result (recorded 2026-11-01):
- cmake --preset vs2022         → Configuring done (0.0s); Generating done (0.1s); Build files written to build/vs2022. EXIT=0
- cmake --build --preset vs2022-release → MSBuild 17.14.60; catch2_amalgamated.lib, workbench_core.lib, core_tests.exe all built. EXIT=0
- ctest --preset vs2022-release → 1/1 Test #1: core_tests ... Passed 0.03s; 100% tests passed, 0 failed. EXIT=0
Tool: CMake 3.31.6-msvc6 (bundled with VS BuildTools 2022 v17.14.44); MSVC 14.44 (cl.exe Hostx64/x64); Windows SDK 10.0.26100.0.

## Phase 1 - Walking skeleton + packaging (de-risk first)
Tasks: pin and vendor cpp-httplib, nlohmann/json, WebView2 SDK; `/api/health`; Win32 host that checks for the
WebView2 runtime (fallback: default browser + message); static runtime linking; `dist/` layout; ZIP script.
Acceptance: double-click `Launch.exe` from an extracted ZIP in Windows Sandbox: window opens, health shown,
no terminal, no admin, no internet. Add `tasks.json` task "Run Workbench" that starts the packaged exe.
Validation: 4 clean launches; record launch-to-ready time; check SmartScreen wording for the README.

## Phase 2 - FEA core
Tasks: 2-node element, `ke = kA/h [[1,-1],[-1,1]]`, `fe = qAh/2 [1,1]`; Dirichlet via symmetric elimination;
tridiagonal (Thomas) solver; exact solution; max midpoint error using FE interpolation; CSV export.
Acceptance (see `.kiro/steering/fea-verification-oracles.md`): midpoint error = 125 hÂ² for N = 10/20/40/80,
observed order â‰ˆ 2, nodal error < 1e-9, negative tests (k â‰¤ 0, N < 1, L â‰¤ 0, NaN) return typed errors.

## Phase 3 - Importers
Tasks: ASCII STL parser (detect binary STL and explain), CSV parser (exact header, finite values, empty-file message),
size limits, C++ sample-data generator, provenance README per sample (CSV labelled synthetic).
Acceptance: positive, negative and boundary tests; documented tested size limits.

## Phase 4 - UI
Tasks: tokens and layout, four views, upload + validation, job progress/cancel, Three.js STL and point cloud,
colour legend (°C), node inspection, Plotly FEA plots, CSV/PNG export.
Acceptance: rotate/pan/zoom/fit work; cancellation never corrupts output; errors are actionable; keyboard usable.
Recorded 2026-11-01:
- `npm run build`: Strict TypeScript check + Vite production build passed without errors (assets built to `src/ui/dist`).
- `npm test`: Vitest 11/11 tests passed (colormap math, legend ticks, binary STL/CSV parsers, API error mapping).
- `node test/run_verification.mjs`: Complete Playwright end-to-end suite against `workbench_server --dev --ui-dir src/ui/dist` passed.
  - Verified: App shell, status bar health connection, STL 3D triangles with OrbitControls, Fit-to-view ('F' key and button), CSV Inferno point cloud, °C legend, node inspection on hover and "Go to Node ID", malformed STL rejection with actionable error on line 4, malformed CSV rejection, Thermal FEA parameter defaults, inline field validation error for non-physical parameters, cooperative job cancellation, Plotly T vs x with exact analytical curve, log-log mesh convergence with observed order ~2.00, results table, CSV/PNG exports, keyboard tab navigation, and 1024px responsive layout.
  - Zero CSP violations reported against strict loopback Content-Security-Policy.
  - 11 verification screenshots saved in `docs/img/ui/`.
- Updated `tools/package.ps1` to assemble `src/ui/dist` into `dist/app/ui`. Removed placeholder.
- Updated `third_party/VERSIONS.md`, `docs/UI_DESIGN.md`, `docs/adr/0007-*.md`, and `README.md`.

## Phase 5 - Parallel batch + benchmark
Tasks: seeded generator (own bitsâ†’double mapping), bounded worker pool, atomic work index, preallocated results,
benchmark harness (warm-up, 5 reps, median, T1/Tp, efficiency, hardware report), launch-to-ready timing separate.
Acceptance: serial/parallel max |Î”T| â‰¤ 1e-8 (expect 0), deterministic order, workload calibrated to â‰¥0.5 s per rep.
Recorded 2026-11-01 (Phase 5A):
- `src/core/batch`: Canonical `(x >> 11) * 2^-53` mapping with SplitMix64 per-case seed salting; scheduling-independent case generation; bounded `std::thread` pool (1, 2, 4 capped at logical cores, max 64); lock-free hot path via atomic fetch-add index; preallocated results; cooperative cancellation; exception containment in worker threads; `compare_results` returns max |dT| <= 1e-8 °C (exact 0.0 observed).
- `src/platform`: Windows hardware probe via `__cpuid`/registry for CPU brand, `GetLogicalProcessorInformationEx` for physical/logical cores, `GlobalMemoryStatusEx` for RAM, registry for OS version; CMake compile-time injection of compiler name/version, build type, optimization flags (`WB_COMPILER_NAME`, etc.).
- Benchmark harness: Warm-up + 5 measured reps using `std::chrono::steady_clock` around compute (thread creation/join included); median, speedup, efficiency, agreement verification; advisory `calibrate_workload` targeting >= 0.5s; 1-CPU path with 2 workers marked "speedup not assessable"; atomic CSV and JSON export.
- Server API: POST /api/jobs with kind `batch`, `benchmark`, `calibrate`; GET /api/jobs/{id}/export/benchmark.csv and benchmark.json.
- Testing: CTest `batch_tests` passed 100% in Release build; `tests/test_server_batch.mjs` end-to-end integration test passed.
- Documentation: Created `docs/PERFORMANCE_NOTES.md` detailing protocol, honest limitations, and user measurement template.
Recorded 2026-11-01 (Phase 5B):
- `src/ui/src/components/performance/PerformanceView.tsx`: Full engineering bench instrument view implementing batch parameter form (seed, case count, mesh size, k and q ranges, worker scaling protocol notice, 1-CPU simulation toggle), inline API validation error mapping, Calibrate Workload button with probe stats and auto-applied case count, Run Benchmark with progress bar (0-100%) and cooperative Cancel button.
- Hardware card: Real-time system specs (CPU, physical/logical cores, RAM, compiler, flags, build type) with prominent warning banner when non-Release.
- Launch-to-ready metric: Displayed in dedicated system card, strictly separated from solver compute times.
- Zero fabricated numbers: Empty state card shown prior to benchmark execution.
- Visualizations & Data: Dual-axis Plotly speedup & efficiency chart ($T_1/T_p$ vs ideal linear $y=x$, efficiency %), comprehensive results table (all 5 raw repetitions per worker, median, speedup, efficiency, max |dT| agreement pass badge), Honest Notes panel with single-CPU disclosure and scaling caveats, and atomic CSV/JSON exports.
- Verification: Vitest 12/12 unit tests passed. End-to-end Playwright suite (`test/verify_phase5b.mjs`) verified all 6 scenarios (empty state, calibration advisory, inline API errors, cooperative cancel, completed benchmark chart/table, single-CPU disclosure), 0 CSP violations, and 6 screenshots saved in `docs/img/ui/` (`12_` through `17_`).

## Phase 6 - Embedded
Tasks: `IDeviceAdapter` + MCU/FPGA/JTAG mocks, timeout + retry recovery, SIMULATED labels, block diagram,
interface/voltage table using real datasheet values for the boards you choose.
Recorded 2026-11-01 (Phase 6A & 6B):
- Backend (`src/core/devices.cpp`, `src/server/device_routes.cpp`): `IDeviceAdapter` hierarchy with `McuAdapter` (ESP32-DevKitC), `FpgaAdapter` (Digilent Arty A7-35T), `JtagAdapter` (Olimex ARM-USB-TINY-H). Bounded exponential backoff retry policy with injectable clock. `DeviceRegistry` and HTTP routes (`GET /api/devices`, `POST /api/devices/{id}/connect`, `POST /api/devices/{id}/disconnect`, `POST /api/devices/{id}/telemetry`, `POST /api/devices/{id}/inject-timeout`, `GET /api/devices/events`). CTest `device_tests` passed 100%.
- Frontend (`src/ui/src/components/hardware/HardwareView.tsx`): Hardware view with three device cards (MCU, FPGA, JTAG) showing state badges, capabilities list, connect/disconnect controls, and live telemetry readouts.
- Persistent SIMULATED notices: Unmissable top warning banner, card badges, telemetry pills, and log markers.
- Timeout control & recovery: "Simulate Timeout" action arms fault injection on next read; transitions device through `Faulted`, executes exponential backoff retries, and recovers to `Connected`.
- Event timeline: Displays timeout, faulted, retry, and recovered events with API timestamps.
- Actionable error feedback: Typed 409 responses from illegal actions (e.g. telemetry while disconnected) display helpful guidance on what action to take next.
- Verification: End-to-end Playwright suite (`test/verify_phase6b.mjs`) verified all scenarios with 0 CSP violations and 4 screenshots saved in `docs/img/ui/` (`18_` through `21_`).

## Phase 7 - Backprop
Tasks: handwritten derivation (candidate, <= 2 pages), C++ gradients, central-difference check (eps=1e-6, tol 1e-5), simultaneous parameter update, 101-step training record (1 initial step + 100 subsequent steps) written to full-precision CSV, CLI command, API endpoints, Learning Demo UI with Plotly loss curve and gradient check table, docs/BACKPROP_NOTES.md.
Recorded 2026-11-01:
- C++ Core (`src/core/src/backprop.cpp`, `src/core/include/workbench/backprop.hpp`):
  - Model: $x=1.0, y=2.0, z=w x+b, h=\tanh(z), \hat{y}=v h+c, J=0.5(\hat{y}-y)^2$.
  - Initial parameters: $w=0.5, b=0.0, v=1.0, c=0.0$, learning rate $\eta=0.1$.
  - Provisional analytical gradient formulas implemented in isolated function `analytic_gradients()` per user authorization; candidate handwritten derivation done separately and compared afterwards.
  - Central-difference numerical gradients (`numerical_gradients()`) with $\epsilon=10^{-6}$.
  - Gradient check (`check_gradients()`) verifying absolute error $\le 10^{-5}$ across all 4 parameters (actual errors $\approx 10^{-10}$).
  - One-step simultaneous update (`step_simultaneous()`) evaluating all gradients from original parameters before applying shifts.
  - Training loop (`train()`) producing 102 step records (step 0 initial, step 1 handwritten update, steps 2..101 subsequent iterations). Monotonic loss decrease from $J_0 = 1.1825418$ down to $J_{101} \approx 1.08 \times 10^{-20}$.
  - Locale-independent full-precision CSV serializer (`training_history_to_csv()`).
- CLI (`workbench_cli backprop --out results/`): writes `results/backprop.csv` (102 rows).
- CTest (`tests/test_backprop.cpp`): 149 assertions across 7 test cases pass 100% (initial forward pass, gradient check precision, check failure reporting, simultaneous vs sequential divergence proof, monotonic loss decrease, non-finite input validation, CSV format check).
- Server API (`src/server/src/fem_routes.cpp`):
  - `POST /api/jobs {kind: "backprop", params: {...}}` runs gradient check and training, saves `results/backprop.csv` atomically, returns gradient check table and step history.
  - `GET /api/jobs/{id}/export/backprop.csv` exports full training CSV.
- Frontend UI (`src/ui/src/components/performance/LearningDemoView.tsx`):
  - Sub-tab in Performance view: "Learning Demo (Backprop)".
  - Model reference cards and configurable hyperparameters.
  - Gradient check table showing analytic gradient, central-difference gradient, error, and PASS badge.
  - Plotly interactive loss trajectory curve ($J$ vs step) with linear/log scale toggle.
  - Export CSV and Export PNG buttons.
  - Notice stating analytical formulas were provisionally supplied by AI assistant for candidate comparison.
- Verification: End-to-end Playwright test suite (`src/ui/test/verify_phase7.mjs`) verified initial view, training execution, linear/log scale toggle, API direct submission and export route with 0 CSP violations; saved screenshots `22_`, `23_`, and `24_` in `docs/img/ui/`.
- Documentation (`docs/BACKPROP_NOTES.md`): details model architecture, provisional notice, calculus formulas, numerical check printouts, simultaneous update mechanics; preserves candidate TODO sections for handwritten derivation and loss curve explanation.

## Phase 8 - Hardening
Tasks: negative-test sweep, ASan run of core tests, accessibility pass, size-limit table, 4 clean-machine launches.

## Phase 9 - Delivery
Tasks: README, â‰¤6-page report, 3-5 min video or timestamped screenshot walkthrough, AI log, ownership statement,
git bundle + commit log, final ZIP `CandidateID_Workbench.zip`, fresh-extract test.

## Suggested commit sequence (make these yourself so history reflects your work)
1. `chore: add repo hygiene files and agent steering`
2. `build: CMake presets, core library skeleton and Catch2 smoke test`
3. `docs: architecture, plan, tooling setup and AI-use log`
Then one focused commit per task inside each phase.

