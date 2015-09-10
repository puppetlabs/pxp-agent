#include <pxp-agent/request_processor.hpp>
#include <pxp-agent/action_outcome.hpp>
#include <pxp-agent/pxp_schemas.hpp>
#include <pxp-agent/external_module.hpp>
#include <pxp-agent/modules/echo.hpp>
#include <pxp-agent/modules/ping.hpp>
#include <pxp-agent/modules/status.hpp>

#include <leatherman/json_container/json_container.hpp>
#include <leatherman/file_util/file.hpp>
#include <leatherman/file_util/directory.hpp>
#include <leatherman/util/strings.hpp>
#include <leatherman/util/timer.hpp>

#define LEATHERMAN_LOGGING_NAMESPACE "puppetlabs.pxp_agent.request_processor"
#include <leatherman/logging/logging.hpp>

#include <boost/filesystem/operations.hpp>

#include <vector>
#include <atomic>
#include <functional>
#include <stdexcept>  // out_of_range

namespace PXPAgent {

namespace fs = boost::filesystem;
namespace lth_jc = leatherman::json_container;
namespace lth_file = leatherman::file_util;
namespace lth_util = leatherman::util;

//
// Results Storage
//

class ResultsStorage {
  public:
    struct Error : public std::runtime_error {
        explicit Error(std::string const& msg) : std::runtime_error(msg) {}
    };

    // Throw a ResultsStorage::Error in case of failure while writing
    // to any of result files
    ResultsStorage(const ActionRequest& request, const std::string& results_dir)
            : module { request.module() },
              action { request.action() },
              out_path { results_dir + "/stdout" },
              err_path { results_dir + "/stderr" },
              status_path { results_dir + "/status" },
              action_status {} {
        initialize(request, results_dir);
    }

    void write(const ActionOutcome& outcome, const std::string& exec_error,
               const std::string& duration) {
        action_status.set<std::string>("status", "completed");
        action_status.set<std::string>("duration", duration);
        action_status.set<int>("exitcode", outcome.exitcode);
        lth_file::atomic_write_to_file(action_status.toString() + "\n", status_path);

        if (exec_error.empty()) {
            if (outcome.type == ActionOutcome::Type::External) {
                lth_file::atomic_write_to_file(outcome.stdout + "\n", out_path);
                if (!outcome.stderr.empty()) {
                    lth_file::atomic_write_to_file(outcome.stderr + "\n", err_path);
                }
            } else {
                // ActionOutcome::Type::Internal
                lth_file::atomic_write_to_file(outcome.results.toString()
                                               + "\n", out_path);
            }
        } else {
            lth_file::atomic_write_to_file(exec_error, err_path);
        }
    }

  private:
    std::string module;
    std::string action;
    std::string out_path;
    std::string err_path;
    std::string status_path;
    lth_jc::JsonContainer action_status;

    void initialize(const ActionRequest& request, const std::string results_dir) {
        if (!fs::exists(results_dir)) {
            LOG_DEBUG("Creating results directory for '%1% %2%', transaction "
                       "%3%, in '%4%'", request.module(), request.action(),
                       request.transactionId(), results_dir);
            if (!fs::create_directory(results_dir)) {
                throw Error { "failed to create directory '" + results_dir + "'" };
            }
        }

        action_status.set<std::string>("module", module);
        action_status.set<std::string>("action", action);
        action_status.set<std::string>("status", "running");
        action_status.set<std::string>("duration", "0 s");
        action_status.set<int>("exitcode", EXIT_SUCCESS);

        if (!request.paramsTxt().empty()) {
            action_status.set<std::string>("input", request.paramsTxt());
        } else {
            action_status.set<std::string>("input", "none");
        }

        lth_file::atomic_write_to_file("", out_path);
        lth_file::atomic_write_to_file("", err_path);
        lth_file::atomic_write_to_file(action_status.toString() + "\n", status_path);
    }
};

//
// Non-blocking action task
//

void nonBlockingActionTask(std::shared_ptr<Module> module_ptr,
                           ActionRequest&& request,
                           std::string job_id,
                           ResultsStorage results_storage,
                           std::shared_ptr<PXPConnector> connector_ptr,
                           std::shared_ptr<std::atomic<bool>> done) {
    lth_util::Timer timer {};
    std::string exec_error {};
    ActionOutcome outcome {};

    try {
        outcome = module_ptr->executeAction(request);

        if (request.parsedChunks().data.get<bool>("notify_outcome")) {
            connector_ptr->sendNonBlockingResponse(request, outcome.results, job_id);
        }
    } catch (Module::ProcessingError& e) {
        connector_ptr->sendPXPError(request, e.what());
        exec_error = "Failed to execute '" + request.module() + " "
                     + request.action() + "': " + e.what() + "\n";
    } catch (PCPClient::connection_error& e) {
        exec_error = "Failed to send non blocking response for '"
                     + request.module() + " " + request.action() + "': "
                     + e.what() + "\n";
    }

    // Store results on disk
    auto duration = std::to_string(timer.elapsed_seconds()) + " s";
    results_storage.write(outcome, exec_error, duration);

    // Flag end of processing
    *done = true;
}

//
// Public interface
//

RequestProcessor::RequestProcessor(std::shared_ptr<PXPConnector> connector_ptr,
                                   const Configuration::Agent& agent_configuration)
        : thread_container_ { "Action Executer" },
          connector_ptr_ { connector_ptr },
          spool_dir_ { agent_configuration.spool_dir },
          modules_ {},
          modules_config_dir_ { agent_configuration.modules_config_dir },
          modules_config_ {} {
    assert(!spool_dir_.empty());

    // NB: certificate paths have been validated by HW

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
                                      const PCPClient::ParsedChunks& parsed_chunks) {
    try {
        // Inspect and validate the request message format
        ActionRequest request { request_type, parsed_chunks };

        LOG_INFO("About to process %1% request %2% by %3%, transaction %4%",
                 requestTypeNames[request_type], request.id(), request.sender(),
                 request.transactionId());

        try {
            // We can access the request content; validate it
            validateRequestContent(request);
        } catch (RequestProcessor::Error& e) {
            // Invalid request; send *PXP error*

            LOG_ERROR("Invalid %1% request %2% by %3%, transaction %4%: %5%",
                      requestTypeNames[request_type], request.id(),
                      request.sender(), request.transactionId(), e.what());
            connector_ptr_->sendPXPError(request, e.what());
            return;
        }

        try {
            if (request.type() == RequestType::Blocking) {
                processBlockingRequest(request);
            } else {
                processNonBlockingRequest(request);
            }
        } catch (std::exception& e) {
            // Process failure; send *PXP error*
            LOG_ERROR("Failed to process %1% request %2% by %3%, transaction %4%: "
                      "%5%", requestTypeNames[request.type()], request.id(),
                      request.sender(), request.transactionId(), e.what());
            connector_ptr_->sendPXPError(request, e.what());
        }
    } catch (ActionRequest::Error& e) {
        // Failed to instantiate ActionRequest - bad message; send *PCP error*

        auto id = parsed_chunks.envelope.get<std::string>("id");
        auto sender = parsed_chunks.envelope.get<std::string>("sender");
        std::vector<std::string> endpoints { sender };
        LOG_ERROR("Invalid %1% request by %2%: %3%", id, sender, e.what());
        connector_ptr_->sendPCPError(id, e.what(), endpoints);
    }
}

//
// Private interface
//

void RequestProcessor::validateRequestContent(const ActionRequest& request) {
    // Validate requested module and action
    try {
        if (!modules_.at(request.module())->hasAction(request.action())) {
            throw RequestProcessor::Error { "unknown action '" + request.action()
                                            + "' for module " + request.module() };
        }
    } catch (std::out_of_range& e) {
        throw RequestProcessor::Error { "unknown module: " + request.module() };
    }

    // Validate request input params
    try {
        LOG_DEBUG("Validating input for parameters of '%1% %2%' request %3% "
                  "by %4%, transaction %5%", request.module(), request.action(),
                  request.id(), request.sender(), request.transactionId());

        // NB: the registred schemas have the same name as the action
        auto& validator = modules_.at(request.module())->input_validator_;
        validator.validate(request.params(), request.action());
    } catch (PCPClient::validation_error& e) {
        LOG_DEBUG("Invalid '%1% %2%' request %3%: %4%", request.module(),
                  request.action(), request.id(), e.what());
        throw RequestProcessor::Error { "invalid input for '" + request.module()
                                        + " " + request.action() + "'" };
    }
}

void RequestProcessor::processBlockingRequest(const ActionRequest& request) {
    // Execute action; possible request errors will be propagated
    auto outcome = modules_[request.module()]->executeAction(request);

    connector_ptr_->sendBlockingResponse(request, outcome.results);
}

void RequestProcessor::processNonBlockingRequest(const ActionRequest& request) {
    // HERE(ale): assuming spool_dir ends with '/' (up to Configuration)
    std::string results_dir { spool_dir_ + request.transactionId() };
    std::string err_msg {};

    LOG_DEBUG("Starting '%1% %2%' job with ID %3% for non-blocking request %4% "
              "by %5%", request.module(), request.action(),
              request.transactionId(), request.id(), request.sender());

    try {
        // Flag to enable signaling from task to thread_container
        auto done = std::make_shared<std::atomic<bool>>(false);

        thread_container_.add(std::thread(&nonBlockingActionTask,
                                          modules_[request.module()],
                                          request,
                                          request.transactionId(),
                                          ResultsStorage { request, results_dir },
                                          connector_ptr_,
                                          done),
                              done);
    } catch (ResultsStorage::Error& e) {
        // Failed to instantiate ResultsStorage
        LOG_ERROR("Failed to initialize the result files for '%1% %2%' action "
                  "job with ID %3%: %4%", request.module(), request.action(),
                  request.transactionId(), e.what());
        err_msg = std::string { "failed to initialize result files: " } + e.what();
    } catch (std::exception& e) {
        LOG_ERROR("Failed to spawn '%1% %2%' action job with ID %3%: %4%",
                  request.module(), request.action(), request.transactionId(),
                  e.what());
        err_msg = std::string { "failed to start action task: " } + e.what();
    }

    if (err_msg.empty()) {
        connector_ptr_->sendProvisionalResponse(request);
    } else {
        connector_ptr_->sendPXPError(request, err_msg);
    }
}

void RequestProcessor::loadModulesConfiguration() {
    LOG_INFO("Loading external modules configuration from %1%",
             modules_config_dir_);

    if (fs::is_directory(modules_config_dir_)) {
        lth_file::each_file(
            modules_config_dir_,
            [this](std::string const& s) -> bool {
                try {
                    fs::path s_path { s };
                    auto file_name = s_path.stem().string();
                    // NB: cfg suffix guaranteed by each_file()
                    auto pos_suffix = file_name.find(".cfg");
                    auto module_name = file_name.substr(0, pos_suffix);
                    modules_config_[module_name] =
                        lth_jc::JsonContainer(lth_file::read(s));
                    LOG_DEBUG("Loaded module configuration for module '%1%' "
                              "from %2%", module_name, s);
                } catch (lth_jc::data_parse_error& e) {
                    LOG_WARNING("Cannot load module config file '%1%'. File "
                                "contains invalid json: %2%", s, e.what());
                }
                return true;
                // naming convention for config files are .cfg. Don't
                // process files that don't end in this extension
            },
            "\\.cfg$");
    } else {
        LOG_WARNING("Directory '%1%' specified by modules-config-dir doesn't "
                    "exist", modules_config_dir_);
    }
}

void RequestProcessor::loadInternalModules() {
    // HERE(ale): no external configuration for internal modules
    modules_["echo"] = std::shared_ptr<Module>(new Modules::Echo);
    modules_["ping"] = std::shared_ptr<Module>(new Modules::Ping);
    modules_["status"] = std::shared_ptr<Module>(new Modules::Status);
}

void RequestProcessor::loadExternalModulesFrom(fs::path dir_path) {
    LOG_INFO("Loading external modules from %1%", dir_path.string());

    if (fs::is_directory(dir_path)) {
        fs::directory_iterator end;

        for (auto f = fs::directory_iterator(dir_path); f != end; ++f) {
            if (!fs::is_directory(f->status())) {
                auto f_p = f->path().string();

                try {
                    ExternalModule* e_m = new ExternalModule(f_p);
                    auto config_itr = modules_config_.find(e_m->module_name);

                    if (config_itr != modules_config_.end()) {
                        e_m->validateAndSetConfiguration(config_itr->second);
                        LOG_DEBUG("The '%1%' module configuration has been "
                                  "validated: %2%", e_m->module_name,
                                  config_itr->second.toString());
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
    } else {
        LOG_WARNING("Failed to locate the modules directory; no external "
                    "module will be loaded");
    }
}

void RequestProcessor::logLoadedModules() const {
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
        LOG_INFO("Loaded '%1%' module - %2%%3%%4%",
                 module.first, txt, txt_suffix, actions_list);
    }
}

}  // namespace PXPAgent
