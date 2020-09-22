require 'pxp-agent/config_helper.rb'
require 'pcp/client'
require 'net/http'
require 'openssl'
require 'socket'
require 'json'
require 'securerandom'

# This file contains general test helper methods for pxp-agent acceptance tests

# The standard path for git checkouts is where pcp-broker will be
GIT_CLONE_FOLDER = '/opt/puppet-git-repos'

# Some helpers for working with the log file
PXP_LOG_DIR_CYGPATH = '/cygdrive/c/ProgramData/PuppetLabs/pxp-agent/var/log'
PXP_LOG_DIR_POSIX = '/var/log/puppetlabs/pxp-agent'

# Set lein profile based on configuration to access puppet.net internal network or not.
LEIN_PROFILE = (ENV['GEM_SOURCE'] =~ /puppetlabs.net/) ? 'internal-integration' : 'integration'

def logdir(host)
  windows?(host)?
    PXP_LOG_DIR_CYGPATH :
    PXP_LOG_DIR_POSIX
end

# @param host the beaker host you want the path to the log file on
# @return The path to the log file on the host. For Windows, a cygpath is returned
def logfile(host)
  logdir(host)+'/pxp-agent.log'
end

def reset_logfile(host)
  old_logfile = logfile(host)
  new_logfile = old_logfile+'.'+SecureRandom.uuid
  logger.notify "---- Saving pxp-agent logfile to #{new_logfile}"
  retry_on(host, "test ! -f #{old_logfile} || mv #{old_logfile} #{new_logfile}")
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

def append_jvm_limits(command)
  "export JVM_OPTS=\"-Xms2g -Xmx2g\"; export LEIN_JVM_OPTS=\"-Xms2g -Xmx2g\"; #{command}"
end

# Some helpers for working with a pcp-broker 'lein tk' instance
def run_pcp_broker(host, instance=0)
  timeout = 120
  host[:pcp_broker_instance] = instance

  lein_command = "cd #{GIT_CLONE_FOLDER}/pcp-broker#{instance}; export LEIN_ROOT=ok; \
     lein with-profile #{LEIN_PROFILE} tk </dev/null >/var/log/puppetlabs/pcp-broker.log.#{SecureRandom.uuid} 2>&1 &"
  lein_command = append_jvm_limits(lein_command) if host[:gke_container]
  on(host, lein_command)

  assert(port_open_within?(host, PCP_BROKER_PORTS[instance], timeout),
         "pcp-broker port #{PCP_BROKER_PORTS[instance].to_s} not open within #{(timeout/60).to_s} minutes of starting the broker")
  broker_state = nil
  attempts = 0
  until broker_state == "running" or attempts == 100 do
    broker_state = get_pcp_broker_status(host)
    if broker_state != "running"
      attempts += 1
      sleep 0.5
    end
  end
  assert_equal("running", broker_state, "Shortly after startup, pcp-broker should report its state as being 'running'")
end

def kill_all_pcp_brokers(host)
  on(host, "ps -C java -f | grep pcp-broker | sed 's/[^0-9]*//' | cut -d\\  -f1") do |result|
    pids = result.stdout.chomp.split("\n")
    pids.each do |pid|
      # Send SIGKILL (9); not all shells support '-s KILL'
      on(host, "kill -9 #{pid}")
    end
  end
end

def block_pcp_broker(host, instance)
  on(host, "iptables -A INPUT -p tcp --destination-port #{PCP_BROKER_PORTS[instance].to_s} -j DROP; iptables -S")
end

def unblock_pcp_broker(host, instance)
  host[:pcp_broker_instance] = instance
  on(host, "iptables -D INPUT -p tcp --destination-port #{PCP_BROKER_PORTS[instance].to_s} -j DROP; iptables -S")
end

def get_pcp_broker_status(host)
  uri = URI.parse("https://#{host}:#{PCP_BROKER_PORTS[host[:pcp_broker_instance]]}/status/v1/services/broker-service")
  begin
    http = Net::HTTP.new(uri.host, uri.port)
    http.use_ssl = true
    http.verify_mode = OpenSSL::SSL::VERIFY_NONE
    res = http.get(uri.request_uri)
    document = JSON.load(res.body)
    return document["state"]
  rescue => e
    logger.trace "Could not get pcp-broker status. This may happen if pcp-broker is still starting up. Exception is: #{e.inspect}"
    return nil
  end
end

# Start an eventmachine reactor in a thread, if one is not already running
# @return Thread object where the EM reactor should be running
def assert_eventmachine_thread_running
  if EventMachine.reactor_running?
    return EventMachine.reactor_thread
  end

  em_thread = Thread.new do
    begin
      EventMachine.run
    rescue Exception => e
      logger.error "Problem in eventmachine reactor thread: #{e.message} Backtrace: #{e.backtrace.join("\n\t")}"
    end
  end

  # busy wait this thread until reactor has started
  Thread.pass until EventMachine.reactor_running?
  em_thread
end

# Query pcp-broker's inventory of associated clients
# Reference: https://github.com/puppetlabs/pcp-specifications/blob/master/pcp/versions/1.0/inventory.md
# @param broker hostname or beaker host object of the pcp-broker host
# @param query pattern of client identities that you want to check e.g. pcp://*/agent for all agents, or pcp://*/* for everything
# @return Ruby array of strings that are the client identities that match the query pattern
# @raise Exception if pcp-client cannot connect to make the inventory request, does not receive a response, or the response is missing
#        the required [:data]["uris"] property
def pcp_broker_inventory(broker, query)
  mutex = Mutex.new
  have_response = ConditionVariable.new
  response = nil

  client = connect_pcp_client(broker)

  client.on_message = proc do |message|
    mutex.synchronize do
      resp = {
        :envelope => message.envelope,
        :data     => JSON.load(message.data)
      }
      response = resp
      have_response.signal
    end
  end

  message = PCP::Message.new({
    :message_type => 'http://puppetlabs.com/inventory_request',
    :targets => ["pcp:///server"]
  })

  message.data = {
    :query => [query]
  }.to_json

  message_expiry = 10 # Seconds for the PCP message to be considered failed
  inventory_expiry = 60 # Seconds for the inventory request to be considered failed
  message.expires(message_expiry)

  client.send(message)

  begin
    Timeout::timeout(inventory_expiry) do
      loop do
        mutex.synchronize do
          have_response.wait(mutex)
        end
        break if response
      end
    end
  rescue Timeout::Error
    raise "Didn't receive a response for PCP inventory request in #{inventory_expiry} seconds"
  ensure
    client.close
  end # wait for message

  if(!response.has_key?(:data))
    raise 'Response to PCP inventory request is missing :data'
  end
  if(!response[:data].has_key?("uris"))
    raise 'Response to PCP inventory request is missing an array of uri\'s'
  end
  response[:data]["uris"]
end

# Check if a PCP identity is present or absent from a pcp-broker's inventory
# Retries several times to allow for pxp-agent service to start up or shut down
# Note: if you expect an identity to not be associated; instead use is_not_associated? to avoid needless retries
# @param broker hostname or beaker host object of the machine running PCP broker
# @param identity PCP identity of the client/agent to check e.g. "pcp://client01.example.com/agent"
# @param retries the number of times to retry checking the inventory. Default 60 to allow for slow test VMs. Set to 0 to only check once.
# @return if the identity is in the broker's inventory within the allowed number of retries
#         false if the identity remains absent from the broker's inventory after the allowed number of retries
PCP_INVENTORY_RETRIES = 60
def is_associated?(broker, identity, retries = PCP_INVENTORY_RETRIES)
  if retries == 0
    return pcp_broker_inventory(broker,identity).include?(identity)
  end
  retries.times do
    if pcp_broker_inventory(broker,identity).include?(identity)
      return true
    end
    sleep 1
  end
  return false
end

# Check if a PCP identity is present or absent from a pcp-broker's inventory
# Retries several times to allow for pxp-agent service to start up or shut down
# @param broker hostname or beaker host object of the machine running PCP broker
# @param identity PCP identity of the client/agent to check e.g. "pcp://client01.example.com/agent"
# @param retries the number of times to retry checking the inventory. Default 60 to allow for slow test VMs. Set to 0 to only check once.
# @return true if the identity is absent from the broker's inventory within the allowed number of retries
#         false if the identity persists in the broker's inventory after the allowed number of retries
def is_not_associated?(broker, identity, retries = PCP_INVENTORY_RETRIES)
  if retries == 0
    return !pcp_broker_inventory(broker,identity).include?(identity)
  end
  retries.times do
    if !pcp_broker_inventory(broker,identity).include?(identity)
      return true
    end
    sleep 1
  end
  return false
end

# Make an rpc_blocking_request to pxp-agent
# Reference: https://github.com/puppetlabs/pcp-specifications/blob/master/pxp/versions/1.0/request_response.md
# @param broker hostname or beaker host object of the machine running PCP broker
# @param targets array of PCP identities to send request to
#                e.g. ["pcp://client01.example.com/agent","pcp://client02.example.com/agent"]
# @param pxp_module which PXP module to call, default pxp-module-puppet
# @param action which action in the PXP module to call, default run
# @param params params to send to the module. e.g for pxp-module-puppet:
#               {:env => [], :flags => ['--noop', '--onetime', '--no-daemonize']}
# @return hash of responses, with target identities as keys, and value being the response message for that identity as a hash
# @raise String indicating something went wrong, test case can use to fail
def rpc_blocking_request(broker, targets,
                         pxp_module = 'pxp-module-puppet', action = 'run',
                         params = {})
  rpc_request(broker, targets, pxp_module, action, params, true)
end

# Make an rpc_non_blocking_request to pxp-agent
# Reference: https://github.com/puppetlabs/pcp-specifications/blob/master/pxp/versions/1.0/request_response.md
# @param broker hostname or beaker host object of the machine running PCP broker
# @param targets array of PCP identities to send request to
#                e.g. ["pcp://client01.example.com/agent","pcp://client02.example.com/agent"]
# @param pxp_module which PXP module to call, default pxp-module-puppet
# @param action which action in the PXP module to call, default run
# @param params params to send to the module. e.g for pxp-module-puppet:
#               {:env => [], :flags => ['--noop', '--onetime', '--no-daemonize']}
# @return hash of responses, with target identities as keys, and value being the response message as a hash
#         Responses should be of message type http://puppetlabs.com/rpc_provisional_response
# @raise String indicating something went wrong, test case can use to fail
def rpc_non_blocking_request(broker, targets,
                         pxp_module = 'pxp-module-puppet', action = 'run',
                         params = {})
  rpc_request(broker, targets, pxp_module, action, params, false)
end

def rpc_request(broker, targets,
                pxp_module = 'pxp-module-puppet', action = 'run',
                params = {} , blocking = false, transaction_id = nil)
  transaction_id ||= SecureRandom.uuid
  mutex = Mutex.new
  have_response = ConditionVariable.new
  responses = Hash.new

  client = connect_pcp_client(broker)

  client.on_message = proc do |message|
    mutex.synchronize do
      resp = {
        :envelope => message.envelope,
        :data     => JSON.load(message.data)
      }
      responses[resp[:envelope][:sender]] = resp
      logger.debug "Received response from #{resp[:envelope][:sender]}: #{resp}"
      have_response.signal
    end
  end

  message_data = {
    :transaction_id => transaction_id,
    :module         => pxp_module,
    :action         => action,
    :params         => params
  }
  if !blocking then
    message_data[:notify_outcome] = false
  end

  targets.each do |target|
    message = PCP::Message.new({
      :message_type => blocking ? 'http://puppetlabs.com/rpc_blocking_request' : 'http://puppetlabs.com/rpc_non_blocking_request',
      :targets => [target]
    })

    message.data = message_data.to_json

    message_expiry = 10 # Seconds for the PCP message to be considered failed
    message.expires(message_expiry)

    client.send(message)
  end

  rpc_action_expiry = 360 # Seconds for the entire RPC action to be considered failed

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
      if !have_all_rpc_responses?(targets, responses)
        raise "Didn't receive all PCP responses when requesting #{pxp_module} #{action} on #{targets}. Responses received were: #{responses.to_s}"
      end
    end
  ensure
    client.close
  end # wait for message

  responses
end

# Connect a ruby-pcp-client instance to pcp-broker
# @param broker hostname or beaker host object of the machine running PCP broker
# @return instance of pcp-client associated with the broker
# @raise exception if could not connect or client is not associated with broker after connecting
def connect_pcp_client(broker)
  client = nil
  retries = 0
  max_retries = 10
  connected = false
  hostname = Socket.gethostname
  until (connected || retries == max_retries) do
    # Event machine is required by the ruby-pcp-client gem
    # https://github.com/puppetlabs/ruby-pcp-client
    assert_eventmachine_thread_running

    client_type = "ruby-pcp-client-#{$$}"
    client = PCP::Client.new({
      :server => broker_ws_uri(broker, 1)+client_type,
      :ssl_ca_cert => "tmp/ssl/certs/ca.pem",
      :ssl_cert => "tmp/ssl/certs/#{hostname.downcase}.pem",
      :ssl_key => "tmp/ssl/private_keys/#{hostname.downcase}.pem",
      :loglevel => logger.is_debug? ? Logger::DEBUG : Logger::WARN,
      :type => client_type
    })
    connected = client.connect(5)
    retries += 1

    # If the connection was not established, close it. Otherwise we can end up with 2 successful connections
    # and get an earlier attempt superseding later connections.
    client.close if !connected
  end
  if !connected
    raise "Controller PCP client failed to connect with pcp-broker on #{broker} after #{max_retries} attempts: #{client.inspect}"
  end
  if !client.associated?
    client.close
    raise "Controller PCP client failed to associate with pcp-broker on #{broker} #{client.inspect}"
  end

  client
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

# A suite of functions for creating a manifest that sleeps for a long period
# and interacting with Puppet runs of that manifest.
def create_sleep_manifest(master, manifest, seconds_to_sleep)
    create_remote_file(master, manifest, <<-MODULEPP)
node default {
  case $::osfamily {
    'windows': { exec { 'sleep': command => 'C:/Windows/system32/ping.exe 127.0.0.1 -n #{seconds_to_sleep}', returns => 1 } }
    'Darwin':  { exec { 'sleep': command => '/bin/sleep #{seconds_to_sleep} || /usr/bin/true', } }
    default:   { exec { 'sleep': command => '/bin/sleep #{seconds_to_sleep} || /bin/true', } }
  }
}
MODULEPP
    on(master, "chmod 644 #{manifest}")
end

def get_process_pids(host, process)
  pids = []

  case host['platform']
  when /osx/
    command = "ps -e -o pid,command | grep '#{process}' | grep -v 'grep' | sed 's/^[^0-9]*//g' | cut -d\\  -f1"
  when /win/
    if process == 'puppet agent'
      # Puppet agent just appears as Ruby.exe in cygwin ps
      # Need to check ruby's command line string to check it is actually puppet agent
      # because pxp-module-puppet will also appear in ps as Ruby.exe
      command = "cmd.exe /C WMIC path win32_process WHERE Name=\\\"Ruby.exe\\\" get CommandLine,ProcessId | "\
        "grep 'puppet(\")? agent' | egrep -o '[0-9]+\s*$'"
    else
      command = "ps -eW | grep -E '\\\\#{process}\(.exe\)' | sed 's/^[^0-9]*//g' | cut -d\\  -f1"
    end
  else
    command = "ps -ef | grep '#{process}' | grep -v 'grep' | grep -v 'true' | sed 's/^[^0-9]*//g' | cut -d\\  -f1"
  end

  on(host, command, :accept_all_exit_codes => true) do |output|
    pids = output.stdout.chomp.split
  end

  pids
end

def wait_for_sleep_process(target, seconds_to_sleep)
  begin
    ps_cmd = target['platform'] =~ /win/ ? 'ps -efW' : 'ps -ef'
    sleep_process = target['platform'] =~ /win/ ? 'PING' : " /bin/sleep #{seconds_to_sleep}"
    retry_on(target, "#{ps_cmd} | grep '#{sleep_process}' | grep -v 'grep'", {:max_retries => 120, :retry_interval => 1})
  rescue
    raise("After triggering a puppet run on #{target} the expected sleep process did not appear in process list")
  end
end

def stop_sleep_process(targets, seconds_to_sleep, accept_no_pid_found = false)
  targets = [targets].flatten

  targets.each do |target|
    case target['platform']
    when /osx/
      command = "ps -e -o pid,comm | grep sleep | sed 's/^[^0-9]*//g' | cut -d\\  -f1"
    when /win/
      command = "ps -efW | grep PING | sed 's/^[^0-9]*[0-9]*[^0-9]*//g' | cut -d ' ' -f1"
    else
      command = "ps -ef | grep ' /bin/sleep #{seconds_to_sleep}' | grep -v 'grep' | grep -v 'true' | sed 's/^[^0-9]*//g' | cut -d\\  -f1"
    end

    # A failed test may leave an orphaned sleep process, handle multiple matches.
    pids = nil
    on(target, command) do |output|
      pids = output.stdout.chomp.split
      if pids.empty? && !accept_no_pid_found
        raise("Did not find a pid for a sleep process on #{target}")
      end
    end

    pids.each do |pid|
      target['platform'] =~ /win/ ?
        on(target, "taskkill /F /pid #{pid}") :
        # Send SIGTERM (15); not all shells support '-s TERM'
        on(target, "kill -15 #{pid}")
    end
  end
end

# Initiates an rpc_non_blocking_request, verifies that it receives a provisional response for each target,
# and returns the transaction ids.
def start_puppet_non_blocking_request(broker, targets, environment = 'production')
  transaction_ids = []

  provisional_responses = nil
  begin
    provisional_responses = rpc_non_blocking_request(broker, targets,
                                                     'pxp-module-puppet', 'run',
                                                     {:flags => ['--onetime',
                                                                 '--no-daemonize',
                                                                 '--environment', environment]})
  rescue => exception
    fail("Exception occured when trying to send rpc_non_blocking_request to all agents: #{exception}")
  end

  targets.each do |identity|
    assert_equal("http://puppetlabs.com/rpc_provisional_response",
                 provisional_responses[identity][:envelope][:message_type],
                 "Did not receive expected rpc_provisional_response in reply to non-blocking request")
    transaction_ids << provisional_responses[identity][:data]["transaction_id"]
  end
  transaction_ids
end

def check_non_blocking_response(broker, identity, transaction_id, max_retries, query_interval, &block)
  run_result = nil
  query_attempts = 0
  loop do
    query_responses = rpc_blocking_request(broker, [identity],
                                           'status', 'query', {:transaction_id => transaction_id})
    action_result = query_responses[identity][:data]['results']
    assert(action_result, "Response to status query was an error: #{query_responses[identity][:data]}")
    if action_result.has_key?('exitcode')
      run_result = action_result
      break
    end
    query_attempts += 1
    break if query_attempts >= max_retries
    sleep query_interval
  end

  fail("Run puppet non-blocking transaction did not contain stdout of puppet run after #{query_attempts} attempts " \
       "and #{query_attempts * query_interval} seconds") unless run_result

  block.call run_result
  run_result
end

def check_puppet_non_blocking_response(identity, transaction_id, max_retries, query_interval,
                                       expected_result, expected_environment = 'production')
  run_result = check_non_blocking_response(master, identity, transaction_id, max_retries, query_interval) do |run_result|
    assert_equal("success", run_result['status'], "PXP run puppet action did not have expected 'success' result")

    stdout = JSON.parse(run_result['stdout'])
    puppet_run_result = stdout['status']
    puppet_run_environment = stdout['environment']
    assert_equal(expected_environment, stdout['environment'], "Puppet run did not use the expected environment")
    assert_equal(expected_result, stdout['status'], "Puppet run did not have expected result of '#{expected_result}'")
  end

  run_result['status']
end

def get_mode(host, path)
  ruby = ruby_command(host)
  on(host, "#{ruby} -e 'puts (File.stat(\"#{path}\").mode & 07777).to_s(8)'").stdout.chomp
end

def get_package_manager(host)
  platform = fact_on(host, 'osfamily')

  case platform
    when 'RedHat'
      pkg_manager = 'yum'
    when 'Debian'
      pkg_manager = 'apt-get'
  end
  pkg_manager
end

def setup_squid_proxy(host)
  pkg_manager = get_package_manager(host)
  install_squid = "#{pkg_manager} install -y squid"
  squid_conf = "/etc/squid/squid.conf"
  ssl_port_ws = "acl SSL_ports port 8142"
  ssl_port_master = "acl SSL_ports port 8140"
  iptables_conf_in = "iptables -A INPUT -p tcp --dport 3128 -m state --state NEW,ESTABLISHED -j ACCEPT"
  iptables_conf_out = "iptables -A OUTPUT -p tcp --sport 3128 -m state --state ESTABLISHED -j ACCEPT"
  on(host, install_squid)
  # configure squid to allow tcp and open up squid port on iptables
  on(host, "sed -i \'1s;^;#{ssl_port_ws}\\n#{ssl_port_master}\\n;\' #{squid_conf}")
  on(host, puppet("apply -e 'service {'squid' : ensure => running}'"))
  on(host, iptables_conf_in)
  on(host, iptables_conf_out)
  # return the address of the proxy
  "http://#{host}:3128"
end

def teardown_squid_proxy(host)
  pkg_manager = get_package_manager(host)
  remove_squid = "#{pkg_manager} remove -y squid"
  delete_squid_log = "rm -rf /var/log/squid/"
  on(host, puppet("apply -e 'service {'squid' : ensure => stopped}'"))
  on(host, remove_squid)
  on(host, delete_squid_log)
end

def clear_squid_log(host)
  squid_log = "/var/log/squid/access.log"
  clear_log = "cat /dev/null > #{squid_log}"
  on(host, clear_log)
end
