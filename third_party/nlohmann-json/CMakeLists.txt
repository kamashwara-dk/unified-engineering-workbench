# nlohmann/json v3.12.0 — header-only, MIT licence
# Single-header import target; no compile step.
add_library(nlohmann_json INTERFACE)
target_include_directories(nlohmann_json INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")