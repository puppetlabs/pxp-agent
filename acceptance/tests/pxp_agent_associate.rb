require 'pxp-agent/test_helper.rb'

test_name 'C93807 - Associate pxp-agent with a PCP broker'

agents.each_with_index do |agent, i|

  cert_dir = configure_std_certs_on_host(agent)
  create_remote_file(agent, pxp_agent_config_file(agent), pxp_config_json_using_test_certs(master, agent, i + 1, cert_dir).to_s)

  step 'Stop pxp-agent if it is currently running' do
    on agent, puppet('resource service pxp-agent ensure=stopped')
  end

  step 'Clear existing logs so we don\'t match an existing association entry' do
    on(agent, "rm -rf #{logfile(agent)}")
  end

  step 'Start pxp-agent service' do
    on agent, puppet('resource service pxp-agent ensure=running')
  end

  step 'Check that within 60 seconds, log file contains entry for WebSocket connection being established' do
    expect_file_on_host_to_contain(agent, logfile(agent), PXP_AGENT_LOG_ENTRY_WEBSOCKET_SUCCESS, 60)
  end

  step 'Check that log file contains entry that association has succeeded' do
    expect_file_on_host_to_contain(agent, logfile(agent), PXP_AGENT_LOG_ENTRY_ASSOCIATION_SUCCESS)
  end
end
