# UI Design System & Architecture Specification

## 1. Design Direction: Engineering Bench Instrument

The Unified Engineering Workbench frontend is styled as an **engineering bench instrument**, prioritizing high-density technical information, clarity, precision, and low-latency feedback over generic consumer dashboard aesthetics.

Key principles:
- **Graphite Neutrals**: High-contrast dark theme by default, reducing eye strain during long engineering sessions.
- **Electric Precision Accent**: Technical cyan (`#0284c7` / `#38bdf8`) used purposefully for active elements, selected entities, and primary interactions.
- **Authoritative Engineering Results**: The UI never computes engineering metrics or validation bounds; all bounds, statistics, error tolerances, and units come strictly from the C++ backend.
- **Self-Hosted Typography (OFL)**: Zero runtime network requests or CDN dependencies; bundled with IBM Plex Sans and IBM Plex Mono.
- **Tabular Mono Numerals**: Every physical measurement, coordinate, temperature value, element count, and numerical error is rendered using monospaced tabular numerals (`tabular-nums`) to prevent layout jitter and ensure column alignment.

---

## 2. Design Tokens

Design system tokens are declared as CSS custom properties in `src/ui/src/styles/tokens.css` and exposed via Tailwind CSS in `tailwind.config.js`.

### 2.1 Color Palette

| Token | Hex / Value | Semantic Role |
|---|---|---|
| `--bg-canvas` | `#111215` | Deepest graphite application canvas background |
| `--bg-panel` | `#18191e` | Primary instrument panels, toolbars, and cards |
| `--bg-card` | `#202228` | Elevated containers, inspector cards, dropzones |
| `--bg-subtle` | `#292c34` | Hover states, active buttons, subtle highlights |
| `--bg-input` | `#15161a` | Form input fields and text areas |
| `--border-subtle` | `#2e313a` | Dividers and secondary boundaries |
| `--border-default` | `#3e434f` | Input borders, panel separators, table borders |
| `--border-strong` | `#525867` | Active borders and highlighted containers |
| `--text-primary` | `#f0f2f5` | High-contrast body text and headers |
| `--text-secondary` | `#9da3b4` | Form labels, secondary metadata, unit labels |
| `--text-muted` | `#646a7c` | Placeholders, de-emphasized units, keyboard hints |
| `--accent-default` | `#0284c7` | Primary action buttons, selected tabs |
| `--accent-hover` | `#0369a1` | Hover state for primary actions |
| `--accent-active` | `#38bdf8` | Active focus rings, selected 3D vertices |
| `--color-success` | `#10b981` | Successful validations, completed FEA jobs |
| `--color-warning` | `#f59e0b` | Warnings, queued jobs, midpoint error traces |
| `--color-error` | `#ef4444` | Validation rejections, failed runs, invalid inputs |
| `--color-info` | `#38bdf8` | Running jobs, temperature values, nodal error traces |

### 2.2 Typography & Radii

- **Sans-Serif**: `'IBM Plex Sans', -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif`
- **Monospace**: `'IBM Plex Mono', ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, monospace`
- **Tabular Numerals**: `font-variant-numeric: tabular-nums; font-feature-settings: 'tnum' 1;`
- **Radii Scale**: `xs` (2px), `sm` (4px), `md` (6px), `lg` (8px)
- **Focus Ring**: `0 0 0 2px #111215, 0 0 0 4px #38bdf8` (high-contrast dual ring)

---

## 3. Views and Component Specifications

### 3.1 App Shell
- **TopBar**:
  - Brand identity: "Unified Engineering Workbench" + C++ core version badge (`v0.1.0`).
  - View navigation tabs (`role="tablist"`): `Geometry`, `Thermal Analysis`, `Performance` (Phase 5 placeholder), `Hardware` (Phase 6 placeholder).
  - Keyboard navigation: ArrowLeft / ArrowRight to switch tabs, Home / End to jump, Enter / Space to select.
  - Theme toggle: Seamless switch between Dark (default) and Light mode.
- **StatusBar**:
  - `role="status"` with `aria-live="polite"`.
  - Left: Backend connection indicator (green dot when connected to `127.0.0.1`, animated yellow when checking, red when unreachable) and backend uptime.
  - Center: Actionable error banner with dismiss button.
  - Right: Active job state (`Idle`, `Queued`, `Running [progress %]`, `Succeeded`, `Failed`, `Cancelled`).
- **ViewTransition**:
  - Uses Framer Motion exclusively for view entry/exit transitions.
  - Fully respects `prefers-reduced-motion` (animations zeroed out when requested).

### 3.2 Geometry View
- **File Ingestion**:
  - Button file selection and full drag-and-drop support.
  - Supports ASCII `.stl` and ANSYS nodal `.csv`.
  - Backend validation: Uploads directly to C++ server via `POST /api/files/stl` or `POST /api/files/csv`.
  - Actionable Error Mapping: If a file contains syntax errors or non-finite values, displays the C++ error message and highlights the exact line number (`Fault detected on line N`).
- **3D Viewport (Three.js with OrbitControls)**:
  - STL rendering:
    - Fetches little-endian binary mesh `GET /api/files/{id}/stl.bin` using `Float32Array`.
    - Double-sided shaded mesh with server face normals.
    - Explicit unitless notice: *"STL is unitless; assumed unit is metres"*.
    - Interactive controls: Rotate (left drag), Pan (right drag / Shift+drag), Zoom (wheel).
    - Fit-to-view button and `F` keyboard shortcut.
    - Reset view button.
  - CSV Point Cloud:
    - Fetches binary payload `GET /api/files/{id}/points.bin`.
    - Perceptually uniform, color-blind-safe **Inferno colormap** lookup table (256 RGB stops).
    - Dynamic point size slider and numeric display.
    - Continuous color legend with ticks in `°C` derived from API `min_temp_C` and `max_temp_C`.
    - Interactive hover & click inspection: displays `node_id`, `x, y, z (m)`, and `temperature (°C)`.
    - Keyboard-accessible "Go to Node ID" search: highlights node in 3D and displays coordinates.
    - High performance: 100% typed arrays, zero per-point React elements.

### 3.3 Thermal Analysis View
- **Parameter Input Form**:
  - Defaults match assignment specifications ($L=1.0\text{ m}, A=1\times 10^{-4}\text{ m}^2, k=10\text{ W/(m}\cdot\text{K)}, q=1\times 10^4\text{ W/m}^3, T(0)=100^\circ\text{C}, T(L)=20^\circ\text{C}, N=10$).
  - Inline field validation errors mapped from C++ 422 HTTP responses.
- **Job Execution & Cancellation**:
  - Asynchronous background job execution with cooperative cancellation via `DELETE /api/jobs/{id}`.
  - Real-time progress bar and state polling.
- **Engineering Plots (Plotly.js Cartesian Bundle)**:
  - **Temperature Profile ($T$ vs $x$)**:
    - FEA solution curves for $N = 10, 20, 40, 80$ plotted alongside exact analytical curve.
    - Fully labelled axes with units ($x$ in m, $T$ in °C).
  - **Mesh Convergence (Log-Log)**:
    - Max midpoint error vs element size $h$ (showing quadratic $O(h^2)$ slope).
    - Max nodal error vs element size $h$ (showing machine precision level $O(10^{-13})$).
    - Labelled observed order ($\approx 2.00$).
- **Results Table & Exports**:
  - Verification table with $N$, $h$, Max Midpoint Error, Max Nodal Error, Observed Order.
  - Export Solution CSV and Export Convergence CSV directly from C++ API.
  - Self-explanatory PNG plot exports via Plotly `downloadImage`.

### 3.3 Performance & Benchmark View (Phase 5)
- **Workload Configuration**:
  - Seed, Case count, Mesh size $N$, Conductivity range $[k_{\min}, k_{\max}]$, Heat generation range $[q_{\min}, q_{\max}]$.
  - Worker scaling protocol notes showing automatic worker count evaluation ($p \in \{1, 2, 4\}$ capped by logical core count).
  - Single-CPU path simulation toggle for hardware verification.
  - Inline validation error mapping from the C++ API directly below the offending input field.
- **Calibration Engine**:
  - One-click workload calibration executing a small probe run to calculate time per solve.
  - Automatically updates case count to target $\ge 500\text{ ms}$ on 1 worker, mitigating timer granularity noise.
- **Hardware Topology & System Reflection**:
  - Hardware card detailing CPU brand, Physical/Logical cores, Total RAM in GB, Compiler name and version, Build type, and Optimization flags.
  - Prominent warning banner displayed when non-Release build is detected.
  - Launch-to-Ready latency displayed in a distinct system card, strictly separated from solver compute times.
- **Benchmark Execution**:
  - Background execution with real-time percentage progress bar and cooperative cancellation.
  - Zero fabricated numbers: Empty state rendered when no benchmark has been run.
- **Results & Scaling Visualizations**:
  - **Speedup & Efficiency Chart (Plotly.js)**: Dual-axis plot displaying measured speedup ($T_1 / T_p$), ideal linear speedup reference, and parallel efficiency percentage.
  - **Repetitions Table**: All 5 raw repetition times per worker count, median, speedup, efficiency, and exact serial agreement verification badge ($\max |\Delta T| \le 10^{-8}\,^\circ\text{C}$).
  - **Honest Engineering Caveats Panel**: Transparent documentation of thread startup latency, memory bandwidth limits, tiny Thomas solve cache dynamics, SMT scaling realities, and single-CPU disclosures.
  - **Exports**: Atomic CSV and JSON download triggers.

### 3.4 Hardware & Embedded View (Phase 6)
- **Persistent SIMULATED Environment**:
  - Unmissable top warning banner emphasizing that all devices, voltages, temperatures, and recovery loops are software mocks.
  - Persistent `SIMULATED` badges on view, each device card, telemetry readouts, and event messages.
- **Three Concrete Device Mocks**:
  - **MCU (ESP32-DevKitC)**: Live supply voltage ($3.30\text{ V}$) and die temperature ($35.0^\circ\text{C}$).
  - **FPGA (Digilent Arty A7-35T)**: Configuration DONE pin, clock PLL lock state, and clock register status.
  - **JTAG Adapter (Olimex ARM-USB-TINY-H)**: Target reference voltage ($V_{\text{REF}}$), TCK clock frequency ($1.0\text{ MHz}$), scan chain TAP length, and debug interface disclosure.
- **Controls & Interactions**:
  - Individual Connect and Disconnect controls per device.
  - Read Telemetry with live readout sampling.
  - Simulate Timeout control arming fault injection on the next read.
- **Fault Recovery & Event Timeline**:
  - Real-time event log displaying chronological transitions: Connect, Disconnect, Timeout, Faulted, Retry (with exponential backoff delays), and Recovered.
  - High-precision timestamps directly from the API.
- **Typed Actionable Error Messages**:
  - Intercepts 409 conflict responses on illegal state transitions (e.g. reading telemetry while disconnected, double-connect) and renders actionable guidance explaining how to proceed.

---

## 4. Accessibility and Quality Standards

1. **WCAG AA Contrast**: All body text and control elements exceed the 4.5:1 contrast ratio against graphite backgrounds.
2. **Keyboard Accessibility**:
   - Tab navigation between tabs using Left/Right arrows, Home, End.
   - Visible focus rings with high-contrast dual ring outlines.
   - Form inputs with explicit `<label htmlFor="...">` bindings.
   - 3D Fit view triggerable via `F` key.
   - Go to Node ID fully operable with keyboard input + Enter.
3. **Motion Sensitivity**: Respects `prefers-reduced-motion` media queries.
4. **Responsive Layout**: Fluid flex layout down to 1024px width without canvas clipping or table truncation.
5. **Strict Content Security Policy (CSP)**:
   - Evaluated against backend CSP:
     `default-src 'self'; img-src 'self' data: blob:; worker-src blob:; style-src 'self' 'unsafe-inline'; font-src 'self'; connect-src 'self'; frame-ancestors 'none'; base-uri 'none'; form-action 'self'; object-src 'none'`
   - **CSP Status**: 100% compliant. Exactly **zero** CSP relaxations required for Three.js, OrbitControls, and Plotly cartesian bundle.

---

## 5. Visual Verification Evidence (Screenshots)

The automated Playwright test suite verified all user interactions against `workbench_server --dev --ui-dir src/ui/dist`. High-resolution captures are preserved in `docs/img/ui/`:

| Screenshot | What it Proves |
|---|---|
| `01_app_shell_geometry_default.png` | App shell initialization, TopBar tabs, graphite theme, connected status bar (`127.0.0.1`), and Geometry view dropzone. |
| `02_stl_validation_and_3d_render.png` | C++ validation of `synthetic_lbracket.stl` (142 triangles), bounds calculation, unitless note, and Three.js 3D mesh rendering with OrbitControls. |
| `03_csv_validation_point_cloud_legend_inspect.png` | C++ validation of `synthetic_temperature.csv` (3,000 nodes), Inferno point cloud, °C legend with min/max ticks, and inspection of Node #42. |
| `04_geometry_malformed_stl_error_line.png` | Rejection of malformed STL file with actionable C++ error and line number badge (`Fault detected on line 4`). |
| `05_geometry_malformed_csv_error.png` | Rejection of malformed CSV with incorrect header schema, showing actionable error message. |
| `06_thermal_analysis_form_and_results.png` | Full FEA solve: parameter form defaults, Plotly $T$ vs $x$ curves ($N=10, 20, 40, 80$ + exact), log-log convergence plot, and numerical table. |
| `07_thermal_job_cancellation.png` | Cooperative background job cancellation in C++ server via Cancel button. |
| `08_thermal_field_validation_error.png` | Field-level validation error from C++ server when non-physical parameters are entered ($k \le 0$). |
| `09_keyboard_navigation_and_focus.png` | Keyboard focus ring visibility and Arrow key tablist switching. |
| `10_responsive_1024px_layout.png` | Viewport adaptation at 1024px width: dual-column sidebar/canvas layout remains unclipped and usable. |
| `11_placeholders_performance_hardware.png` | Honest placeholder for Hardware (Phase 6) with no fake content. |
| `12_performance_view_empty_state.png` | Performance view initial empty state: hardware card with real CPU, cores, RAM, compiler, flags, build type, launch-to-ready metric, and empty results card (zero fabricated numbers). |
| `13_performance_calibration_advisory.png` | Workload calibration advisory banner: probe timing, estimated time per case, and suggested case count automatically applied to form. |
| `14_performance_invalid_spec_error.png` | Inline API validation error display (`k_max must be >= k_min`) positioned directly under the offending input field. |
| `15_performance_benchmark_cancellation.png` | Cooperative background benchmark job cancellation via Cancel button with progress bar and status bar updates. |
| `16_performance_benchmark_results_and_charts.png` | Completed benchmark: dual-axis Plotly speedup & efficiency chart, full results table with all 5 repetitions per worker, median, speedup, efficiency, exact agreement PASS check ($|\Delta T| \le 10^{-8}\,^\circ\text{C}$), honest scaling caveats panel, and CSV/JSON export actions. |
| `17_performance_single_cpu_disclosure.png` | Single-CPU simulation disclosure: explicit single-core disclaimer rendered in the Honest Notes panel and results table when running on 1-core topology. |
| `18_hardware_initial_disconnected_state.png` | Hardware view initial state: persistent SIMULATED warning banner, three device cards (MCU, FPGA, JTAG) in disconnected state with capabilities, and empty event timeline. |
| `19_hardware_illegal_action_error.png` | Actionable error message when attempting an illegal action (reading telemetry while disconnected): actionable guidance instructing user to connect first. |
| `20_hardware_connected_and_telemetry.png` | All three devices connected: live simulated telemetry readings displayed for MCU (voltage, temperature), FPGA (DONE, PLL locked), and JTAG (VREF, TCK, scan chain). |
| `21_hardware_timeout_and_recovery_timeline.png` | Fault injection & recovery: timeout armed on MCU, read telemetry triggered timeout and transitioned to faulted, retry policy executed exponential backoff, recovered to connected, and full event timeline with API timestamps. |
| `22_backprop_learning_demo_initial.png` | Learning Demo (Backprop) initial view: model reference equations ($z = w x + b, h = \tanh(z), \hat{y} = v h + c, J = 0.5(\hat{y}-y)^2$), hyperparameters, pending gradient check table, and loss plot placeholder. |
| `23_backprop_learning_demo_trained.png` | Completed Backpropagation run: gradient check table ($w, b, v, c$) all passed ($\le 10^{-5}$), Plotly loss trajectory chart ($J$ vs step), step 0 / step 1 / final summary cards, and CSV/PNG export buttons. |
| `24_backprop_linear_scale.png` | Linear/Logarithmic scale toggle: loss trajectory displayed on linear axis, showing steep descent at step 1 followed by asymptotic convergence. |

