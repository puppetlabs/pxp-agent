#include "test/test.hpp"
#include <catch.hpp>

#include <cthun-agent/thread_container.hpp>

#include <memory>
#include <unistd.h>
#include <exception>  // set_terminate
#include <vector>

namespace CthunAgent {

TEST_CASE("ThreadContainer::ThreadContainer", "[utils]") {
    SECTION("can successfully instantiate a container") {
        REQUIRE_NOTHROW(ThreadContainer("TESTING_1_1"));
    }
}

void testTask(std::shared_ptr<std::atomic<bool>> a,
              const uint32_t task_duration_us) {
    usleep(task_duration_us);
    *a = true;
}

void addTasksTo(ThreadContainer& container,
                const uint32_t num_tasks,
                const uint32_t caller_duration_us,
                const uint32_t task_duration_us) {
    uint32_t idx;
    for (idx = 0; idx < num_tasks; idx++) {
        std::shared_ptr<std::atomic<bool>> a { new  std::atomic<bool> { false } };
        auto t = std::thread(testTask, a, task_duration_us);
        container.add(std::move(t), a);
    }

    if (task_duration_us == 0) {
        // Ensure that the spawned thred doesn't outlive the caller
        // when it's suppose to finish immediately (that could happen
        // due to thread processing ovehead for the OS), otherwise a
        // terminate call will abort the tests... See below
        usleep(10000);  // 10 ms
    }

    usleep(caller_duration_us);
}

TEST_CASE("ThreadContainer::add, ~ThreadContainer", "[async]") {
    SECTION("can add and erase a thread that completes immediately") {
        // NB: using a lambda in order to have a block that will
        // trigger the ThreadContainer dtor
        auto f = []{
                    ThreadContainer container { "TESTING_2_1" };
                    addTasksTo(container, 1, 0, 0);
                 };
        REQUIRE_NOTHROW(f());
    }

    SECTION("can add and erase a thread that completes before its caller") {
        auto f = []{
                    ThreadContainer container { "TESTING_2_2" };
                    addTasksTo(container, 1, 100000, 0);
                 };
        REQUIRE_NOTHROW(f());
    }

    SECTION("can add and erase 42 threads that complete before the caller") {
        auto f = []{
                    ThreadContainer container { "TESTING_2_3" };
                    addTasksTo(container, 42, 200000, 100000);
                 };
        REQUIRE_NOTHROW(f());
    }

    SECTION("threds are properly added") {
        ThreadContainer container { "TESTING_2_4" };
        addTasksTo(container, 42, 0, 0);
        REQUIRE(container.getNumAddedThreads() == 42);
    }
}

auto monitoring_interval_us = THREADS_MONITORING_INTERVAL_MS * 1000;

TEST_CASE("ThreadContainer::monitoringTask", "[async]") {
    // NB: tesitng start, stop, and restart of the monitoring thread
    // in a single section to reduce the test duration
    SECTION("the monitoring thread is properly started, stopped, and restarted") {
        uint32_t task_duration_us { 100000 };  // 100 ms
        ThreadContainer container { "TESTING_3_1" };

        addTasksTo(container, 10 * THREADS_THRESHOLD, 0, task_duration_us);
        INFO("should be started");
        REQUIRE(container.isMonitoring());
        REQUIRE(container.getNumAddedThreads() == 10 * THREADS_THRESHOLD);

        // Wait for two monitoring intervals plus the task duration;
        // all threads should be done by then
        usleep(2 * monitoring_interval_us + task_duration_us);
        INFO("should be stopped");
        REQUIRE_FALSE(container.isMonitoring());

        // Add a few more threads to restart the monitoring thread
        addTasksTo(container, THREADS_THRESHOLD + 4, 0, task_duration_us);
        INFO("should be restarted");
        REQUIRE(container.isMonitoring());
        REQUIRE(container.getNumAddedThreads() == (10 + 1) * THREADS_THRESHOLD + 4);

        // Threads can't outlive the caller otherwise std::terminate()
        // will be invoked; sleep for an interval greater than the
        // duration
        usleep(2 * task_duration_us);
    }

    SECTION("the monitoring thread can delete threads") {
        ThreadContainer container { "TESTING_3_2" };
        REQUIRE(container.getNumErasedThreads() == 0);

        addTasksTo(container, THREADS_THRESHOLD + 4, 0, 0);
        REQUIRE(container.getNumAddedThreads() == THREADS_THRESHOLD + 4);

        // Pause, to let the monitoring thread erase
        usleep(2 * monitoring_interval_us);
        REQUIRE_FALSE(container.isMonitoring());

        // NB: we cannot be certain about the number of erased threads
        // because the monitoring thread depends upnon the threshold
        REQUIRE(container.getNumErasedThreads() > 0);

        // NB: no need pause before returning; the threads already
        // completed their execution
    }

    SECTION("can add threds while the monitoring thread is running") {
        uint32_t task_duration_us { 100000 };
        ThreadContainer container { "TESTING_3_3" };

        addTasksTo(container, THREADS_THRESHOLD + 4, 0, task_duration_us);
        REQUIRE(container.isMonitoring());

        REQUIRE_NOTHROW(addTasksTo(container, 10, 0, 0));
        REQUIRE(container.getNumAddedThreads() == THREADS_THRESHOLD + 4 + 10);
        usleep(2 * task_duration_us);
    }
}

}  // namespace CthunAgent
