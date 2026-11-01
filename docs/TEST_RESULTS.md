# Test & Sanitizer Results (Phase 8 Hardening)

This document records the exact test and sanitizer execution commands, test environments, and outcomes across the C++ core and server components.

---

## 1. Test Environment

* **Host OS**: Windows 11 x64 (Build 26300)
* **Compiler**: Microsoft Visual C++ 19.44.35229.0 (MSVC 2022 v17.14)
* **Build System**: CMake 3.31.6-msvc6, MSBuild 17.14.60
* **Target Architecture**: x86_64
* **Runtime Library**: MultiThreaded (Static CRT `/MT`) for production distributable; MultiThreadedDLL (`/MD`) with AddressSanitizer runtime for ASan analysis

---

## 2. AddressSanitizer (MSVC `/fsanitize=address`)

### Configuration & Build Command
```powershell
# Configure out-of-tree build with MSVC AddressSanitizer
cmake -B Workbench/build/asan -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_MSVC_RUNTIME_LIBRARY="MultiThreadedDLL" `
  -DCMAKE_CXX_FLAGS="/EHsc /Zi /fsanitize=address" Workbench

# Build test suites with ASan instrumentation
cmake --build Workbench/build/asan --config Release `
  --target core_tests fem1d_tests job_tests geometry_tests batch_tests device_tests backprop_tests
```

### Execution Command
```powershell
$env:PATH = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64;" + $env:PATH
ctest --test-dir Workbench/build/asan -C Release -E server_tests --output-on-failure
```

### ASan Results Summary
| Test Target | Test Cases | Assertions | ASan Outcome | Time |
| :--- | :---: | :---: | :---: | :---: |
| **`core_tests`** | 1 | 3 | **0 errors / 0 leaks** | 0.09 s |
| **`fem1d_tests`** | 17 | 67 | **0 errors / 0 leaks** | 0.09 s |
| **`job_tests`** | 6 | 28 | **0 errors / 0 leaks** | 1.36 s |
| **`geometry_tests`** (incl. Fuzz) | 39 | 448 | **0 errors / 0 leaks** | 0.20 s |
| **`batch_tests`** (Parallel pool) | 8 | 331 | **0 errors / 0 leaks** | 0.12 s |
| **`device_tests`** | 13 | 42 | **0 errors / 0 leaks** | 0.09 s |
| **`backprop_tests`** | 7 | 149 | **0 errors / 0 leaks** | 0.09 s |
| **TOTAL** | **91** | **1,068** | **100% Passed** | **2.07 s** |

* **Zero heap-buffer-overflows**
* **Zero stack-buffer-overflows**
* **Zero use-after-free**
* **Zero memory leaks detected**

---

## 3. Linux Sanitizer Protocols (Cross-Platform Reference)

For continuous integration on Linux (GCC 13+ / Clang 17+), the following build configurations are used:

### A. ASan + UBSan (Address & Undefined Behavior Sanitizers)
```bash
cmake -B build/linux-asan -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer -g"
cmake --build build/linux-asan -j$(nproc)
ctest --test-dir build/linux-asan --output-on-failure
```
* **Scope**: Validates all core algorithms (`fem1d`, `geometry_io`, `backprop`), bounds checking, null pointer dereferences, integer overflow, and float invalid conversions.

### B. ThreadSanitizer (TSan) on Multicore Components
```bash
cmake -B build/linux-tsan -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="-fsanitize=thread -fno-omit-frame-pointer -g"
cmake --build build/linux-tsan -j$(nproc)
ctest --test-dir build/linux-tsan -R "batch_tests|job_tests" --output-on-failure
```
* **Scope**: Validates lock-free atomic next-index indexing in `run_parallel()`, `JobManager` job queue mutexes, cooperative cancel token reads, and concurrent `atomic_write` tempfile operations.

---

## 4. Fuzz-Testing & Negative Sweep Findings

1. **Random Byte Fuzzing (`geometry_tests`)**:
   * Evaluated lengths from 0 to 16,384 bytes with fixed seed `0xC001CAFEULL`.
   * Result: Parsers cleanly rejected invalid input sequences without hanging, unhandled exceptions, or out-of-bounds reads.
2. **File Slicing & Truncation Fuzzing (`geometry_tests`)**:
   * Truncated valid ASCII STL files at every byte offset $0 \le i \le \text{len}$. Result: Incomplete tokens and missing keywords returned structured error messages.
   * Truncated valid CSV files at every byte offset $0 \le i \le \text{len}$. Result: Partial lines cleanly handled; completed lines parsed with consistent node IDs.
3. **Boundary Sweep Corrections Made**:
   * Discovered that `run_convergence()` previously accepted empty `n_values` without returning an error. Fixed by adding validation check in `src/core/src/fem1d.cpp`:
     ```cpp
     if (n_values.empty()) return Status::invalid("n_values list cannot be empty");
     ```
   * Discovered `compare_results()` required explicit checks for mismatched solution vector sizes. Verified that `std::invalid_argument` is thrown.

