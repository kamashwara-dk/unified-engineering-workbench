# cpp-httplib v0.64.0 — header-only, MIT licence
# Single-header import target; no compile step.
add_library(cpp_httplib INTERFACE)
target_include_directories(cpp_httplib INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")