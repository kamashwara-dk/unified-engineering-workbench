# workbench_platform: hardware telemetry, compiler flags, and multicore benchmark harness.
# Separate library from workbench_core.
add_library(workbench_platform STATIC
    src/platform.cpp
    src/benchmark.cpp
)
target_include_directories(workbench_platform PUBLIC include)
target_link_libraries(workbench_platform PUBLIC workbench_core)

target_compile_definitions(workbench_platform PRIVATE
    WB_COMPILER_NAME="${CMAKE_CXX_COMPILER_ID}"
    WB_COMPILER_VERSION="${CMAKE_CXX_COMPILER_VERSION}"
    WB_BUILD_TYPE="$<CONFIG>"
)

if(MSVC)
    target_compile_definitions(workbench_platform PRIVATE
        WB_OPTIMIZATION_FLAGS="$<$<CONFIG:Release>:/O2 /Oi /Ot /Oy /GL>$<$<CONFIG:Debug>:/Od /RTC1>"
    )
else()
    target_compile_definitions(workbench_platform PRIVATE
        WB_OPTIMIZATION_FLAGS="$<$<CONFIG:Release>:-O3>$<$<CONFIG:Debug>:-O0 -g>"
    )
endif()

wb_set_warnings(workbench_platform)

