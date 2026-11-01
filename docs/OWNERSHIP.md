# Statement of Ownership & Attribution of Code

This document distinguishes original work developed for the **PASS Science Works R&D Internship Assessment** from third-party open-source components and libraries, and provides a formal confirmation block for the candidate.

---

## 1. Candidate Identification

- **Candidate ID:** `[MISSING: Candidate ID to confirm]`
- **Actual Total Effort Hours:** `[MISSING: Candidate actual effort hours to confirm]`
- **Assessment Track:** PASS Science Works Common Assessment Prototype

---

## 2. Original Work Authored for this Submission

All files listed below were designed, implemented, and verified specifically for this project:

### 2.1 C++ Algorithmic Core (`src/core/`)
- `fem1d.cpp`, `fem1d.hpp`: 1-D steady-state finite element thermal solver for $-d/dx(kA\,dT/dx) = qA$, two-node linear element stiffness and load formulation, symmetric elimination Dirichlet boundary enforcement, Thomas algorithm linear solver with near-zero pivot detection, exact closed-form solution evaluator, midpoint error calculations ($125 h^2$), and convergence rate computation.
- `geometry_io.cpp`, `geometry_io.hpp`: ASCII STL parser with line-numbered error diagnostics and binary detection, nodal CSV parser with strict header verification, bounding box accumulator, face normal recalculation, and little-endian binary serialisation.
- `batch.cpp`, `batch.hpp`: Reproducible case generator using `std::mt19937_64` and SplitMix64 seeding, canonical bit-to-double mapping $((x \gg 11) \times 2^{-53})$, bounded `std::thread` worker pool with atomic counter scheduling, and serial/parallel agreement verification ($|\Delta T| \le 10^{-8}\,^\circ\text{C}$).
- `devices.cpp`, `devices.hpp`: Polymorphic device adapter hierarchy (`IDeviceAdapter`, `McuAdapter`, `FpgaAdapter`, `JtagAdapter`), deterministic state machine, exponential backoff retry policy, fault injection, and mock injectable clock.
- `backprop.cpp`, `backprop.hpp`: Forward activation pass ($z = wx+b, h=\tanh(z), \hat{y}=vh+c, J=0.5(\hat{y}-y)^2$), provisional analytical gradients, central-difference numerical gradient check ($\epsilon = 10^{-6}$, absolute error $\le 10^{-5}$), simultaneous parameter update step, and 101-step training loop with full-precision CSV generation.
- `status.hpp`: Non-allocating status code and typed `Result<T>` monadic wrapper.

### 2.2 Platform Telemetry (`src/platform/`)
- `platform.cpp`, `platform.hpp`: Win32 and POSIX hardware discovery (CPU brand string, physical core count, logical processor count, total physical RAM, OS version string) and CMake compiler reflection.

### 2.3 Embedded Loopback Server (`src/server/`)
- `server.cpp`, `server.hpp`: Loopback HTTP server lifecycle on `127.0.0.1`, cryptographic token validation middleware, and Content Security Policy (CSP) headers.
- `fem_routes.cpp`, `file_routes.cpp`, `device_routes.cpp`: REST route handlers for thermal solving, batch/benchmark jobs, file parsing, and hardware control.
- `job_manager.cpp`, `job_manager.hpp`: Background worker thread pool with thread-safe job tracking and cooperative cancellation tokens.
- `file_store.cpp`, `file_store.hpp`: Thread-safe in-memory caching for uploaded geometry buffers.
- `atomic_write.cpp`, `atomic_write.hpp`: Crash-consistent file persistence via temporary file creation and atomic rename.
- `workbench_cli.cpp`: Standalone command-line interface for batch solves, convergence sweeps, and backpropagation training.

### 2.4 Native Windows Host (`src/host/`)
- `main.cpp`: Win32 desktop application entry point, named single-instance mutex (`Local\PASS_UnifiedWorkbench_SingleInstance`), Microsoft Edge WebView2 initialization, random session token generation, and default system browser fallback via `ShellExecuteW`.
- `Launch.manifest`: Visual styles and dpiAware manifest.

### 2.5 User Interface (`src/ui/src/`)
- `App.tsx`, `components/layout/TopBar.tsx`, `components/layout/StatusBar.tsx`: Application shell with `role="tablist"` keyboard navigation, persistent `aria-live="polite"` status tracking, and theme switching.
- `styles/tokens.css`, `tailwind.config.js`: Custom design system tokens (graphite neutrals, electric cyan accent, semantic status colors, tabular mono numerals, WCAG AA compliant contrast ratios).
- `components/geometry/`: `GeometryView.tsx`, `StlViewport.tsx` (Three.js 3D mesh rendering, OrbitControls, fit-to-view), `PointCloudViewport.tsx` (Inferno colormap point cloud, node hover tooltip, "Go to Node" targeting).
- `components/thermal/`: `ThermalView.tsx`, `ThermalResultsPlot.tsx` (Plotly FEA vs exact curve), `ThermalConvergencePlot.tsx` (log-log convergence plot).
- `components/performance/`: `PerformanceView.tsx` (batch form, calibration helper, hardware card, results table, speedup/efficiency plots, single-core honest disclosure banner), `LearningDemoView.tsx` (gradient check table, Plotly loss trajectory curve, step record table).
- `components/hardware/`: `HardwareView.tsx` (cards for MCU, FPGA, and JTAG with persistent `SIMULATED` badges, telemetry polling, timeout fault injection, and recovery timeline).
- `services/api.ts`: Typed client wrapper mapping API error responses with field-level and file-line diagnostics.

### 2.6 Automation, Tests & Packaging (`tests/`, `tools/`)
- `tests/`: 7 C++ Catch2 test suites (`core_tests`, `fem1d_tests`, `job_tests`, `geometry_tests`, `batch_tests`, `device_tests`, `backprop_tests`) including fixed-seed byte fuzzing and truncation tests; Vitest frontend test suites (`colormap.test.ts`, `api.test.ts`); Playwright E2E verification scripts (`verify_phase5b.mjs`, `verify_phase6b.mjs`, `verify_phase7.mjs`, `audit_accessibility_and_ui.mjs`).
- `tools/`: `package.ps1` (Release build & ZIP packager), `verify_package.ps1` (automated packaging audit), `make_git_evidence.ps1` (git bundle & log exporter).

---

## 3. Reused Code and Third-Party Dependencies

All third-party libraries and assets are permissibly licensed, pinned, and documented in [`third_party/VERSIONS.md`](file:///third_party/VERSIONS.md):

| Component | Version | License | Usage Scope | Reused As |
|:---|:---:|:---:|:---|:---|
| **Catch2** | v3.16.1 | BSL-1.0 | Unit and fuzz testing | Vendored single-header library |
| **cpp-httplib** | v0.64.0 | MIT | Local loopback HTTP transport | Vendored single-header library |
| **nlohmann/json** | v3.12.0 | MIT | JSON serialization/deserialization | Vendored single-header library |
| **WebView2 SDK** | 1.0.4314.35 | Microsoft SDK | Native webview embedding on Windows | Vendored loader and headers |
| **React & React DOM** | 18.3.1 | MIT | UI component framework | npm dependency (pinned lockfile) |
| **Three.js** | 0.169.0 | MIT | WebGL 3D rendering & OrbitControls | npm dependency (pinned lockfile) |
| **Plotly.js (Cartesian)** | 2.35.2 | MIT | 2D technical plotting | npm dependency (pinned lockfile) |
| **Framer Motion** | 11.11.11 | MIT | View transition animations only | npm dependency (pinned lockfile) |
| **IBM Plex Sans / Mono** | 5.1.0 | OFL-1.1 | Self-hosted typography | npm fontsource packages |
| **Tailwind CSS** | 3.4.14 | MIT | Utility styling | npm devDependency |
| **TypeScript & Vite** | 5.6.3 / 5.4.10 | Apache-2.0 / MIT | Compiler & build bundler | npm devDependencies |
| **Vitest & Playwright** | 2.1.4 / 1.48.2 | MIT / Apache-2.0 | Unit & E2E test runners | npm devDependencies |
| **@axe-core/playwright** | 4.10.0 | MPL-2.0 | Automated accessibility auditing | npm devDependency |

*No proprietary, unvetted, or uncredited source code has been included.*

---

## 4. Candidate Confirmation & Sign-Off Template

```text
================================================================================
                    CANDIDATE STATEMENT OF OWNERSHIP
================================================================================

I confirm that:
1. All original work listed in Section 2 of this document was developed for this
   assessment under my direction and review.
2. All third-party dependencies are open-source, listed in Section 3, and pinned
   in third_party/VERSIONS.md with their respective licenses.
3. The AI interactions documented in docs/AI_USE_LOG.md represent genuine usage
   during development, including candidate verification and review.
4. The handwritten derivation in the report appendix is my own work and will be
   verified against the provisional C++ backpropagation implementation.

Candidate Name:      ____________________________________________________
Candidate ID:        [MISSING: Candidate ID to confirm]
Actual Effort Hours: [MISSING: Candidate actual effort hours to confirm]
Date:                ____________________________________________________
Signature:           ____________________________________________________
================================================================================
```

