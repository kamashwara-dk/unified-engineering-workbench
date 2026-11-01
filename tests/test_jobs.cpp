// test_jobs.cpp -- Phase 2 job manager tests.
// Tests: success, failure, cancel, concurrency limit, unique IDs, progress.
#include "catch_amalgamated.hpp"
#include "workbench/job_manager.hpp"
#include <chrono>
#include <set>
#include <stdexcept>
#include <thread>

using namespace workbench;

TEST_CASE("Job success: result available after completion", "[jobs]") {
    JobManager jm(2);
    const std::string id = jm.submit("test", [](CancelToken, JobManager::ProgressFn prog) {
        prog(0.5f);
        return std::string(R"({"ok":true})");
    });
    // Wait up to 2s.
    for (int i = 0; i < 200; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        auto r = jm.get(id);
        REQUIRE(r);
        if (r->state == JobState::Succeeded) {
            CHECK(r->result_json == R"({"ok":true})");
            CHECK(r->progress == 1.0f);
            return;
        }
    }
    FAIL("job did not complete within 2s");
}

TEST_CASE("Job failure: throws -> state is Failed with message", "[jobs]") {
    JobManager jm(2);
    const std::string id = jm.submit("test",
        [](CancelToken, JobManager::ProgressFn) -> std::string {
            throw std::runtime_error("deliberate failure");
        });
    for (int i = 0; i < 200; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        auto r = jm.get(id);
        REQUIRE(r);
        if (r->state == JobState::Failed) {
            CHECK(r->error_message.find("deliberate failure") != std::string::npos);
            CHECK(r->result_json.empty());
            return;
        }
    }
    FAIL("job did not fail within 2s");
}

TEST_CASE("Job cancel: slow job ends in Cancelled with no result", "[jobs]") {
    JobManager jm(2);
    const std::string id = jm.submit("test",
        [](CancelToken cancel, JobManager::ProgressFn) -> std::string {
            for (int i = 0; i < 1000; ++i) {
                if (cancel.cancelled()) {
                    throw std::runtime_error("cancelled by token");
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
            return "{}";
        });
    // Let it start.
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    jm.cancel(id);

    for (int i = 0; i < 300; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        auto r = jm.get(id);
        REQUIRE(r);
        if (r->state == JobState::Cancelled || r->state == JobState::Failed) {
            CHECK(r->result_json.empty());
            return;
        }
    }
    FAIL("job did not cancel within 3s");
}

TEST_CASE("Job concurrency: at most max_concurrent run simultaneously", "[jobs]") {
    const int max = 2;
    JobManager jm(max);
    std::atomic<int> concurrent{0}, peak{0};
    const int total = 5;

    for (int i = 0; i < total; ++i) {
        (void)jm.submit("test", [&](CancelToken, JobManager::ProgressFn) -> std::string {
            const int n = ++concurrent;
            int p = peak.load();
            while (n > p && !peak.compare_exchange_weak(p, n)) {}
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            --concurrent;
            return "{}";
        });
    }
    jm.drain();
    CHECK(peak.load() <= max);
}

TEST_CASE("Job unique IDs: 20 consecutive IDs are distinct", "[jobs]") {
    JobManager jm(4);
    std::set<std::string> ids;
    for (int i = 0; i < 20; ++i) {
        ids.insert(jm.submit("test",
            [](CancelToken, JobManager::ProgressFn) { return std::string("{}"); }));
    }
    jm.drain();
    CHECK(ids.size() == 20);
}

TEST_CASE("Job progress visible while running", "[jobs]") {
    JobManager jm(2);
    std::string id;
    id = jm.submit("test", [](CancelToken, JobManager::ProgressFn prog) {
        prog(0.3f);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        prog(0.7f);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        return std::string("{}");
    });
    // Poll until we catch it mid-run.
    bool saw_partial = false;
    for (int i = 0; i < 100; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        auto r = jm.get(id);
        if (r && r->state == JobState::Running && r->progress > 0.0f && r->progress < 1.0f)
            saw_partial = true;
        if (r && r->state == JobState::Succeeded) break;
    }
    CHECK(saw_partial);
}

TEST_CASE("Job get unknown ID returns null", "[jobs]") {
    JobManager jm(2);
    CHECK(jm.get("deadbeefdeadbeefdeadbeefdeadbeef") == nullptr);
}
