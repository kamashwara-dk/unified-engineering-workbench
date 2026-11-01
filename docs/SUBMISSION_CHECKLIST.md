# Final Submission Checklist & Traceability to Assignment Section 8

This document maps each requirement of **Section 8 (Submission & Delivery / Requirements Z1 through Z6)** to its satisfying artifacts in the repository, and provides a step-by-step manual verification procedure for candidate quality assurance before final delivery.

---

## 1. Traceability Matrix: Section 8 Requirements vs. Deliverable Files

| Requirement ID | Assignment Requirement Summary | Satisfying Repository File(s) | Verification Evidence & Location | Status |
|:---:|:---|:---|:---|:---:|
| **Z1** | Distributable ZIP named `CandidateID_Workbench.zip` containing `Launch.exe`, `src/`, `CMakeLists.txt`, dependency/version info, editor config, `data/` with provenance, `tests/`, `results/`, `docs/`, `README.md`, and version-control evidence. | [`tools/package.ps1`](file:///tools/package.ps1), [`tools/verify_package.ps1`](file:///tools/verify_package.ps1) | Automated package verification tool confirms 260 files, 140.9 MB unpacked dist, zero absolute paths, zero credentials. | **Verified** |
| **Z2** | `README.md` stating prerequisites, one-click launch steps, architecture with diagram, formats and schemas, build/test steps, and limitations. | [`README.md`](file:///README.md), [`docs/BUILD_FROM_SOURCE.md`](file:///docs/BUILD_FROM_SOURCE.md) | Fully documented with Mermaid architecture diagram, SmartScreen instructions, schemas, and limitations. | **Verified** |
| **Z3** | Compact technical report $\le 6$ pages rendered plus 2-page appendix; 3–5 min demo video or timestamped screenshot walkthrough with logs. | [`docs/REPORT.md`](file:///docs/REPORT.md), [`docs/DEMO_SCRIPT.md`](file:///docs/DEMO_SCRIPT.md), [`docs/img/ui/`](file:///docs/img/ui/) (28 screenshots) | `REPORT.md` covers all 6 required sections; `DEMO_SCRIPT.md` provides 0:00–4:45 walkthrough and fallback screenshot mapping; candidate handwritten appendix pending. | **Verified (Code & Report)** / **Partially (Handwritten Appendix & Video)** |
| **Z4** | Git bundle or repository with meaningful history plus readable commit log (`git log --stat --date=iso`) with personal data sanitized. | [`tools/make_git_evidence.ps1`](file:///tools/make_git_evidence.ps1), [`docs/COMMIT_LOG.md`](file:///docs/COMMIT_LOG.md), [`docs/repo.bundle`](file:///docs/repo.bundle) | Automated script `make_git_evidence.ps1` exports bundle and sanitized commit log. Candidate runs script prior to final zip. | **Verified (Script)** / **Candidate Run Pending** |
| **Z5** | AI-use log documenting tools, prompts, outputs, edits, validation, and at least one incorrect suggestion or deliberate challenge test; ownership statement. | [`docs/AI_USE_LOG.md`](file:///docs/AI_USE_LOG.md), [`docs/OWNERSHIP.md`](file:///docs/OWNERSHIP.md) | Consolidated log across Phases 0–9 with candidate review placeholders and ownership statement template. | **Verified (Log Structure)** / **Candidate Review Pending** |
| **Z6** | Candidate ID and actual effort hours recorded without fabrication. | [`docs/OWNERSHIP.md`](file:///docs/OWNERSHIP.md), [`docs/REPORT.md`](file:///docs/REPORT.md) | Placeholders `[MISSING: Candidate ID to confirm]` and `[MISSING: Candidate actual effort hours to confirm]` provided. | **Pending Candidate Input** |

---

## 2. Pre-Submission Manual Verification Checklist

Follow these steps sequentially on a fresh machine or clean directory to verify the complete user experience:

### Step 1: Package Creation
- [ ] Run the packaging script with your official Candidate ID:
  ```powershell
  .\tools\package.ps1 -CandidateId <YOUR_CANDIDATE_ID>
  ```
- [ ] Confirm output ZIP exists: `<YOUR_CANDIDATE_ID>_Workbench.zip`.
- [ ] Run the automated verification script:
  ```powershell
  .\tools\verify_package.ps1
  ```
  *Expected outcome:* `PACKAGE VERIFICATION PASSED cleanly!` (0 errors).

### Step 2: Fresh Extract & One-Click Launch (Req A1)
- [ ] Copy `<YOUR_CANDIDATE_ID>_Workbench.zip` to a clean directory (e.g. `C:\Users\<user>\Desktop\TestExtract\`).
- [ ] Extract the ZIP contents completely.
- [ ] Double-click `Launch.exe`.
  - [ ] If Windows SmartScreen displays "Windows protected your PC", click **More info** $\to$ **Run anyway**.
  - [ ] Application window opens promptly without command-line terminals or administrator prompts.
  - [ ] Status bar displays connection state `Connected` and version `v0.1.0`.

### Step 3: Geometry View Inspection (Req B1, B2, B3)
- [ ] Open the **Geometry** view tab.
- [ ] Click **Upload STL** and select `data/synthetic_lbracket.stl`:
  - [ ] C++ validation summary displays bounding box $[-20.0, 20.0]$ and 528 triangles.
  - [ ] 3D surface mesh renders; rotate with left mouse drag, pan with right mouse drag, zoom with scroll wheel.
  - [ ] Press 'F' key: camera smoothly frames the mesh.
- [ ] Click **Upload CSV** and select `data/synthetic_temperature.csv`:
  - [ ] Point cloud renders in Inferno colormap (dark purple to bright yellow).
  - [ ] Colormap legend displays range $[20.0, 100.0]\,^\circ\text{C}$.
  - [ ] Hover over nodes: tooltip displays `Node ID`, 3D coordinates, and temperature.
  - [ ] Type `100` into "Go to Node" and press Enter: camera targets node 100.
- [ ] Test error handling: upload any non-STL file; verify actionable error message appears.

### Step 4: Thermal Analysis View (Req C1, C2, C3, A5)
- [ ] Open the **Thermal Analysis** view tab.
- [ ] Click **Run Analysis** with default parameters:
  - [ ] Convergence table populates with $N = 10, 20, 40, 80$.
  - [ ] Observed convergence order is $2.000000$.
  - [ ] Dual Plotly chart displays FEA nodal points matching exact analytical curve.
  - [ ] Log-log convergence chart displays $O(h^2)$ slope.
- [ ] Click **Export Solution CSV**:
  - [ ] File downloads cleanly with complete nodal coordinates and temperatures.
- [ ] Test cancellation: set $N = 100,000$ and click Run, then immediately click **Cancel**.
  - [ ] Job cancels cooperatively; status bar updates to `Job cancelled`.

### Step 5: Performance View & Multicore Benchmark (Req D1–D7)
- [ ] Open the **Performance** view tab.
- [ ] Verify Hardware Information card displays your CPU brand, physical/logical core count, and RAM.
- [ ] Click **Calibrate**:
  - [ ] Probe job runs and displays recommended case count.
- [ ] Click **Run Benchmark** (e.g. 50,000 cases):
  - [ ] Progress bar advances through warm-up and 5 repetitions across 1, 2, and 4 workers.
  - [ ] Results table shows raw repetition times, median $T_p$, speedup, efficiency, and agreement $|dT| \le 10^{-8}\,^\circ\text{C}$.
  - [ ] Speedup and efficiency curves render in Plotly chart.
  - [ ] Click **Export CSV** and **Export JSON** to verify exports.

### Step 6: Hardware View Simulation & Recovery (Req E1–E5)
- [ ] Open the **Hardware** view tab.
- [ ] Confirm prominent `SIMULATED` badge on header and all cards.
- [ ] Click **Connect** on MCU, FPGA, and JTAG cards:
  - [ ] Cards transition from `Disconnected` to `Connecting` to `Connected`.
- [ ] Click **Read Telemetry** on each card:
  - [ ] Simulated telemetry updates with realistic values.
- [ ] Click **Simulate Timeout** on MCU, then click **Read Telemetry**:
  - [ ] State transitions to `Faulted`.
  - [ ] Event timeline records timeout, exponential backoff retries, and recovery to `Connected`.

### Step 7: Learning Demo (Backpropagation) (Req F1–F3)
- [ ] In the Performance view, switch to the **Learning Demo (Backprop)** sub-tab.
- [ ] Confirm model equations and initial parameters ($w=0.5, b=0, v=1, c=0, \text{lr}=0.1$).
- [ ] Inspect gradient check table: verify analytical and numerical gradients agree with absolute error $\le 10^{-5}$.
- [ ] Click **Run Gradient Check & Train**:
  - [ ] Plotly loss curve renders 101 steps showing monotonic loss decrease from 0.840 to 0.005.
  - [ ] Table displays exact parameters for all 101 steps.
  - [ ] Click **Export Training CSV**.

### Step 8: VS Code Editor Task Verification (Req A8)
- [ ] Open the folder in VS Code.
- [ ] Press `Ctrl+Shift+P` $\to$ **Tasks: Run Task** $\to$ select **"Run Workbench"**.
- [ ] Confirm Workbench starts directly from the editor task.

### Step 9: Version Control Evidence & Candidate Sign-Off
- [ ] Run `powershell .\tools\make_git_evidence.ps1`.
- [ ] Confirm `docs/COMMIT_LOG.md` and `docs/repo.bundle` are created.
- [ ] Confirm your Candidate ID and actual effort hours in `docs/OWNERSHIP.md`.
- [ ] Attach your 2-page handwritten derivation to the submission package per Requirement F1.

