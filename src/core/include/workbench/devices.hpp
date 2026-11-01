#pragma once
// devices.hpp -- simulated device adapter hierarchy for Phase 6.
//
// ALL behaviour in this module is SIMULATED.  No physical hardware is
// accessed.  Every telemetry payload and state change carries a
// simulated = true field so it can never be mistaken for a real reading.
//
// Three concrete adapters:
//   McuAdapter  -- ESP32-DevKitC mock
//   FpgaAdapter -- Digilent Arty A7-35T mock
//   JtagAdapter -- Olimex ARM-USB-TINY-H mock
//
// Fault injection: call inject_timeout_on_nth_read(n) to make the nth
// subsequent read_telemetry() fail with a simulated timeout.  The adapter
// then enters Faulted and the built-in retry policy drives it back to
// Connected (or exhausts retries and stays Faulted).
//
// Injectable clock: the retry backoff uses a Clock& so unit tests never
// need real wall-clock sleeps.
#include "workbench/status.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace workbench {

// ── Device state ─────────────────────────────────────────────────────────────
enum class DeviceState {
    Disconnected,
    Connecting,
    Connected,
    Faulted,
};
[[nodiscard]] const char* device_state_str(DeviceState s) noexcept;

// ── Event log ─────────────────────────────────────────────────────────────────
struct DeviceEvent {
    std::string device_id;
    std::string description;   // human-readable; always says SIMULATED where relevant
    DeviceState new_state;
    bool        simulated{true};
    int64_t     timestamp_ms{0};
};

// ── Telemetry structs ─────────────────────────────────────────────────────────
// Each carries simulated=true and only the fields appropriate to the device.

struct McuTelemetry {
    float       supply_voltage_V{0.0f};   // simulated supply voltage reading
    float       die_temperature_C{0.0f};  // simulated on-die temperature
    bool        simulated{true};
};

struct FpgaTelemetry {
    bool        configuration_done{false}; // DONE pin state (simulated)
    bool        pll_locked{false};         // clock PLL lock status (simulated)
    std::string clock_status;              // free-text status (simulated)
    bool        simulated{true};
};

struct JtagTelemetry {
    float       target_vref_V{0.0f};   // reference voltage sensed from target (simulated)
    uint32_t    clock_freq_hz{0};      // configured TCK frequency (simulated)
    uint32_t    scan_chain_length{0};  // IR/DR chain length last detected (simulated)
    bool        simulated{true};
};

// ── Device capabilities ───────────────────────────────────────────────────────
#ifdef DeviceCapabilities
#  undef DeviceCapabilities
#endif
struct DeviceCapabilities {
    std::string device_id;
    std::string device_type;    // "mcu" | "fpga" | "jtag_adapter"
    std::string board_name;     // e.g. "ESP32-DevKitC (SIMULATED)"
    std::vector<std::string> capabilities;  // e.g. {"telemetry", "connect", "disconnect"}
    bool        simulated{true};
};

// ── Injectable clock ──────────────────────────────────────────────────────────
// Allows tests to advance time without sleeping.
class Clock {
public:
    virtual ~Clock() = default;
    virtual std::chrono::milliseconds now_ms() const noexcept = 0;
    virtual void advance_ms(std::chrono::milliseconds delta) noexcept = 0;
};

// Real-time clock (used in production path).
class WallClock final : public Clock {
public:
    std::chrono::milliseconds now_ms() const noexcept override {
        using namespace std::chrono;
        return duration_cast<milliseconds>(steady_clock::now().time_since_epoch());
    }
    void advance_ms(std::chrono::milliseconds) noexcept override {} // no-op
};

// Fake clock for tests.
class FakeClock final : public Clock {
public:
    explicit FakeClock(std::chrono::milliseconds start = std::chrono::milliseconds{0})
        : t_(start) {}
    std::chrono::milliseconds now_ms() const noexcept override { return t_; }
    void advance_ms(std::chrono::milliseconds delta) noexcept override { t_ += delta; }
private:
    std::chrono::milliseconds t_;
};

// ── Retry policy ──────────────────────────────────────────────────────────────
struct RetryPolicy {
    int                        max_retries{3};        // 0 = no retry
    std::chrono::milliseconds  initial_backoff{100};  // first wait
    int                        backoff_multiplier{2}; // exponential factor
    std::chrono::milliseconds  max_backoff{2000};     // cap
};

// ── IDeviceAdapter ────────────────────────────────────────────────────────────
// Pure interface.  All implementations simulate hardware.
class IDeviceAdapter {
public:
    virtual ~IDeviceAdapter() = default;

    // Discovery: returns capabilities of this adapter.
    [[nodiscard]] virtual DeviceCapabilities capabilities() const = 0;

    // Returns the current device state.
    [[nodiscard]] virtual DeviceState state() const noexcept = 0;

    // Transition Disconnected -> Connecting -> Connected.
    // Returns error if already connected or if in Faulted.
    [[nodiscard]] virtual Status connect() = 0;

    // Transition Connected -> Disconnected.
    // Returns error if not connected.
    [[nodiscard]] virtual Status disconnect() = 0;

    // Read one telemetry sample.  Returns error if not connected,
    // or a simulated timeout if fault injection is active.
    // On timeout: state -> Faulted; retry policy drives recovery.
    [[nodiscard]] virtual Status read_telemetry_json(std::string& out_json) = 0;

    // Inject a timeout on the Nth subsequent telemetry read (1-based).
    virtual void inject_timeout_on_nth_read(int n) noexcept = 0;

    // Drain and return all accumulated events since last call.
    virtual std::vector<DeviceEvent> drain_events() = 0;
};

// ── Concrete adapters ─────────────────────────────────────────────────────────
// Each is constructed with a RetryPolicy and a Clock reference.
// Defaults use WallClock and a standard RetryPolicy.

std::unique_ptr<IDeviceAdapter> make_mcu_adapter(
    RetryPolicy policy = {}, Clock* clock = nullptr);

std::unique_ptr<IDeviceAdapter> make_fpga_adapter(
    RetryPolicy policy = {}, Clock* clock = nullptr);

std::unique_ptr<IDeviceAdapter> make_jtag_adapter(
    RetryPolicy policy = {}, Clock* clock = nullptr);

// ── Device registry ───────────────────────────────────────────────────────────
// Owns the three adapters; used by the server routes.
class DeviceRegistry {
public:
    DeviceRegistry();

    // Returns pointer to adapter by id ("mcu" | "fpga" | "jtag"), or nullptr.
    [[nodiscard]] IDeviceAdapter* find(const std::string& id);

    // Returns capabilities of all adapters.
    [[nodiscard]] std::vector<DeviceCapabilities> all_capabilities() const;

    // Drain events from all adapters.
    [[nodiscard]] std::vector<DeviceEvent> drain_all_events();

private:
    std::unique_ptr<IDeviceAdapter> mcu_;
    std::unique_ptr<IDeviceAdapter> fpga_;
    std::unique_ptr<IDeviceAdapter> jtag_;
};

}  // namespace workbench
