// file_store.cpp -- in-memory geometry file store.
#include "workbench/file_store.hpp"
#include "workbench/token.hpp"  // generate_token for IDs
#include <algorithm>

namespace workbench {

std::string FileStore::store(FileKind kind, std::string summary_json,
                              std::string binary_data,
                              StlMesh stl_mesh, PointCloud csv_cloud) {
    auto entry        = std::make_shared<StoredFile>();
    entry->id         = generate_token(); // random 32-char hex ID
    entry->kind       = kind;
    entry->summary_json = std::move(summary_json);
    entry->binary_data  = std::move(binary_data);
    entry->stl_mesh     = std::move(stl_mesh);
    entry->csv_cloud    = std::move(csv_cloud);

    std::lock_guard<std::mutex> lk(mu_);
    // Evict oldest if at cap.
    while (static_cast<int>(order_.size()) >= kMaxStoredFiles && !order_.empty()) {
        files_.erase(order_.front());
        order_.erase(order_.begin());
    }
    files_[entry->id] = entry;
    order_.push_back(entry->id);
    return entry->id;
}

std::shared_ptr<const StoredFile> FileStore::get(const std::string& id) const {
    std::lock_guard<std::mutex> lk(mu_);
    auto it = files_.find(id);
    return it == files_.end() ? nullptr : it->second;
}

}  // namespace workbench