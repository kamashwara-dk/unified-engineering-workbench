#pragma once
// atomic_write.hpp -- write a file atomically: temp file + flush + rename.
// Prevents partial writes visible to readers on crash or cancel.
#include "workbench/status.hpp"
#include <string>
namespace workbench {
// Write content to path atomically. Returns error on any failure.
[[nodiscard]] Status atomic_write_file(const std::string& path,
                                       const std::string& content);
}  // namespace workbench