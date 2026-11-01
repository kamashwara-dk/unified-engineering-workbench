---
inclusion: always
---
# Technology
- Backend/core: C++17 (MSVC 2022 primary, GCC for CI-style checks). No Rust/Node in the backend.
- Server: cpp-httplib bound to 127.0.0.1 only, random port, per-launch token; nlohmann/json for payloads.
- Host: Win32 + WebView2 (static loader). If runtime is missing, open the default browser and show a message.
- UI: Vite + React + TypeScript + Tailwind; Three.js for 3D; Plotly (cartesian bundle) for plots; Framer Motion for view transitions only.
- Tests: Catch2 v3 amalgamated. Build: CMake >= 3.24 with presets. Static MSVC runtime.
- No network access at runtime. All fonts and libraries are bundled.
