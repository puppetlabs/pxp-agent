#include <cthun-agent/request_processor.hpp>
#include <cthun-agent/action_outcome.hpp>
#include <cthun-agent/configuration.hpp>
#include <cthun-agent/file_utils.hpp>
#include <cthun-agent/rpc_schemas.hpp>
#include <cthun-agent/string_utils.hpp>
#include <cthun-agent/timer.hpp>
#include <cthun-agent/uuid.hpp>

#include <boost/filesystem.hpp>

#define LEATHERMAN_LOGGING_NAMESPACE "puppetlabs.cthun_agent.action_executer"
#include <leatherman/logging/logging.hpp>

#include <vector>
#include <atomic>

namespace CthunAgent {

// Utility free function

std::vector<CthunClient::DataContainer> wrapDebug(
                        const CthunClient::ParsedChunks& parsed_chunks) {
    auto request_id = parsed_chunks.envelope.get<std::string>("id");
    if (parsed_chunks.num_invalid_debug) {
        LOG_WARNING("Message %1% contained %2% bad debug chunk%3%",
                    request_id, parsed_chunks.num_invalid_debug,
                    StringUtils::plural(parsed_chunks.num_invalid_debug));
    }
    std::vector<CthunClient::DataContainer> debug {};
    for (auto& debug_entry : parsed_chunks.debug) {
        debug.push_back(debug_entry);
    }
    return debug;
}

// Action task

void nonBlockingActionTask(std::shared_ptr<Module> module_ptr,
                           ActionRequest request,
                           std::string job_id,
                           std::string results_dir,
                           std::shared_ptr<CthunClient::Connector> connector_ptr,
                           std::shared_ptr<std::atomic<bool>> done) {
    // Initialize result files
    CthunClient::DataContainer action_status {};
    action_status.set<std::string>("module", request.module());
    action_status.set<std::string>("action", request.action());
    action_status.set<std::string>("status", "running");
    action_status.set<std::string>("duration", "0 s");

    if (!request.paramsTxt().empty()) {
        action_status.set<std::string>("input", request.paramsTxt());
    } else {
        action_status.set<std::string>("input", "none");
    }

    FileUtils::writeToFile("", results_dir + "/stdout");
    FileUtils::writeToFile("", results_dir + "/stderr");
    FileUtils::writeToFile(action_status.toString() + "\n",
                           results_dir + "/status");

    // Execute action
    CthunAgent::Timer timer {};
    std::string err_msg {};
    ActionOutcome outcome {};

    try {
        outcome = module_ptr->executeAction(request.action(),
                                            request.parsedChunks());

        if (request.parsedChunks().data.get<bool>("notify_outcome")) {
            // Send back results
            CthunClient::DataContainer response_data {};
            response_data.set<std::string>("transaction_id",
                                           request.transactionId());
            response_data.set<std::string>("job_id", job_id);
            response_data.set<CthunClient::DataContainer>("results",
                                                          outcome.results);

            try {
                // NOTE(ale): debug was sent in provisional response
                connector_ptr->send(std::vector<std::string> { request.sender() },
                                    RPCSchemas::NON_BLOCKING_RESPONSE_TYPE,
                                    DEFAULT_MSG_TIMEOUT_SEC,
                                    response_data);
                LOG_INFO("Sent response for non-blocking request %1% by %2%, "
                         "transaction %3%", request.id(), request.sender(),
                         request.transactionId());
            } catch (CthunClient::connection_error& e) {
                LOG_ERROR("Failed to reply to non-blocking request %1% by %2%, "
                          "transaction %3% (no further attempts): %4%",
                          request.id(), request.sender(), request.transactionId(),
                          e.what());
            }
        }
    } catch (request_error& e) {
        err_msg = e.what();

        // Send back an RPC error message
        CthunClient::DataContainer rpc_error_data {};
        rpc_error_data.set<std::string>("transaction_id", request.transactionId());
        rpc_error_data.set<std::string>("id", request.id());
        rpc_error_data.set<std::string>("description", e.what());

        try {
            connector_ptr->send(std::vector<std::string> { request.sender() },
                                RPCSchemas::RPC_ERROR_MSG_TYPE,
                                DEFAULT_MSG_TIMEOUT_SEC,
                                rpc_error_data);
            LOG_INFO("Replied to non-blocking request %1% by %2%, transaction "
                     "%3%, job ID %4%, with an RPC error message", request.id(),
                     request.sender(), request.transactionId(), job_id);
        } catch (CthunClient::connection_error& e) {
            LOG_ERROR("Failed to send RPC error message for non-blocking request "
                      "%1% by %2%, transaction %3%, job ID %4% (no further "
                      "attempts): %5%", request.id(), request.sender(),
                      request.transactionId(), job_id, e.what());
        }
    }

    // Store results on disk
    std::string duration { std::to_string(timer.elapsedSeconds()) + " s" };
    action_status.set<std::string>("status", "completed");
    action_status.set<std::string>("duration", duration);
    FileUtils::writeToFile(action_status.toString() + "\n",
                           results_dir + "/status");
    if (err_msg.empty()) {
        if (outcome.type == ActionOutcome::Type::External) {
            FileUtils::writeToFile(outcome.stdout + "\n",
                                   results_dir + "/stdout");
            if (!outcome.stderr.empty()) {
                FileUtils::writeToFile(outcome.stderr + "\n",
                                       results_dir + "/stderr");
            }
        } else {
            // ActionOutcome::Type::Internal
            FileUtils::writeToFile(outcome.results.toString() + "\n",
                                   results_dir + "/stdout");
        }
    } else {
        err_msg = "Failed to execute '" + request.module() + " "
                  + request.action() + "': " + err_msg;
        FileUtils::writeToFile(err_msg + "\n", results_dir + "/stderr");
    }

    // Flag end of processing
    *done = true;
}

//
// Public interface
//

RequestProcessor::RequestProcessor(
                        std::shared_ptr<CthunClient::Connector> connector_ptr)
        : connector_ptr_ { connector_ptr },
          spool_dir_ { Configuration::Instance().get<std::string>("spool-dir") },
          thread_container_ { "Action Executer" } {
    if (!boost::filesystem::exists(spool_dir_)) {
        LOG_INFO("Creating spool directory '%1%'", spool_dir_);
        if (!FileUtils::createDirectory(spool_dir_)) {
            throw fatal_error { "failed to create the results directory '"
                                + spool_dir_ + "'" };
        }
    }
}

void RequestProcessor::processBlockingRequest(std::shared_ptr<Module> module_ptr,
                                              const ActionRequest& request) {
    // Execute action; possible request errors will be propagated
    auto outcome = module_ptr->executeAction(request.action(), request.parsedChunks());

    // Send back response
    auto debug = wrapDebug(request.parsedChunks());
    CthunClient::DataContainer response_data {};
    response_data.set<std::string>("transaction_id", request.transactionId());
    response_data.set<CthunClient::DataContainer>("results", outcome.results);

    try {
        connector_ptr_->send(std::vector<std::string> { request.sender() },
                             RPCSchemas::BLOCKING_RESPONSE_TYPE,
                             DEFAULT_MSG_TIMEOUT_SEC,
                             response_data,
                             debug);
    } catch (CthunClient::connection_error& e) {
        // We failed to send the response; it's up to the requester to
        // request the action again
        LOG_ERROR("Failed to reply to blocking request %1% from %2%, "
                  "transaction %3%: %4%", request.id(), request.sender(),
                  request.transactionId(), e.what());
    }
}

void RequestProcessor::processNonBlockingRequest(std::shared_ptr<Module> module_ptr,
                                                 const ActionRequest& request) {
    auto job_id = UUID::getUUID();

    // HERE(ale): assuming spool_dir ends with '/' (up to Configuration)
    std::string results_dir { spool_dir_ + job_id };

    if (!boost::filesystem::exists(results_dir)) {
        LOG_DEBUG("Creating results directory for the '%1% %2%' job with ID %3% "
                  "for request transaction %4%", request.module(),
                  request.action(), job_id, request.transactionId());
        if (!FileUtils::createDirectory(results_dir)) {
            throw request_processing_error { "failed to create directory '"
                                             + results_dir + "'" };
        }
    }

    // Spawn action task
    LOG_DEBUG("Starting '%1% %2%' job with ID %3% for non-blocking request %4% "
              "by %5%, transaction %6%",request.module(), request.action(),
              job_id, request.id(), request.sender(), request.transactionId());
    std::string err_msg {};

    try {
        // Flag to enable signaling from task to thread_container
        auto done = std::make_shared<std::atomic<bool>>(false);

        thread_container_.add(std::thread(&nonBlockingActionTask,
                                          module_ptr,
                                          request,
                                          job_id,
                                          results_dir,
                                          connector_ptr_,
                                          done),
                              done);
    } catch (std::exception& e) {
        LOG_ERROR("Failed to spawn '%1% %2%' action job with ID %3%: %4%",
                  request.module(), request.action(), job_id, e.what());
        err_msg = std::string { "failed to start action task: " } + e.what();
    }

    // Send back provisional data
    auto debug = wrapDebug(request.parsedChunks());
    CthunClient::DataContainer provisional_data {};
    provisional_data.set<std::string>("transaction_id", request.transactionId());
    provisional_data.set<bool>("success", err_msg.empty());
    provisional_data.set<std::string>("job_id", job_id);
    if (!err_msg.empty()) {
        provisional_data.set<std::string>("error", err_msg);
    }

    try {
        connector_ptr_->send(std::vector<std::string> { request.sender() },
                             RPCSchemas::PROVISIONAL_RESPONSE_TYPE,
                             DEFAULT_MSG_TIMEOUT_SEC,
                             provisional_data,
                             debug);
        LOG_INFO("Sent provisional response for request %1% by %2%, "
                 "transaction %3%", request.id(), request.sender(),
                 request.transactionId());
    } catch (CthunClient::connection_error& e) {
        LOG_ERROR("Failed to send provisional response for request %1% by "
                  "%2%, transaction %3% (no further attempts): %4%",
                  request.id(), request.sender(), request.transactionId(), e.what());
    }
}

}  // namespace CthunAgent
