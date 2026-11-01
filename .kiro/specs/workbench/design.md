# Design

## Components
`Launch.exe` (host) → `server` (HTTP, jobs) → `workbench_core` (pure C++). UI is a static Vite build served by the server.

## Core modules (src/core)
| Module | Responsibility | Key types |
|---|---|---|
| `fem1d` | assembly, BCs, Thomas solve, exact solution, error metrics | `RodProblem`, `RodSolution`, `solve_rod()` |
| `batch` | seeded generator, serial + pooled-parallel runners, benchmark | `BatchSpec`, `run_serial()`, `run_parallel(workers)` |
| `geometry_io` | ASCII STL and nodal-CSV parse + validate | `Mesh`, `PointCloud`, `ParseResult` |
| `backprop` | forward, analytic gradients, numeric check, training loop | `Params`, `Gradients` |
| `devices` | `IDeviceAdapter`, three mocks, recoverable-timeout simulation | `DeviceInfo`, `Telemetry` |
Errors: `Status{code, message, location}`; core never throws across the module boundary.

## HTTP API (loopback only, token header `X-WB-Token`)
| Route | Purpose |
|---|---|
| `GET /api/health` | readiness + version |
| `POST /api/files/stl`, `POST /api/files/csv` | upload, validate, return summary; `GET .../data` returns binary typed arrays |
| `POST /api/jobs` `{kind, params}` | start fem / batch / benchmark / backprop job |
| `GET /api/jobs/{id}` | state, progress, result or error |
| `DELETE /api/jobs/{id}` | cooperative cancel |
| `GET /api/devices`, `POST /api/devices/{id}/connect|disconnect|telemetry` | simulated hardware |
| `GET /api/export/{job}.csv` | atomic-write export |

## Concurrency model
Pool of `min(requested, hardware_concurrency)` threads; shared atomic next-index; per-index preallocated result slots
(no locks on hot path); cancel flag checked between cases/chunks. Timing excludes parsing, rendering and compilation.

## Packaging layout (dist/ → ZIP)
`Launch.exe`, `app/ui/**` (static assets), `README.md`, `docs/`, `data/`, `results/`, `tests/`, `src/`, run config.

## Risks and mitigations
SmartScreen warning on unsigned exe (document); WebView2 runtime missing (browser fallback);
Antigravity extension availability (tasks.json); tiny solves vs thread overhead (calibrate workload, disclose).
