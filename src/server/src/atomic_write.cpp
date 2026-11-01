// atomic_write.cpp -- atomic file write via temp + rename.
#include "workbench/atomic_write.hpp"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace workbench {

Status atomic_write_file(const std::string& path, const std::string& content) {
    namespace fs = std::filesystem;
    const fs::path dest = path;
    const fs::path tmp  = fs::path(path + ".tmp");
    {
        std::ofstream ofs(tmp, std::ios::binary | std::ios::trunc);
        if (!ofs) return Status::invalid("atomic_write: cannot open tmp file: " + path + ".tmp");
        ofs.write(content.data(), static_cast<std::streamsize>(content.size()));
        if (!ofs) return Status::invalid("atomic_write: write failed: " + path);
        ofs.flush();
        if (!ofs) return Status::invalid("atomic_write: flush failed: " + path);
    }  // ofs closes here
    std::error_code ec;
    fs::rename(tmp, dest, ec);
    if (ec) {
        fs::remove(tmp, ec);  // clean up temp on failure
        return Status::invalid("atomic_write: rename failed: " + ec.message());
    }
    return Status::success();
}

}  // namespace workbench