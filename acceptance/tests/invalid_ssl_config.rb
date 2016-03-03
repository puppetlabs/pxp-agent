require 'pxp-agent/config_helper.rb'
require 'pxp-agent/test_helper.rb'
require 'json'

test_name 'Attempt to start pxp-agent with invalid SSL config'

# On teardown, restore valid config file on each agent
teardown do
  agents.each_with_index do |agent, i|
    on agent, puppet('resource service pxp-agent ensure=stopped')
    cert_dir = configure_std_certs_on_host(agent)
    create_remote_file(agent, pxp_agent_config_file(agent), pxp_config_json_using_test_certs(master, agent, i+1, cert_dir).to_s)
    on agent, puppet('resource service pxp-agent ensure=running')
  end
end

agents.each_with_index do |agent, i|

  cert_dir = configure_std_certs_on_host(agent)
  cert_number = i + 1

  step 'Setup - Stop pxp-agent service' do
    on agent, puppet('resource service pxp-agent ensure=stopped')
  end

  step "Setup - Wipe pxp-agent log" do
    on(agent, "rm -rf #{logfile(agent)}")
  end

  step "Setup - Change pxp-agent config to use a cert that doesn't match private key" do
    invalid_config_mismatching_keys = {:broker_ws_uri => broker_ws_uri(master),
                                       :ssl_key => ssl_key_file(agent, cert_number, cert_dir),
                                       :ssl_ca_cert => ssl_ca_file(agent, cert_dir),
                                       :ssl_cert => ssl_cert_file(agent, cert_number, cert_dir, true)}
    create_remote_file(agent, pxp_agent_config_file(agent), pxp_config_json(master, agent, invalid_config_mismatching_keys).to_s)
  end

  step 'C94730 - Attempt to run pxp-agent with mismatching SSL cert and private key' do
    on agent, puppet('resource service pxp-agent ensure=running')
    expect_file_on_host_to_contain(agent, logfile(agent), 'failed to load private key')
  end

  step "Ensure pxp-agent service is stopped, and wipe log" do
    on agent, puppet('resource service pxp-agent ensure=stopped')
    on(agent, "rm -rf #{logfile(agent)}")
  end

  step "Change pxp-agent config so the cert and key match but they are of a different ca than the broker" do
    invalid_config_wrong_ca = {:broker_ws_uri => broker_ws_uri(master),
                               :ssl_key => ssl_key_file(agent, cert_number, cert_dir, true),
                               :ssl_ca_cert => ssl_ca_file(agent, cert_dir),
                               :ssl_cert => ssl_cert_file(agent, cert_number, cert_dir, true)}
    create_remote_file(agent, pxp_agent_config_file(agent), pxp_config_json(master, agent, invalid_config_wrong_ca).to_s)
  end

  step 'C94729 - Attempt to run pxp-agent with SSL keypair from a different ca' do
    on agent, puppet('resource service pxp-agent ensure=running')
    expect_file_on_host_to_contain(agent, logfile(agent), 'TLS handshake failed')
    expect_file_on_host_to_contain(agent, logfile(agent), 'retrying in')
    on agent, puppet('resource service pxp-agent') do |result|
      assert_match(/running/, result.stdout, "pxp-agent service should be running (failing handshake)")
    end
    on agent, puppet('resource service pxp-agent ensure=stopped')
    on agent, puppet('resource service pxp-agent') do |result|
      assert_match(/stopped/, result.stdout,
                   "pxp-agent service should stop cleanly when it is running in a loop retrying invalid certs")
    end
  end

  step 'C97365 - Attempt to run pxp-agent with a different CA to the broker' do
    step 'Create an alternate ca cert in a temp folder'
    temp_ca_dir = agent.tmpdir('alt_ca')
    on agent, puppet("cert list --ssldir #{temp_ca_dir}") # this sets up a CA on the agent
    windows?(agent) ?
      alt_ca_cert = "#{temp_ca_dir}\\ca\\ca_crt.pem" :
      alt_ca_cert = "#{temp_ca_dir}/ca/ca_crt.pem"

    step 'Stop pxp-agent and wipe its existing log file'
    on agent, puppet('resource service pxp-agent ensure=stopped')
    on(agent, "rm -rf #{logfile(agent)}")

    step 'Create pxp-agent.conf with an alternate CA cert'
    pxp_config = pxp_config_hash_using_puppet_certs(master, agent)
    pxp_config['ssl-ca-cert'] = alt_ca_cert
    create_remote_file(agent, pxp_agent_config_file(agent), pxp_config.to_json.to_s)

    step 'Start pxp-agent and assert that it does not connect to pcp-broker'
    on agent, puppet('resource service pxp-agent ensure=running')
    assert(is_not_associated?(master, "pcp://#{agent}/agent"),
           "Agent identity pcp://#{agent}/agent for agent host #{agent} should not appear in pcp-broker's inventory " \
           "when pxp-agent is using the wrong CA cert")
    expect_file_on_host_to_contain(agent, logfile(agent), 'TLS handshake failed')
  end

  step 'C97366 - Attempt to connect to pcp-broker without using its certified hostname' do
    step 'Stop pxp-agent and wipe its existing log file'
    on agent, puppet('resource service pxp-agent ensure=stopped')
    on(agent, "rm -rf #{logfile(agent)}")

    step 'Create pxp-agent.conf that connects to pcp-broker using its IP address'
    pxp_config = pxp_config_hash_using_puppet_certs(master, agent)
    pxp_config['broker-ws-uri'] = broker_ws_uri(master.ip)
    create_remote_file(agent, pxp_agent_config_file(agent), pxp_config.to_json.to_s)

    step 'Start pxp-agent and assert that it does not connect to broker'
    on agent, puppet('resource service pxp-agent ensure=running')
    assert(is_not_associated?(master, "pcp://#{agent}/agent"),
           "Agent identity pcp://#{agent}/agent for agent host #{agent} should not appear in pcp-broker's inventory " \
           "when pxp-agent attempts to connect by broker IP instead of broker certified hostname")
    expect_file_on_host_to_contain(agent, logfile(agent), 'TLS handshake failed')
  end
end
