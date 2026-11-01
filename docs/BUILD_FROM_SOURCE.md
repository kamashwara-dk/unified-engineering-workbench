# Building Unified Engineering Workbench from Source

This document contains complete, step-by-step instructions for building the C++ core, server, native host, and web frontend from source code across Windows and Linux environments.

---

## 1. Prerequisites

### Windows 11 x64 (Native Build)
- **Visual Studio 2022** (Community, Professional, or Build Tools) with:
  - "Desktop development with C++" workload
  - MSVC v143 toolset (C++17 or C++20)
  - Windows 10/11 SDK (10.0.22000.0 or later)
- **CMake 3.25+** (included with Visual Studio or standalone)
- **Node.js 20+** and **npm** (required for building the web frontend)

### Linux (Ubuntu 22.04+ / Debian 12+)
- **GCC 12+** or **Clang 15+** (C++17 support)
- **CMake 3.25+**
- **Ninja** or GNU Make
- **Node.js 20+** and **npm**

---

## 2. Directory Structure

Ensure you are located at the root of the repository:
```bash
cd Workbench
```

---

## 3. Building the Frontend Web UI

The frontend is built using Vite, React 18, TypeScript (strict mode), and Tailwind CSS.

```powershell
# Navigate to the UI project
cd src/ui

# Install dependencies strictly from lockfile
npm ci

# Run strict TypeScript type check and production bundle build
npm run build

# (Optional) Run frontend Vitest unit test suite
npm test

# Return to repository root
cd ../..
```
*Artifact:* The production frontend assets are emitted to `src/ui/dist/` (static `index.html`, minified JS/CSS, self-hosted OFL fonts).

---

## 4. Building the C++ Backend (Windows MSVC)

The project includes `CMakePresets.json` with pre-configured build profiles.

### Visual Studio 2022 Preset (`vs2022`)
```powershell
# 1. Configure solution for Visual Studio 2022 x64
cmake --preset vs2022

# 2. Build Release configuration
cmake --build --preset vs2022-release

# 3. Execute all core CTest test suites
ctest --test-dir build/vs2022 -C Release -E server_tests --output-on-failure
```

### Targets Built:
- `build/vs2022/src/host/Release/Launch.exe`: Native Windows launcher embedding WebView2 with loopback token security.
- `build/vs2022/src/server/Release/workbench_server.exe`: Embedded loopback HTTP server.
- `build/vs2022/src/server/Release/workbench_cli.exe`: Standalone CLI utility for batch and backprop runs.
- `build/vs2022/src/core/Release/workbench_core.lib`: Pure C++17 numerical and algorithmic core.
- `build/vs2022/src/platform/Release/workbench_platform.lib`: Hardware topology discovery library.
- `build/vs2022/tests/Release/*.exe`: Catch2 unit test executables.

---

## 5. Building the C++ Backend with AddressSanitizer (MSVC ASan)

To compile with runtime memory safety instrumentation:

```powershell
# Configure out-of-tree ASan build
cmake -B build/asan -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_CXX_FLAGS="/EHsc /Zi /fsanitize=address" `
  -DCMAKE_MSVC_RUNTIME_LIBRARY="MultiThreadedDLL"

# Build all targets in Debug or RelWithDebInfo
cmake --build build/asan --config RelWithDebInfo

# Execute tests with ASan runtime loaded
# (Ensure the MSVC ASan runtime DLLs are on your PATH via vcvars64.bat or VS developer shell)
& "build/asan/tests/RelWithDebInfo/core_tests.exe"
& "build/asan/tests/RelWithDebInfo/fem1d_tests.exe"
& "build/asan/tests/RelWithDebInfo/job_tests.exe"
& "build/asan/tests/RelWithDebInfo/geometry_tests.exe"
& "build/asan/tests/RelWithDebInfo/batch_tests.exe"
& "build/asan/tests/RelWithDebInfo/device_tests.exe"
& "build/asan/tests/RelWithDebInfo/backprop_tests.exe"
```

---

## 6. Building on Linux (GCC / Clang)

On Linux, `workbench_core`, `workbench_platform`, `workbench_server`, and test suites build without Win32 dependencies.

```bash
# Configure Release build with Ninja
cmake -B build/linux -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_STANDARD=17

# Build all targets
ninja -C build/linux

# Run tests
ctest --test-dir build/linux --output-on-failure
```

### Running with Sanitizers on Linux (Clang)
```bash
# ASan + UBSan
cmake -B build/sanitizer -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer"
ninja -C build/sanitizer
ctest --test-dir build/sanitizer --output-on-failure

# ThreadSanitizer (TSan) for Batch / Worker Pool
cmake -B build/tsan -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="-fsanitize=thread -fno-omit-frame-pointer"
ninja -C build/tsan
ctest --test-dir build/tsan -R batch_tests --output-on-failure
```

---

## 7. Packaging the Binary Release

To build the complete self-contained ZIP distribution:
```powershell
.\tools\package.ps1 -CandidateId <CandidateID>
```
To verify the resulting package against all requirements:
```powershell
.\tools\verify_package.ps1
```
