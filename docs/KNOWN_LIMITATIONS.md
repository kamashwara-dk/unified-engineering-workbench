# Known Technical Limitations

This document records the genuine engineering boundaries, constraints, and platform behaviors identified during development, automated testing, and hardening of the **Unified Engineering Workbench**.

---

## 1. ASCII STL File Format & Memory Bounds

- **Technical Boundary**: The STL parser strictly accepts ASCII STL files up to **64 MiB** (`kMaxFileBytes`) and at most **2,000,000 triangles** (`kMaxStlTriangles`).
- **Rationale**: ASCII STL is an uncompressed, textual representation that requires approximately 180–250 bytes of memory per facet. Enforcing explicit size limits prevents buffer allocation spikes and memory exhaustion.
- **Encountered Behavior**: Uploading files larger than 64 MiB or containing more than 2,000,000 triangles returns a typed error (`Status::invalid`) with actionable guidance. Files detected as Binary STL (via 80-byte binary header inspection and null-byte checks) are rejected with a clear instruction: *"Binary STL detected. Please export as ASCII STL."*

---

## 2. Benchmark Speedup on Single-Core / Virtualized Topologies

- **Technical Boundary**: Parallel worker speedup ($S = T_1 / T_p$) is bounded by the physical hardware concurrency available to the operating system.
- **Rationale**: When executed on a virtual machine, cloud runner, or system with only 1 logical processor (`std::thread::hardware_concurrency() == 1`), worker pools cannot execute concurrently. Multi-worker benchmarks on such systems observe thread context-switching overhead, resulting in speedup slightly below $1.0\times$ and parallel efficiency below $100\%$.
- **Mitigation & UI Disclosure**: The platform hardware detection layer (`src/platform`) probes physical cores, logical cores, and CPU topology. When logical core count is 1, the Performance View explicitly displays an honest disclosure caveat banner:
  > *"Single-core CPU topology detected: parallel execution cannot achieve theoretical speedup due to lack of independent hardware execution units."*

---

## 3. Sandboxed Execution & Loopback Network Permissions

- **Technical Boundary**: The Workbench host shell communicates with the internal C++ core engine over local loopback (`127.0.0.1`) using a randomized ephemeral port and a cryptographically random session token.
- **Rationale**: In heavily locked-down enterprise environments or containerized CI sandboxes where Windows low-integrity policies forbid socket binding on `localhost`, the server cannot open the listening port.
- **Encountered Behavior**: Under normal Windows 11 desktop execution, loopback binding requires no elevated administrator privileges. In sandboxed automated test runners, commands invoking browser automation or socket operations must be granted loopback access (`BypassSandbox: true`).

---

## 4. WebView2 Runtime Dependency with Browser Fallback

- **Technical Boundary**: `Launch.exe` embeds Microsoft Edge WebView2 (`WebView2Loader.dll`) to render the React UI within a lightweight native desktop frame.
- **Rationale**: Stripped Windows PE environments or machines with WebView2 disabled cannot initialize the native webview control.
- **Mitigation & Resilience**: If `CreateCoreWebView2EnvironmentWithOptions` fails or the runtime is missing, `Launch.exe` automatically falls back to launching the user's default system web browser via Win32 `ShellExecuteW(..., L"http://127.0.0.1:<port>/?token=<token>", ...)`:
  - Users still experience one-click startup without installing additional software.
  - The UI operates identically in modern Chromium, Edge, Firefox, or Safari browsers.

---

## 5. Scope of Hardware View (Simulated Mocks)

- **Technical Boundary**: Phase 6 hardware device adapters (`McuAdapter`, `FpgaAdapter`, `JtagAdapter`) are deterministic software emulations designed to test supervisory control, connection state machines, fault injection, and exponential backoff recovery.
- **Rationale**: The Workbench is an engineering simulation environment and does not bundle proprietary FTDI, WinUSB, or JTAG kernel drivers.
- **Encountered Behavior**: Every telemetry payload, capability descriptor, event log, and UI card explicitly carries a persistent `SIMULATED` badge and flag (`simulated: true`). No physical electrical signals or USB ports are addressed.

---

## 6. 1-D Thermal FEA vs. 3D Geometric Domain

- **Technical Boundary**: The thermal analysis engine implements the exact 1-D steady-state heat equation along a rod with insulated lateral surfaces:
  $$\frac{d}{dx}\left(k A \frac{dT}{dx}\right) + q A = 0$$
  subject to Dirichlet boundary conditions at $x = 0$ and $x = L$.
- **Rationale**: The 3D Geometry Viewer renders arbitrary 3D surface meshes (STL) and 3D nodal point clouds (CSV), whereas the Thermal Analysis view solves the 1-D governing equations along the longitudinal axis.
- **Encountered Behavior**: The UI maintains clear separation between 3D Spatial Geometry visualization and 1-D Rod Thermal Analysis.

---

## 7. Provisional Nature of Analytical Backpropagation Formulas

- **Technical Boundary**: In Phase 7 (Learning Demo), analytical gradients ($dJ/dw, dJ/db, dJ/dv, dJ/dc$) are provisionally implemented in `analytic_gradients()` using standard chain-rule calculus and verified against central differences ($|\Delta| \le 1\times 10^{-5}$).
- **Candidate Ownership**: The candidate's handwritten mathematical derivation is performed separately. Per project protocol, the candidate will compare their handwritten steps against this provisional implementation prior to final submission.

