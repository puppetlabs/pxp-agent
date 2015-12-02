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
    expect_file_on_host_to_contain(agent, logfile(agent), 'INFO.*Received associate session response.*success', 60)
  end
end

step 'On master, stop then restart the broker' do
  kill_pcp_broker(master)
  run_pcp_broker(master)
end

step 'On each agent, test that a 2nd association has occurred' do
  agents.each do |agent|
    expect_file_on_host_to_contain(agent, logfile(agent), "WebSocket on fail event (connection loss)", 60)
    begin
      retry_on(agent, "test 2 -eq $(grep 'INFO.*Received associate session response.*success'  #{logfile(agent)} | wc -l)",
               {:max_retries => 60, :retry_interval => 1})
    rescue
      on(agent, "grep 'INFO.*Received associate session response.*success' #{logfile(agent)} | wc -l") do |result|
        number_of_association_entries = result.stdout.chomp
        fail("There should be 2 association success messages in the log - " \
             "1 for the service starting, and the 2nd for the agent reconnecting. " \
             "Actual number of association success entries found in log: #{number_of_association_entries}")
      end
    end
  end
end
