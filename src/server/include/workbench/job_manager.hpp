#pragma once
// job_manager.hpp -- bounded-concurrency job lifecycle manager.
// States: queued -> running -> succeeded | failed | cancelled
// At most max_concurrent jobs run simultaneously (default 2).
// Cancel is cooperative: the job receives a CancelToken it must check.
#include "workbench/status.hpp"
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace workbench {

enum class JobState { Queued, Running, Succeeded, Failed, Cancelled };

struct JobResult {
    JobState    state   {JobState::Queued};
    float       progress{0.0f};   // 0..1
    std::string message {};
    std::string result_json {};   // non-empty on Succeeded
    std::string error_message {}; // non-empty on Failed
};

class JobManager {
public:
    // A job function receives a CancelToken and a progress callback.
    using ProgressFn = std::function<void(float)>;
    using JobFn      = std::function<std::string(CancelToken, ProgressFn)>;
    // JobFn returns a result JSON string on success; throws std::runtime_error on failure.

    explicit JobManager(int max_concurrent = 2);
    ~JobManager();

    // Submit a job. Returns the new job ID (UUID-like string).
    [[nodiscard]] std::string submit(std::string kind, JobFn fn);

    // Get current state of a job. Returns nullopt if id unknown.
    [[nodiscard]] std::unique_ptr<JobResult> get(const std::string& id) const;

    // Request cancellation. No-op if already done.
    void cancel(const std::string& id);

    // Block until all queued/running jobs complete (for clean shutdown).
    void drain();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace workbench