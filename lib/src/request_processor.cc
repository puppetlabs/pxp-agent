#include <pxp-agent/request_processor.hpp>
#include <pxp-agent/results_mutex.hpp>
#include <pxp-agent/action_response.hpp>
#include <pxp-agent/action_status.hpp>
#include <pxp-agent/pxp_schemas.hpp>
#include <pxp-agent/external_module.hpp>
#include <pxp-agent/module_type.hpp>
#include <pxp-agent/request_type.hpp>
#include <pxp-agent/time.hpp>
#include <pxp-agent/modules/echo.hpp>
#include <pxp-agent/modules/ping.hpp>
#include <pxp-agent/modules/task.hpp>
#include <pxp-agent/util/process.hpp>

#include <leatherman/json_container/json_container.hpp>
#include <leatherman/file_util/file.hpp>
#include <leatherman/file_util/directory.hpp>
#include <leatherman/util/scope_exit.hpp>
#include <leatherman/locale/locale.hpp>

#include <cpp-pcp-client/validator/validator.hpp>
#include <cpp-pcp-client/validator/schema.hpp>
#include <cpp-pcp-client/util/thread.hpp>   // this_thread::sleep_for
#include <cpp-pcp-client/util/chrono.hpp>

#define LEATHERMAN_LOGGING_NAMESPACE "puppetlabs.pxp_agent.request_processor"
#include <leatherman/logging/logging.hpp>

#include <boost/filesystem/operations.hpp>
#include <boost/format.hpp>
#include <boost/integer/common_factor_rt.hpp>

#include <vector>
#include <atomic>
#include <functional>
#include <stdexcept>  // out_of_range
#include <memory>
#include <numeric>
#include <stdint.h>

namespace PXPAgent {

namespace fs = boost::filesystem;
namespace lth_jc   = leatherman::json_container;
namespace lth_file = leatherman::file_util;
namespace lth_util = leatherman::util;
namespace lth_loc  = leatherman::locale;
namespace pcp_util = PCPClient::Util;

// Time interval to allow the non-blocking action task to ask for the
// named mutex lock, before updating the metadata
static const uint32_t METADATA_RACE_MS { 100 };

//
// Static functions
//

static bool isStatusRequest(const ActionRequest& request)
{
    return (request.module() == "status" && request.action() == "query");
}

static const std::string STATUS_QUERY_SCHEMA { "query" };

static PCPClient::Validator getStatusQueryValidator()
{
    PCPClient::Schema sch { STATUS_QUERY_SCHEMA };
    sch.addConstraint("transaction_id", PCPClient::TypeConstraint::String, true);
    PCPClient::Validator validator {};
    validator.registerSchema(sch);
    return validator;
}

//
// Non-blocking action task
//
void nonBlockingActionTask(std::shared_ptr<Module> module_ptr,
                           ActionRequest request,
                           std::shared_ptr<PXPConnector> connector_ptr,
                           std::shared_ptr<ResultsStorage> storage_ptr,
                           // cppcheck-suppress passedByValue
                           std::shared_ptr<std::atomic<bool>> done)
{
    ResultsMutex::Mutex_Ptr mtx_ptr;
    std::unique_ptr<ResultsMutex::Lock> lck_ptr;
    try {
        ResultsMutex::LockGuard a_l { ResultsMutex::Instance().access_mtx };
        if (ResultsMutex::Instance().exists(request.transactionId())) {
            // Mutex already exists; unexpected
            LOG_DEBUG("Mutex for transaction ID {1} is already cached",
                      request.transactionId());
        } else {
            ResultsMutex::Instance().add(request.transactionId());
            mtx_ptr = ResultsMutex::Instance().get(request.transactionId());
            lck_ptr.reset(new ResultsMutex::Lock(*mtx_ptr, pcp_util::defer_lock));
        }
    } catch (const ResultsMutex::Error& e) {
        // This is unexpected
        LOG_ERROR("Failed to obtain the mutex pointer for transaction {1}: {2}",
                  request.transactionId(), e.what());
    }

    lth_util::scope_exit task_cleaner {
        [&]() {
            if (lck_ptr != nullptr) {
                // Remove the mutex for this non-blocking transaction
                try {
                    ResultsMutex::LockGuard a_l {
                        ResultsMutex::Instance().access_mtx };  // RAII
                    ResultsMutex::Instance().remove(request.transactionId());
                } catch (const ResultsMutex::Error& e) {
                    // Unexpected; if we have a lock pointer it means
                    // that the mutex was cached
                    LOG_ERROR("Failed to remove the mutex pointer for "
                              "transaction {1}: {2}",
                              request.transactionId(), e.what());
                }
                lck_ptr->unlock();
                LOG_TRACE("Unlocked transaction mutex {1}",
                          request.transactionId());
            }

            // Flag the end of execution, for the thread container
            *done = true;
        }
    };

    auto response = module_ptr->executeAction(request);
    assert(response.request_type == RequestType::NonBlocking);

    if (lck_ptr != nullptr) {
        LOG_TRACE("Locking transaction mutex {1}", request.transactionId());
        lck_ptr->lock();
    } else {
        // This is unexpected
        LOG_TRACE("We previously failed to obtain the mutex pointer for "
                  "transaction {1}; we will not lock the access to the "
                  "metadata file", request.transactionId());
    }

    if (response.action_metadata.get<bool>("results_are_valid")) {
        LOG_INFO("The {1}, request ID {2} by {3}, has successfully completed",
                 request.prettyLabel(), request.id(), request.sender());
    } else {
        LOG_ERROR(response.action_metadata.get<std::string>("execution_error"));
    }

    if (response.action_metadata.get<bool>("notify_outcome")) {
        if (response.action_metadata.get<bool>("results_are_valid")) {
            connector_ptr->sendNonBlockingResponse(response);
        } else {
            connector_ptr->sendPXPError(response);
        }
    }

    try {
        storage_ptr->updateMetadataFile(request.transactionId(),
                                        response.action_metadata);
    } catch (const ResultsStorage::Error& e) {
        LOG_ERROR("Failed to write metadata of the {1}: {2}",
                  request.prettyLabel(), e.what());
    }
}

//
// Public interface
//

RequestProcessor::RequestProcessor(std::shared_ptr<PXPConnector> connector_ptr,
                                   const Configuration::Agent& agent_configuration)
        : thread_container_ { "Action Executer" },
          thread_container_mutex_ {},
          connector_ptr_ { connector_ptr },
          storage_ptr_ { new ResultsStorage(agent_configuration.spool_dir,
                                            agent_configuration.spool_dir_purge_ttl) },
          spool_dir_path_ { agent_configuration.spool_dir },
          modules_ {},
          modules_config_dir_ { agent_configuration.modules_config_dir },
          modules_config_ {},
          is_destructing_ { false }
{
    assert(!spool_dir_path_.string().empty());
    registerPurgeable(storage_ptr_);
    loadModulesConfiguration();
    loadInternalModules(agent_configuration);

    if (!agent_configuration.modules_dir.empty()) {
        loadExternalModulesFrom(agent_configuration.modules_dir);
    } else {
        LOG_WARNING("The modules directory was not provided; no external "
                    "module will be loaded");
    }

    logLoadedModules();

    if (!purgeables_.empty()) {
        for (auto purgeable : purgeables_) {
            purgeable->purge(purgeable->get_ttl(), thread_container_.getThreadNames());
        }
        purge_thread_ptr_.reset(
            new pcp_util::thread(&RequestProcessor::purgeTask, this));
    }
}

RequestProcessor::~RequestProcessor()
{
    {
        pcp_util::lock_guard<pcp_util::mutex> the_lock { purge_mutex_ };
        is_destructing_ = true;
        purge_cond_var_.notify_one();
    }

    if (purge_thread_ptr_ != nullptr && purge_thread_ptr_->joinable())
        purge_thread_ptr_->join();
}

void RequestProcessor::processRequest(const RequestType& request_type,
                                      const PCPClient::ParsedChunks& parsed_chunks)
{
    try {
        // Inspect and validate the request message format
        ActionRequest request { request_type, parsed_chunks };

        LOG_INFO("Processing {1}, request ID {2}, by {3}",
                 request.prettyLabel(), request.id(), request.sender());

        try {
            // We can access the request content; validate it
            validateRequestContent(request);
        } catch (RequestProcessor::Error& e) {
            // Invalid request; send *RPC Error message*
            LOG_ERROR("Invalid {1}, request ID {2} by {3}. Will reply with an "
                      "RPC Error message. Error: {4}",
                      request.prettyLabel(), request.id(), request.sender(), e.what());
            connector_ptr_->sendPXPError(request, e.what());
            return;
        }

        LOG_DEBUG("The {1} has been successfully validated", request.prettyLabel());

        try {
            if (isStatusRequest(request)) {
                processStatusRequest(request);
            } else if (request.type() == RequestType::Blocking) {
                processBlockingRequest(request);
            } else {
                processNonBlockingRequest(request);
            }

            LOG_DEBUG("The {1}, request ID {2} by {3}, has been successfully processed",
                      request.prettyLabel(), request.id(), request.sender());
        } catch (std::exception& e) {
            // Process failure; send a *RPC Error message*
            LOG_ERROR("Failed to process {1}, request ID {2} by {3}. Will reply "
                      "with an RPC Error message. Error: {4}",
                      request.prettyLabel(), request.id(), request.sender(), e.what());
            connector_ptr_->sendPXPError(request, e.what());
        }
    } catch (ActionRequest::Error& e) {
        // Failed to instantiate ActionRequest - bad message;
        // send a *PCP error*
        auto id = parsed_chunks.envelope.get<std::string>("id");
        auto sender = parsed_chunks.envelope.get<std::string>("sender");
        std::vector<std::string> endpoints { sender };
        LOG_ERROR("Invalid request with ID {1} by {2}. Will reply with a PCP "
                  "error. Error: {3}",
                  id, sender, e.what());
        connector_ptr_->sendPCPError(id, e.what(), endpoints);
    }
}

bool RequestProcessor::hasModule(const std::string& module_name) const
{
    return modules_.find(module_name) != modules_.end();
}

bool RequestProcessor::hasModuleConfig(const std::string& module_name) const
{
    return modules_config_.find(module_name) != modules_config_.end();
}

std::string RequestProcessor::getModuleConfig(const std::string& module_name) const
{
    if (!hasModuleConfig(module_name))
        throw RequestProcessor::Error {
            lth_loc::format("no configuration loaded for the module '{1}'",
                            module_name) };

    return modules_config_.at(module_name).toString();
}

//
// Process requests (private interface)
//

void RequestProcessor::validateRequestContent(const ActionRequest& request) const
{
    static PCPClient::Validator status_query_validator { getStatusQueryValidator() };

    auto is_status_request = isStatusRequest(request);

    try {
        if (!is_status_request
                && !modules_.at(request.module())->hasAction(request.action()))
            throw RequestProcessor::Error {
                lth_loc::format("unknown action '{1}' for module '{2}'",
                                request.action(), request.module()) };
    } catch (std::out_of_range& e) {
        throw RequestProcessor::Error { lth_loc::format("unknown module: {1}",
                                                        request.module()) };
    }

    // Verify the module supports the non-blocking / asynchronous requests
    // if such a request is passed.
    // NB: we rely on short-circuiting OR (otherwise, modules_ access
    // could break)
    if (request.type() == RequestType::NonBlocking
            && (is_status_request
                || !modules_.at(request.module())->supportsAsync()))
        throw RequestProcessor::Error {
            lth_loc::format("the module '{1}' supports only blocking PXP requests",
                            request.module()) };


    // Validate request input params
    try {
        LOG_DEBUG("Validating input parameters of the {1}, request ID {2} by {3}",
                  request.prettyLabel(), request.id(), request.sender());

        // NB: the registred schemas have the same name as the action
        auto& validator = (is_status_request
                                ? status_query_validator
                                : modules_.at(request.module())->input_validator_);
        validator.validate(request.params(), request.action());
    } catch (PCPClient::validation_error& e) {
        LOG_DEBUG("Invalid input parameters of the {1}, request ID {2} by {3}: {4}",
                  request.prettyLabel(), request.id(), request.sender(), e.what());
        throw RequestProcessor::Error {
            lth_loc::format("invalid input for {1}", request.prettyLabel()) };
    }
}

void RequestProcessor::processBlockingRequest(const ActionRequest& request)
{
    auto response = modules_[request.module()]->executeAction(request);

    if (response.action_metadata.get<bool>("results_are_valid")) {
        LOG_INFO("The {1}, request ID {2} by {3}, has successfully completed",
                 request.prettyLabel(), request.id(), request.sender());
        connector_ptr_->sendBlockingResponse(response, request);
    } else {
        LOG_ERROR(response.action_metadata.get<std::string>("execution_error"));
        connector_ptr_->sendPXPError(response);
    }
}

void RequestProcessor::processNonBlockingRequest(const ActionRequest& request)
{
    request.setResultsDir(
        std::move((spool_dir_path_ / request.transactionId()).string()));
    std::string err_msg {};

    LOG_DEBUG("Preparing the task for the {1}, request ID {2} by {3} (using the "
              "transaction ID as identifier)",
              request.prettyLabel(), request.id(), request.sender());

    try {
        // NB: this locked check prevents multiple requests with the
        // same transaction_id
        pcp_util::lock_guard<pcp_util::mutex> lck { thread_container_mutex_ };

        // If the task has already been started or run, return a provisional response again.
        if (thread_container_.find(request.transactionId())) {
            LOG_DEBUG("already exists an ongoing task with transaction id {1}", request.transactionId());
        } else if (storage_ptr_->find(request.transactionId())) {
            LOG_DEBUG("already exists a previous task with transaction id {1}", request.transactionId());
        } else {
            try {
                // Initialize the action metadata file
                auto metadata = ActionResponse::getMetadataFromRequest(request);
                storage_ptr_->initializeMetadataFile(request.transactionId(),
                                                     metadata);
            } catch (const ResultsStorage::Error& e) {
                err_msg = lth_loc::format("Failed to initialize the metadata file: {1}",
                                          e.what());
            }

            if (err_msg.empty()) {
                // Metadata file was created; we can start the task

                // Flag to enable signaling from task to thread_container
                auto done = std::make_shared<std::atomic<bool>>(false);

                // NB: we got the_lock, so we're sure this will not throw
                // due to another stored thread with the same name
                thread_container_.add(request.transactionId(),
                                      pcp_util::thread(&nonBlockingActionTask,
                                                       modules_[request.module()],
                                                       request,
                                                       connector_ptr_,
                                                       storage_ptr_,
                                                       done),
                                      done);
            }
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to spawn the action job for transaction {1}; error: {2}",
                  request.transactionId(), e.what());
        err_msg += "failed to start action task: ";
        err_msg += e.what();
    }

    if (err_msg.empty()) {
        connector_ptr_->sendProvisionalResponse(request);
    } else {
        connector_ptr_->sendPXPError(request, err_msg);
    }
}

// TODO(ale): update table and use UNDETERMINED and RPC errors (v2.0)

//                       TRANSACTION STATUS RESPONSE TABLE
//
// |+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++|
// |         |          |         |          |           |         |                     |
// |metadata |completed |exitcode | PID file |PID process| action  |      response:      |
// | exists? |    by    |  file   |  exists? |  exists?  | thread  |       status        |
// |  valid  |metadata? | exists? |          |           | exists? |          &          |
// |metadata?|(status   |(got     |          |           |         |   included files    |
// |         |!=RUNNING)| output?)|          |           |         |                     |
// |         |          |         |          |           |         |                     |
// |+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++|
// | ~exists |     -    |    -    |     -    |     -     |    -    |       UNKNOWN       |
// |+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++|
// | ~valid  |     -    |    -    |     -    |     -     |    -    |       UNKNOWN       |
// |+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++|
// |   yes   |    no    |   no    |    no    |     -     |   yes   |       RUNNING       |
// |+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++|
// |   yes   |    no    |   no    |    yes   |    yes    |    -    |       RUNNING       |
// |+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++|
// |   yes   |    no    |   no    |    yes   |    no     |    -    |       UNKNOWN       |
// |+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++|
// |   yes   |    no    |   yes   |     -    |     -     |    -    |  SUCCESS / FAILURE  |
// |         |          |         |          |           |         | + stdout & stderr   |
// |         |          |         |          |           |         | + metadata update   |
// |+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++|
// |   yes   |    yes   |    -    |     -    |     -     |    -    |  SUCCESS / FAILURE  |
// |         |          |         |          |           |         | + stdout & stderr   |
// |+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++|
//

void RequestProcessor::processStatusRequest(const ActionRequest& request)
{
    auto t_id = request.params().get<std::string>("transaction_id");
    ActionResponse status_response { ModuleType::Internal, request, t_id };
    lth_jc::JsonContainer status_results {};
    const auto& AS = ACTION_STATUS_NAMES;
    status_results.set<std::string>("transaction_id", t_id);
    status_results.set<std::string>("status", AS.at(ActionStatus::Unknown));

    if (!storage_ptr_->find(t_id)) {
        LOG_DEBUG("Found no results for the {1}", request.prettyLabel());
        status_response.setValidResultsAndEnd(
            std::move(status_results),
            lth_loc::translate("found no results directory"));
        connector_ptr_->sendStatusResponse(status_response, request);
        return;
    }

    // There's a results directory for the requested transaction!

    // Get PID information before metadata, to avoid a race with the
    // nonBlockingActionTask thread when accessing the metadata file,
    // due to the elapsed time between the end of the external module
    // process and the metadata update (note that we use a lock based
    // on a named mutex to access the metadata file in a safe way)

    bool running_by_pid { false };
    bool not_running_by_pid { false };

    if (!storage_ptr_->pidFileExists(t_id)) {
        LOG_DEBUG("The PID file for the transaction {1} task does not exist", t_id);
    } else {
        try {
            auto pid = storage_ptr_->getPID(t_id);
            LOG_DEBUG("The PID of the action process for the transaction {1} is {2}",
                      t_id, pid);

            // NOTE(ale): processExists() does not throw
            if (Util::processExists(pid)) {
                running_by_pid = true;
            } else {
                not_running_by_pid = true;
                bool cached { false };
                {
                    ResultsMutex::LockGuard a_l { ResultsMutex::Instance().access_mtx };
                    cached = ResultsMutex::Instance().exists(t_id);
                }
                if (cached)
                    // The process does not exist anymore, but its
                    // mutex is still cached - wait a bit before
                    // reading the metadata to allow the non-blocking
                    // task to lock its mutex and update the metadata
                    pcp_util::this_thread::sleep_for(
                        pcp_util::chrono::milliseconds(METADATA_RACE_MS));
            }
        } catch (const ResultsStorage::Error& e) {
            LOG_ERROR("Failed to get the PID for transaction {1}: {2}",
                      t_id, e.what());
        }
    }

    // Retrieve metadata

    LOG_DEBUG("Retrieving metadata and output for the transaction {1}", t_id);
    std::string metadata_retrieval_error {};
    lth_jc::JsonContainer metadata;

    // NB: later on, we determine if the mutex of the requested
    // transaction was cached by checking if mtx_ptr is defined
    ResultsMutex::Mutex_Ptr mtx_ptr;
    {
        ResultsMutex::LockGuard a_l { ResultsMutex::Instance().access_mtx };
        if (ResultsMutex::Instance().exists(t_id))
            mtx_ptr = ResultsMutex::Instance().get(t_id);
    }

    try {
        // Acquire the result mutex lock, if the transaction is cached
        if (mtx_ptr != nullptr) {
            ResultsMutex::LockGuard r_l { *mtx_ptr };  // RAII
            metadata = storage_ptr_->getActionMetadata(t_id);
        } else {
            metadata = storage_ptr_->getActionMetadata(t_id);
        }

        // We know that metadata has a valid format; ensure that the
        // stored module/action pair is known (it would be very
        // unexpected otherwise, but we may rely on this later)
        auto mod = metadata.get<std::string>("module");
        auto act = metadata.get<std::string>("action");
        if (!hasModule(mod) || !modules_.at(mod)->hasAction(act))
            throw Error {
                lth_loc::format("unknown action stored in metadata file: '{1} {2}'",
                                mod, act) };
    } catch (const ResultsStorage::Error& e) {
        LOG_ERROR("Cannot access the stored metadata for the transaction {1}: {2}",
                  t_id, e.what());
        metadata_retrieval_error = e.what();
    } catch (const std::exception& e) {
        LOG_ERROR("Unexpected failure while retrieving metadata for the "
                  "transaction {1}: {2}",
                  t_id, e.what());
        std::string err_msg { e.what() };
        metadata_retrieval_error =
            lth_loc::format("unexpected failure while retrieving metadata{1}",
                            (err_msg.empty() ? "" : ": " + err_msg));
    }

    if (!metadata_retrieval_error.empty()) {
        // TODO(ale): send RPC error once PXP v2.0 changes are in
        status_response.setValidResultsAndEnd(std::move(status_results),
                                              metadata_retrieval_error);
        connector_ptr_->sendStatusResponse(status_response, request);
        return;
    }

    // At this point, we have a valid metadata object;
    // inspect it and see if it was finalized

    std::string execution_error {};
    auto stored_status = metadata.get<std::string>("status");
    LOG_TRACE("The status of the transaction {1} is '{2}', as reported in its "
              "metadata file",
              t_id, stored_status);

    if (stored_status != AS.at(ActionStatus::Running)) {
        // The metadata was finalized (status != RUNNING); just use it
        // TODO(ale): use UNDETERMINED after PXP v2.0 changes; leaving
        // as UNKNOWN for now
        if (stored_status != AS.at(ActionStatus::Undetermined))
            status_results.set<std::string>("status", stored_status);

        if (metadata.includes("execution_error"))
            execution_error = metadata.get<std::string>("execution_error");

        // Get the output if possible, otherwise move on
        try {
            status_response.output = storage_ptr_->getOutput(t_id);
        } catch (const ResultsStorage::Error& e) {
            // Log an error and update the execution_error only if
            // the metadata says that the results were valid
            if (metadata.includes("results_are_valid")
                && metadata.get<bool>("results_are_valid")) {
                LOG_ERROR("Failed to get the output of the trasaction {1}: {2}",
                          t_id, e.what());
                if (execution_error.empty())
                    execution_error =
                        lth_loc::format("failed to retrieve the output: {1}",
                                        e.what());
            }
        }

        status_response.setValidResultsAndEnd(std::move(status_results),
                                              execution_error);
        connector_ptr_->sendStatusResponse(status_response, request);
        return;
    }

    // The metadata was not finalized (status == RUNNING); if the
    // output is not available, use the PID data to ensure that the
    // task is still running

    if (!storage_ptr_->outputIsReady(t_id)) {
        // No output; use the PID information to determine the status
        // and update the metadata if UNDETERMINED
        LOG_TRACE("The output of the transaction {1} is not ready; using the PID "
                  "information to ensure that the action process is running", t_id);

        if (running_by_pid) {
            // It's running --> RUNNING
            LOG_TRACE("The action process of the transaction {1} is running", t_id);
            status_results.set<std::string>("status", AS.at(ActionStatus::Running));
        } else  if (not_running_by_pid) {
            // It's not running --> UNDETERMINED / execution_error
            // TODO(ale): use UNDETERMINED after PXP v2.0 changes
            LOG_WARNING("The action process of the transaction {1} is not "
                        "running; updating its status to 'undetermined' "
                        "on its metadata file",
                        t_id);
            execution_error = lth_loc::translate("task process is not running, "
                                                 "but no output is available");
            metadata.set<std::string>("status", AS.at(ActionStatus::Undetermined));
            if (!metadata.includes("execution_error"))
                // Update this for the sake of future status requests
                metadata.set<std::string>("execution_error", execution_error);
            try {
                if (mtx_ptr != nullptr) {
                    ResultsMutex::LockGuard r_l { *mtx_ptr };
                    storage_ptr_->updateMetadataFile(t_id, metadata);
                } else {
                    storage_ptr_->updateMetadataFile(t_id, metadata);
                }
            } catch (const ResultsStorage::Error& err) {
                LOG_ERROR("Failed to update the metadata file of the "
                          "transaction {1}: {2}",
                          t_id, err.what());
            }
        } else if (thread_container_.find(t_id)) {
            // Leave checking the thread container until now, as the thread may still
            // be running if we never restarted. It runs until the external action ends
            // to send a non-blocking response (if notify_outcome is true).
            LOG_TRACE("The action thread of the transaction {1} is running", t_id);
            status_results.set<std::string>("status", AS.at(ActionStatus::Running));
        } else {
            // We failed to get any indication from the PID file;
            // leave the status as UNKNOWN as the process may finish
            // later and make its output available
            LOG_WARNING("We cannot determine the status of the transaction {1} "
                        "from the action process' PID", t_id);
            execution_error = lth_loc::translate("the PID and output of this "
                                                 "task are not available");
        }

        status_response.setValidResultsAndEnd(std::move(status_results),
                                              execution_error);
        connector_ptr_->sendStatusResponse(status_response, request);
        return;
    }

    // The exitcode file exists, so the external module process should
    // have completed; if, by the PID information, the process was
    // still executing, wait a bit, then retrieve the output, process
    // it and update metadata file

    if (running_by_pid) {
        // Wait a bit to relax the requirement for the exitcode file
        // being written before the output ones
        LOG_DEBUG("The output for the transaction {1} is now available, but the "
                  "action process was still executing when this handler started; "
                  "wait for {2} ms before retrieving the output from file",
                  t_id, ExternalModule::OUTPUT_DELAY_MS);
        pcp_util::this_thread::sleep_for(
            pcp_util::chrono::milliseconds(ExternalModule::OUTPUT_DELAY_MS));
    } else {
        LOG_TRACE("Output of {1} is ready; retrieving it", t_id);
    }

    try {
        status_response.output = storage_ptr_->getOutput(t_id);
    } catch (const ResultsStorage::Error& e) {
        LOG_ERROR("Failed to get the output of the transaction {1} (it status "
                  "will be updated to 'undetermined' on its metadata file): {2}",
                  t_id, e.what());
        // TODO(ale): use UNDETERMINED after PXP v2.0 changes
        status_response.setValidResultsAndEnd(
                std::move(status_results),
                lth_loc::translate("found no results directory"));
        connector_ptr_->sendStatusResponse(status_response, request);

        // Update the metadata with a final 'status' value
        metadata.set<std::string>("status", AS.at(ActionStatus::Undetermined));
        try {
            if (mtx_ptr != nullptr) {
                ResultsMutex::LockGuard r_l { *mtx_ptr };
                storage_ptr_->updateMetadataFile(t_id, metadata);
            } else {
                storage_ptr_->updateMetadataFile(t_id, metadata);
            }
        } catch (const ResultsStorage::Error& err) {
            LOG_ERROR("Failed to update metadata for the {1}: {2}",
                      request.prettyLabel(), err.what());
        }
        return;
    }

    // We previously verified the module and action pair to exist
    // so this cannot throw
    std::shared_ptr<Module> mod_ptr {
        modules_.at(metadata.get<std::string>("module")) };

    // Create a new response object, to process the output
    // NOTE(ale): this is not ideal since we're copying the output; on
    // the other hand, this is an exceptional case (available output
    // and non finalized metadata; pxp-agent crashed during a task...)
    ActionResponse a_r { mod_ptr->type(),
                         RequestType::NonBlocking,
                         status_response.output,
                         std::move(metadata) };
    // HERE(ale): ^^ don't use the 'metadata' ref below!!!

    mod_ptr->processOutputAndUpdateMetadata(a_r);

    if (a_r.action_metadata.get<bool>("results_are_valid"))
        // Validate by using the action's output JSON schema
        mod_ptr->validateOutputAndUpdateMetadata(a_r);

    // Update metadata file while holding the lock (if cached)
    LOG_INFO("Setting the status of the transaction {1} to '{2}' on its "
             "metadata file",
             t_id, a_r.action_metadata.get<std::string>("status"));
    try {
        if (mtx_ptr != nullptr) {
            ResultsMutex::LockGuard r_l { *mtx_ptr };
            storage_ptr_->updateMetadataFile(t_id, a_r.action_metadata);
        } else {
            storage_ptr_->updateMetadataFile(t_id, a_r.action_metadata);
        }
    } catch (const ResultsStorage::Error& err) {
        LOG_ERROR("Failed to update metadata of the transaction {1}: {2}",
                  t_id, err.what());
    }

    // Update status query response's status / execution_error
    if (a_r.action_metadata.get<bool>("results_are_valid")) {
        status_results.set<std::string>("status", AS.at(ActionStatus::Success));
    } else {
        status_results.set<std::string>("status", AS.at(ActionStatus::Failure));
        status_results.set<std::string>("execution_error",
            a_r.action_metadata.get<std::string>("execution_error"));
    }

    status_response.setValidResultsAndEnd(std::move(status_results),
                                          execution_error);
    connector_ptr_->sendStatusResponse(status_response, request);
}

//
// Load Modules (private interface)
//

void RequestProcessor::loadModulesConfiguration()
{
    LOG_INFO("Loading external modules configuration from {1}",
             modules_config_dir_);

    if (fs::is_directory(modules_config_dir_)) {
        lth_file::each_file(
            modules_config_dir_,
            [this](std::string const& s) -> bool {
                fs::path s_path { s };
                auto file_name = s_path.stem().string();
                // NB: ".conf" suffix guaranteed by each_file()
                auto pos_suffix = file_name.find(".conf");
                auto module_name = file_name.substr(0, pos_suffix);

                try {
                    auto config_json = lth_jc::JsonContainer(lth_file::read(s));
                    modules_config_[module_name] = std::move(config_json);
                    LOG_DEBUG("Loaded module configuration for module '{1}' "
                              "from {2}", module_name, s);
                } catch (lth_jc::data_parse_error& e) {
                    LOG_WARNING("Cannot load module config file '{1}'; invalid "
                                "JSON format. If the module's metadata contains "
                                "the 'configuration' entry, the module won't be "
                                "loaded. Error: '{2}'", s, e.what());
                    modules_config_[module_name] = lth_jc::JsonContainer { "null" };
                }
                return true;
                // naming convention for config files suffixes; don't
                // process files that don't end in this extension
            },
            "\\.conf$");
    } else {
        LOG_DEBUG("Directory '{1}' specified by modules-config-dir doesn't "
                  "exist; no module configuration file will be loaded",
                  modules_config_dir_);
    }
}

void RequestProcessor::registerModule(std::shared_ptr<Module> module_ptr)
{
    if (!modules_.emplace(module_ptr->module_name, module_ptr).second) {
        LOG_WARNING("Ignoring attempt to re-register module: {1}", module_ptr->module_name);
    }
}

void RequestProcessor::registerPurgeable(std::shared_ptr<Util::Purgeable> p)
{
    if (Timestamp::getMinutes(p->get_ttl()) > 0) {
        purgeables_.emplace_back(std::move(p));
    }
}

void RequestProcessor::loadInternalModules(const Configuration::Agent& agent_configuration)
{
    registerModule(std::make_shared<Modules::Echo>());
    registerModule(std::make_shared<Modules::Ping>());
    auto task = std::make_shared<Modules::Task>(
        Configuration::Instance().getExecPrefix(),
        agent_configuration.task_cache_dir,
        agent_configuration.task_cache_dir_purge_ttl,
        agent_configuration.master_uris,
        agent_configuration.ca,
        agent_configuration.crt,
        agent_configuration.key,
        agent_configuration.master_proxy,
        agent_configuration.task_download_connect_timeout_s,
        agent_configuration.task_download_timeout_s,
        storage_ptr_);
    registerModule(task);
    registerPurgeable(task);
}

void RequestProcessor::loadExternalModulesFrom(fs::path dir_path)
{
    if (!fs::is_directory(dir_path)) {
        LOG_WARNING("Failed to locate the modules directory '{1}'; no external "
                            "modules will be loaded", dir_path.string());
        return;
    }

    LOG_INFO("Loading external modules from {1}", dir_path.string());
    fs::directory_iterator end;

    for (auto f = fs::directory_iterator(dir_path); f != end; ++f) {
        if (!fs::is_directory(f->status())) {
            auto f_p = fs::canonical(f->path());
            auto extension = f_p.extension();

            // valid module have no extension on *nix, .bat
            // extensions on Windows
#ifndef _WIN32
            if (extension == "") {
#else
            if (extension == ".bat" || extension == ".exe") {
#endif
                try {
                    std::shared_ptr<ExternalModule> e_m;
                    auto config_itr = modules_config_.find(f_p.stem().string());

                    if (config_itr != modules_config_.end()) {
                        e_m = std::make_shared<ExternalModule>(
                            f_p.string(), config_itr->second, storage_ptr_);
                        e_m->validateConfiguration();
                        LOG_DEBUG("The '{1}' module configuration has been "
                                  "validated: {2}", e_m->module_name,
                                  config_itr->second.toString());
                    } else {
                        e_m = std::make_shared<ExternalModule>(
                            f_p.string(), storage_ptr_);
                    }

                    registerModule(e_m);
                } catch (Module::LoadingError& e) {
                    LOG_ERROR("Failed to load {1}; {2}", f_p, e.what());
                } catch (PCPClient::validation_error& e) {
                    LOG_ERROR("Failed to configure {1}; {2}", f_p, e.what());
                } catch (std::exception& e) {
                    LOG_ERROR("Unexpected error when loading {1}; {2}",
                              f_p, e.what());
                } catch (...) {
                    LOG_ERROR("Unexpected error when loading {1}", f_p);
                }
            }
        }
    }
}

void RequestProcessor::logLoadedModules() const
{
    std::string actions_label { lth_loc::translate("actions") };

    for (auto& module : modules_) {
        std::string actions_list { "" };
        size_t count = 0u;

        for (auto& action : module.second->actions) {
            ++count;
            if (actions_list.empty()) {
                actions_list += ": ";
            } else {
                actions_list += ", ";
            }
            actions_list += action;
        }

        // TODO(ale): deal with locale & plural (PCP-257)
        auto txt = (count == 0) ?  lth_loc::translate("found no action")
            : ((count == 1) ? lth_loc::translate("action")
               : lth_loc::translate("actions"));
        LOG_DEBUG("Loaded '{1}' module - {2}{3}", module.first, txt, actions_list);
    }
}

//
// Resource purge task (private interface)
//

static unsigned int minutes_gcd(std::vector<std::shared_ptr<Util::Purgeable>> purgeables)
{
    assert(!purgeables.empty());
    auto init = Timestamp::getMinutes(purgeables.front()->get_ttl());
    if (purgeables.size() > 1) {
        return std::accumulate(purgeables.begin()+1, purgeables.end(), init,
            [](unsigned int result, std::shared_ptr<Util::Purgeable> &p) {
                return boost::integer::gcd(result, Timestamp::getMinutes(p->get_ttl()));
            });
    } else {
        return init;
    }
}

void RequestProcessor::purgeTask()
{
    // Use min of 1h and gcd of purgeable TTLs.
    auto num_minutes = std::min(60u, minutes_gcd(purgeables_));
    LOG_INFO(lth_loc::format_n(
        // LOCALE: info
        "Scheduling the check every {1} minute for directories to purge; thread id {2}",
        "Scheduling the check every {1} minutes for directories to purge; thread id {2}",
        num_minutes, num_minutes, pcp_util::this_thread::get_id()));

    while (true) {
        pcp_util::unique_lock<pcp_util::mutex> the_lock { purge_mutex_ };
        auto now = pcp_util::chrono::system_clock::now();

        if (!is_destructing_)
            purge_cond_var_.wait_until(
                the_lock,
                now + pcp_util::chrono::minutes(num_minutes));

        if (is_destructing_)
            return;

        for (auto purgeable : purgeables_) {
            purgeable->purge(purgeable->get_ttl(), thread_container_.getThreadNames());
        }
    }
}

}  // namespace PXPAgent
