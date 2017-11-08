#include <pxp-agent/thread_container.hpp>

#include <cpp-pcp-client/util/chrono.hpp>

#include <leatherman/locale/locale.hpp>

#define LEATHERMAN_LOGGING_NAMESPACE "puppetlabs.pxp_agent.thread_container"
#include <leatherman/logging/logging.hpp>

namespace PXPAgent {

namespace pcp_util = PCPClient::Util;
namespace lth_loc  = leatherman::locale;

// Check if the thread has completed; if so, and if it's joinable,
// detach it. Return true if completed, false otherwise.
bool detachIfCompleted(std::shared_ptr<ManagedThread> thread_ptr) {
    if (*(thread_ptr->is_done)) {
        // The execution thread is done

        if (thread_ptr->the_instance.joinable()) {
            // Detach the thread object; it will be then possible to
            // delete it without triggering std::terminate
            LOG_TRACE("Detaching thread {1}", thread_ptr->the_instance.get_id());
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
        pcp_util::lock_guard<pcp_util::mutex> the_lock { mutex_ };
        destructing_ = true;
        cond_var_.notify_one();
    }

    if (monitoring_thread_ptr_ != nullptr && monitoring_thread_ptr_->joinable()) {
        monitoring_thread_ptr_->join();
    }

    // Detach the completed threads
    bool all_detached { true };
    for (auto itr = threads_.begin(); itr != threads_.end(); ++itr) {
        if (!detachIfCompleted(itr->second)) {
            all_detached = false;
        }
    }

    if (!all_detached) {
        // NB: std::terminate will be invoked
        LOG_WARNING("Not all threads stored by the '{1}' ThreadContainer have "
                    "completed; pxp-agent will not terminate gracefully", name_);
    }
}

void ThreadContainer::add(std::string thread_name,
                          pcp_util::thread task,
                          std::shared_ptr<std::atomic<bool>> is_done) {
    LOG_TRACE(lth_loc::format_n(
        // LOCALE: trace
        "Adding thread {1} (named '{2}') to the '{3}' ThreadContainer; added {4} thread so far",
        "Adding thread {1} (named '{2}') to the '{3}' ThreadContainer; added {4} threads so far",
        num_added_threads_, task.get_id(), thread_name,  name_, num_added_threads_));
    pcp_util::lock_guard<pcp_util::mutex> the_lock { mutex_ };

    if (findLocked(thread_name))
        throw Error { lth_loc::translate("thread name is already stored") };

    threads_.insert(
        std::make_pair(
            std::move(thread_name),
            std::shared_ptr<ManagedThread>(
                new ManagedThread { std::move(task), is_done })));

    num_added_threads_++;

    // Start the monitoring thread, if necessary
    if (!is_monitoring_ && threads_.size() > threads_threshold) {
        LOG_DEBUG(lth_loc::format_n(
            // LOCALE: debug
            "{1} thread stored in the '{2}' ThreadContainer; about to start the monitoring thread",
            "{1} threads stored in the '{2}' ThreadContainer; about to start the monitoring thread",
            threads_.size(), threads_.size(), name_));

        if (monitoring_thread_ptr_ != nullptr
                && monitoring_thread_ptr_->joinable()) {
            LOG_DEBUG("Joining the old (idle) monitoring thread instance");
            monitoring_thread_ptr_->join();
        }

        is_monitoring_ = true;
        monitoring_thread_ptr_.reset(
            new pcp_util::thread(&ThreadContainer::monitoringTask_, this));
    }
}

bool ThreadContainer::find(const std::string& thread_name) const {
    pcp_util::lock_guard<pcp_util::mutex> the_lock { mutex_ };
    return findLocked(thread_name);
}

bool ThreadContainer::isMonitoring() const {
    pcp_util::lock_guard<pcp_util::mutex> the_lock { mutex_ };
    return is_monitoring_;
}

uint32_t ThreadContainer::getNumAddedThreads() const {
    pcp_util::lock_guard<pcp_util::mutex> the_lock { mutex_ };
    return num_added_threads_;
}

uint32_t ThreadContainer::getNumErasedThreads() const {
    pcp_util::lock_guard<pcp_util::mutex> the_lock { mutex_ };
    return num_erased_threads_;
}

std::vector<std::string> ThreadContainer::getThreadNames() const {
    pcp_util::lock_guard<pcp_util::mutex> the_lock { mutex_ };
    std::vector<std::string> names;
    for (auto itr = threads_.begin(); itr != threads_.end(); itr++)
        names.push_back(itr->first);
    return names;
}

void ThreadContainer::setName(const std::string& name) {
    pcp_util::lock_guard<pcp_util::mutex> the_lock { mutex_ };
    name_ = name;
}

//
// Private methods
//

bool ThreadContainer::findLocked(const std::string& thread_name) const {
    return threads_.find(thread_name) != threads_.end();
}

void ThreadContainer::monitoringTask_() {
    LOG_DEBUG("Starting monitoring task for the '{1}' ThreadContainer, "
              "with id {2}", name_, pcp_util::this_thread::get_id());

    while (true) {
        pcp_util::unique_lock<pcp_util::mutex> the_lock { mutex_ };
        auto now = pcp_util::chrono::system_clock::now();

        // Wait for thread objects or for the check interval timeout
        cond_var_.wait_until(the_lock,
                             now + pcp_util::chrono::milliseconds(check_interval));

        if (destructing_) {
            // The dtor has been invoked
            return;
        }

        if (threads_.empty()) {
            // Nothing to do
            the_lock.unlock();
            break;
        }

        int num_deletions { 0 };

        for (auto itr = threads_.begin(); itr != threads_.end();) {
            if (detachIfCompleted(itr->second)) {
                LOG_DEBUG("Deleting thread {1} (named '{2}')",
                          itr->second->the_instance.get_id(), itr->first);
                itr = threads_.erase(itr);
                num_deletions++;
            } else {
                ++itr;
            }
        }

        if (num_deletions > 0) {
            num_erased_threads_ += num_deletions;
            LOG_DEBUG("Deleted {1} thread objects that have completed their "
                      "execution; in total, deleted {2} threads so far",
                      num_deletions, num_erased_threads_);
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
