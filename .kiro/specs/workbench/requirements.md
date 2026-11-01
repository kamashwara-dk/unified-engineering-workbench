# Requirements - Unified Engineering Workbench

Source of truth: the assignment PDF. IDs are stable; reference them in tests and commits.
Open items are marked **[CONFIRM]** and must be resolved before the related phase starts.

## A - Interface, backend, one-click execution
- A1 WHEN the user double-clicks `Launch.exe` from an extracted ZIP on Windows 11 x64, THE SYSTEM SHALL open the working application without terminal, PowerShell, admin rights, package installation, first-run build or internet.
- A2 THE SYSTEM SHALL provide Geometry, Thermal Analysis, Performance and Hardware views in one interface.
- A3 WHEN the user supplies a file or parameters, THE SYSTEM SHALL validate them in C++ and show an actionable error that states what failed, where, and how to fix it.
- A4 WHILE a computation runs, THE SYSTEM SHALL keep the UI responsive and show progress/status.
- A5 WHEN the user cancels a job, THE SYSTEM SHALL stop cooperatively and never write a partial result as complete.
- A6 THE SYSTEM SHALL export results (CSV and PNG plots) via temp file + atomic rename.
- A7 THE SYSTEM SHALL keep the C++ core independent from presentation and transport code.
- A8 THE REPO SHALL include an editor task that starts the packaged application in one action; any extension prerequisite is documented; the launcher needs neither editor nor internet.
- A9 THE REPO SHALL document CMake source-build instructions separately.

## B - STL and ANSYS CSV visualisation
- B1 WHEN an ASCII STL is loaded, THE SYSTEM SHALL parse and validate it in C++ and render triangles with rotate, pan, zoom and fit-to-view.
- B2 WHEN a CSV with header `node_id,x_m,y_m,z_m,temperature_C` is loaded, THE SYSTEM SHALL validate it in C++ and render a coloured 3D point cloud with a °C legend and node inspection.
- B3 IF a file is malformed, contains non-finite values, is binary STL, or is empty, THEN THE SYSTEM SHALL reject it with an explanatory message.
- B4 THE REPO SHALL ship one small STL and one small CSV with provenance, units and schema notes; the CSV is labelled synthetic.
- B5 THE REPO SHALL document tested file-size limits. **[CONFIRM]** target limits (proposal: 50 MB STL, 1 M CSV nodes).

## C - Thermal FEA
- C1 THE SYSTEM SHALL solve -d/dx(kA dT/dx) = qA with two-node linear elements, L=1 m, A=1e-4 m², k=10 W/(m·K), q=1e4 W/m³, T(0)=100 °C, T(L)=20 °C.
- C2 THE SYSTEM SHALL run N = 10, 20, 40, 80, plot T vs x against the exact solution, and export the CSV.
- C3 THE SYSTEM SHALL report max absolute error at element midpoints using FE interpolation, and show mesh convergence.
- C4 THE REPORT SHALL explain assumptions, units, boundary conditions and one debugging insight.

## D - Multicore
- D1 THE SYSTEM SHALL provide serial and parallel batch solves using a bounded worker pool (std::thread or OpenMP).
- D2 THE SYSTEM SHALL generate batches reproducibly from a fixed seed with varied positive k and q and configurable case count and mesh size.
- D3 THE SYSTEM SHALL keep cases independent and results in input order.
- D4 THE SYSTEM SHALL measure compute-only wall time for 1, 2 and up to 4 workers (capped at logical CPUs): warm-up, five repetitions, median, speedup T1/Tp, efficiency.
- D5 THE SYSTEM SHALL report CPU, physical/logical cores, RAM, compiler, optimisation flags and workload, and report launch-to-ready separately.
- D6 THE SYSTEM SHALL verify serial/parallel max |ΔT| ≤ 1e-8 °C.
- D7 IF only one logical CPU exists, THEN THE SYSTEM SHALL exercise the parallel path and disclose that speedup cannot be assessed.

## E - Embedded design and mocks
- E1 THE DOCS SHALL include a labelled block diagram and an interface/pin-voltage table (power, ground, voltage domains, level shifting, connectors, host comms, JTAG signal ownership, device-specific pinout assumptions) for one MCU board, one FPGA board and one JTAG adapter. **[CONFIRM]** board choices (candidate decides; use datasheet values).
- E2 THE DOCS SHALL explain why a JTAG adapter is a debug interface, not an interchangeable compute board.
- E3 THE SYSTEM SHALL provide C++ mock adapters with discovery, capability reporting, connect/disconnect and one telemetry message for all three device types, labelled SIMULATED.
- E4 THE SYSTEM SHALL demonstrate one recoverable failure (timeout then recovery) in the UI.
- E5 THE DOCS SHALL include a safe integration plan and limitations.

## F - Backpropagation
- F1 THE APPENDIX SHALL contain a handwritten derivation (max two pages) for x=1, y=2, h=tanh(wx+b), ŷ=vh+c, J=0.5(ŷ-y)², w=0.5, b=0, v=1, c=0, lr=0.1, with a simultaneous update.
- F2 THE SYSTEM SHALL compute the same gradients in C++ without automatic differentiation and check each by central differences (ε=1e-6, abs error ≤ 1e-5).
- F3 THE SYSTEM SHALL plot loss over 100 further steps with lr=0.1 and explain the result.

## Z - Delivery and evidence
- Z1 ZIP name `CandidateID_Workbench.zip` with Launch.exe, src/, CMakeLists.txt, dependency/version info, editor config, data/ with provenance, tests/, results/, docs/, README, version-control evidence.
- Z2 README states prerequisites, one-click steps, architecture, formats, test/build steps, limitations.
- Z3 Report ≤ 6 pages plus the 2-page appendix; 3-5 min demo video or timestamped screenshot walkthrough with logs.
- Z4 Git bundle or repo with meaningful history plus a readable commit log.
- Z5 AI-use log (tools, prompts, outputs, edits, validation, at least one incorrect suggestion or a challenge test) and an ownership statement. Do not fabricate.
- Z6 Candidate ID and actual effort hours are recorded. **[CONFIRM]** candidate ID.
