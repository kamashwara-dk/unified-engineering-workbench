// test_devices.cpp -- Phase 6A device mock tests.
// Uses FakeClock so no real sleep is needed.
#include "catch_amalgamated.hpp"
#include "workbench/devices.hpp"
#include <string>

using namespace workbench;

// ── Discovery ────────────────────────────────────────────────────────────────
TEST_CASE("Discovery lists exactly three devices", "[devices][discovery]") {
    DeviceRegistry reg;
    auto caps = reg.all_capabilities();
    REQUIRE(caps.size() == 3);
    // All marked simulated.
    for (const auto& c : caps) CHECK(c.simulated == true);
    // IDs are distinct.
    CHECK(caps[0].device_id != caps[1].device_id);
    CHECK(caps[1].device_id != caps[2].device_id);
    CHECK(caps[0].device_id != caps[2].device_id);
}

TEST_CASE("Capability reporting: MCU, FPGA, JTAG each have required fields", "[devices][caps]") {
    DeviceRegistry reg;
    auto* mcu  = reg.find("mcu");
    auto* fpga = reg.find("fpga");
    auto* jtag = reg.find("jtag");
    REQUIRE(mcu);  REQUIRE(fpga);  REQUIRE(jtag);

    auto mc = mcu->capabilities();
    CHECK(mc.device_id   == "mcu");
    CHECK(mc.device_type == "mcu");
    CHECK(!mc.board_name.empty());
    CHECK(mc.simulated   == true);
    CHECK(!mc.capabilities.empty());

    auto fc = fpga->capabilities();
    CHECK(fc.device_type == "fpga");
    CHECK(fc.simulated   == true);

    auto jc = jtag->capabilities();
    CHECK(jc.device_type == "jtag_adapter");
    CHECK(jc.simulated   == true);
}

TEST_CASE("Unknown device id returns nullptr", "[devices][discovery]") {
    DeviceRegistry reg;
    CHECK(reg.find("nonexistent") == nullptr);
    CHECK(reg.find("") == nullptr);
}

// ── State machine ────────────────────────────────────────────────────────────
TEST_CASE("Connect/disconnect state machine: happy path", "[devices][state]") {
    auto dev = make_mcu_adapter();
    CHECK(dev->state() == DeviceState::Disconnected);

    auto s1 = dev->connect();
    REQUIRE(s1.ok());
    CHECK(dev->state() == DeviceState::Connected);

    auto s2 = dev->disconnect();
    REQUIRE(s2.ok());
    CHECK(dev->state() == DeviceState::Disconnected);
}

TEST_CASE("Connect while already connected returns error", "[devices][state]") {
    auto dev = make_mcu_adapter();
    REQUIRE(dev->connect().ok());
    auto s = dev->connect();
    CHECK(!s.ok());
    CHECK(s.code == ErrorCode::InvalidArgument);
    CHECK(!s.message.empty());
    CHECK(dev->state() == DeviceState::Connected);  // state unchanged
}

TEST_CASE("Disconnect while disconnected returns error", "[devices][state]") {
    auto dev = make_mcu_adapter();
    auto s = dev->disconnect();
    CHECK(!s.ok());
    CHECK(!s.message.empty());
    CHECK(dev->state() == DeviceState::Disconnected);
}

TEST_CASE("Telemetry on disconnected device returns typed error", "[devices][state]") {
    auto dev = make_mcu_adapter();
    std::string json;
    auto s = dev->read_telemetry_json(json);
    CHECK(!s.ok());
    CHECK(s.code == ErrorCode::InvalidArgument);
    CHECK(!s.message.empty());
    CHECK(json.empty());
}

// ── Telemetry shape ───────────────────────────────────────────────────────────
TEST_CASE("MCU telemetry JSON contains required fields and simulated=true", "[devices][telemetry]") {
    auto dev = make_mcu_adapter();
    REQUIRE(dev->connect().ok());
    std::string json;
    REQUIRE(dev->read_telemetry_json(json).ok());
    CHECK(json.find("\"simulated\":true") != std::string::npos);
    CHECK(json.find("\"supply_voltage_V\"") != std::string::npos);
    CHECK(json.find("\"die_temperature_C\"") != std::string::npos);
    CHECK(json.find("\"device_id\"") != std::string::npos);
}

TEST_CASE("FPGA telemetry JSON contains configuration_done and pll_locked", "[devices][telemetry]") {
    auto dev = make_fpga_adapter();
    REQUIRE(dev->connect().ok());
    std::string json;
    REQUIRE(dev->read_telemetry_json(json).ok());
    CHECK(json.find("\"simulated\":true") != std::string::npos);
    CHECK(json.find("\"configuration_done\"") != std::string::npos);
    CHECK(json.find("\"pll_locked\"") != std::string::npos);
    CHECK(json.find("\"clock_status\"") != std::string::npos);
}

TEST_CASE("JTAG telemetry JSON contains target_vref_V and scan_chain_length", "[devices][telemetry]") {
    auto dev = make_jtag_adapter();
    REQUIRE(dev->connect().ok());
    std::string json;
    REQUIRE(dev->read_telemetry_json(json).ok());
    CHECK(json.find("\"simulated\":true") != std::string::npos);
    CHECK(json.find("\"target_vref_V\"") != std::string::npos);
    CHECK(json.find("\"scan_chain_length\"") != std::string::npos);
    CHECK(json.find("\"clock_freq_hz\"") != std::string::npos);
}

// ── Fault injection and recovery (fake clock) ─────────────────────────────────
TEST_CASE("Timeout then recovery: sequence via fake clock", "[devices][fault]") {
    FakeClock clk;
    RetryPolicy policy{3, std::chrono::milliseconds{50}, 2, std::chrono::milliseconds{500}};
    auto dev = make_mcu_adapter(policy, &clk);

    REQUIRE(dev->connect().ok());
    CHECK(dev->state() == DeviceState::Connected);

    // Inject fault on the next (1st) read.
    dev->inject_timeout_on_nth_read(1);

    // This read triggers fault, then recovers via retry policy.
    std::string json;
    auto st = dev->read_telemetry_json(json);
    REQUIRE(st.ok());  // recovered
    CHECK(dev->state() == DeviceState::Connected);

    // Event log should contain: Connected, timeout, retries, recovered.
    auto events = dev->drain_events();
    REQUIRE(!events.empty());
    bool saw_timeout  = false;
    bool saw_retry    = false;
    bool saw_recovered = false;
    for (const auto& e : events) {
        CHECK(e.simulated == true);
        if (e.description.find("timeout") != std::string::npos)   saw_timeout   = true;
        if (e.description.find("Retry")   != std::string::npos)   saw_retry     = true;
        if (e.description.find("Recovered") != std::string::npos) saw_recovered = true;
    }
    CHECK(saw_timeout);
    CHECK(saw_retry);
    CHECK(saw_recovered);
}

TEST_CASE("All events carry simulated=true", "[devices][fault]") {
    auto dev = make_mcu_adapter();
    REQUIRE(dev->connect().ok());
    REQUIRE(dev->disconnect().ok());
    auto events = dev->drain_events();
    for (const auto& e : events)
        CHECK(e.simulated == true);
}

TEST_CASE("DeviceRegistry drain_all_events collects from all three", "[devices][events]") {
    DeviceRegistry reg;
    REQUIRE(reg.find("mcu") ->connect().ok());
    REQUIRE(reg.find("fpga")->connect().ok());
    REQUIRE(reg.find("jtag")->connect().ok());
    auto events = reg.drain_all_events();
    // Three connects = at least 3 events (Connecting + Connected per device).
    CHECK(events.size() >= 3);
    // After drain, the log is empty.
    auto events2 = reg.drain_all_events();
    CHECK(events2.empty());
}

// ═══════════════════════════════════════════════════════════════════════════════
// NEGATIVE & BOUNDARY TEST SWEEP (Phase 8 Hardening)
// ═══════════════════════════════════════════════════════════════════════════════

TEST_CASE("Device negative sweep: unrecoverable timeout when max_retries = 0", "[devices][neg]") {
    FakeClock clk;
    RetryPolicy policy{0, std::chrono::milliseconds{10}, 1, std::chrono::milliseconds{10}};
    auto dev = make_mcu_adapter(policy, &clk);

    REQUIRE(dev->connect().ok());
    dev->inject_timeout_on_nth_read(1);

    std::string json;
    auto st = dev->read_telemetry_json(json);
    REQUIRE_FALSE(st.ok());
    CHECK(st.message.find("retries exhausted") != std::string::npos);
}

TEST_CASE("Device negative sweep: illegal state operations on FPGA and JTAG", "[devices][neg]") {
    auto fpga = make_fpga_adapter();
    CHECK_FALSE(fpga->disconnect().ok());
    std::string json;
    CHECK_FALSE(fpga->read_telemetry_json(json).ok());

    auto jtag = make_jtag_adapter();
    CHECK_FALSE(jtag->disconnect().ok());
    CHECK_FALSE(jtag->read_telemetry_json(json).ok());
}

TEST_CASE("Device boundary sweep: device_state_str enum mapping", "[devices][boundary]") {
    CHECK(std::string(device_state_str(DeviceState::Disconnected)) == "disconnected");
    CHECK(std::string(device_state_str(DeviceState::Connecting))   == "connecting");
    CHECK(std::string(device_state_str(DeviceState::Connected))    == "connected");
    CHECK(std::string(device_state_str(DeviceState::Faulted))      == "faulted");
}
