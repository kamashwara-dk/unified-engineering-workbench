# Tasks (check off only when acceptance evidence is recorded in docs/IMPLEMENTATION_PLAN.md)

## Phase 0 - Setup
- [x] Repo hygiene files, CMake presets, core library skeleton, vendored Catch2, smoke test (Linux/GCC verified)
- [x] Steering files, spec drafts, tooling guide, AI-log template
- [x] Run configure/build/ctest on Windows 11 with MSVC (`--preset vs2022`) and record the result
- [~] Resolve [CONFIRM] items: candidate ID, board choices, size limits
- [~] Install MCP servers and skills per docs/TOOLING_SETUP.md

## Phase 1 - Walking skeleton (A1, A7, A8)
- [x] Pin + vendor cpp-httplib, nlohmann/json, WebView2 SDK; update third_party/VERSIONS.md
- [x] `/api/health`; loopback bind; random port; token
- [x] Win32 host + WebView2 + runtime check + browser fallback
- [x] Static runtime link; dist layout; zip script
- [~] Windows Sandbox launch x4; `tasks.json` "Run Workbench"

## Phase 2 - FEA (C1-C3)
- [x] Assembly + elimination BCs + Thomas solver
- [x] Exact solution, midpoint metric, convergence table, CSV export
- [x] Tests: oracle table, observed order, negative inputs

## Phase 3 - Importers (B1-B5)
- [x] STL parser + tests (valid, malformed, NaN, binary, empty, truncated)
- [x] CSV parser + tests (header, columns, non-finite, duplicates, BOM/CRLF, empty)
- [x] C++ sample generators; data/ provenance; size-limit tests

## Phase 4 - UI (A2-A6, B1-B2, C2)
- [x] Tokens + layout + four views; upload/validation UX; progress/cancel; exports
- [x] Three.js STL + point cloud + legend + inspection; Plotly FEA plots

## Phase 5 - Parallel (D1-D7)
- [x] Generator, serial + parallel runners, agreement check, benchmark harness, hardware report (Phase 5A)
- [x] Performance view; launch-to-ready metric (Phase 5B)

## Phase 6 - Embedded (E1-E5)
- [x] Adapters + failure/recovery (Phase 6A); Hardware view (Phase 6B)
- [~] Embedded design doc + block diagram + voltage table (Phase 6C)

## Phase 7 - Backprop (F1-F3)
- [~] Handwritten pages (candidate); C++ gradients + central-difference check; loss CSV + plot

## Phase 8 - Hardening
- [~] Negative-test sweep; ASan; accessibility; size-limit table; 4 clean launches

## Phase 9 - Delivery (Z1-Z6)
- [~] README, report, video/walkthrough, AI log, ownership statement, git bundle + log, final ZIP, fresh-extract test
