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

step 'Allow 10 seconds after service start-up for association to complete'
sleep(10)

websocket_success = /INFO.*Successfully established a WebSocket connection with the PCP broker.*/
association_success = /INFO.*Received associate session response.*success/
on(agent1, "cat #{logfile(agent1)}") do |result|
  log_contents = result.stdout
  step 'Check pxp-agent.log for websocket connection'
  assert_match(websocket_success, log_contents,
               "Did not match expected websocket connection message '#{websocket_success.to_s}' " \
               "in actual pxp-agent.log contents '#{log_contents}'")
  step 'Check pxp-agent.log for successful association response'
  assert_match(association_success, log_contents,
               "Did not match expected association success message '#{association_success.to_s}' " \
               "in actual pxp-agent.log contents '#{log_contents}'")
end
