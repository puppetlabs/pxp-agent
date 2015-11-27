require 'pxp-agent/test_helper.rb'

test_name 'C93807 - Associate pxp-agent with a PCP broker'

agent1 = agents[0]

cert_dir = configure_std_certs_on_host(agent1)
create_remote_file(agent1, pxp_agent_config_file(agent1), pxp_config_json_using_test_certs(master, agent1, 1, cert_dir).to_s)

step 'Stop pxp-agent if it is currently running'
on agent1, puppet('resource service pxp-agent ensure=stopped')

step 'Clear existing logs so we don\'t match an existing association entry'
on(agent1, "rm -rf #{logfile(agent1)}")

step 'Start pxp-agent service'
on agent1, puppet('resource service pxp-agent ensure=running')

step 'Check that within 60 seconds, log file contains entry for WebSocket connection being established' do
  expect_file_on_host_to_contain(agent1, logfile(agent1), 'INFO.*Successfully established a WebSocket connection with the PCP broker.*', 60)
end
step 'Check that log file contains entry that association has succeeded' do
  expect_file_on_host_to_contain(agent1, logfile(agent1), 'INFO.*Received associate session response.*success')
end
