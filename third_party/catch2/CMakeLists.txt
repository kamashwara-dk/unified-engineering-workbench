# Vendored Catch2 v3 amalgamated build (Boost Software License 1.0). See ../VERSIONS.md.
add_library(catch2_amalgamated STATIC catch_amalgamated.cpp)
target_include_directories(catch2_amalgamated PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(catch2_amalgamated PUBLIC cxx_std_17)
