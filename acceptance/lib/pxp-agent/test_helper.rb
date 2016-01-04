require 'pxp-agent/config_helper.rb'

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
