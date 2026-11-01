#pragma once
// status.hpp -- typed error model for workbench_core.
// No exceptions cross the module boundary. Use Result<T> for all fallible returns.
// CancelToken wraps a raw pointer to an atomic_bool so long functions can check cheaply.
#include <optional>
#include <atomic>
#include <string>
#include <variant>

namespace workbench {

enum class ErrorCode {
    Ok = 0,
    InvalidArgument,   // caller passed bad value (field name in message)
    SingularSystem,    // near-zero pivot in Thomas solver
    Cancelled,         // caller set the cancel flag
    InternalError,     // should not happen
};

struct Status {
    ErrorCode   code    {ErrorCode::Ok};
    std::string message {};

    [[nodiscard]] bool ok() const noexcept { return code == ErrorCode::Ok; }
    static Status success() noexcept { return {ErrorCode::Ok, {}}; }
    static Status invalid(std::string msg) {
        return {ErrorCode::InvalidArgument, std::move(msg)};
    }
    static Status singular(std::string msg) {
        return {ErrorCode::SingularSystem, std::move(msg)};
    }
    static Status cancelled() {
        return {ErrorCode::Cancelled, "operation cancelled"};
    }
    static Status internal(std::string msg) {
        return {ErrorCode::InternalError, std::move(msg)};
    }
};

// Result<T>: either a value or a Status error. Never both.
template <typename T>
struct Result {
    std::variant<T, Status> data;

    // Construct from value (success).
    // NOLINTNEXTLINE(google-explicit-constructor)
    Result(T val)      : data(std::move(val)) {}
    // Construct from error.
    // NOLINTNEXTLINE(google-explicit-constructor)
    Result(Status err) : data(std::move(err)) {}

    [[nodiscard]] bool ok()    const noexcept { return std::holds_alternative<T>(data); }
    [[nodiscard]] const T&      value()  const { return std::get<T>(data); }
    [[nodiscard]] T&             value()        { return std::get<T>(data); }
    [[nodiscard]] const Status& error()  const { return std::get<Status>(data); }
};

// CancelToken: a non-owning, nullable handle to an atomic cancel flag.
// The flag is owned externally (e.g. by the job manager). Passing a default-
// constructed token disables cancellation checks (safe for CLI use).
class CancelToken {
public:
    CancelToken() noexcept = default;
    explicit CancelToken(const std::atomic<bool>* flag) noexcept : flag_(flag) {}

    // Returns true if cancellation has been requested.
    [[nodiscard]] bool cancelled() const noexcept {
        return flag_ && flag_->load(std::memory_order_relaxed);
    }

private:
    const std::atomic<bool>* flag_{nullptr};
};

}  // namespace workbench
