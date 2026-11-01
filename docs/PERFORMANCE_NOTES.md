# Performance Notes & Benchmark Protocol

This document details the benchmarking protocol, thread pool architecture, workload calibration, honest hardware scaling limitations, and measured results for the Unified Engineering Workbench 1-D FEA batch solver (Phase 5A).

---

## 1. Benchmarking Protocol

The benchmark harness assesses the parallel scaling of the 1-D finite element thermal solver across worker counts ($p \in \{1, 2, 4\}$, capped at logical CPU core count).

### Execution Rules & Timing Boundaries
- **Generation Excluded:** All benchmark problem cases are generated in advance using `generate_cases()` before timing commences. Case generation time is strictly excluded from compute metrics.
- **Reproducible Case Generation:** A deterministic pseudo-random number generator (`std::mt19937_64`) seeds each case $i$ independently using SplitMix64 (`spec.seed ^ splitmix64(i)`) and a canonical bits-to-double mapping $((x \gg 11) \times 2^{-53})$. This guarantees that generated test cases are bitwise identical across compilers (MSVC, GCC, Clang) and completely independent of worker scheduling order.
- **Warm-Up Run:** Exactly 1 warm-up iteration is executed for each worker count prior to measurement to prime the OS thread scheduler, CPU instruction/data caches, and memory allocators.
- **Repetitions:** Exactly 5 measured repetitions are executed per worker count.
- **Compute Timing:** Wall-clock time is captured using `std::chrono::steady_clock` tightly bounding the solver computation. Thread creation and thread join overheads are **included** in the measured duration to reflect end-to-end task latency. All file I/O, network transport, UI rendering, and compilation overheads are strictly excluded.
- **Metrics Computed:**
  - Raw times: $t_1, t_2, t_3, t_4, t_5$ (milliseconds)
  - Extrema: $t_{\min}, t_{\max}$
  - Representative metric: Median time $T_p = \operatorname{median}(t_1, \dots, t_5)$
  - Speedup: $S_p = T_1 / T_p$
  - Parallel efficiency: $E_p = S_p / p = (T_1 / T_p) / p$
- **Numerical Verification:** Every worker run verifies that its nodal temperature output matches the serial baseline within $|T_{\text{parallel}} - T_{\text{serial}}| \le 10^{-8}\,^\circ\text{C}$ across all nodes of all cases (exact equality $0.0$ is observed).
- **Single-Core Architecture Handling:** If running on a machine with exactly 1 logical CPU core, the harness caps evaluated workers to 1, then deliberately exercises the parallel multi-threaded pipeline with 2 workers to verify correctness and safety. The resulting speedup is flagged as `"speedup not assessable"`.
- **Launch-to-Ready Metric:** The server records the wall-clock duration from initial process entry (`main`) to when the HTTP listener successfully binds and enters the event loop. This metric is logged and reported separately from compute benchmarks.
- **Atomic Exports:** Benchmark results are formatted and written atomically to disk in both CSV and JSON formats upon completion.

---

## 2. Workload Calibration

To prevent timer quantization artifacts (where sub-millisecond durations skew speedup ratios), the harness provides an advisory calibration helper (`calibrate_workload`):
- Runs a small probe workload (e.g. 50 cases) on a single thread.
- Estimates the time per single-rod FEA solve.
- Recommends an optimal `case_count` targeting approximately $\ge 500\text{ ms}$ on 1 worker (typically 100,000 to 300,000 cases depending on CPU frequency and mesh density).
- Calibration output is advisory; the actual workload used is recorded permanently in the benchmark report metadata.

---

## 3. Honest Engineering Limitations

When interpreting parallel benchmark numbers on modern x86-64 hardware, several physical and architectural realities prevent ideal linear speedup ($S_p = p$):

### 1. Thread Creation and Join Latency
The benchmark harness instantiates a bounded pool of `std::thread` workers per repetition to isolate runs. On Windows, thread creation, context allocation, and teardown via the NT kernel require several hundred microseconds. For tiny batch sizes, thread lifecycle overhead dominates the compute time.

### 2. Tiny Problem Size vs. Cache Locality
A single 1-D finite element solve with $N \in [10, 80]$ elements involves assembling a tridiagonal matrix of size $(N+1)$ and solving it via the Thomas algorithm (Gaussian elimination for tridiagonal systems).
- The working set of a single solve is only a few kilobytes and fits entirely inside the L1 data cache (typically 32 KiB).
- The Thomas algorithm executes in $O(N)$ operations with minimal arithmetic intensity.
- Because the solve is so fast, workers consume cases rapidly, shifting pressure to atomic index retrieval (`std::atomic<size_t>::fetch_add`) and thread synchronization.

### 3. Memory Bus Bandwidth Saturation
When 4 or more threads simultaneously execute solves, they preallocate and write nodal solution arrays into shared memory. When batch sizes grow into hundreds of thousands of cases, the aggregate memory traffic can saturate L3 cache interconnects and DRAM channels, leading to memory bus contention.

### 4. Hyper-Threading (SMT) vs. Physical Cores
Simultaneous Multithreading (SMT on AMD, Hyper-Threading on Intel) presents two logical hardware threads per physical core. However, both logical threads share the same execution units (ALUs, vector units, branch predictors) and L1/L2 caches:
- Compute-bound workloads that saturate the integer or floating-point pipeline do not achieve $2\times$ throughput on a hyper-threaded sibling core.
- Typical SMT scaling yields 20% to 35% additional throughput, not 100%.

### 5. Thermal Throttling & Dynamic CPU Boosting
Modern processors dynamically adjust core clock frequencies based on power (TDP) and temperature:
- When a single thread is active ($p=1$), the processor can sustain its maximum single-core boost clock (e.g., 4.4 GHz).
- When all cores are fully loaded ($p=4$), thermal and power limits force the processor to reduce all-core clock speeds (e.g., to 3.6 GHz).
- Consequently, $T_1$ benefits from higher clock frequencies than $T_4$, artificially compressing the observed speedup.

### 6. OS Scheduler Jitter & Background Noise
Desktop operating systems (Windows / macOS / non-realtime Linux) run background services, audio engines, network stacks, and GUI compositors. Even with high thread priority, periodic OS context switches introduce noise into individual run repetitions. Taking the median of 5 repetitions effectively discards outlier samples caused by scheduler preemption.

---

## 4. How to Reproduce

### Building in Release Mode
Benchmarking must **always** be performed in a `Release` build with compiler optimizations enabled (`/O2` on MSVC, `-O3` on GCC/Clang). Non-release builds will emit a warning in the report metadata.

```powershell
# Configure Release build
cmake -B build/vs2022 -S . -DCMAKE_BUILD_TYPE=Release

# Build workbench_server and batch_tests
cmake --build build/vs2022 --config Release --target workbench_server batch_tests
```

### Running Automated Batch Tests
```powershell
ctest --test-dir build/vs2022 -C Release -R batch_tests --output-on-failure
```

### Running via Workbench Server API
Start the server:
```powershell
build/vs2022/src/server/Release/workbench_server.exe --port 8080 --dev
```

Submit a calibration request:
```bash
curl -X POST http://localhost:8080/api/jobs \
  -H "Content-Type: application/json" \
  -d '{"kind":"calibrate","params":{"seed":42,"n_elements":10}}'
```

Submit a benchmark job with the recommended case count:
```bash
curl -X POST http://localhost:8080/api/jobs \
  -H "Content-Type: application/json" \
  -d '{"kind":"benchmark","params":{"seed":42,"case_count":200000,"n_elements":10}}'
```

Download benchmark artifacts:
```bash
curl -o benchmark.csv http://localhost:8080/api/jobs/<job-id>/export/benchmark.csv
curl -o benchmark.json http://localhost:8080/api/jobs/<job-id>/export/benchmark.json
```

---

## 5. My Measured Results

### Hardware & Environment
- **CPU:** AMD Ryzen 7 5800H with Radeon Graphics
- **Physical Cores:** 8
- **Logical Cores:** 16
- **Total RAM:** 16,472,936,448 bytes (15.34 GiB)
- **OS:** Windows 10 Home 26H2 (Build 26300)
- **Compiler:** MSVC 19.44.35229.0
- **Build Type & Flags:** Release (`/O2 /Oi /Ot /Oy /GL`)
- **Launch-to-ready time:** Process entry to listener ready: recorded via host shell (`Launch.exe`)

### Workload Parameters
- **Seed:** 1337
- **Case count:** 50,000
- **Elements per rod ($N$):** 10
- **Nodes per rod:** 11
- **Case generation time:** 78.51 ms (excluded from compute timing)
- **Warmup time:** 42.59 ms

### Scaling Measurements

| Worker Count ($p$) | Rep 1 (ms) | Rep 2 (ms) | Rep 3 (ms) | Rep 4 (ms) | Rep 5 (ms) | Median $T_p$ (ms) | Min (ms) | Max (ms) | Speedup ($T_1/T_p$) | Efficiency ($E_p$) | Max $\Delta T$ vs Serial (°C) |
|:------------------:|:----------:|:----------:|:----------:|:----------:|:----------:|:-----------------:|:--------:|:--------:|:-------------------:|:------------------:|:-----------------------------:|
| 1                  | 42.56      | 42.17      | 41.93      | 42.12      | 41.24      | 42.12             | 41.24    | 42.56    | 1.00x               | 100.0%             | 0.000000e+00                  |
| 2                  | 26.41      | 25.81      | 25.15      | 25.18      | 39.69      | 25.81             | 25.15    | 39.69    | 1.63x               | 81.6%              | 0.000000e+00                  |
| 4                  | 16.19      | 15.21      | 14.69      | 14.11      | 14.14      | 14.69             | 14.11    | 16.19    | 2.87x               | 71.7%              | 0.000000e+00                  |

### Observations & Analysis
- **Scaling Behavior:** Moving from 1 to 2 workers reduces median wall time from 42.12 ms to 25.81 ms ($1.63\times$ speedup, 81.6% efficiency). Moving to 4 workers further decreases median wall time to 14.69 ms ($2.87\times$ speedup, 71.7% efficiency).
- **Overhead Drivers:** 
  1. *Sub-millisecond thread coordination:* Each rod solve takes only $\approx 840\text{ ns}$ (50,000 cases in 42 ms). At this extreme solve velocity, worker threads query the shared atomic next-index (`fetch_add`) continuously, incurring memory bus coherency traffic across L3 cache slices.
  2. *Thread lifecycle:* `run_parallel()` instantiates and joins a fresh `std::thread` worker pool per iteration, adding NT kernel context creation and teardown latency into the measured duration.
  3. *Zero numerical divergence:* The maximum observed $|\Delta T|$ across all 50,000 cases and 550,000 nodes was exactly $0.000000\times 10^0\ ^\circ\text{C}$, confirming bitwise equivalence between parallel and serial execution paths.

