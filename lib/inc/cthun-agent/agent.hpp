#ifndef SRC_AGENT_ENDPOINT_H_
#define SRC_AGENT_ENDPOINT_H_

#include <cthun-agent/module.hpp>

#include <cthun-client/connector/connector.hpp>
#include <cthun-client/message/chunks.hpp>       // ParsedChunk
#include <cthun-client/message/message.hpp>      // schema names
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
    CthunClient::Connector connector_;

    // Modules
    std::map<std::string, std::shared_ptr<Module>> modules_;

    // Load the modules from the src/modules directory.
    void loadInternalModules();

    // Load the external modules contained in the specified directory.
    void loadExternalModulesFrom(boost::filesystem::path modules_dir_path);

    // Log the loaded modules.
    void logLoadedModules() const;

    // Returns the json validation schema for a cnc request.
    CthunClient::Schema getCncRequestSchema() const;

    // Callback for the CthunClient::Connector to handle incoming
    // messages. It will reply to the sender with the requested
    // output.
    // Throw a request_validation error in case: no parsed data; data
    // is not in JSON format; unknown module.
    void cncRequestCallback(const CthunClient::ParsedChunks& parsed_chunks);
};

}  // namespace CthunAgent

#endif  // SRC_AGENT_ENDPOINT_H_
