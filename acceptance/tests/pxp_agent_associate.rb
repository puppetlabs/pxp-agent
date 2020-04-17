require 'pxp-agent/test_helper.rb'

test_name 'C93807 - Associate pxp-agent with a PCP broker'

  tag 'audit:low',        # this behavior is covered by necessity in other tests
      'audit:acceptance'

agents.each do |agent|

  create_remote_file(agent, pxp_agent_config_file(agent), pxp_config_hocon_using_puppet_certs(master, agent))

  step 'Stop pxp-agent if it is currently running' do
    on agent, puppet('resource service pxp-agent ensure=stopped')
  end

  step 'Assert that the agent is not listed in pcp-broker inventory' do
    assert(is_not_associated?(master, "pcp://#{agent}/agent"),
           "Agent identity pcp://#{agent}/agent for agent host #{agent} appears in pcp-broker's client inventory " \
           "but pxp-agent service is supposed to be stopped")
  end

  step 'Start pxp-agent service' do
    on agent, puppet('resource service pxp-agent ensure=running')
  end

  step 'Assert that agent is listed in pcp-broker inventory' do
    assert(is_associated?(master, "pcp://#{agent}/agent"),
           "Agent identity pcp://#{agent}/agent for agent host #{agent} does not appear in pcp-broker's client inventory")
  end
end
