require 'pxp-agent/test_helper.rb'

test_name 'C93065 - Run puppet and expect puppet agent disabled' do

  teardown do
    on agents, puppet('agent --enable')
  end

  step 'Ensure each agent host has pxp-agent running and associated' do
    agents.each do |agent|
      on agent, puppet('resource service pxp-agent ensure=stopped')
      create_remote_file(agent, pxp_agent_config_file(agent), pxp_config_hocon_using_puppet_certs(master, agent))
      on agent, puppet('resource service pxp-agent ensure=running')

      assert(is_associated?(master, "pcp://#{agent}/agent"),
             "At the start of the test, #{agent} (with PCP identity pcp://#{agent}/agent ) should be associated with pcp-broker")
    end
  end

  step 'Set puppet agent on agent hosts to disabled' do
    on agents, puppet('agent --disable')
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
                                      {:env => [], :flags => []}
                                      )
    rescue => exception
      fail("Exception occurred when trying to run Puppet on all agents: #{exception.message}")
    end
    agents.each_with_index do |agent|
      step "Check Run Puppet response for #{agent}" do
        identity = "pcp://#{agent}/agent"
        action_result = responses[identity][:data]["results"]
        assert(action_result.has_key?('error_type'), 'Results from pxp-module-puppet should contain an \'error_type\' entry')
        assert_equal('agent_disabled', action_result['error_type'],
                     'error_type of pxp-module-puppet response should be \'agent_disabled\'')
        assert(action_result.has_key?('error'), 'Results from pxp-module-puppet should contain an \'error\' entry')
        assert_equal('Puppet agent is disabled', action_result['error'],
                     'error of pxp-module-puppet response should be \'Puppet agent is disabled\'')
      end
    end
  end
end
