// device_routes.cpp -- HTTP routes for simulated device adapters.
// GET  /api/devices                       -- list all three with capabilities
// POST /api/devices/{id}/connect          -- connect
// POST /api/devices/{id}/disconnect       -- disconnect
// POST /api/devices/{id}/telemetry        -- read one telemetry sample
// POST /api/devices/{id}/inject-timeout   -- simulation control
// GET  /api/devices/events                -- drain event log from all adapters
//
// Every JSON response includes "simulated": true.
#ifdef _MSC_VER
#  pragma warning(push)
#  pragma warning(disable: 4127 4244 4456 4702 4996)
#endif
#include "httplib.h"
#ifdef DeviceCapabilities
#  undef DeviceCapabilities
#endif
#ifdef _MSC_VER
#  pragma warning(pop)
#endif
#include "json.hpp"
#include "workbench/device_routes.hpp"

namespace workbench {
namespace {

nlohmann::json caps_to_json(const DeviceCapabilities& c) {
    nlohmann::json j;
    j["device_id"]   = c.device_id;
    j["device_type"] = c.device_type;
    j["board_name"]  = c.board_name;
    j["capabilities"] = c.capabilities;
    j["state"]       = "unknown";  // caller fills in
    j["simulated"]   = true;
    return j;
}

nlohmann::json event_to_json(const DeviceEvent& e) {
    nlohmann::json j;
    j["device_id"]    = e.device_id;
    j["description"]  = e.description;
    j["new_state"]    = device_state_str(e.new_state);
    j["simulated"]    = e.simulated;
    j["timestamp_ms"] = e.timestamp_ms;
    return j;
}

}  // namespace

void register_device_routes(httplib::Server& svr, DeviceRegistry& reg) {

    // GET /api/devices
    svr.Get("/api/devices", [&reg](const httplib::Request&, httplib::Response& res) {
        auto caps = reg.all_capabilities();
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& c : caps) {
            auto j = caps_to_json(c);
            auto* dev = const_cast<DeviceRegistry&>(reg).find(c.device_id);
            if (dev) j["state"] = device_state_str(dev->state());
            arr.push_back(std::move(j));
        }
        nlohmann::json resp;
        resp["devices"]   = std::move(arr);
        resp["simulated"] = true;
        res.set_content(resp.dump(), "application/json");
    });

    // POST /api/devices/{id}/connect
    svr.Post(R"(/api/devices/(\w+)/connect)",
        [&reg](const httplib::Request& req, httplib::Response& res) {
            const std::string id = req.matches[1].str();
            auto* dev = reg.find(id);
            if (!dev) { res.status = 404;
                res.set_content("{\"error\":\"device not found\",\"simulated\":true}", "application/json");
                return; }
            auto st = dev->connect();
            nlohmann::json j;
            j["device_id"] = id;
            j["simulated"] = true;
            if (st.ok()) { j["state"] = device_state_str(dev->state()); res.status = 200; }
            else         { j["error"] = st.message; res.status = 409; }
            res.set_content(j.dump(), "application/json");
        });

    // POST /api/devices/{id}/disconnect
    svr.Post(R"(/api/devices/(\w+)/disconnect)",
        [&reg](const httplib::Request& req, httplib::Response& res) {
            const std::string id = req.matches[1].str();
            auto* dev = reg.find(id);
            if (!dev) { res.status = 404;
                res.set_content("{\"error\":\"device not found\",\"simulated\":true}", "application/json");
                return; }
            auto st = dev->disconnect();
            nlohmann::json j;
            j["device_id"] = id;
            j["simulated"] = true;
            if (st.ok()) { j["state"] = device_state_str(dev->state()); res.status = 200; }
            else         { j["error"] = st.message; res.status = 409; }
            res.set_content(j.dump(), "application/json");
        });

    // POST /api/devices/{id}/telemetry
    svr.Post(R"(/api/devices/(\w+)/telemetry)",
        [&reg](const httplib::Request& req, httplib::Response& res) {
            const std::string id = req.matches[1].str();
            auto* dev = reg.find(id);
            if (!dev) { res.status = 404;
                res.set_content("{\"error\":\"device not found\",\"simulated\":true}", "application/json");
                return; }
            std::string telemetry_json;
            auto st = dev->read_telemetry_json(telemetry_json);
            if (st.ok()) {
                // telemetry_json is already a JSON object string; wrap it.
                auto inner = nlohmann::json::parse(telemetry_json);
                res.set_content(inner.dump(), "application/json");
            } else {
                nlohmann::json j;
                j["error"]     = st.message;
                j["device_id"] = id;
                j["simulated"] = true;
                res.status = 409;
                res.set_content(j.dump(), "application/json");
            }
        });

    // POST /api/devices/{id}/inject-timeout  (simulation control)
    svr.Post(R"(/api/devices/(\w+)/inject-timeout)",
        [&reg](const httplib::Request& req, httplib::Response& res) {
            const std::string id = req.matches[1].str();
            auto* dev = reg.find(id);
            if (!dev) { res.status = 404;
                res.set_content("{\"error\":\"device not found\",\"simulated\":true}", "application/json");
                return; }
            int n = 1;
            try {
                auto body = nlohmann::json::parse(req.body);
                if (body.contains("on_nth_read")) n = body["on_nth_read"].get<int>();
            } catch (...) {}
            dev->inject_timeout_on_nth_read(n);
            nlohmann::json j;
            j["device_id"]       = id;
            j["inject_on_nth"]   = n;
            j["simulated"]       = true;
            j["note"]            = "Timeout will be simulated on the Nth subsequent telemetry read";
            res.set_content(j.dump(), "application/json");
        });

    // GET /api/devices/events
    svr.Get("/api/devices/events",
        [&reg](const httplib::Request&, httplib::Response& res) {
            auto events = reg.drain_all_events();
            nlohmann::json arr = nlohmann::json::array();
            for (const auto& e : events) arr.push_back(event_to_json(e));
            nlohmann::json resp;
            resp["events"]    = std::move(arr);
            resp["simulated"] = true;
            res.set_content(resp.dump(), "application/json");
        });
}

}  // namespace workbench