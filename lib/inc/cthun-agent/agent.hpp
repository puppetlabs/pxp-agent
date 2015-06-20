#ifndef SRC_AGENT_ENDPOINT_H_
#define SRC_AGENT_ENDPOINT_H_

#include <cthun-agent/module.hpp>
#include <cthun-agent/request_processor.hpp>
#include <cthun-agent/action_request.hpp>

#include <cthun-client/connector/connector.hpp>
#include <cthun-client/protocol/chunks.hpp>      // ParsedChunk
#include <cthun-client/protocol/message.hpp>     // schema names
#include <cthun-client/validator/schema.hpp>     // ContentType, Schema

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>

#include <map>
#include <memory>
#include <string>

namespace CthunAgent {

class Agent {
  public:
    Agent() = delete;

    Agent(const std::string& modules_dir,
          const std::string& server_url,
          const std::string& ca_crt_path,
          const std::string& client_crt_path,
          const std::string& client_key_path);

    // Start the agent and loop indefinetely, by:
    //  - loading the internal and external modules;
    //  - retrieving the agent identity from the certificate file;
    //  - establishing the underlying communications layer connection;
    //  - connecting to the Cthun server;
    //  - setting the handlers to process incoming requests.
    //
    // Throw a fatal_error in case of unexpected failures; errors
    // such as message sending failures are only logged.
    void start();

  private:
    // Cthun connector
    std::shared_ptr<CthunClient::Connector> connector_ptr_;

    // Modules
    std::map<std::string, std::shared_ptr<Module>> modules_;

    // Action Executer
    RequestProcessor request_processor_;

    // Load the modules from the src/modules directory.
    void loadInternalModules();

    // Load the external modules contained in the specified directory.
    void loadExternalModulesFrom(boost::filesystem::path modules_dir_path);

    // Log the loaded modules.
    void logLoadedModules() const;

    // Callback for CthunClient::Connector handling incoming RPC
    // blocking requests; it will execute the requested action and,
    // once finished, reply to the sender with an RPC blocking
    // response containing the action outcome.
    void blockingRequestCallback(const CthunClient::ParsedChunks& parsed_chunks);

    // Callback for CthunClient::Connector handling incoming RPC
    // non-blocking requests; it will start a job for the requested
    // action and reply with a provisional response containing the job
    // id. The reults will be stored in files in spool-dir.
    // In case the request has the notify_outcome field flagged, it
    // will send an RPC non-blocking response containing the action
    // outcome when finished.
    void nonBlockingRequestCallback(const CthunClient::ParsedChunks& parsed_chunks);

    void validateAndProcessRequest(const RequestType& request_type,
                                   const CthunClient::ParsedChunks& parsed_chunks);

    void validateRequestContent(const ActionRequest& request);
};

}  // namespace CthunAgent

#endif  // SRC_AGENT_ENDPOINT_H_
