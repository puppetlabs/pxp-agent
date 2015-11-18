#include <pxp-agent/results_mutex.hpp>

#include <cpp-pcp-client/util/chrono.hpp>

#define LEATHERMAN_LOGGING_NAMESPACE "puppetlabs.pxp_agent.results_mutex"
#include <leatherman/logging/logging.hpp>

namespace PXPAgent {

// Private ctor

ResultsMutex::ResultsMutex()
        : access_mtx {},
          mutexes_ {} {
}

// Public interface

void ResultsMutex::reset() {
    LockGuard a_lck { access_mtx };
    mutexes_.clear();
}

bool ResultsMutex::exists(std::string const& transaction_id) {
    return (mutexes_.find(transaction_id) != mutexes_.end());
}

ResultsMutex::Mutex_Ptr ResultsMutex::get(std::string const& transaction_id) {
    if (!exists(transaction_id)) {
        throw Error { "does not exists" };
    }
    return mutexes_[transaction_id];
}

void ResultsMutex::add(std::string const& transaction_id) {
    LOG_TRACE("Adding transaction id %1%", transaction_id);
    if (exists(transaction_id)) {
        throw Error { "already exists" };
    }
    auto mtx = std::make_shared<Mutex>();
    mutexes_.emplace(transaction_id, mtx);
}

void ResultsMutex::remove(std::string const& transaction_id) {
    LOG_TRACE("Removing transaction id %1%", transaction_id);
    if (!exists(transaction_id)) {
        throw Error { "does not exist" };
    }
    mutexes_.erase(transaction_id);
}


}  // namespace PXPAgent
