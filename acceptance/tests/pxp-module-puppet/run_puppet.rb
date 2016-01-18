require 'pcp/client'
require 'pxp-agent/test_helper.rb'

test_name 'C95972 - pxp-module-puppet run'

step 'Ensure each agent host has pxp-agent running and associated' do
  agents.each_with_index do |agent, i|
    on agent, puppet('resource service pxp-agent ensure=stopped')
    cert_dir = configure_std_certs_on_host(agent)
    create_remote_file(agent, pxp_agent_config_file(agent), pxp_config_json_using_test_certs(master, agent, i + 1, cert_dir).to_s)
    on agent, puppet('resource service pxp-agent ensure=running')
    expect_file_on_host_to_contain(agent, logfile(agent), PXP_AGENT_LOG_ENTRY_ASSOCIATION_SUCCESS, 60)
  end
end

step "Send an rpc_blocking_request to all agents" do
  target_identities = []
  agents.each_with_index do |agent, i|
    target_identities << "pcp://client0#{i+1}.example.com/agent"
  end
  responses = nil # Declare here so not local to begin/rescue below
  begin
    responses = rpc_blocking_request(master, target_identities,
                                    'pxp-module-puppet', 'run',
                                    {:env => [], :flags => ['--noop',
                                                            '--onetime',
                                                            '--no-daemonize']
                                    })
  rescue => exception
    fail("Exception occurred when trying to run Puppet on all agents: #{exception.message}")
  end
  agents.each_with_index do |agent, i|
    step "Check Run Puppet response for #{agent}" do
      identity = "pcp://client0#{i+1}.example.com/agent"
      response = responses[identity]
      assert(response.has_key?(:envelope), "Response from PCP for #{agent} is missing :envelope")
      assert(response[:envelope].has_key?(:message_type), "Response from PCP for #{agent} is missing "\
                                                          ":message_type in :envelope")
      assert_equal('http://puppetlabs.com/rpc_blocking_response',
                   response[:envelope][:message_type],
                   "Received a message from pcp-broker for #{agent} but it wasn't of "\
                   "type http://puppetlabs.com/rpc_blocking_response")
      assert_equal(identity,
                   response[:envelope][:sender],
                   "Received the expected rpc_blocking_response for #{agent} "\
                   "but not from the expected identity")
    end # Step for this agent
  end # iterating on each agent
end
