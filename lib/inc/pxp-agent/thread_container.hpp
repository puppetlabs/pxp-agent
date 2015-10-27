#ifndef SRC_THREAD_CONTAINER_H_
#define SRC_THREAD_CONTAINER_H_

#include <cpp-pcp-client/util/thread.hpp>
#include <pxp-agent/export.h>

#include <vector>
#include <memory>   // shared_ptr
#include <atomic>
#include <string>

namespace PXPAgent {

static const uint32_t THREADS_MONITORING_INTERVAL_MS { 500 };  // [ms]

// Number of stored threads above which we start the monitoring task
static const uint32_t THREADS_THRESHOLD { 10 };

struct ManagedThread {
    /// Thread object
    PCPClient::Util::thread the_instance;

    /// Flag that indicates whether or not the thread has finished its
    /// execution
    std::shared_ptr<std::atomic<bool>> is_done;
};

/// Store thread objects. If the number of stored threads is greater
/// than the specified threshold, the container will delete the
/// instances of those threads that have completed their execution,
/// after detaching them. It is assumed that the completion of a given
/// thread is indicated by the associated atomic flag 'done'.
///
/// The purpose of this class is to manage the lifecycle of threads;
/// in case one or more stored threads are executing by the time
/// the ThreadContainer destructor is called, the std::terminate
/// function will be invoked; the program will then immediately abort.
class LIBPXP_AGENT_EXPORT ThreadContainer {
  public:
    uint32_t check_interval;  // [ms]
    uint32_t threads_threshold;  // number of stored thread objects

    ThreadContainer(const std::string& name = "",
                    uint32_t _check_interval = THREADS_MONITORING_INTERVAL_MS,
                    uint32_t _threads_threshold = THREADS_THRESHOLD);
    ~ThreadContainer();

    /// Add the specified thread instance to the container together
    /// with the pointer to the atomic boolean that will tell when
    /// it has completed its execution
    void add(PCPClient::Util::thread task, std::shared_ptr<std::atomic<bool>> done);

    /// Return true if the monitoring thread is actually executing,
    /// false otherwise
    bool isMonitoring();

    uint32_t getNumAddedThreads();
    uint32_t getNumErasedThreads();

    void setName(const std::string& name);

  private:
    std::string name_;
    std::vector<std::shared_ptr<ManagedThread>> threads_;
    std::unique_ptr<PCPClient::Util::thread> monitoring_thread_ptr_;
    bool destructing_;
    PCPClient::Util::mutex mutex_;
    PCPClient::Util::condition_variable cond_var_;
    bool is_monitoring_;
    uint32_t num_added_threads_;
    uint32_t num_erased_threads_;

    void monitoringTask_();
};

}  // namespace PXPAgent

#endif  // SRC_THREAD_CONTAINER_H_
