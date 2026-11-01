# Unified Engineering Workbench -- Technical Report

> **Document Class:** Technical Assessment Final Report  
> **Target Length:** $\le 6$ pages rendered (excluding handwritten derivation appendix)  
> **Rendering Guidance:** To render this document to PDF and verify page count, use Chrome/Edge headless print:  
> `msedge --headless --print-to-pdf=docs/REPORT.pdf --print-to-pdf-no-header docs/REPORT.html`  
> Alternatively, open in VS Code with the *Markdown PDF* extension. Open the resulting PDF in Adobe Acrobat or Edge to inspect the exact page count. Do not claim a page count without measuring the generated PDF.

---

## 1. Design Choices & System Architecture

### 1.1 Architectural Decoupling
The Unified Engineering Workbench enforces a strict separation between numerical computation, host orchestration, and user interface presentation:
1. **Algorithmic Core (`src/core`):** Pure C++17 library containing no Win32, socket, filesystem-path, or GUI dependencies. It compiles independently on any C++17 conformant toolchain (MSVC, GCC, Clang) and houses all finite element numerics, linear algebra solvers, file format parsers, multicore worker scheduling, device mock state machines, and backpropagation mathematics.
2. **Platform Telemetry (`src/platform`):** Separate abstraction library querying hardware topology (physical/logical cores, cache hierarchy, RAM) and reflecting compile-time definitions (compiler version, optimization flags, build configuration).
3. **Loopback Server (`src/server`):** Embedded C++ HTTP server (`cpp-httplib`) operating strictly on `127.0.0.1`. It encapsulates a concurrent job manager with thread-safe cancellation tokens, binary serialization routines for little-endian 3D geometry transport, and static asset delivery under strict Content Security Policy (CSP).
4. **Host Shell (`src/host`):** Lightweight Win32 process (`Launch.exe`) embedding Microsoft Edge WebView2. It implements single-instance mutual exclusion via a named Win32 mutex (`Local\PASS_UnifiedWorkbench_SingleInstance`) and generates a cryptographically secure 32-character hexadecimal token passed via command line and session cookie. If WebView2 is absent, it falls back to the default system browser via `ShellExecuteW`.
5. **Presentation Layer (`src/ui`):** Single-page application built with React 18, TypeScript (strict mode), and Tailwind CSS. The UI serves strictly as an instrument display and input collector: **it never computes engineering results, error bounds, or physics transformations**.

### 1.2 IPC & Zero-Authority Presentation
Inter-process communication between the native host/UI and the C++ engine occurs over local loopback using structured JSON for parameters/state and compact little-endian binary buffers for 3D geometry (STL vertex positions/normals, CSV nodal coordinates/scalar temperatures). This eliminates string parsing overhead in JavaScript and keeps the UI responsive at 60 FPS under large point-cloud manipulation.

---

## 2. 1-D Thermal FEA Verification

### 2.1 Governing Equation & Formulation
The thermal solver addresses 1-D steady-state heat conduction along a rod of length $L$ with uniform cross-sectional area $A$, constant thermal conductivity $k$, and constant volumetric heat generation rate $q$:
$$-\frac{d}{dx}\left(k A \frac{dT}{dx}\right) = q A, \quad x \in [0, L]$$
subject to Dirichlet boundary conditions:
$$T(0) = T_0, \quad T(L) = T_L$$
Lateral surfaces are assumed perfectly insulated (1-D heat flow). All variables follow strict SI units: $L\text{ [m]}$, $A\text{ [m}^2\text{]}$, $k\text{ [W}/(\text{m}\cdot\text{K})\text{]}$, $q\text{ [W}/\text{m}^3\text{]}$, and $T\text{ [}^\circ\text{C]}$.

### 2.2 Finite Element Formulation & Linear System
Discretization utilizes $N$ two-node linear elements of uniform length $h = L/N$. Using the standard Galerkin weak form with linear shape functions $N_1(\xi) = 1 - \xi$ and $N_2(\xi) = \xi$, the element stiffness matrix $\mathbf{k}^e$ and load vector $\mathbf{f}^e$ are:
$$\mathbf{k}^e = \frac{k A}{h} \begin{bmatrix} 1 & -1 \\ -1 & 1 \end{bmatrix}, \quad \mathbf{f}^e = \frac{q A h}{2} \begin{bmatrix} 1 \\ 1 \end{bmatrix}$$
Global assembly scatters element contributions into a global tridiagonal system. Dirichlet boundary conditions at nodes $0$ and $N$ are imposed via symmetric elimination, preserving matrix symmetry and preventing precision loss. The tridiagonal system is solved directly via the Thomas algorithm (Gaussian elimination without pivoting, $O(N)$ operations), with near-zero pivot detection ($\text{tolerance} = 10^{-15} \times \max|K_{ii}|$).

### 2.3 Closed-Form Analytical Solution & Midpoint Error
The exact closed-form solution satisfying the boundary value problem is:
$$T_{\text{exact}}(x) = T_0 + \frac{T_L - T_0}{L} x + \frac{q}{2k} x (L - x)$$
Because the exact solution is a quadratic polynomial and the finite element approximation is piecewise linear, nodal temperatures match the exact solution to machine precision ($< 1.2\times 10^{-12}\,^\circ\text{C}$). The maximum absolute error occurs at element midpoints:
$$\text{Midpoint Error} = \left| T_{\text{FE}}\left(x_e + \frac{h}{2}\right) - T_{\text{exact}}\left(x_e + \frac{h}{2}\right) \right| = \frac{q}{8k} h^2$$
For the assignment parameters ($L = 1\text{ m}, A = 10^{-4}\text{ m}^2, k = 10\text{ W}/(\text{m}\cdot\text{K}), q = 10^4\text{ W}/\text{m}^3, T_0 = 100\,^\circ\text{C}, T_L = 20\,^\circ\text{C}$):
$$\text{Midpoint Error} = \frac{10^4}{8 \times 10} h^2 = 125 \cdot h^2\,[^\circ\text{C}]$$

### 2.4 Numerical Convergence Table
Measured results from `fem1d_tests` and `workbench_cli fem`:

| $N$ (elements) | $h$ (m) | Max Midpoint Error ($^\circ\text{C}$) | Theoretical ($125 h^2$) | Max Nodal Error ($^\circ\text{C}$) | Observed Convergence Order |
|:---:|:---:|:---:|:---:|:---:|:---:|
| 10 | 0.1000 | $1.250000\times 10^0$ | 1.250000 | $5.684342\times 10^{-14}$ | — |
| 20 | 0.0500 | $3.125000\times 10^{-1}$ | 0.312500 | $2.557954\times 10^{-13}$ | 2.000000 |
| 40 | 0.0250 | $7.812500\times 10^{-2}$ | 0.078125 | $7.673862\times 10^{-13}$ | 2.000000 |
| 80 | 0.0125 | $1.953125\times 10^{-2}$ | 0.019531 | $1.136868\times 10^{-12}$ | 2.000000 |

Global thermal equilibrium was verified via boundary reactions: $R_0 + R_N = -q A L = -1.0\text{ W}$, satisfying conservation of energy.

### 2.5 Author Debugging Insight
[MISSING: Candidate debugging insight on FEA assembly or solver implementation to be added by candidate]

---

## 3. Multicore Batch Performance & Scaling

### 3.1 Workload Protocol & Hardware Environment
- **Workload:** 50,000 independent rod cases ($N = 10$ elements, 11 nodes per case, total 550,000 nodes solved per repetition).
- **RNG Seeding:** Independent SplitMix64 PRNG seeded via `spec.seed ^ splitmix64(i)` with bit-level canonical double conversion $((x \gg 11) \times 2^{-53})$ to eliminate cross-platform divergence. Case generation time (78.51 ms) is strictly excluded from compute timing.
- **Timing Measurement:** `std::chrono::steady_clock` tightly bracketing computation, including worker thread creation and join overhead. 1 warm-up run precedes 5 measured repetitions per worker count ($p \in \{1, 2, 4\}$).
- **Target Hardware:** AMD Ryzen 7 5800H (8 physical cores, 16 logical cores, base clock 3.2 GHz, boost up to 4.4 GHz), 16 GB DDR4 RAM, Windows 10 Home 26H2 (Build 26300), MSVC 19.44.35229.0, Release build (`/O2 /Oi /Ot /Oy /GL`).

### 3.2 Measured Benchmark Results

| Workers ($p$) | Rep 1 (ms) | Rep 2 (ms) | Rep 3 (ms) | Rep 4 (ms) | Rep 5 (ms) | Median $T_p$ (ms) | Speedup ($T_1/T_p$) | Efficiency ($E_p$) | Max $|\Delta T|$ vs Serial ($^\circ\text{C}$) |
|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| 1 | 42.56 | 42.17 | 41.93 | 42.12 | 41.24 | **42.12 ms** | 1.00× | 100.0% | $0.000000\times 10^0$ |
| 2 | 26.41 | 25.81 | 25.15 | 25.18 | 39.69 | **25.81 ms** | 1.63× | 81.6% | $0.000000\times 10^0$ |
| 4 | 16.19 | 15.21 | 14.69 | 14.11 | 14.14 | **14.69 ms** | 2.87× | 71.7% | $0.000000\times 10^0$ |

### 3.3 Overhead Analysis & Scaling Limits
1. **Sub-Microsecond Solve Latency:** Each rod solve executes in approximately $840\text{ ns}$ ($42.12\text{ ms} / 50,000$). At this granularity, worker threads execute hundreds of thousands of solves per second, placing heavy contention on the shared atomic work-stealing counter (`fetch_add`) and cache line invalidations.
2. **Thread Pool Lifecycle Overhead:** Instantiating and joining fresh `std::thread` worker pools for each test repetition incurs operating system context-switch and thread teardown overhead.
3. **Memory Bus & SMT Bottlenecks:** Although 16 logical threads exist, physical core scaling is limited by memory bus write throughput into preallocated contiguous solution memory, as well as dynamic core clock downclocking when multiple cores are loaded simultaneously.
4. **Numerical Precision:** Maximum difference between parallel output and serial baseline was exactly $0.000000\times 10^0\,^\circ\text{C}$ across all 550,000 nodes, confirming zero race conditions or numerical divergence.

---

## 4. Embedded Architecture & Hardware Simulation

### 4.1 System Topology & Signal Domains
The embedded architecture integrates three target hardware devices under supervisory host control:
- **MCU Target:** Espressif ESP32-DevKitC (3.3V I/O domain, USB-UART bridge host link).
- **FPGA Target:** Digilent Arty A7-35T (1.0V core, 1.8V AUX, 3.3V I/O banks, onboard FTDI USB-JTAG/UART bridge).
- **JTAG Adapter:** Olimex ARM-USB-TINY-H (FTDI FT2232H high-speed USB-to-JTAG bridge with buffered open-drain outputs and target voltage sensing).

```
+---------------------------------------------------------------------------------+
|                                HOST WORKSTATION                                 |
|  [Workbench UI] <--> [Loopback Server (C++)] <--> [Device Adapter Subsystem]    |
+--------------------------+------------------------------+-----------------------+
                           | USB Virtual COM              | High-Speed USB 2.0
                           v                              v
            +------------------------------+   +------------------------------+
            |      MCU (ESP32-DevKitC)     |   |    JTAG (ARM-USB-TINY-H)     |
            | - 3.3V Domain                |   | - 1.2V-5.0V Vref Sensor      |
            | - Supply Voltage Telemetry   |   | - Bus Master Controller      |
            +--------------+---------------+   +--------------+---------------+
                           |                                  |
                           | UART Stream (3.3V)               | JTAG Bus (TCK,TMS,TDI,TDO)
                           | (Level shifted if required)      |
                           v                                  v
            +-----------------------------------------------------------------+
            |                   FPGA (Digilent Arty A7-35T)                   |
            | - Dual Domain: 3.3V I/O Banks, 1.0V Internal Core Matrix        |
            | - Clock PLL Status, Configuration DONE Pin Telemetry            |
            +-----------------------------------------------------------------+
```

### 4.2 Pinout, Voltage Domains & Signal Ownership Summary

| Device | Domain / Supply | Key Interface Signals | Voltage Tolerance | Host Protocol | Role & Ownership |
|:---|:---:|:---|:---:|:---:|:---|
| **ESP32-DevKitC** | 5.0V USB / 3.3V LDO | TXD0, RXD0, EN, GPIO0 | 3.3V LVCMOS (non-5V tol.) | USB-CDC (115,200 baud) | Edge telemetry & sensor node |
| **Arty A7-35T** | 5.0V Ext / 1.0V Core | TCK, TMS, TDI, TDO, DONE | 3.3V Bank 0/14 (1.8V VCCAUX) | SPI / UART over FTDI | High-throughput DSP / pipeline |
| **ARM-USB-TINY-H** | 5.0V USB / Target Vref | TCK, TMS, TDI, TDO, nSRST | 1.2V to 5.0V auto-sensing | FTDI D2XX / LibUSB | Debug master; owns scan chain |

### 4.3 JTAG Architectural Role
The JTAG adapter is strictly an **out-of-band hardware debug and configuration interface**, not an interchangeable compute board. It drives the IEEE 1149.1 boundary-scan state machine (TAP controller), manages FPGA bitstream SRAM loading, and controls target halt/resume lines. It cannot execute application firmware or perform autonomous floating-point computations.

### 4.4 Simulation & Fault Recovery Protocol
All hardware adapters operate in C++ under deterministic software emulation, prominently flagged with `SIMULATED = true` across all telemetry payloads and UI elements. The supervisory state machine transitions across `Disconnected`, `Connecting`, `Connected`, and `Faulted`. When an injected timeout fault occurs, the driver transitions to `Faulted` and initiates an exponential retry backoff policy ($t_{\text{wait}} = t_{\text{base}} \cdot 2^{\text{attempt}}$), successfully recovering to `Connected` within 3 attempts and logging timestamped events to the UI timeline.

---

## 5. Test Outcomes & Quality Assurance

### 5.1 Verification Matrix Summary

| Test Domain | Harness / Tool | Cases / Checks | Pass Rate | Evidence & Artifacts |
|:---|:---|:---:|:---:|:---|
| **C++ Core & Math** | Catch2 v3.8.0 / CTest | 91 test cases, 1,068 assertions | **100% Passed** | 7 test suites, 0.70s real time |
| **Memory Safety** | MSVC AddressSanitizer (`/fsanitize=address`) | 91 test cases, 1,068 assertions | **100% Passed** | 0 leaks, 0 overflows, 0 use-after-free |
| **Negative & Fuzzing** | Deterministic byte fuzzing (`0xC001CAFEULL`) | Random slicing, corrupt tokens | **100% Passed** | Clean error returns; 0 crashes/hangs |
| **Frontend Unit** | Vitest v2.1.4 | 12 tests across 2 suites | **100% Passed** | Colormap interpolation, binary parsers |
| **Accessibility** | Axe-Core v4.10.0 / Playwright | All 4 views, 1024px, reduced motion | **0 Violations** | 0 critical/serious WCAG AA issues |
| **End-to-End UI** | Playwright Chromium Automation | Full user journeys (Phases 4-8) | **100% Passed** | 28 timestamped screenshots in `docs/img/ui/` |
| **Package Integrity** | `tools/verify_package.ps1` | 260 files, path & secret scanners | **100% Passed** | 0 absolute paths, 0 secrets, 0 artifacts |

---

## 6. Known Engineering Limitations

1. **ASCII STL Parsing Throughput:** Restricted to text files $\le 64\text{ MiB}$ and $\le 2,000,000$ triangles to protect 32-bit addresses and heap buffers from catastrophic allocation spikes.
2. **Single-Core Speedup Bound:** On 1-vCPU or single-core execution environments, multi-threaded parallel speedup is physically unachievable; the application detects this topology and displays an explicit caveat.
3. **Simulated Hardware Scope:** Hardware adapters provide behavioral protocol simulation for integration testing; physical USB/FTDI kernel drivers are not attached.
4. **1-D Heat Equation Domain:** Numerical FEA is strictly 1-D longitudinal rod conduction; 3-D volumetric solid conduction is excluded.
5. **Backpropagation Derivation:** Gradient formulas are provisionally implemented in C++ and verified against central differences ($|\Delta| \le 10^{-5}$); the candidate's independent handwritten derivation is maintained in the appendix.

---

## Appendix: Handwritten Backpropagation Derivation
*(Refer to candidate handwritten sheets attached separately per Requirement F1 / Z3)*

