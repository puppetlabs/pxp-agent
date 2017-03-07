require 'pxp-agent/test_helper.rb'

test_name 'pxp-module-puppet run with PCP v1' do

  skip_test('Already using PCP version 1') if ENV['PCP_VERSION'] == '1'

  # On teardown, restore v2 config file on each agent
  teardown do
    agents.each do |agent|
      on agent, puppet('resource service pxp-agent ensure=stopped')
      create_remote_file(agent, pxp_agent_config_file(agent), pxp_config_json_using_puppet_certs(master, agent).to_s)
      on agent, puppet('resource service pxp-agent ensure=running')
    end
  end

  step 'Ensure each agent host has pxp-agent running and associated with PCPv1' do
    agents.each do |agent|
      on agent, puppet('resource service pxp-agent ensure=stopped')
      pxp_config = pxp_config_hash_using_puppet_certs(master, agent)
      pxp_config['pcp-version'] = '1'
      pxp_config['broker-ws-uris'] = [broker_ws_uri(master, 1)]
      create_remote_file(agent, pxp_agent_config_file(agent), pxp_config.to_json.to_s)
      on agent, puppet('resource service pxp-agent ensure=running')
      assert(is_associated?(master, "pcp://#{agent}/agent"),
             "Agent #{agent} with PCP identity pcp://#{agent}/agent should be associated with pcp-broker")
    end
  end

  step "Send an rpc_blocking_request to all agents" do
    target_identities = []
    agents.each do |agent|
      target_identities << "pcp://#{agent}/agent"
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
    agents.each do |agent|
      step "Check Run Puppet response for #{agent}" do
        identity = "pcp://#{agent}/agent"
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
  end # test step
end # test
