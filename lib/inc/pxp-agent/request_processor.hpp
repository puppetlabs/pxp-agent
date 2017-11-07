#ifndef SRC_AGENT_REQUEST_PROCESSOR_HPP_
#define SRC_AGENT_REQUEST_PROCESSOR_HPP_

#include <pxp-agent/module.hpp>
#include <pxp-agent/thread_container.hpp>
#include <pxp-agent/action_request.hpp>
#include <pxp-agent/pxp_connector.hpp>
#include <pxp-agent/configuration.hpp>
#include <pxp-agent/results_storage.hpp>

#include <cpp-pcp-client/util/thread.hpp>

#include <boost/filesystem/path.hpp>

#include <memory>
#include <string>
#include <vector>

namespace PXPAgent {

class RequestProcessor {
  public:
    struct Error : public std::runtime_error {
        explicit Error(std::string const& msg) : std::runtime_error(msg) {}
    };

    RequestProcessor() = delete;
    RequestProcessor(const RequestProcessor&) = delete;
    RequestProcessor& operator=(const RequestProcessor&) = delete;

    RequestProcessor(std::shared_ptr<PXPConnector> connector_ptr,
                     const Configuration::Agent& agent_configuration);

    ~RequestProcessor();

    /// Execute the specified action.
    ///
    /// In case of blocking action, once it's done, send back to the
    /// requester a blocking response containing the action results.
    /// Propagates possible request errors raised by the action logic.
    /// In case it fails to send the response, no further attempt will
    /// be made.
    ///
    /// In case of non-blocking action, start a task for the specified
    /// action in a separate execution thread.
    /// Once the task has started, send a provisional response to the
    /// requester. In case the request has the notify_outcome field
    /// flagged, the task will send a non-blocking response
    /// containing the action outcome, after the action is done. The
    /// task will also write the action outcome and request metadata
    /// to disk.
    void processRequest(const RequestType& request_type,
                        const PCPClient::ParsedChunks& parsed_chunks);

    /// Whether the specified module was loaded
    bool hasModule(const std::string& module_name) const;

    /// Whether the specified module was loaded together with a valid
    /// configuration from file
    bool hasModuleConfig(const std::string& module_name) const;

    /// Throw an Error in case no configuration was loaded for the
    /// specified module
    std::string getModuleConfig(const std::string& module_name) const;

  private:
    /// Manages the lifecycle of non-blocking action jobs
    ThreadContainer thread_container_;

    PCPClient::Util::mutex thread_container_mutex_;

    /// PXP Connector pointer
    std::shared_ptr<PXPConnector> connector_ptr_;

    /// ResultsStorage pointer
    std::shared_ptr<ResultsStorage> storage_ptr_;

    /// Where the directories that will store the outcome of
    /// non-blocking actions will be created
    const boost::filesystem::path spool_dir_path_;

    /// Time duration after which results directories get deleted
    const std::string spool_dir_purge_ttl_;

    /// Modules
    std::map<std::string, std::shared_ptr<Module>> modules_;

    /// Where the configuration files of modules are stored
    const std::string modules_config_dir_;

    /// Modules configuration
    std::map<std::string, leatherman::json_container::JsonContainer> modules_config_;

    /// To manage the spool purge task
    std::unique_ptr<PCPClient::Util::thread> spool_dir_purge_thread_ptr_;
    PCPClient::Util::mutex spool_dir_purge_mutex_;
    PCPClient::Util::condition_variable spool_dir_purge_cond_var_;

    /// Flag; set to true if the dtor has been called
    bool is_destructing_;

    /// Throw a RequestProcessor::Error in case of unknown module,
    /// unknown action, or if the requested input parameters entry
    /// does not match the JSON schema defined for the relevant action
    void validateRequestContent(const ActionRequest& request) const;

    void processBlockingRequest(const ActionRequest& request);

    void processNonBlockingRequest(const ActionRequest& request);

    // Provides the status of the task performed for a non-blocking
    // request by processing the results data from the spool dir.
    // Updates the metadata file if necessary.
    //
    // NOTE(ale): the 'status query' action is implemented as a
    // RequestProcessor member function as it needs to access the
    // loaded modules' interface
    void processStatusRequest(const ActionRequest& request);

    /// Load the modules configuration files
    void loadModulesConfiguration();

    /// Register module in the module map
    void registerModule(Module *module);

    /// Load the modules from the src/modules directory
    void loadInternalModules(const Configuration::Agent& agent_configuration);

    /// Load the external modules contained in the specified directory
    void loadExternalModulesFrom(boost::filesystem::path modules_dir_path);

    /// Log the loaded modules
    void logLoadedModules() const;

    /// Spool directory purge task; the purge call will be triggered
    /// in intervals of min("1h", TTL) duration
    void spoolDirPurgeTask();
};

}  // namespace PXPAgent

#endif  // SRC_AGENT_REQUEST_PROCESSOR_HPP_
