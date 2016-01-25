require 'pxp-agent/config_helper.rb'
require 'pcp/client'

# This file contains general test helper methods for pxp-agent acceptance tests

# Some helpers for working with the log file
PXP_LOG_FILE_CYGPATH = '/cygdrive/c/ProgramData/PuppetLabs/pxp-agent/var/log/pxp-agent.log'
PXP_LOG_FILE_POSIX = '/var/log/puppetlabs/pxp-agent/pxp-agent.log'

PXP_AGENT_LOG_ENTRY_WEBSOCKET_SUCCESS = 'INFO.*Successfully established a WebSocket connection with the PCP broker.*'
PXP_AGENT_LOG_ENTRY_ASSOCIATION_SUCCESS = 'INFO.*Received Associate Session response.*success'

# @param host the beaker host you want the path to the log file on
# @return The path to the log file on the host. For Windows, a cygpath is returned
def logfile(host)
  windows?(host)?
    PXP_LOG_FILE_CYGPATH :
    PXP_LOG_FILE_POSIX
end

# A general helper for waiting for a file to contain some string, and failing the test if it doesn't appear
# @param host the host to check the file on
# @param file path to file to check
# @param expected the string or pattern expected in the file
# @param seconds number of seconds to retry (one retry per second). Default 30.
def expect_file_on_host_to_contain(host, file, expected, seconds=30)
  # If the expected file entry does not appear in the file within 30 seconds, then do an explicit assertion
  # that the file should contain the expected text, so we get a test fail (not an unhandled error), and the
  # log contents will appear in the test failure output
  begin
    retry_on(host, "grep '#{expected}' #{file}", {:max_retries => 30,
                                                     :retry_interval => 1})
  rescue
    on(host, "cat #{file}") do |result|
      assert_match(expected, result.stdout,
                  "Expected text '#{expected}' did not appear in file #{file}")
    end
  end
end

# Some helpers for working with a pcp-broker 'lein tk' instance
def run_pcp_broker(host)
  on(host, 'cd ~/pcp-broker; export LEIN_ROOT=ok; lein tk </dev/null >/var/log/pcp-broker.log 2>&1 &')
  assert(port_open_within?(host, PCP_BROKER_PORT, 60),
         "pcp-broker port #{PCP_BROKER_PORT.to_s} not open within 1 minutes of starting the broker")
end

def kill_pcp_broker(host)
  on(host, "ps -C java -f | grep pcp-broker | sed 's/[^0-9]*//' | cut -d\\  -f1") do |result|
    pid = result.stdout.chomp
    on(host, "kill -9 #{pid}")
  end
end

# Make an rpc_blocking_request to pxp-agent
# Reference: https://github.com/puppetlabs/pcp-specifications/blob/master/pxp/request_response.md
# @param broker the hostname or beaker host object of the machine running PCP broker
# @param targets array of PCP identities to send request to
#                e.g. ["pcp://client01.example.com/agent","pcp://client02.example.com/agent"]
# @param pxp_module which PXP module to call, default pxp-module-puppet
# @param action which action in the PXP module to call, default run
# @param params params to send to the module. e.g for pxp-module-puppet:
#               {:env => [], :flags => ['--noop', '--onetime', '--no-daemonize']}
# @return hash of responses, with target identities as keys, and value being the response message for that identity as a hash
# @raise String indicating something went wrong, test case can use to fail
def rpc_blocking_request(broker, targets,
                         pxp_module = 'pxp-module-puppet', action = 'run',
                         params)
  # Event machine is required by the ruby-pcp-client gem
  # https://github.com/puppetlabs/ruby-pcp-client
  em = Thread.new { EM.run }

  mutex = Mutex.new
  have_response = ConditionVariable.new
  responses = Hash.new

  client = PCP::Client.new({
    :server => broker_ws_uri(broker),
    :ssl_cert => "../test-resources/ssl/certs/controller01.example.com.pem",
    :ssl_key => "../test-resources/ssl/private_keys/controller01.example.com.pem"
  })

  client.on_message = proc do |message|
    mutex.synchronize do
      resp = {
        :envelope => message.envelope,
        :data     => JSON.load(message.data),
        :debug    => JSON.load(message.debug)
      }
      responses[resp[:envelope][:sender]] = resp
      print resp
      have_response.signal
    end
  end

  client.connect()
  if !client.associated?
    raise "Controller PCP client failed to associate with pcp-broker on #{broker}"
  end

  message = PCP::Message.new({
    :message_type => 'http://puppetlabs.com/rpc_blocking_request',
    :targets => targets
  })

  message.data = {
    :transaction_id => SecureRandom.uuid,
    :module         => pxp_module,
    :action         => action,
    :params         => params
  }.to_json

  message_expiry = 10 # Seconds for the PCP message to be considered failed
  rpc_action_expiry = 60 # Seconds for the entire RPC action to be considered failed
  message.expires(message_expiry)

  client.send(message)

  begin
    Timeout::timeout(rpc_action_expiry) do
      done = false
      loop do
        mutex.synchronize do
          have_response.wait(mutex)
          done = have_all_rpc_responses?(targets, responses)
        end
        break if done
      end
    end
  rescue Timeout::Error
    mutex.synchronize do
      if !have_all_rpc_responses?
        raise "Didn't receive all PCP responses when requesting puppet run on #{targets}. Responses received were: #{responses.to_s}"
      end
    end
  end # wait for message

  em.exit

  responses

end

# Quick test of RPC targets vs RPC responses to check if we have them all yet
def have_all_rpc_responses?(targets, responses)
  targets.each do |target|
    if !responses.has_key?(target)
      return false
    end
  end
  return true
end
