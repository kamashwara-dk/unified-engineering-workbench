# workbench_core: pure C++17. No UI, HTTP, Win32, or filesystem-path code.
add_library(workbench_core STATIC
    src/version.cpp
    src/fem1d.cpp
    src/geometry_io.cpp
    src/devices.cpp
    src/batch.cpp
    src/backprop.cpp
)
target_include_directories(workbench_core PUBLIC include)
target_compile_features(workbench_core PUBLIC cxx_std_17)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_libraries(workbench_core PUBLIC m)
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "9.1")
        target_link_libraries(workbench_core PUBLIC stdc++fs)
    endif()
endif()
wb_set_warnings(workbench_core)