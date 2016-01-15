require 'pcp/client'
require 'pxp-agent/test_helper.rb'

test_name 'C95972 - pxp-module-puppet run'

# Event machine is required by the ruby-pcp-client gem
# https://github.com/puppetlabs/ruby-pcp-client
em = Thread.new { EM.run }

mutex = Mutex.new
have_response = ConditionVariable.new
response = nil

client = PCP::Client.new({
  :server => "wss://#{master}:8142/pcp/",
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
    response = resp
    have_response.signal
  end
end

step 'Connect PCP client to the pcp-broker' do
  client.connect()
  if !client.associated?
    fail "Controller PCP client failed to associate with pcp-broker on #{master}"
  end
end

step 'Ensure each agent host has pxp-agent running and associated' do
  agents.each_with_index do |agent, i|
    on agent, puppet('resource service pxp-agent ensure=stopped')
    cert_dir = configure_std_certs_on_host(agent)
    create_remote_file(agent, pxp_agent_config_file(agent), pxp_config_json_using_test_certs(master, agent, i + 1, cert_dir).to_s)
    on agent, puppet('resource service pxp-agent ensure=running')
    expect_file_on_host_to_contain(agent, logfile(agent), PXP_AGENT_LOG_ENTRY_ASSOCIATION_SUCCESS, 60)
  end
end

agents.each_with_index do |agent, i|
  step "Send pxp-module-puppet run RPC action to #{agent}" do
    response = nil

    message = PCP::Message.new({
      :message_type => 'http://puppetlabs.com/rpc_blocking_request',
      :targets => ["pcp://client0#{i+1}.example.com/agent"]
    })

    message.data = {
      :transaction_id => SecureRandom.uuid,
      :module         => 'pxp-module-puppet',
      :action         => 'run',
      :params         => {
        :env => [],
        :flags => [
          '--noop',
          '--onetime',
          '--no-daemonize'
        ]
      }
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
            if response
              assert_equal('http://puppetlabs.com/rpc_blocking_response',
                           response[:envelope][:message_type],
                           "Received a message from pcp-broker but it wasn't of "\
                           "type http://puppetlabs.com/rpc_blocking_response")
              assert_equal("pcp://client0#{i+1}.example.com/agent",
                           response[:envelope][:sender],
                           "Received the expected rpc_blocking_response "\
                           "but not from the expected agent")
              done = true
            end
          end
          break if done
        end
      end
    rescue Timeout::Error
      if !response
        fail("No PCP response received when requesting puppet run on #{agent}")
      end
    end # wait for message
  end # beaker step
end # iterate on every agent

em.exit
