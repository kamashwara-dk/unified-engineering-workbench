#pragma once
// file_store.hpp -- bounded in-memory store for uploaded geometry files.
// Stores the parsed result + raw binary for transport. Max 20 files total.
#include "workbench/geometry_io.hpp"
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace workbench {

inline constexpr int kMaxStoredFiles = 20;

enum class FileKind { Stl, Csv };

struct StoredFile {
    FileKind    kind;
    std::string id;
    std::string summary_json;  // JSON summary returned to the client
    std::string binary_data;   // serialised binary payload for GET .../data
    StlMesh     stl_mesh;      // only valid when kind==Stl
    PointCloud  csv_cloud;     // only valid when kind==Csv
};

// Thread-safe in-memory file store.
class FileStore {
public:
    // Store a parsed file. Returns the ID. Evicts the oldest if at cap.
    std::string store(FileKind kind, std::string summary_json,
                      std::string binary_data,
                      StlMesh stl_mesh, PointCloud csv_cloud);

    // Returns nullptr if not found.
    std::shared_ptr<const StoredFile> get(const std::string& id) const;

private:
    mutable std::mutex mu_;
    std::unordered_map<std::string, std::shared_ptr<StoredFile>> files_;
    std::vector<std::string> order_;  // insertion order for eviction
};

}  // namespace workbench