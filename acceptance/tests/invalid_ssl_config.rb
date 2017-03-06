require 'pxp-agent/config_helper.rb'
require 'pxp-agent/test_helper.rb'
require 'json'

test_name 'Attempt to start pxp-agent with invalid SSL config'

# On teardown, restore valid config file on each agent
teardown do
  agents.each do |agent|
    on agent, puppet('resource service pxp-agent ensure=stopped')
    create_remote_file(agent, pxp_agent_config_file(agent), pxp_config_json_using_puppet_certs(master, agent).to_s)
    on agent, puppet('resource service pxp-agent ensure=running')
  end
end

agents.each do |agent|

  temp_ca_dir = agent.tmpdir('alternate_ssl')
  if windows?(agent) then
    alternate_cert = "#{temp_ca_dir}\\certs\\#{agent}.pem"
    alternate_key = "#{temp_ca_dir}\\private_keys\\#{agent}.pem"
    alternate_ca_cert = "#{temp_ca_dir}\\ca\\ca_crt.pem"
  else
    alternate_cert = "#{temp_ca_dir}/certs/#{agent}.pem"
    alternate_key = "#{temp_ca_dir}/private_keys/#{agent}.pem"
    alternate_ca_cert = "#{temp_ca_dir}/ca/ca_crt.pem"
  end

  step 'Create an alternate ca in a temp folder' do
    on agent, puppet("cert list --ssldir #{temp_ca_dir}") # this sets up a CA on the agent
    on agent, puppet("cert generate #{agent} --ssldir #{temp_ca_dir}")
  end

  step 'Ensure pxp-agent is currently running and associated' do
    on agent, puppet('resource service pxp-agent ensure=stopped')
    create_remote_file(agent, pxp_agent_config_file(agent), pxp_config_json_using_puppet_certs(master, agent).to_s)
    on agent, puppet('resource service pxp-agent ensure=running')

    assert(is_associated?(master, "pcp://#{agent}/agent"),
           "Agent #{agent} with PCP identity pcp://#{agent}/agent should be associated with pcp-broker")
  end

  step 'Setup - Stop pxp-agent service and wipe its log' do
    on agent, puppet('resource service pxp-agent ensure=stopped')
    reset_logfile(agent)
  end

  step "Setup - Change pxp-agent config to use a cert that doesn't match private key" do
    pxp_config = pxp_config_hash_using_puppet_certs(master, agent)
    pxp_config["ssl-cert"] = alternate_cert
    create_remote_file(agent, pxp_agent_config_file(agent), pxp_config.to_json.to_s)
  end

  step 'C94730 - Attempt to run pxp-agent with mismatching SSL cert and private key' do
    on agent, puppet('resource service pxp-agent ensure=running')
    expect_file_on_host_to_contain(agent, logfile(agent), 'failed to load private key')
    unless (agent['platform'] =~ /osx/) then # on OSX, service remains running and continually retries executing pxp-agent
      agent['platform'] =~ /solaris/ ? expected_service_state = /maintenance/
                                     : expected_service_state = /stopped/
      on agent, puppet('resource service pxp-agent') do |result|
        assert_match(expected_service_state, result.stdout, "pxp-agent service should not be running due to invalid SSL config")
      end
    end
  end

  step "Ensure pxp-agent service is stopped, and wipe log" do
    on agent, puppet('resource service pxp-agent ensure=stopped')
    reset_logfile(agent)
  end

  step "Change pxp-agent config so the cert and key match but they are of a different ca than the broker" do
    pxp_config = pxp_config_hash_using_puppet_certs(master, agent)
    pxp_config["ssl-cert"] = alternate_cert
    pxp_config["ssl-key"] = alternate_key
    create_remote_file(agent, pxp_agent_config_file(agent), pxp_config.to_json.to_s)
  end

  step 'C94729 - Attempt to run pxp-agent with SSL keypair from a different ca' do
    on agent, puppet('resource service pxp-agent ensure=running')
    expect_file_on_host_to_contain(agent, logfile(agent), 'TLS handshake failed')
    expect_file_on_host_to_contain(agent, logfile(agent), 'retrying in')
    on agent, puppet('resource service pxp-agent') do |result|
      assert_match(/running/, result.stdout, "pxp-agent service should be running (failing handshake)")
    end
    unless (agent['platform'] =~ /osx/) then # on OSX, service remains running and continually retries executing pxp-agent
      on agent, puppet('resource service pxp-agent ensure=stopped')
      on agent, puppet('resource service pxp-agent') do |result|
        assert_match(/stopped/, result.stdout,
                     "pxp-agent service should stop cleanly when it is running in a loop retrying invalid certs")
      end
    end
  end

  step 'C97365 - Attempt to run pxp-agent with a different CA to the broker' do

    step 'Stop pxp-agent and wipe its existing log file'
    on agent, puppet('resource service pxp-agent ensure=stopped')
    reset_logfile(agent)

    step 'Create pxp-agent.conf with an alternate CA cert'
    pxp_config = pxp_config_hash_using_puppet_certs(master, agent)
    pxp_config['ssl-ca-cert'] = alternate_ca_cert
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
    reset_logfile(agent)

    step 'Create pxp-agent.conf that connects to pcp-broker using its IP address'
    pxp_config = pxp_config_hash_using_puppet_certs(master, agent)
    pxp_config.delete('broker-ws-uris')
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
