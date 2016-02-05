require 'pxp-agent/test_helper.rb'

test_name 'C93807 - Associate pxp-agent with a PCP broker'

agents.each_with_index do |agent, i|

  cert_dir = configure_std_certs_on_host(agent)
  create_remote_file(agent, pxp_agent_config_file(agent), pxp_config_json_using_test_certs(master, agent, i + 1, cert_dir).to_s)

  step 'Stop pxp-agent if it is currently running' do
    on agent, puppet('resource service pxp-agent ensure=stopped')
  end

  step 'Assert that the agent is not listed in pcp-broker inventory' do
    assert(!is_associated?(master, "pcp://client0#{i+1}.example.com/agent"),
           "Agent identity pcp://client0#{i+1}.example.com/agent for agent host #{agent} appears in pcp-broker's client inventory " \
           "but pxp-agent service is supposed to be stopped")
  end

  step 'Start pxp-agent service' do
    on agent, puppet('resource service pxp-agent ensure=running')
  end

  step 'Assert that agent is listed in pcp-broker inventory' do
    assert(is_associated?(master, "pcp://client0#{i+1}.example.com/agent"),
           "Agent identity pcp://client0#{i+1}.example.com/agent for agent host #{agent} does not appear in pcp-broker's client inventory")
  end
end
