#ifndef SRC_AGENT_RESULTS_MUTEX_HPP_
#define SRC_AGENT_RESULTS_MUTEX_HPP_

#include <leatherman/util/thread.hpp>

#include <map>
#include <string>
#include <memory>
#include <stdexcept>

namespace PXPAgent {

// NOTE(ale): this class does not provide general synchronizaion
// functions; it provides a cache for named mutexes where it is
// assumed that a single actor (e.g. the RequestProcessor instance)
// is allowed to add and remove mutexes, whereas all actors (including
// instances of the Transaction Status request handler) can lock the
// cached mutexes

class ResultsMutex {
  public:
    struct Error : public std::runtime_error {
        explicit Error(std::string const& msg) : std::runtime_error(msg) {}
    };

    static ResultsMutex& Instance() {
        static ResultsMutex instance {};
        return instance;
    }

    using Mutex = leatherman::util::mutex;
    using Mutex_Ptr = std::shared_ptr<leatherman::util::mutex>;
    using Lock = leatherman::util::unique_lock<leatherman::util::mutex>;
    using LockGuard = leatherman::util::lock_guard<leatherman::util::mutex>;

    // Singleton users should lock this class mutex before accessing
    // the cache.
    // Note that the singleton caches shared_ptrs to mutexes; because
    // of that, it is safe to use the mutex pointer returned by get()
    // after releasing the access_mtx lock, even if a concurrent
    // thread is removing such mutex from the cache.
    Mutex access_mtx;

    // Useful for testing
    void reset();

    // Whether the specified transaction mutex exists
    bool exists(std::string const& transaction_id);

    // Throw Error if transaction_id does not exist
    Mutex_Ptr get(std::string const& transaction_id);

    // Throw Error if transaction_id exists
    void add(std::string const& transaction_id);

    // Throw Error if transaction_id does not exist
    void remove(std::string const& transaction_id);

  private:
    // Cache
    std::map<std::string, Mutex_Ptr> mutexes_;

    // Private ctor
    ResultsMutex();
};

}  // namespace PXPAgent

#endif  // SRC_AGENT_RESULTS_MUTEX_HPP_
