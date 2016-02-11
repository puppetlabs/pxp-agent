require 'pxp-agent/config_helper.rb'
require 'pxp-agent/test_helper.rb'

test_name 'C94789 - An associated agent should automatically reconnect when the broker was temporarily unavailable'

step 'Ensure each agent host has pxp-agent running and associated' do
  agents.each_with_index do |agent, i|
    on agent, puppet('resource service pxp-agent ensure=stopped')
    cert_dir = configure_std_certs_on_host(agent)
    create_remote_file(agent, pxp_agent_config_file(agent), pxp_config_json_using_test_certs(master, agent, i + 1, cert_dir).to_s)
    on(agent, "rm -rf #{logfile(agent)}")
    on agent, puppet('resource service pxp-agent ensure=running')
    assert(is_associated?(master, "pcp://client0#{i+1}.example.com/agent"),
           "Agent identity pcp://client0#{i+1}.example.com/agent for agent host #{agent} does not appear in pcp-broker's client inventory")
  end
end

step 'On master, stop then restart the broker' do
  kill_pcp_broker(master)
  run_pcp_broker(master)
end

step 'On each agent, test that a 2nd association has occurred' do
  agents.each_with_index do |agent, i|
    assert(is_associated?(master, "pcp://client0#{i+1}.example.com/agent"),
           "Agent identity pcp://client0#{i+1}.example.com/agent for agent host #{agent} does not appear in pcp-broker's client inventory")
  end
end
