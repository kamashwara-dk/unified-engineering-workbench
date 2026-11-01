// devices.cpp -- simulated device adapters for Phase 6.
// ALL STATE AND DATA IS SIMULATED.  No physical hardware is accessed.
#include "workbench/devices.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdexcept>

namespace workbench {

// ── Helpers ───────────────────────────────────────────────────────────────────

const char* device_state_str(DeviceState s) noexcept {
    switch (s) {
    case DeviceState::Disconnected: return "disconnected";
    case DeviceState::Connecting:   return "connecting";
    case DeviceState::Connected:    return "connected";
    case DeviceState::Faulted:      return "faulted";
    }
    return "unknown";
}

static WallClock g_wall_clock;

// ── BaseAdapter -- shared state machine and fault injection ───────────────────
// Derived classes override telemetry_json_impl() and capabilities().
class BaseAdapter : public IDeviceAdapter {
public:
    BaseAdapter(std::string id, RetryPolicy policy, Clock* clock)
        : id_(std::move(id))
        , policy_(policy)
        , clock_(clock ? clock : &g_wall_clock)
    {}

    DeviceState state() const noexcept override {
        std::lock_guard<std::mutex> lk(mu_);
        return state_;
    }

    Status connect() override {
        std::lock_guard<std::mutex> lk(mu_);
        if (state_ == DeviceState::Connected)
            return Status::invalid(id_ + ": already connected");
        if (state_ == DeviceState::Faulted)
            return Status::invalid(id_ + ": device is faulted; call disconnect first");
        push_event("Connecting... (SIMULATED)", DeviceState::Connecting);
        state_ = DeviceState::Connecting;
        // Simulate instant connect (no real hardware).
        state_ = DeviceState::Connected;
        push_event("Connected (SIMULATED)", DeviceState::Connected);
        return Status::success();
    }

    Status disconnect() override {
        std::lock_guard<std::mutex> lk(mu_);
        if (state_ != DeviceState::Connected && state_ != DeviceState::Faulted)
            return Status::invalid(id_ + ": not connected (state: "
                                   + device_state_str(state_) + ")");
        state_ = DeviceState::Disconnected;
        read_counter_ = 0;
        fault_on_nth_ = -1;
        retry_count_  = 0;
        push_event("Disconnected (SIMULATED)", DeviceState::Disconnected);
        return Status::success();
    }

    Status read_telemetry_json(std::string& out) override {
        std::lock_guard<std::mutex> lk(mu_);
        if (state_ != DeviceState::Connected)
            return Status::invalid(id_ + ": cannot read telemetry in state "
                                   + device_state_str(state_));

        ++read_counter_;

        // Fault injection: Nth read triggers a simulated timeout.
        if (fault_on_nth_ > 0 && read_counter_ >= fault_on_nth_) {
            fault_on_nth_ = -1;  // arm once
            state_ = DeviceState::Faulted;
            push_event("Telemetry timeout (SIMULATED) -- device faulted", DeviceState::Faulted);

            // Attempt recovery with bounded backoff.
            auto backoff = policy_.initial_backoff;
            bool recovered = false;
            for (int attempt = 0; attempt < policy_.max_retries; ++attempt) {
                clock_->advance_ms(backoff);
                push_event("Retry " + std::to_string(attempt + 1)
                           + " after " + std::to_string(backoff.count()) + " ms (SIMULATED)",
                           DeviceState::Faulted);
                backoff = std::min(
                    std::chrono::milliseconds(backoff.count() * policy_.backoff_multiplier),
                    policy_.max_backoff);
                // Simulate recovery on the last retry attempt.
                if (attempt == policy_.max_retries - 1) {
                    state_ = DeviceState::Connected;
                    push_event("Recovered after retry (SIMULATED)", DeviceState::Connected);
                    recovered = true;
                }
            }
            if (!recovered) {
                return Status::invalid(id_ + ": telemetry timeout; retries exhausted (SIMULATED)");
            }
            // Fall through to return real telemetry after recovery.
        }

        if (state_ != DeviceState::Connected)
            return Status::invalid(id_ + ": cannot read telemetry after fault (SIMULATED)");

        out = telemetry_json_impl();
        return Status::success();
    }

    void inject_timeout_on_nth_read(int n) noexcept override {
        std::lock_guard<std::mutex> lk(mu_);
        fault_on_nth_ = read_counter_ + n;
    }

    std::vector<DeviceEvent> drain_events() override {
        std::lock_guard<std::mutex> lk(mu_);
        std::vector<DeviceEvent> out;
        out.swap(events_);
        return out;
    }

protected:
    virtual std::string telemetry_json_impl() = 0;

    void push_event(const std::string& desc, DeviceState ns) {
        // Called with mu_ held.
        events_.push_back({id_, desc, ns, true, clock_->now_ms().count()});
    }

    const std::string id_;

private:
    RetryPolicy             policy_;
    Clock*                  clock_;
    mutable std::mutex      mu_;
    DeviceState             state_{DeviceState::Disconnected};
    int                     read_counter_{0};
    int                     fault_on_nth_{-1};
    int                     retry_count_{0};
    std::vector<DeviceEvent> events_;
};
// ── MCU adapter (ESP32-DevKitC, SIMULATED) ────────────────────────────────────
class McuAdapter final : public BaseAdapter {
public:
    McuAdapter(RetryPolicy p, Clock* c) : BaseAdapter("mcu", p, c) {}

    DeviceCapabilities capabilities() const override {
        return {"mcu", "mcu", "ESP32-DevKitC (SIMULATED)",
                {"connect", "disconnect", "telemetry"}, true};
    }

protected:
    std::string telemetry_json_impl() override {
        // Simulated readings -- values drift slowly for realism.
        ++tick_;
        const float v = 3.3f + 0.02f * std::sin(static_cast<float>(tick_) * 0.1f);
        const float t = 35.0f + 5.0f  * std::sin(static_cast<float>(tick_) * 0.07f);
        std::ostringstream j;
        j << "{\"device_id\":\"mcu\","
          << "\"supply_voltage_V\":" << v << ","
          << "\"die_temperature_C\":" << t << ","
          << "\"simulated\":true}";
        return j.str();
    }
private:
    int tick_{0};
};

// ── FPGA adapter (Arty A7-35T, SIMULATED) ────────────────────────────────────
class FpgaAdapter final : public BaseAdapter {
public:
    FpgaAdapter(RetryPolicy p, Clock* c) : BaseAdapter("fpga", p, c) {}

    DeviceCapabilities capabilities() const override {
        return {"fpga", "fpga", "Digilent Arty A7-35T (SIMULATED)",
                {"connect", "disconnect", "telemetry"}, true};
    }

protected:
    std::string telemetry_json_impl() override {
        ++tick_;
        const bool done   = tick_ > 2;   // simulates: done after a few reads
        const bool locked = tick_ > 3;
        std::ostringstream j;
        j << "{\"device_id\":\"fpga\","
          << "\"configuration_done\":" << (done   ? "true" : "false") << ","
          << "\"pll_locked\":"         << (locked ? "true" : "false") << ","
          << "\"clock_status\":\"" << (locked ? "PLL locked (SIMULATED)" : "PLL unlocked (SIMULATED)") << "\","
          << "\"simulated\":true}";
        return j.str();
    }
private:
    int tick_{0};
};

// ── JTAG adapter (Olimex ARM-USB-TINY-H, SIMULATED) ──────────────────────────
class JtagAdapter final : public BaseAdapter {
public:
    JtagAdapter(RetryPolicy p, Clock* c) : BaseAdapter("jtag", p, c) {}

    DeviceCapabilities capabilities() const override {
        return {"jtag", "jtag_adapter", "Olimex ARM-USB-TINY-H (SIMULATED)",
                {"connect", "disconnect", "telemetry"}, true};
    }

protected:
    std::string telemetry_json_impl() override {
        // JTAG adapter senses target VREF; simulated at 3.3V.
        // TCK frequency and scan-chain length are also simulated.
        std::ostringstream j;
        j << "{\"device_id\":\"jtag\","
          << "\"target_vref_V\":3.3,"
          << "\"clock_freq_hz\":1000000,"
          << "\"scan_chain_length\":4,"
          << "\"note\":\"Olimex ARM-USB-TINY-H: debug interface only, no user compute (SIMULATED)\","
          << "\"simulated\":true}";
        return j.str();
    }
};

// ── Factory functions ─────────────────────────────────────────────────────────
std::unique_ptr<IDeviceAdapter> make_mcu_adapter(RetryPolicy p, Clock* c) {
    return std::make_unique<McuAdapter>(p, c);
}
std::unique_ptr<IDeviceAdapter> make_fpga_adapter(RetryPolicy p, Clock* c) {
    return std::make_unique<FpgaAdapter>(p, c);
}
std::unique_ptr<IDeviceAdapter> make_jtag_adapter(RetryPolicy p, Clock* c) {
    return std::make_unique<JtagAdapter>(p, c);
}

// ── DeviceRegistry ────────────────────────────────────────────────────────────
DeviceRegistry::DeviceRegistry()
    : mcu_ (make_mcu_adapter())
    , fpga_(make_fpga_adapter())
    , jtag_(make_jtag_adapter())
{}

IDeviceAdapter* DeviceRegistry::find(const std::string& id) {
    if (id == "mcu")  return mcu_.get();
    if (id == "fpga") return fpga_.get();
    if (id == "jtag") return jtag_.get();
    return nullptr;
}

std::vector<DeviceCapabilities> DeviceRegistry::all_capabilities() const {
    return {mcu_->capabilities(), fpga_->capabilities(), jtag_->capabilities()};
}

std::vector<DeviceEvent> DeviceRegistry::drain_all_events() {
    auto e1 = mcu_->drain_events();
    auto e2 = fpga_->drain_events();
    auto e3 = jtag_->drain_events();
    e1.insert(e1.end(), e2.begin(), e2.end());
    e1.insert(e1.end(), e3.begin(), e3.end());
    return e1;
}

}  // namespace workbench