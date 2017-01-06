#ifndef SRC_THREAD_CONTAINER_H_
#define SRC_THREAD_CONTAINER_H_

#include <leatherman/util/thread.hpp>

#include <vector>
#include <unordered_map>
#include <memory>   // shared_ptr
#include <atomic>
#include <string>
#include <stdexcept>

namespace PXPAgent {

static const uint32_t THREADS_MONITORING_INTERVAL_MS { 500 };  // [ms]

// Number of stored threads above which we start the monitoring task
static const uint32_t THREADS_THRESHOLD { 10 };

struct ManagedThread {
    /// Thread object
    leatherman::util::thread the_instance;

    /// Flag that indicates whether or not the thread has finished its
    /// execution
    std::shared_ptr<std::atomic<bool>> is_done;
};

/// This container stores named thread objects in a thread-safe way
/// and manages their lifecycle. If the number of stored threads is
/// greater than the specified threshold, the container will delete
/// the instances of those threads that have completed their
/// execution, after detaching them. It is assumed that the completion
/// of a given thread is indicated by the associated atomic flag
/// 'done'.
///
/// The purpose of this class is to manage the lifecycle of threads;
/// in case one or more stored threads are executing by the time
/// the ThreadContainer destructor is called, the std::terminate
/// function will be invoked; the program will then immediately abort.
class ThreadContainer {
  public:
    struct Error : public std::runtime_error {
        explicit Error(std::string const& msg) : std::runtime_error(msg) {}
    };

    uint32_t check_interval;  // [ms]
    uint32_t threads_threshold;  // number of stored thread objects

    ThreadContainer() = delete;
    ThreadContainer(const std::string& name = "",
                    uint32_t _check_interval = THREADS_MONITORING_INTERVAL_MS,
                    uint32_t _threads_threshold = THREADS_THRESHOLD);
    ThreadContainer(const ThreadContainer&) = delete;
    ThreadContainer& operator=(const ThreadContainer&) = delete;
    ~ThreadContainer();

    /// Add the specified thread instance to the container together
    /// with the pointer to the atomic boolean that will tell when
    /// it has completed its execution and a name string.
    /// Throw an Error in case a thread instance with the same name
    /// already exists.
    void add(std::string thread_name,
             leatherman::util::thread task,
             std::shared_ptr<std::atomic<bool>> done);

    /// Return true if a task with the specified name is currently
    /// stored, false otherwise.
    bool find(const std::string& thread_name) const;

    /// Return true if the monitoring thread is actually executing,
    /// false otherwise
    bool isMonitoring() const;

    uint32_t getNumAddedThreads() const;
    uint32_t getNumErasedThreads() const;
    std::vector<std::string> getThreadNames() const;

    void setName(const std::string& name);

  private:
    std::string name_;
    std::unordered_map<std::string, std::shared_ptr<ManagedThread>> threads_;
    std::unique_ptr<leatherman::util::thread> monitoring_thread_ptr_;
    bool destructing_;
    mutable leatherman::util::mutex mutex_;
    leatherman::util::condition_variable cond_var_;
    bool is_monitoring_;
    uint32_t num_added_threads_;
    uint32_t num_erased_threads_;

    // Must hold the lock to call this one
    bool findLocked(const std::string& thread_name) const;

    void monitoringTask_();
};

}  // namespace PXPAgent

#endif  // SRC_THREAD_CONTAINER_H_
