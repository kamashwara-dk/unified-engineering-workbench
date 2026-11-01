cmake_minimum_required(VERSION 3.24)
cmake_policy(SET CMP0091 NEW)  # honour CMAKE_MSVC_RUNTIME_LIBRARY

project(Workbench VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Static MSVC runtime: Launch.exe must run on a clean Windows 11 machine without the
# Visual C++ Redistributable installed.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

option(WB_BUILD_TESTS "Build unit tests" ON)
option(WB_WARNINGS_AS_ERRORS "Treat warnings as errors in project code" ON)

# Strict warnings apply to project targets only (never to vendored code).
function(wb_set_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
        if(WB_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
        if(WB_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

add_subdirectory(src/core)
add_subdirectory(src/platform)

if(WB_BUILD_TESTS)
    enable_testing()
    add_subdirectory(third_party/catch2)
    add_subdirectory(tests)
endif()

add_subdirectory(third_party/cpp-httplib)
add_subdirectory(third_party/nlohmann-json)
if(WIN32)
    add_subdirectory(third_party/webview2)
endif()
add_subdirectory(src/server)
if(WIN32)
    add_subdirectory(src/host)
endif()
