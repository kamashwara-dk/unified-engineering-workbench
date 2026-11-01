# LIMITS.md -- Measured parse performance

**Machine:** Windows 11 x64, Intel (dev machine)
**Build:** Release, MSVC 19.44 (VS Build Tools 2022 v17.14), CMake 3.31.6
**Method:** Dedicated time_parse.exe (tools/time_parse.cpp) timing parse_stl() / parse_csv()
  on files produced by gen_samples --stress N. Wall time measured with std::chrono::steady_clock.
**Memory:** Peak memory was NOT instrumented (no profiler available). Estimated working
  set = file size (read into std::string) + output arrays (positions: 36 bytes/tri, normals: 12 bytes/tri).
  For 44.4 MiB STL with 327,680 triangles: ~44 MiB input + ~15.7 MiB positions + ~3.7 MiB normals ≈ 63 MiB.

## STL parser (parse_stl)

| File size (MiB) | Triangles | Parse time (ms) | Rate (tri/s)   |
|-----------------|-----------|-----------------|----------------|
| 0.9             |   6,553   | 16              | ~410,000       |
| 8.7             |  65,536   | 172             | ~381,000       |
| 44.4            | 327,680   | 965             | ~340,000       |

Notes:
- Performance scales linearly with triangle count.
- Files > 64 MiB are rejected before parsing with the kMaxFileBytes cap.
- Files > 2,000,000 triangles are rejected mid-parse with the kMaxStlTriangles cap.

## CSV parser (parse_csv)

| File size (MiB) | Nodes     | Parse time (ms) | Rate (nodes/s) |
|-----------------|-----------|-----------------|----------------|
| 0.9             |  21,952   | 43              | ~510,000       |
| 8.9             | 216,000   | 494             | ~437,000       |
| 44.4            | >1,000,000| rejected by cap | -              |

Notes:
- 44.4 MiB CSV was rejected by the kMaxCsvNodes = 1,000,000 cap (correct behaviour).
- CSV parsing is slower than STL per byte because it tracks duplicate node IDs via
  an unordered_set (O(1) amortised but with constant factor).
- Files > 64 MiB are rejected before parsing.

## Caps (confirmed in code and tests)

| Resource           | Cap            | Source constant     | Test               |
|--------------------|----------------|---------------------|--------------------|
| Upload file size   | 64 MiB         | kMaxFileBytes       | geometry_tests     |
| STL triangles      | 2,000,000       | kMaxStlTriangles    | geometry_tests     |
| CSV nodes          | 1,000,000       | kMaxCsvNodes        | geometry_tests     |
| Stored files       | 20 (LRU evict) | kMaxStoredFiles     | -                  |
| Concurrent FEM jobs| 2 (default)    | ServerConfig::max_jobs | job_tests       |

## How to reproduce

From the workspace root (Windows, Release build):

    # Generate stress files
    build\vs2022\src\server\Release\gen_samples.exe --stress 1  --out data\stress
    build\vs2022\src\server\Release\gen_samples.exe --stress 10 --out data\stress
    build\vs2022\src\server\Release\gen_samples.exe --stress 50 --out data\stress

    # Measure parse time
    build\vs2022\src\server\Release\time_parse.exe data\stress\stress_1mib.stl
    build\vs2022\src\server\Release\time_parse.exe data\stress\stress_10mib.csv