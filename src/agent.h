#ifndef SRC_AGENT_ENDPOINT_H_
#define SRC_AGENT_ENDPOINT_H_

#include "src/module.h"
#include "src/message.h"
#include "src/data_container.h"
#include "src/websocket/endpoint.h"

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>

#include <map>
#include <memory>
#include <string>

namespace CthunAgent {

class Agent {
  public:
    Agent() = delete;

    // Throw a fatal_error in case it fails to open the client
    // certificate file to load its identity.
    Agent(std::string bin_path,
          std::string ca_crt_path,
          std::string client_crt_path,
          std::string client_key_path);

    ~Agent();

    // Daemon entry point.
    void startAgent(std::string server_url);

    // Identity getter.
    std::string getIdentity() const;

  private:
    // Certificate paths.
    std::string ca_crt_path_;
    std::string client_crt_path_;
    std::string client_key_path_;

    // Cthun agent identity, as in the cert.
    std::string identity_;

    // Modules
    std::map<std::string, std::shared_ptr<Module>> modules_;

    // Pointer to the ransport layer endpoint instance.
    std::unique_ptr<WebSocket::Endpoint> ws_endpoint_ptr_;

    // Load the external modules contained in the specified directory.
    void loadExternalModules_(boost::filesystem::path modules_dir_path);

    // Log the loaded modules.
    void listModules_() const;

    // Set the event callbacks for the connection.
    void setConnectionCallbacks_();

    // Callback for the transport layer triggered when  connection .
    // Send a login message.
    void sendLogin_();

    // Add the common envelope entries to the specified container.
    void addCommonEnvelopeEntries_(DataContainer& envelope_entries);

    // Send a response message with specified request ID and output
    // to the receiver endpoint.
    void sendResponse_(std::string receiver_endpoint,
                       std::string request_id,
                       DataContainer output,
                       std::vector<MessageChunk> debug_chunks);

    // Callback for the transport layer to handle incoming messages.
    // Parse and validate the passed message; reply to the sender
    // with the requested output.
    void processMessageAndSendResponse_(std::string message_txt);

    // Periodically check the connection state; reconnect the agent
    // in case the connection is not open
    void monitorConnectionState_();
};

}  // namespace CthunAgent

#endif  // SRC_AGENT_ENDPOINT_H_
