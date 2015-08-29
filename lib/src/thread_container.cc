#include <pxp-agent/thread_container.hpp>

#include <chrono>
#include <algorithm>  // remove_if
#include <iterator>   // distance

#define LEATHERMAN_LOGGING_NAMESPACE "puppetlabs.pxp_agent.thread_container"
#include <leatherman/logging/logging.hpp>

namespace PXPAgent {

// Check if the thread has completed; if so, and if it's joinable,
// detach it. Return true if completed, false otherwise.
bool detachIfCompleted(std::shared_ptr<ManagedThread> thread_ptr) {
    if (*(thread_ptr->is_done)) {
        // The execution thread is done

        if (thread_ptr->the_instance.joinable()) {
            // Detach the thread object; it will be then possible to
            // delete it without triggering std::terminate
            LOG_TRACE("Detaching thread %1%", thread_ptr->the_instance.get_id());
            thread_ptr->the_instance.detach();
        }

        // The thread is not joinable and can be safely deleted
        return true;
    } else {
        // The thread is still executing and should not be deleted
        return false;
    }
}

//
// ThreadContainer
//

ThreadContainer::ThreadContainer(const std::string& name,
                                 uint32_t _check_interval,
                                 uint32_t _threads_threshold)
        : check_interval { _check_interval },
          threads_threshold { _threads_threshold },
          name_ { name },
          threads_ {},
          monitoring_thread_ptr_ { nullptr },
          destructing_ { false },
          mutex_ {},
          cond_var_ {},
          is_monitoring_ { false },
          num_added_threads_ { 0 },
          num_erased_threads_ { 0 } {
}

ThreadContainer::~ThreadContainer() {
    {
        std::lock_guard<std::mutex> the_lock { mutex_ };
        destructing_ = true;
        cond_var_.notify_one();
    }

    if (monitoring_thread_ptr_ != nullptr && monitoring_thread_ptr_->joinable()) {
        monitoring_thread_ptr_->join();
    }

    // Detach the completed threads
    bool all_detached { true };
    for (auto thread_ptr : threads_) {
        if (!detachIfCompleted(thread_ptr)) {
            all_detached = false;
        }
    }

    if (!all_detached) {
        // NB: std::terminate will be invoked
        LOG_WARNING("Not all threads stored by the '%1%' ThreadContainer have "
                    "completed; pxp-agent will not terminate gracefully", name_);
    }
}

void ThreadContainer::add(std::thread task,
                          std::shared_ptr<std::atomic<bool>> is_done) {
    LOG_TRACE("Adding thread %1% to the '%2%' ThreadContainer; added %3% "
              "threads so far", task.get_id(), name_, num_added_threads_);
    std::lock_guard<std::mutex> the_lock { mutex_ };
    threads_.push_back(std::shared_ptr<ManagedThread> {
        new ManagedThread { std::move(task), is_done } });
    num_added_threads_++;

    // Start the monitoring thread, if necessary
    if (!is_monitoring_ && threads_.size() > threads_threshold) {
        LOG_DEBUG("%1% threads stored in the '%2%' ThreadContainer; about "
                  "to start a the monitoring thread", threads_.size(), name_);

        if (monitoring_thread_ptr_ != nullptr
                && monitoring_thread_ptr_->joinable()) {
            LOG_DEBUG("Joining the old (idle) monitoring thread instance");
            monitoring_thread_ptr_->join();
        }

        is_monitoring_ = true;
        monitoring_thread_ptr_.reset(
            new std::thread(&ThreadContainer::monitoringTask_, this));
    }
}

bool ThreadContainer::isMonitoring() {
    std::lock_guard<std::mutex> the_lock { mutex_ };
    return is_monitoring_;
}

uint32_t ThreadContainer::getNumAddedThreads() {
    std::lock_guard<std::mutex> the_lock { mutex_ };
    return num_added_threads_;
}

uint32_t ThreadContainer::getNumErasedThreads() {
    std::lock_guard<std::mutex> the_lock { mutex_ };
    return num_erased_threads_;
}

void ThreadContainer::setName(const std::string& name) {
    std::lock_guard<std::mutex> the_lock { mutex_ };
    name_ = name;
}

//
// Private methods
//

void ThreadContainer::monitoringTask_() {
    LOG_DEBUG("Starting monitoring task for the '%1%' ThreadContainer, "
              "with id %2%", name_, std::this_thread::get_id());

    while (true) {
        std::unique_lock<std::mutex> the_lock { mutex_ };
        auto now = std::chrono::system_clock::now();

        // Wait for thread objects or for the check interval timeout
        cond_var_.wait_until(the_lock,
                             now + std::chrono::milliseconds(check_interval));

        if (destructing_) {
            // The dtor has been invoked
            return;
        }

        if (threads_.empty()) {
            // Nothing to do
            the_lock.unlock();
            break;
        }

        // Get the range with the pointers of the thread objects
        // that have completed their execution
        auto detached_threads_it = std::remove_if(threads_.begin(),
                                                  threads_.end(),
                                                  detachIfCompleted);

        // ... and delete them; note that we keep the pointers of the
        // thread instances that are still executing to be able to
        // destroy them when exiting
        auto num_deletes = std::distance(detached_threads_it, threads_.end());
        if (num_deletes > 0) {
            LOG_DEBUG("About to delete %1% thread objects that have completed "
                      "their execution; deleted %2% threads so far",
                      num_deletes, num_erased_threads_);
            threads_.erase(detached_threads_it, threads_.end());
            num_erased_threads_ += num_deletes;
        }

        if (threads_.size() < threads_threshold) {
            // Not enough threads stored; don't need to monitor
            LOG_DEBUG("Stopping the monitoring task");
            is_monitoring_ = false;
            return;
        }

        the_lock.unlock();
    }
}

}  // namespace PXPAgent
