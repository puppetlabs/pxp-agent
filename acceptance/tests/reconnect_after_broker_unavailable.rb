require 'pxp-agent/config_helper.rb'
require 'pxp-agent/test_helper.rb'

test_name 'C94789 - An associated agent should automatically reconnect when the broker was temporarily unavailable'

tag 'audit:medium',      # broker failover connection behavior is critical
                         # functionally, is this test a duplicate of failover/timeout_failover.rb  ??
    'audit:acceptance'

step 'Ensure each agent host has pxp-agent running and associated' do
  agents.each do |agent|
    on agent, puppet('resource service pxp-agent ensure=stopped')
    create_remote_file(agent, pxp_agent_config_file(agent), pxp_config_hocon_using_puppet_certs(master, agent))
    reset_logfile(agent)
    on agent, puppet('resource service pxp-agent ensure=running')

    assert(is_associated?(master, "pcp://#{agent}/agent"),
           "Agent identity pcp://#{agent}/agent for agent host #{agent} does not appear in pcp-broker's client inventory")
  end
end

step 'On master, stop then restart the broker' do
  kill_all_pcp_brokers(master)
  run_pcp_broker(master)
end

step 'On each agent, test that a 2nd association has occurred' do
  agents.each_with_index do |agent|
    assert(is_associated?(master, "pcp://#{agent}/agent"),
           "Agent identity pcp://#{agent}/agent for agent host #{agent} does not appear in pcp-broker's client inventory")
  end
end
