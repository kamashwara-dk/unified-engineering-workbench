# workbench_server_lib: cross-platform HTTP server library.
add_library(workbench_server_lib STATIC
    src/server.cpp
    src/token.cpp
    src/job_manager.cpp
    src/atomic_write.cpp
    src/fem_routes.cpp
    src/file_store.cpp
    src/file_routes.cpp
    src/device_routes.cpp
)
target_include_directories(workbench_server_lib PUBLIC include)
target_link_libraries(workbench_server_lib PUBLIC
    workbench_core
    workbench_platform
    cpp_httplib
    nlohmann_json
)
find_package(Threads REQUIRED)
target_link_libraries(workbench_server_lib PUBLIC Threads::Threads)
# OpenSSL not used -- do NOT define CPPHTTPLIB_OPENSSL_SUPPORT.
# std::filesystem on older GCC:
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "9.1")
    target_link_libraries(workbench_server_lib PUBLIC stdc++fs)
endif()
wb_set_warnings(workbench_server_lib)

# Console dev server.
add_executable(workbench_server src/workbench_server_main.cpp)
target_link_libraries(workbench_server PRIVATE workbench_server_lib)
wb_set_warnings(workbench_server)

# CLI tool: fem --out <dir>
add_executable(workbench_cli src/workbench_cli.cpp)
target_link_libraries(workbench_cli PRIVATE workbench_core)
wb_set_warnings(workbench_cli)
# Sample data generator (cross-platform, links only workbench_core).
add_executable(gen_samples "${CMAKE_SOURCE_DIR}/tools/gen_samples.cpp")
target_link_libraries(gen_samples PRIVATE workbench_core)
wb_set_warnings(gen_samples)
# Timing tool for LIMITS.md measurement.
add_executable(time_parse "${CMAKE_SOURCE_DIR}/tools/time_parse.cpp")
target_link_libraries(time_parse PRIVATE workbench_core)
wb_set_warnings(time_parse)