---
inclusion: fileMatch
fileMatchPattern: "**/*.{cpp,hpp,h}"
---
# C++ conventions
- C++17, RAII, no raw owning pointers, `const` by default, `[[nodiscard]]` on result-returning functions.
- Errors at module boundaries are typed results (status enum + message), not exceptions across the HTTP layer.
- Functions do one thing, ~40 lines max. Names state units: `length_m`, `temperature_C`, `conductivity_W_per_mK`.
- Validate inputs at the boundary: finite, positive where required, size-capped.
- Concurrency: bounded worker pool, atomic work index, preallocated per-index result slots, cooperative cancellation via atomic flag.
- Warnings as errors (`/W4 /WX`, `-Wall -Wextra -Wpedantic -Werror`). Format with `.clang-format`.
- Comments explain why, not what. Every public header documents units and preconditions.
