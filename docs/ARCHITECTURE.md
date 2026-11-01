# Architecture

```
Launch.exe (src/host, Win32 + WebView2, Windows only)
   |  starts, picks a random free port, generates a per-launch token
   v
HTTP server on 127.0.0.1 (src/server): routes, job manager, serialisation
   |  calls
   v
workbench_core (src/core, pure C++17): fem1d, batch, geometry_io, backprop, devices
```
UI (`src/ui`, Vite static build) is served by the server and talks JSON/binary to `/api/*`.

## Module rules
- `core` knows nothing about HTTP, JSON transport, windows or file paths (it takes streams/buffers).
- `server` translates between HTTP and core types and owns job lifecycle and cancellation flags.
- `host` only creates the window and starts the server. No business logic.
- `ui` renders and collects input. Authoritative validation and all statistics are computed in C++.

## Decisions (ADRs)
| ADR | Decision | Status |
|---|---|---|
| 0001 | C++ Win32 + WebView2 host with loopback HTTP, not Electron/Tauri | accepted |
| 0002 | Static MSVC runtime and static WebView2 loader; browser fallback if runtime missing | accepted |
| 0003 | `tasks.json` task as the editor run configuration (no debugger extension needed) | accepted, verify in Antigravity |
| 0004 | Jobs: POST create, GET poll, DELETE cancel (cooperative atomic flag); exports via temp file + rename | accepted |
| 0005 | Meshes and point clouds sent as binary typed arrays, not JSON | accepted |
| 0006 | Catch2 v3 amalgamated for tests; deps vendored/locked for offline reproducibility | accepted |
| 0007 | Frontend shell, bench instrument design, offline bundling, and strict CSP | accepted |

Write each ADR as `docs/adr/NNNN-title.md` when the decision is first exercised (context, decision, consequences).
