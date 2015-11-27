require 'pxp-agent/config_helper.rb'
require 'pxp-agent/test_helper.rb'

test_name 'Attempt to start pxp-agent with invalid SSL config'

agent1 = agents[0]

cert_dir = configure_std_certs_on_host(agent1)

# On teardown, restore valid config file
teardown do
  on agent1, puppet('resource service pxp-agent ensure=stopped')
  create_remote_file(agent1, pxp_agent_config_file(agent1), pxp_config_json_using_test_certs(master, agent1, 1, cert_dir).to_s)
  on agent1, puppet('resource service pxp-agent ensure=running')
end

step 'Setup - Stop pxp-agent service'
on agent1, puppet('resource service pxp-agent ensure=stopped')

step "Setup - Wipe pxp-agent log"
on(agent1, "rm -rf #{logfile(agent1)}")

step "Setup - Change pxp-agent config to use a cert that doesn't match private key"
invalid_config_mismatching_keys = {:broker_ws_uri => broker_ws_uri(master),
                                   :ssl_key => ssl_key_file(agent1, 1, cert_dir),
                                   :ssl_ca_cert => ssl_ca_file(agent1, cert_dir),
                                   :ssl_cert => ssl_cert_file(agent1, 1, cert_dir, true)}
create_remote_file(agent1, pxp_agent_config_file(agent1), pxp_config_json(master, agent1, invalid_config_mismatching_keys).to_s)

step 'C94730 - Attempt to run pxp-agent with mismatching SSL cert and private key'
on agent1, puppet('resource service pxp-agent ensure=running')
expect_file_on_host_to_contain(agent1, logfile(agent1), 'failed to load private key')
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
                           :ssl_key => ssl_key_file(agent1, 1, cert_dir, true),
                           :ssl_ca_cert => ssl_ca_file(agent1, cert_dir),
                           :ssl_cert => ssl_cert_file(agent1, 1, cert_dir, true)}
create_remote_file(agent1, pxp_agent_config_file(agent1), pxp_config_json(master, agent1, invalid_config_wrong_ca).to_s)

step 'C94729 - Attempt to run pxp-agent with SSL keypair from a different ca'
on agent1, puppet('resource service pxp-agent ensure=running')
expect_file_on_host_to_contain(agent1, logfile(agent1), 'TLS handshake failed')
expect_file_on_host_to_contain(agent1, logfile(agent1), 'retrying in')
on agent1, puppet('resource service pxp-agent') do |result|
  assert_match(/running/, result.stdout, "pxp-agent service should be running (failing handshake)")
end
on agent1, puppet('resource service pxp-agent ensure=stopped')
on agent1, puppet('resource service pxp-agent') do |result|
  assert_match(/stopped/, result.stdout,
               "pxp-agent service should stop cleanly when it is running in a loop retrying invalid certs")
end
