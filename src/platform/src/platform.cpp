// platform.cpp -- Hardware telemetry retrieval.
#include "workbench/platform.hpp"
#include <algorithm>
#include <cstring>
#include <thread>
#include <vector>

#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#  include <intrin.h>
#else
#  include <sys/utsname.h>
#  include <unistd.h>
#  include <fstream>
#  include <sstream>
#endif

namespace workbench {

namespace {

#if defined(_WIN32)

std::string get_windows_cpu_brand() {
    int cpuInfo[4] = {0};
    __cpuid(cpuInfo, static_cast<int>(0x80000000));
    const unsigned int nExIds = static_cast<unsigned int>(cpuInfo[0]);

    if (nExIds >= 0x80000004) {
        char brand[49] = {0};
        __cpuid(reinterpret_cast<int*>(brand),      static_cast<int>(0x80000002));
        __cpuid(reinterpret_cast<int*>(brand + 16), static_cast<int>(0x80000003));
        __cpuid(reinterpret_cast<int*>(brand + 32), static_cast<int>(0x80000004));
        brand[48] = '\0';

        // Trim leading spaces
        char* p = brand;
        while (*p == ' ') p++;
        return std::string(p);
    }

    // Registry fallback
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                      "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
                      0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        char buffer[256] = {0};
        DWORD size = sizeof(buffer);
        if (RegQueryValueExA(hKey, "ProcessorNameString", nullptr, nullptr,
                             reinterpret_cast<LPBYTE>(buffer), &size) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            return std::string(buffer);
        }
        RegCloseKey(hKey);
    }
    return "Unknown x86 CPU";
}

void get_windows_core_counts(int& out_physical, int& out_logical) {
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    out_logical = static_cast<int>(sysInfo.dwNumberOfProcessors);

    // Physical cores via GetLogicalProcessorInformationEx
    DWORD len = 0;
    GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, &len);
    if (GetLastError() == ERROR_INSUFFICIENT_BUFFER && len > 0) {
        std::vector<char> buffer(len);
        auto* info = reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(buffer.data());
        if (GetLogicalProcessorInformationEx(RelationProcessorCore, info, &len)) {
            int physCount = 0;
            DWORD offset = 0;
            while (offset < len) {
                auto* current = reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(buffer.data() + offset);
                if (current->Relationship == RelationProcessorCore) {
                    physCount++;
                }
                offset += current->Size;
            }
            if (physCount > 0) {
                out_physical = physCount;
                return;
            }
        }
    }

    // Fallback: assume physical = logical
    out_physical = out_logical;
}

uint64_t get_windows_ram() {
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memInfo)) {
        return static_cast<uint64_t>(memInfo.ullTotalPhys);
    }
    return 0;
}

std::string get_windows_os_version() {
    // Read from registry for accurate consumer OS name (e.g. Windows 11 Pro)
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                      "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
                      0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        char prodName[256] = {0};
        DWORD size = sizeof(prodName);
        std::string osName = "Windows";
        if (RegQueryValueExA(hKey, "ProductName", nullptr, nullptr,
                             reinterpret_cast<LPBYTE>(prodName), &size) == ERROR_SUCCESS) {
            osName = prodName;
        }

        char displayVer[64] = {0};
        size = sizeof(displayVer);
        if (RegQueryValueExA(hKey, "DisplayVersion", nullptr, nullptr,
                             reinterpret_cast<LPBYTE>(displayVer), &size) == ERROR_SUCCESS) {
            osName += " " + std::string(displayVer);
        }

        char buildNum[64] = {0};
        size = sizeof(buildNum);
        if (RegQueryValueExA(hKey, "CurrentBuild", nullptr, nullptr,
                             reinterpret_cast<LPBYTE>(buildNum), &size) == ERROR_SUCCESS) {
            osName += " (Build " + std::string(buildNum) + ")";
        }

        RegCloseKey(hKey);
        return osName;
    }
    return "Windows (unidentified build)";
}

#else

std::string get_posix_cpu_brand() {
    std::ifstream cpuinfo("/proc/cpuinfo");
    if (cpuinfo.is_open()) {
        std::string line;
        while (std::getline(cpuinfo, line)) {
            if (line.rfind("model name", 0) == 0) {
                auto pos = line.find(':');
                if (pos != std::string::npos) {
                    std::string brand = line.substr(pos + 1);
                    while (!brand.empty() && brand[0] == ' ') brand.erase(0, 1);
                    return brand;
                }
            }
        }
    }
    return "Generic POSIX CPU";
}

void get_posix_core_counts(int& out_physical, int& out_logical) {
    long conf = sysconf(_SC_NPROCESSORS_CONF);
    long onln = sysconf(_SC_NPROCESSORS_ONLN);
    out_logical = static_cast<int>(onln > 0 ? onln : (conf > 0 ? conf : 1));

    // Try counting unique core id in /proc/cpuinfo
    std::ifstream cpuinfo("/proc/cpuinfo");
    if (cpuinfo.is_open()) {
        std::string line;
        int core_id_count = 0;
        while (std::getline(cpuinfo, line)) {
            if (line.rfind("cpu cores", 0) == 0) {
                auto pos = line.find(':');
                if (pos != std::string::npos) {
                    out_physical = std::stoi(line.substr(pos + 1));
                    return;
                }
            }
        }
    }
    out_physical = out_logical;
}

uint64_t get_posix_ram() {
    long pages = sysconf(_SC_PHYS_PAGES);
    long page_size = sysconf(_SC_PAGE_SIZE);
    if (pages > 0 && page_size > 0) {
        return static_cast<uint64_t>(pages) * static_cast<uint64_t>(page_size);
    }
    return 0;
}

std::string get_posix_os_version() {
    struct utsname u;
    if (uname(&u) == 0) {
        return std::string(u.sysname) + " " + u.release + " (" + u.machine + ")";
    }
    return "POSIX OS";
}

#endif

}  // namespace

HardwareInfo get_hardware_info() {
    HardwareInfo info;

#if defined(_WIN32)
    info.cpu_brand = get_windows_cpu_brand();
    get_windows_core_counts(info.physical_cores, info.logical_cores);
    info.total_ram_bytes = get_windows_ram();
    info.os_version = get_windows_os_version();
#else
    info.cpu_brand = get_posix_cpu_brand();
    get_posix_core_counts(info.physical_cores, info.logical_cores);
    info.total_ram_bytes = get_posix_ram();
    info.os_version = get_posix_os_version();
#endif

    // Injected compile definitions
#if defined(WB_COMPILER_NAME)
    info.compiler_name = WB_COMPILER_NAME;
#elif defined(_MSC_VER)
    info.compiler_name = "MSVC";
#elif defined(__clang__)
    info.compiler_name = "Clang";
#elif defined(__GNUC__)
    info.compiler_name = "GCC";
#else
    info.compiler_name = "Unknown";
#endif

#if defined(WB_COMPILER_VERSION)
    info.compiler_version = WB_COMPILER_VERSION;
#elif defined(_MSC_FULL_VER)
    info.compiler_version = std::to_string(_MSC_FULL_VER);
#elif defined(__VERSION__)
    info.compiler_version = __VERSION__;
#else
    info.compiler_version = "Unknown";
#endif

#if defined(WB_BUILD_TYPE)
    info.build_type = WB_BUILD_TYPE;
#elif defined(NDEBUG)
    info.build_type = "Release";
#else
    info.build_type = "Debug";
#endif

#if defined(WB_OPTIMIZATION_FLAGS)
    info.optimization_flags = WB_OPTIMIZATION_FLAGS;
#elif defined(_MSC_VER)
    #if defined(NDEBUG)
        info.optimization_flags = "/O2 /Oi /Ot /Oy /GL";
    #else
        info.optimization_flags = "/Od /RTC1";
    #endif
#elif defined(__GNUC__) || defined(__clang__)
    #if defined(NDEBUG)
        info.optimization_flags = "-O3";
    #else
        info.optimization_flags = "-O0 -g";
    #endif
#else
    info.optimization_flags = "Default";
#endif

    info.is_release_build = (info.build_type == "Release" || info.build_type == "RelWithDebInfo");

    return info;
}

}  // namespace workbench

