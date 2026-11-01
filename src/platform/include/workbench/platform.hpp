#pragma once
// platform.hpp -- Hardware and build environment telemetry.
//
// Part of workbench_platform (separate from pure C++ core).
#include <cstdint>
#include <string>

namespace workbench {

struct HardwareInfo {
    std::string   cpu_brand;
    int           physical_cores      {0};
    int           logical_cores       {0};
    std::uint64_t total_ram_bytes     {0};
    std::string   os_version;
    std::string   compiler_name;
    std::string   compiler_version;
    std::string   build_type;
    std::string   optimization_flags;
    bool          is_release_build    {false};
};

// Queries hardware details from OS APIs and CMake compile definitions.
[[nodiscard]] HardwareInfo get_hardware_info();

}  // namespace workbench

