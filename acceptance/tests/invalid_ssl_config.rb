require 'pxp-agent/config_helper.rb'

test_name 'Attempt to start pxp-agent with invalid SSL config'

agent1 = agents[0]

# On teardown, restore valid config file
teardown do
  on agent1, puppet('resource service pxp-agent ensure=stopped')
  create_remote_file(agent1, pxp_agent_config_file(agent1), pxp_config_json_using_test_certs(master, agent1, 1).to_s)
  on agent1, puppet('resource service pxp-agent ensure=running')
end

step 'Setup - Stop pxp-agent service'
on agent1, puppet('resource service pxp-agent ensure=stopped')

step "Setup - Wipe pxp-agent log"
on(agent1, "rm -rf #{logfile(agent1)}")

step "Setup - Change pxp-agent config to use a cert that doesn't match private key"
invalid_config_mismatching_keys = {:broker_ws_uri => broker_ws_uri(master),
                                   :ssl_key => ssl_key_file(agent1, 1),
                                   :ssl_ca_cert => ssl_ca_file(agent1),
                                   :ssl_cert => ssl_cert_file(agent1, 1, use_alt_path: true)}
create_remote_file(agent1, pxp_agent_config_file(agent1), pxp_config_json(master, agent1, invalid_config_mismatching_keys).to_s)

step 'C94730 - Attempt to run pxp-agent with mismatching SSL cert and private key'
expected_private_key_error='failed to load private key'
on agent1, puppet('resource service pxp-agent ensure=running')
# If the expected error does not appear in log within 30 seconds, then do an
# explicit assertion so we see the log contents in the test failure output
begin
  retry_on(agent1, "grep '#{expected_private_key_error}' #{logfile(agent1)}", {:max_retries => 30,
                                                                        :retry_interval => 1})
rescue
  on(agent1, "cat #{log_file}") do |result|
    assert_match(expected_private_key_error, result.stdout,
                "Expected error '#{expected_private_key_error}' did not appear in pxp-agent.log")
  end
end
assert(on(agent1, "grep 'pxp-agent will start unconfigured' #{logfile(agent1)}"),
       "pxp-agent should log that is will start unconfigured")
on agent1, puppet('resource service pxp-agent') do |result|
  assert_match(/running/, result.stdout, "pxp-agent service should be running (unconfigured)")
end

step "Stop service and wipe log"
on agent1, puppet('resource service pxp-agent ensure=stopped')
on(agent, "rm -rf #{logfile(agent1)}")

step "Change pxp-agent config so the cert and key match but they are of a different ca than the broker"
invalid_config_wrong_ca = {:broker_ws_uri => broker_ws_uri(master),
                           :ssl_key => ssl_key_file(agent1, 1, use_alt_path: true),
                           :ssl_ca_cert => ssl_ca_file(agent1),
                           :ssl_cert => ssl_cert_file(agent1, 1, use_alt_path: true)}
create_remote_file(agent1, pxp_agent_config_file(agent1), pxp_config_json(master, agent1, invalid_config_wrong_ca).to_s)

step 'C94729 - Attempt to run pxp-agent with SSL keypair from a different ca'
on agent1, puppet('resource service pxp-agent ensure=running')
retry_on(agent1, "grep 'TLS handshake failed' #{logfile(agent1)}", {:max_retries => 30,
                                                             :retry_interval => 1})
assert(on(agent1, "grep 'retrying in' #{logfile(agent1)}"),
       "pxp-agent should log that it will retry connection")
on agent1, puppet('resource service pxp-agent') do |result|
  assert_match(/running/, result.stdout, "pxp-agent service should be running (failing handshake)")
end
on agent1, puppet('resource service pxp-agent ensure=stopped')
on agent1, puppet('resource service pxp-agent') do |result|
  assert_match(/stopped/, result.stdout,
               "pxp-agent service should stop cleanly when it is running in a loop retrying invalid certs")
end
