// file_routes.cpp -- HTTP routes for geometry file upload and binary download.
// POST /api/files/stl   -- upload raw ASCII STL body, validate, store, return summary
// POST /api/files/csv   -- upload raw CSV body, validate, store, return summary
// GET  /api/files/{id}/stl.bin   -- binary STL payload (little-endian)
// GET  /api/files/{id}/points.bin -- binary CSV payload (little-endian)
#ifdef _MSC_VER
#  pragma warning(push)
#  pragma warning(disable: 4127 4244 4456 4702 4996)
#endif
#include "httplib.h"
#ifdef _MSC_VER
#  pragma warning(pop)
#endif
#include "json.hpp"
#include "workbench/file_store.hpp"
#include "workbench/geometry_io.hpp"
#include <sstream>
#include <string>

namespace workbench {

void register_file_routes(httplib::Server& svr, FileStore& store) {

    // POST /api/files/stl -- body is raw ASCII STL bytes.
    svr.Post("/api/files/stl", [&store](const httplib::Request& req, httplib::Response& res) {
        auto result = parse_stl(req.body);
        if (!result.ok()) {
            nlohmann::json err;
            err["error"]   = result.error().message;
            err["field"]   = "file";
            res.status = 422;
            res.set_content(err.dump(), "application/json");
            return;
        }
        const StlMesh& mesh = result.value();
        const auto bin = serialise_stl_bin(mesh);

        nlohmann::json summary;
        summary["triangle_count"]    = mesh.triangle_count;
        summary["degenerate_count"]  = mesh.degenerate_count;
        summary["bounds"]["min"] = {mesh.bounds.min_x, mesh.bounds.min_y, mesh.bounds.min_z};
        summary["bounds"]["max"] = {mesh.bounds.max_x, mesh.bounds.max_y, mesh.bounds.max_z};
        summary["note"]             = "STL is unitless; assumed unit is metres";
        if (!mesh.warning.empty()) summary["warning"] = mesh.warning;

        const std::string id = store.store(FileKind::Stl, summary.dump(), bin,
                                            mesh, PointCloud{});
        summary["id"] = id;
        res.status = 201;
        res.set_content(summary.dump(), "application/json");
    });

    // POST /api/files/csv -- body is raw CSV bytes.
    svr.Post("/api/files/csv", [&store](const httplib::Request& req, httplib::Response& res) {
        auto result = parse_csv(req.body);
        if (!result.ok()) {
            nlohmann::json err;
            err["error"]   = result.error().message;
            err["field"]   = "file";
            res.status = 422;
            res.set_content(err.dump(), "application/json");
            return;
        }
        const PointCloud& cloud = result.value();
        const auto bin = serialise_csv_bin(cloud);

        nlohmann::json summary;
        summary["node_count"]        = cloud.node_count;
        summary["min_temp_C"]        = cloud.min_temp_C;
        summary["max_temp_C"]        = cloud.max_temp_C;
        summary["bounds"]["min"]     = {cloud.bounds.min_x, cloud.bounds.min_y, cloud.bounds.min_z};
        summary["bounds"]["max"]     = {cloud.bounds.max_x, cloud.bounds.max_y, cloud.bounds.max_z};
        summary["note"]              = "Coordinates in metres (x_m, y_m, z_m), temperature in Celsius";
        if (!cloud.warning.empty()) summary["warning"] = cloud.warning;

        const std::string id = store.store(FileKind::Csv, summary.dump(), bin,
                                            StlMesh{}, cloud);
        summary["id"] = id;
        res.status = 201;
        res.set_content(summary.dump(), "application/json");
    });

    // GET /api/files/{id}/stl.bin -- binary STL payload.
    svr.Get(R"(/api/files/([a-f0-9]+)/stl\.bin)",
        [&store](const httplib::Request& req, httplib::Response& res) {
            const std::string id = req.matches[1].str();
            auto file = store.get(id);
            if (!file || file->kind != FileKind::Stl) {
                res.status = 404;
                res.set_content(R"({"error":"not found"})", "application/json");
                return;
            }
            res.set_content(file->binary_data, "application/octet-stream");
        });

    // GET /api/files/{id}/points.bin -- binary CSV payload.
    svr.Get(R"(/api/files/([a-f0-9]+)/points\.bin)",
        [&store](const httplib::Request& req, httplib::Response& res) {
            const std::string id = req.matches[1].str();
            auto file = store.get(id);
            if (!file || file->kind != FileKind::Csv) {
                res.status = 404;
                res.set_content(R"({"error":"not found"})", "application/json");
                return;
            }
            res.set_content(file->binary_data, "application/octet-stream");
        });
}

}  // namespace workbench