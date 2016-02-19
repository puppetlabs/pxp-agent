#include <pxp-agent/request_processor.hpp>
#include <pxp-agent/results_mutex.hpp>
#include <pxp-agent/action_response.hpp>
#include <pxp-agent/action_status.hpp>
#include <pxp-agent/pxp_schemas.hpp>
#include <pxp-agent/external_module.hpp>
#include <pxp-agent/module_type.hpp>
#include <pxp-agent/request_type.hpp>
#include <pxp-agent/modules/echo.hpp>
#include <pxp-agent/modules/ping.hpp>
#include <pxp-agent/util/process.hpp>

#include <leatherman/json_container/json_container.hpp>
#include <leatherman/file_util/file.hpp>
#include <leatherman/file_util/directory.hpp>
#include <leatherman/util/strings.hpp>
#include <leatherman/util/scope_exit.hpp>

#include <cpp-pcp-client/util/thread.hpp>
#include <cpp-pcp-client/validator/validator.hpp>
#include <cpp-pcp-client/validator/schema.hpp>
#include <cpp-pcp-client/util/thread.hpp>   // this_thread::sleep_for
#include <cpp-pcp-client/util/chrono.hpp>

#define LEATHERMAN_LOGGING_NAMESPACE "puppetlabs.pxp_agent.request_processor"
#include <leatherman/logging/logging.hpp>

#include <boost/filesystem/operations.hpp>
#include <boost/format.hpp>

#include <vector>
#include <atomic>
#include <functional>
#include <stdexcept>  // out_of_range
#include <memory>
#include <stdint.h>

namespace PXPAgent {

namespace fs = boost::filesystem;
namespace lth_jc = leatherman::json_container;
namespace lth_file = leatherman::file_util;
namespace lth_util = leatherman::util;
namespace pcp_util = PCPClient::Util;

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
                           std::shared_ptr<std::atomic<bool>> done)
{
    ResultsMutex::Mutex_Ptr mtx_ptr;
    std::unique_ptr<ResultsMutex::Lock> lck_ptr;
    try {
        ResultsMutex::LockGuard a_l { ResultsMutex::Instance().access_mtx };
        if (ResultsMutex::Instance().exists(request.transactionId())) {
            // Mutex already exists; unexpected
            LOG_DEBUG("Mutex for transaction ID %1% is already cached",
                      request.transactionId());
        } else {
            ResultsMutex::Instance().add(request.transactionId());
            mtx_ptr = ResultsMutex::Instance().get(request.transactionId());
            lck_ptr.reset(new ResultsMutex::Lock(*mtx_ptr, pcp_util::defer_lock));
        }
    } catch (const ResultsMutex::Error& e) {
        // This is unexpected
        LOG_ERROR("Failed to obtain the mutex pointer for transaction %1%: %2%",
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
                              "transaction %1%: %2%",
                              request.transactionId(), e.what());
                }
                lck_ptr->unlock();
                LOG_TRACE("Unlocked transaction mutex %1%",
                          request.transactionId());
            }

            // Flag the end of execution, for the thread container
            *done = true;
        }
    };

    auto response = module_ptr->executeAction(request);
    assert(response.request_type == RequestType::NonBlocking);

    if (lck_ptr != nullptr) {
        LOG_TRACE("Locking transaction mutex %1%", request.transactionId());
        lck_ptr->lock();
    } else {
        // This is unexpected
        LOG_TRACE("We previously failed to obtain the mutex pointer for "
                  "transaction %1%; we will not lock the access to the "
                  "metadata file", request.transactionId());
    }

    if (response.action_metadata.get<bool>("results_are_valid")) {
        LOG_INFO("The %1%, request ID %2% by %3%, has successfully completed",
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
        storage_ptr->updateMetadataFile(response.action_metadata);
    } catch (const ResultsStorage::Error& e) {
        LOG_ERROR("Failed to write metadata of the %1%: %2%",
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
          storage_ptr_ { new ResultsStorage(agent_configuration.spool_dir) },
          spool_dir_path_ { agent_configuration.spool_dir },
          modules_ {},
          modules_config_dir_ { agent_configuration.modules_config_dir },
          modules_config_ {}
{
    assert(!spool_dir_path_.string().empty());
    loadModulesConfiguration();
    loadInternalModules();

    if (!agent_configuration.modules_dir.empty()) {
        loadExternalModulesFrom(agent_configuration.modules_dir);
    } else {
        LOG_WARNING("The modules directory was not provided; no external "
                    "module will be loaded");
    }

    logLoadedModules();
}

void RequestProcessor::processRequest(const RequestType& request_type,
                                      const PCPClient::ParsedChunks& parsed_chunks)
{
    LOG_TRACE("About to validate and process PXP request message: %1%",
              parsed_chunks.toString());
    try {
        // Inspect and validate the request message format
        ActionRequest request { request_type, parsed_chunks };

        LOG_INFO("Processing %1%, request ID %2%, by %3%",
                 request.prettyLabel(), request.id(), request.sender());

        try {
            // We can access the request content; validate it
            validateRequestContent(request);
        } catch (RequestProcessor::Error& e) {
            // Invalid request; send *RPC Error message*
            LOG_ERROR("Invalid %1%, request ID %2% by %3%. Will reply with an "
                      "RPC Error message. Error: %4%",
                      request.prettyLabel(), request.id(), request.sender(), e.what());
            connector_ptr_->sendPXPError(request, e.what());
            return;
        }

        LOG_DEBUG("The %1% has been successfully validated", request.prettyLabel());

        try {
            if (request.type() == RequestType::Blocking) {
                processBlockingRequest(request);
            } else {
                processNonBlockingRequest(request);
            }

            LOG_DEBUG("The %1%, request ID %2% by %3%, has been successfully processed",
                      request.prettyLabel(), request.id(), request.sender());
        } catch (std::exception& e) {
            // Process failure; send a *RPC Error message*
            LOG_ERROR("Failed to process %1%, request ID %2% by %3%. Will reply "
                      "with an RPC Error message. Error: %4%",
                      request.prettyLabel(), request.id(), request.sender(), e.what());
            connector_ptr_->sendPXPError(request, e.what());
        }
    } catch (ActionRequest::Error& e) {
        // Failed to instantiate ActionRequest - bad message;
        // send a *PCP error*
        auto id = parsed_chunks.envelope.get<std::string>("id");
        auto sender = parsed_chunks.envelope.get<std::string>("sender");
        std::vector<std::string> endpoints { sender };
        LOG_ERROR("Invalid request with ID %1% by %2%. Will reply with a PCP "
                  "error. Error: %3%",
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
            (boost::format("no configuration loaded for the module '%1%'")
                % module_name)
            .str() };

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
                (boost::format("unknown action '%1%' for module '%2%'")
                    % request.action()
                    % request.module())
                .str() };
    } catch (std::out_of_range& e) {
        throw RequestProcessor::Error { "unknown module: " + request.module() };
    }

    // If it's an internal module, the request must be blocking
    // NB: we rely on short-circuiting OR (otherwise, modules_ access
    // could break)
    if (request.type() == RequestType::NonBlocking
            && (is_status_request
                || modules_.at(request.module())->type() == ModuleType::Internal))
        throw RequestProcessor::Error {
            (boost::format("the module '%1%' supports only blocking PXP requests")
                % request.module())
            .str() };


    // Validate request input params
    try {
        LOG_DEBUG("Validating input parameters of the %1%, request ID %2% by %3%",
                  request.prettyLabel(), request.id(), request.sender());

        // NB: the registred schemas have the same name as the action
        auto& validator = (is_status_request
                                ? status_query_validator
                                : modules_.at(request.module())->input_validator_);
        validator.validate(request.params(), request.action());
    } catch (PCPClient::validation_error& e) {
        LOG_DEBUG("Invalid input parameters of the %1%, request ID %2% by %3%: %4%",
                  request.prettyLabel(), request.id(), request.sender(), e.what());
        throw RequestProcessor::Error { "invalid input for " + request.prettyLabel() };
    }
}

void RequestProcessor::processBlockingRequest(const ActionRequest& request)
{
    auto response = modules_[request.module()]->executeAction(request);

    if (response.action_metadata.get<bool>("results_are_valid")) {
        LOG_INFO("The %1%, request ID %2% by %3%, has successfully completed",
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

    LOG_DEBUG("Preparing the task for the %1%, request ID %2% by %3% (using the "
              "transaction ID as identifier)",
              request.prettyLabel(), request.id(), request.sender());

    try {
        // NB: this locked check prevents multiple requests with the
        // same transaction_id
        pcp_util::lock_guard<pcp_util::mutex> lck { thread_container_mutex_ };

        if (thread_container_.find(request.transactionId())) {
            err_msg += "already exists an ongoing task with transaction id ";
            err_msg += request.transactionId();
        } else {
            try {
                // Initialize the action metadata file
                auto metadata = ActionResponse::getMetadataFromRequest(request);
                storage_ptr_->initializeMetadataFile(request.transactionId(),
                                                     metadata);
            } catch (const ResultsStorage::Error& e) {
                err_msg += "Failed to initialize the metadata file: ";
                err_msg += e.what();
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
        LOG_ERROR("Failed to spawn the action job for transaction %1%; error: %2%",
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

//
// Load Modules (private interface)
//

void RequestProcessor::loadModulesConfiguration()
{
    LOG_INFO("Loading external modules configuration from %1%",
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
                    LOG_DEBUG("Loaded module configuration for module '%1%' "
                              "from %2%", module_name, s);
                } catch (lth_jc::data_parse_error& e) {
                    LOG_WARNING("Cannot load module config file '%1%'; invalid "
                                "JSON format. If the module's metadata contains "
                                "the 'configuration' entry, the module won't be "
                                "loaded. Error: '%2%'", s, e.what());
                    modules_config_[module_name] = lth_jc::JsonContainer { "null" };
                }
                return true;
                // naming convention for config files suffixes; don't
                // process files that don't end in this extension
            },
            "\\.conf$");
    } else {
        LOG_DEBUG("Directory '%1%' specified by modules-config-dir doesn't "
                  "exist; no module configuration file will be loaded",
                  modules_config_dir_);
    }
}

void RequestProcessor::loadInternalModules()
{
    // HERE(ale): no external configuration for internal modules
    modules_["echo"] = std::shared_ptr<Module>(new Modules::Echo);
    modules_["ping"] = std::shared_ptr<Module>(new Modules::Ping);
}

void RequestProcessor::loadExternalModulesFrom(fs::path dir_path)
{
    LOG_INFO("Loading external modules from %1%", dir_path.string());

    if (fs::is_directory(dir_path)) {
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
                if (extension == ".bat") {
#endif
                    try {
                        ExternalModule* e_m;
                        auto config_itr = modules_config_.find(f_p.stem().string());

                        if (config_itr != modules_config_.end()) {
                            e_m = new ExternalModule(f_p.string(),
                                                     config_itr->second,
                                                     spool_dir_path_.string());
                            e_m->validateConfiguration();
                            LOG_DEBUG("The '%1%' module configuration has been "
                                      "validated: %2%", e_m->module_name,
                                      config_itr->second.toString());
                        } else {
                            e_m = new ExternalModule(f_p.string(),
                                                     spool_dir_path_.string());
                        }

                        modules_[e_m->module_name] = std::shared_ptr<Module>(e_m);
                    } catch (Module::LoadingError& e) {
                        LOG_ERROR("Failed to load %1%; %2%", f_p, e.what());
                    } catch (PCPClient::validation_error& e) {
                        LOG_ERROR("Failed to configure %1%; %2%", f_p, e.what());
                    } catch (std::exception& e) {
                        LOG_ERROR("Unexpected error when loading %1%; %2%",
                                  f_p, e.what());
                    } catch (...) {
                        LOG_ERROR("Unexpected error when loading %1%", f_p);
                    }
                }
            }
        }
    } else {
        LOG_WARNING("Failed to locate the modules directory; no external "
                    "modules will be loaded");
    }
}

void RequestProcessor::logLoadedModules() const
{
    for (auto& module : modules_) {
        std::string txt { "found no action" };
        std::string actions_list { "" };

        for (auto& action : module.second->actions) {
            if (actions_list.empty()) {
                txt = "action";
                actions_list += ": ";
            } else {
                actions_list += ", ";
            }
            actions_list += action;
        }

        auto txt_suffix = lth_util::plural(module.second->actions.size());
        LOG_DEBUG("Loaded '%1%' module - %2%%3%%4%",
                  module.first, txt, txt_suffix, actions_list);
    }
}

}  // namespace PXPAgent
