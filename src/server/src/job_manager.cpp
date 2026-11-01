// job_manager.cpp -- bounded-concurrency job manager implementation.
#include "workbench/job_manager.hpp"
#include <condition_variable>
#include <deque>
#include <iomanip>
#include <mutex>
#include <random>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <unordered_map>
#include <vector>

namespace workbench {
namespace {

// Generate a random hex job ID (128 bits, 32 chars).
std::string make_job_id() {
    std::random_device rd;
    std::mt19937_64 eng((static_cast<uint64_t>(rd()) << 32) | rd());
    std::uniform_int_distribution<uint64_t> dist;
    std::ostringstream oss;
    oss << std::hex << std::setfill('0')
        << std::setw(16) << dist(eng)
        << std::setw(16) << dist(eng);
    return oss.str();
}

}  // namespace

struct JobEntry {
    std::string           id;
    std::string           kind;
    JobManager::JobFn     fn;
    JobResult             result;
    std::atomic<bool>     cancel_flag{false};
};

struct JobManager::Impl {
    int                                              max_concurrent;
    mutable std::mutex                               mu;
    std::condition_variable                          cv;
    std::unordered_map<std::string,
        std::shared_ptr<JobEntry>>                   jobs;
    std::deque<std::shared_ptr<JobEntry>>            queue;
    int                                              running_count{0};
    bool                                             stop{false};
    std::vector<std::thread>                         workers;

    explicit Impl(int mc) : max_concurrent(mc) {
        // One dispatcher thread picks work from the queue.
        workers.emplace_back([this]() { dispatch_loop(); });
    }

    ~Impl() {
        {
            std::lock_guard<std::mutex> lk(mu);
            stop = true;
        }
        cv.notify_all();
        for (auto& t : workers) if (t.joinable()) t.join();
    }

    void dispatch_loop() {
        while (true) {
            std::shared_ptr<JobEntry> entry;
            {
                std::unique_lock<std::mutex> lk(mu);
                cv.wait(lk, [this]() {
                    return stop || (!queue.empty() && running_count < max_concurrent);
                });
                if (stop && queue.empty()) return;
                if (queue.empty()) continue;
                entry = queue.front(); queue.pop_front();
                ++running_count;
                entry->result.state = JobState::Running;
            }
            // Run the job on a fresh thread so the dispatcher can pick the next one.
            std::thread([this, entry]() mutable {
                CancelToken ct(&entry->cancel_flag);
                auto progress_fn = [&entry, this](float p) {
                    std::lock_guard<std::mutex> lk(mu);
                    entry->result.progress = p;
                };
                try {
                    const std::string json = entry->fn(ct, progress_fn);
                    std::lock_guard<std::mutex> lk(mu);
                    if (entry->cancel_flag.load()) {
                        entry->result.state = JobState::Cancelled;
                    } else {
                        entry->result.state       = JobState::Succeeded;
                        entry->result.result_json = json;
                        entry->result.progress    = 1.0f;
                    }
                } catch (const std::exception& ex) {
                    std::lock_guard<std::mutex> lk(mu);
                    entry->result.state         = JobState::Failed;
                    entry->result.error_message = ex.what();
                } catch (...) {
                    std::lock_guard<std::mutex> lk(mu);
                    entry->result.state         = JobState::Failed;
                    entry->result.error_message = "unknown error";
                }
                {
                    std::lock_guard<std::mutex> lk(mu);
                    --running_count;
                }
                cv.notify_all();
            }).detach();
        }
    }
};

JobManager::JobManager(int max_concurrent)
    : impl_(std::make_unique<Impl>(max_concurrent)) {}

JobManager::~JobManager() = default;

std::string JobManager::submit(std::string kind, JobFn fn) {
    auto entry = std::make_shared<JobEntry>();
    entry->id   = make_job_id();
    entry->kind = std::move(kind);
    entry->fn   = std::move(fn);
    entry->result.state = JobState::Queued;
    {
        std::lock_guard<std::mutex> lk(impl_->mu);
        impl_->jobs[entry->id] = entry;
        impl_->queue.push_back(entry);
    }
    impl_->cv.notify_one();
    return entry->id;
}

std::unique_ptr<JobResult> JobManager::get(const std::string& id) const {
    std::lock_guard<std::mutex> lk(impl_->mu);
    auto it = impl_->jobs.find(id);
    if (it == impl_->jobs.end()) return nullptr;
    return std::make_unique<JobResult>(it->second->result);
}

void JobManager::cancel(const std::string& id) {
    std::lock_guard<std::mutex> lk(impl_->mu);
    auto it = impl_->jobs.find(id);
    if (it == impl_->jobs.end()) return;
    it->second->cancel_flag.store(true);
    // If still queued, mark cancelled immediately.
    auto& r = it->second->result;
    if (r.state == JobState::Queued) r.state = JobState::Cancelled;
}

void JobManager::drain() {
    std::unique_lock<std::mutex> lk(impl_->mu);
    impl_->cv.wait(lk, [this]() {
        return impl_->queue.empty() && impl_->running_count == 0;
    });
}

}  // namespace workbench