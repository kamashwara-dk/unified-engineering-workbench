# Demonstration Video Script & Fallback Walkthrough

This document provides a precise, 3-to-5 minute demonstration video script covering requirements **A through F** in chronological order, accompanied by a timestamped screenshot-and-log walkthrough for offline review.

---

## Part 1: 3-to-5 Minute Video Demonstration Script

| Timestamp | Phase / Section | Screen / Feature Shown | Narration & Key Actions |
|:---:|:---:|:---|:---|
| **0:00 - 0:30** | **A: One-Click Launch & Shell** | Desktop $\to$ `Launch.exe` $\to$ Workbench Shell | Double-click `Launch.exe` directly from the extracted ZIP folder. Point out immediate startup without terminal, admin rights, or Node.js. Highlight the SmartScreen "Run anyway" step if applicable. Show the responsive application window, persistent status bar (`aria-live="polite"`), TopBar keyboard navigation (Left/Right arrows, Home/End), and explain the loopback token authentication. Launch a second instance to demonstrate single-instance mutex behavior (first instance is focused, second exits cleanly). |
| **0:30 - 1:15** | **B: 3D Geometry & Point Cloud** | Geometry View (`synthetic_lbracket.stl`, `synthetic_temperature.csv`) | Open Geometry tab. Upload `data/synthetic_lbracket.stl`: show C++ validation summary, 3D surface mesh rendering, rotate, pan, zoom, and press 'F' for fit-to-view. Upload `data/synthetic_temperature.csv`: show colored point cloud using the Inferno colormap, °C legend with API-provided min/max bounds, hover tooltip showing 3D coordinates and temperature, and enter a node ID into "Go to Node" to center and inspect it. Upload `malformed_line4.stl` to demonstrate rejection with exact line number guidance. |
| **1:15 - 2:00** | **C: 1-D Thermal FEA** | Thermal Analysis View | Switch to Thermal Analysis tab. Review rod parameters ($L=1\text{ m}, A=10^{-4}\text{ m}^2, k=10\text{ W}/(\text{m}\cdot\text{K}), q=10^4\text{ W}/\text{m}^3, T_0=100\,^\circ\text{C}, T_L=20\,^\circ\text{C}$). Click **Run Analysis**: show the dual plot overlaying the exact analytical curve against FEA solutions for $N = 10, 20, 40, 80$. Show the log-log convergence plot proving exact $O(h^2)$ slope, and the convergence table showing midpoint error matching theoretical $125 h^2$. Click **Export Solution CSV**. Demonstrate cooperative job cancellation and inline validation for invalid parameters ($L \le 0$). |
| **2:00 - 3:00** | **D: Multicore Benchmark** | Performance View (Worker Pool Benchmark) | Switch to Performance tab. Show the hardware telemetry card (CPU brand, physical/logical cores, RAM, compiler, flags, build type). Click **Calibrate**: show the advisory probe run recommending an optimal case count. Submit a benchmark run for 1, 2, and 4 workers with progress bar and cancel capability. Review the results table showing all 5 raw times per worker, median time, speedup ($S_p$), efficiency ($E_p$), and numerical agreement check ($|\Delta T| \le 10^{-8}\,^\circ\text{C}$). Review the Speedup/Efficiency chart and highlight the "Honest Notes" panel displaying single-CPU disclosures if running on 1 core. |
| **3:00 - 3:45** | **E: Embedded Hardware Mocks** | Hardware View | Switch to Hardware tab. Point out the prominent `SIMULATED` badge on the view header and on each card (MCU ESP32, FPGA Arty A7, JTAG Olimex). Click **Connect** on all three cards: observe state badge transitions from Disconnected to Connecting to Connected. Click **Read Telemetry** to display live simulated telemetry (voltages, clock status, scan chain). Arm **Simulate Timeout** and trigger read: show transition to `Faulted`, followed by automatic exponential retry backoff in the event timeline until recovered to `Connected`. Trigger an illegal action to show actionable error feedback. |
| **3:45 - 4:30** | **F: Learning Demo (Backprop)** | Performance View $\to$ Learning Demo Sub-tab | Open the "Learning Demo (Backprop)" sub-tab. Explain the single-neuron non-linear model ($x=1, y=2, h=\tanh(wx+b), \hat{y}=vh+c, J=0.5(\hat{y}-y)^2$) with initial parameters ($w=0.5, b=0, v=1, c=0, \text{lr}=0.1$). Show the pre-training gradient check table comparing analytical gradients against central differences ($\epsilon = 10^{-6}$, error $\le 10^{-5}$). Click **Run Gradient Check & Train**: show the interactive Plotly loss curve over 101 steps showing monotonic decrease, and parameter trajectories in the table. Click **Export Training CSV**. |
| **4:30 - 5:00** | **Wrap-Up & Verification** | VS Code Editor Task & Tests | Switch briefly to VS Code: run the editor task **"Run Workbench"** from `.vscode/tasks.json`. Run `ctest` in the terminal to display all 7 C++ test suites passing 100% in under 1 second. Conclude the demonstration. |

---

## Part 2: Fallback Timestamped Screenshot & Log Walkthrough

For reviewers assessing the submission asynchronously without video playback, the entire evaluation journey is documented below with corresponding screenshots from [`docs/img/ui/`](file:///docs/img/ui/) and verified log excerpts.

### 1. Requirements A1, A2, A8: App Shell & One-Click Launch
- **Screenshot:** [`01_app_shell_geometry_default.png`](file:///docs/img/ui/01_app_shell_geometry_default.png)
- **Features Verified:** Window initialized via `Launch.exe`, TopBar navigation tabs (`role="tablist"`), persistent status bar (`aria-live="polite"`), version badge `v0.1.0`.
- **Keyboard Navigation:** [`09_keyboard_navigation_and_focus.png`](file:///docs/img/ui/09_keyboard_navigation_and_focus.png) (tested with Tab, ArrowLeft, ArrowRight, Enter, Space).
- **Responsive Layout:** [`10_responsive_1024px_layout.png`](file:///docs/img/ui/10_responsive_1024px_layout.png) (clean rendering at 1024px width).

### 2. Requirements B1, B2, B3: 3D Geometry & Point Cloud
- **Screenshot STL:** [`02_stl_validation_and_3d_render.png`](file:///docs/img/ui/02_stl_validation_and_3d_render.png)
  - *Data:* `data/synthetic_lbracket.stl` (528 triangles).
  - *Verification:* Three.js 3D viewport, OrbitControls, bounding box $[-20.0, 20.0]$, recomputed face normals.
- **Screenshot CSV:** [`03_csv_validation_point_cloud_legend_inspect.png`](file:///docs/img/ui/03_csv_validation_point_cloud_legend_inspect.png)
  - *Data:* `data/synthetic_temperature.csv` (261 nodes).
  - *Verification:* Inferno colormap, °C legend with API bounds $[20.0, 100.0]\,^\circ\text{C}$, node hover inspection tooltip, "Go to Node" targeting.
- **Error Rejections:**
  - [`04_geometry_malformed_stl_error_line.png`](file:///docs/img/ui/04_geometry_malformed_stl_error_line.png): Rejection of `malformed_line4.stl` showing exact line number 4 and token error.
  - [`05_geometry_malformed_csv_error.png`](file:///docs/img/ui/05_geometry_malformed_csv_error.png): Rejection of non-numeric temperature value on line 3.

### 3. Requirements C1, C2, C3: 1-D Thermal FEA
- **Screenshot:** [`06_thermal_analysis_form_and_results.png`](file:///docs/img/ui/06_thermal_analysis_form_and_results.png)
  - *Verification:* Exact analytical vs FEA temperature distribution across $N \in \{10, 20, 40, 80\}$, log-log convergence chart with slope 2, convergence table with observed order $2.000000$ and midpoint error $125 h^2$.
- **Cooperative Cancellation:** [`07_thermal_job_cancellation.png`](file:///docs/img/ui/07_thermal_job_cancellation.png) (cancelling job leaves state clean, 0 partial files).
- **Inline Parameter Errors:** [`08_thermal_field_validation_error.png`](file:///docs/img/ui/08_thermal_field_validation_error.png) (actionable error message for non-positive rod length).

### 4. Requirements D1 - D7: Multicore Benchmark & Performance
- **Screenshot Empty State:** [`12_performance_view_empty_state.png`](file:///docs/img/ui/12_performance_view_empty_state.png)
- **Screenshot Calibration:** [`13_performance_calibration_advisory.png`](file:///docs/img/ui/13_performance_calibration_advisory.png) (advisory case count recommendation targeting $\ge 500\text{ ms}$).
- **Screenshot Benchmark Results:** [`16_performance_benchmark_results_and_charts.png`](file:///docs/img/ui/16_performance_benchmark_results_and_charts.png)
  - *Verification:* Hardware topology card (AMD Ryzen 7 5800H, 8 physical / 16 logical cores, 16 GB RAM, Release build).
  - *Results Table:* 5 raw repetitions per worker count, median, speedup ($1.63\times$ at 2 workers, $2.87\times$ at 4 workers), efficiency, exact agreement check ($|\Delta T| \le 10^{-8}\,^\circ\text{C}$).
  - *Charts:* Dual Plotly speedup and parallel efficiency curves.
- **Single-Core Disclosure:** [`17_performance_single_cpu_disclosure.png`](file:///docs/img/ui/17_performance_single_cpu_disclosure.png) (demonstrating honest banner when executed on single-CPU topology).
- **Error Handling & Cancel:** [`14_performance_invalid_spec_error.png`](file:///docs/img/ui/14_performance_invalid_spec_error.png), [`15_performance_benchmark_cancellation.png`](file:///docs/img/ui/15_performance_benchmark_cancellation.png).

### 5. Requirements E1 - E5: Embedded Hardware Simulation
- **Screenshot Disconnected:** [`18_hardware_initial_disconnected_state.png`](file:///docs/img/ui/18_hardware_initial_disconnected_state.png) (cards for MCU, FPGA, and JTAG, with persistent `SIMULATED` badges).
- **Screenshot Connected & Telemetry:** [`20_hardware_connected_and_telemetry.png`](file:///docs/img/ui/20_hardware_connected_and_telemetry.png) (live simulated telemetry: supply voltage, die temp, PLL lock, scan chain length).
- **Screenshot Timeout & Recovery:** [`21_hardware_timeout_and_recovery_timeline.png`](file:///docs/img/ui/21_hardware_timeout_and_recovery_timeline.png) (injected timeout triggers `Faulted` state, exponential backoff retries, and clean recovery with timestamps).
- **Illegal Action Error:** [`19_hardware_illegal_action_error.png`](file:///docs/img/ui/19_hardware_illegal_action_error.png) (attempting illegal telemetry read when disconnected returns typed 409 error).

### 6. Requirements F1 - F3: Backpropagation & Learning Demo
- **Screenshot Initial State:** [`22_backprop_learning_demo_initial.png`](file:///docs/img/ui/22_backprop_learning_demo_initial.png) (model equations, initial parameters, gradient check table).
- **Screenshot Trained State:** [`23_backprop_learning_demo_trained.png`](file:///docs/img/ui/23_backprop_learning_demo_trained.png) (loss curve across 101 steps showing monotonic decrease from 0.840 to 0.005, step trajectory table).
- **Screenshot Linear Scale:** [`24_backprop_linear_scale.png`](file:///docs/img/ui/24_backprop_linear_scale.png).

### 7. Accessibility & Hardening Audit (Phase 8)
- **Screenshot Keyboard Geometry:** [`25_ui_audit_keyboard_geometry.png`](file:///docs/img/ui/25_ui_audit_keyboard_geometry.png)
- **Screenshot 1024px Thermal:** [`26_ui_audit_1024px_thermal.png`](file:///docs/img/ui/26_ui_audit_1024px_thermal.png)
- **Screenshot Clean Axe Hardware:** [`27_ui_audit_axe_clean_hardware.png`](file:///docs/img/ui/27_ui_audit_axe_clean_hardware.png)
- **Screenshot Clean Axe Backprop:** [`28_ui_audit_learning_demo_a11y.png`](file:///docs/img/ui/28_ui_audit_learning_demo_a11y.png)
- **Axe-Core Audit Outcome:** **0 violations (0 critical/serious)** across all states and views.

