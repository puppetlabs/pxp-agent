require 'pxp-agent/config_helper.rb'
require 'pxp-agent/test_helper.rb'

test_name 'C97934 - agent should use next broker if primary is intentionally shutdown'

teardown do
  broker_instance = 0 # 0 indexed
  kill_pcp_broker(master)
  run_pcp_broker(master, broker_instance)
end

def pxp_config_hash_using_puppet_certs_multiple_uris(broker, agent)
  broker_uris = [broker_ws_uri(master), broker_ws_uri(master)]
  broker_uris[1].sub!(PCP_BROKER_PORTS[0].to_s,PCP_BROKER_PORTS[1].to_s)

  on(agent, puppet('config print ssldir')) do |result|
    puppet_ssldir = result.stdout.chomp
    return { "broker-ws-uris" => broker_uris,
      "ssl-key" => "#{puppet_ssldir}/private_keys/#{agent}.pem",
      "ssl-ca-cert" => "#{puppet_ssldir}/certs/ca.pem",
      "ssl-cert" => "#{puppet_ssldir}/certs/#{agent}.pem"
    }.to_json
  end
end

step 'Ensure each agent host has pxp-agent configured with multiple uris, running and associated' do
  agents.each do |agent|
    on agent, puppet('resource service pxp-agent ensure=stopped')
    create_remote_file(agent, pxp_agent_config_file(agent), pxp_config_hash_using_puppet_certs_multiple_uris(master, agent).to_s)
    on(agent, "rm -rf #{logfile(agent)}")
    on agent, puppet('resource service pxp-agent ensure=running')
    show_pcp_logs_on_failure do
      assert(is_associated?(master, "pcp://#{agent}/agent"),
             "Agent identity pcp://#{agent}/agent for agent host #{agent} does not appear in pcp-broker's client inventory")
    end
  end
end

step 'Stop primary broker, start replica' do
  broker_instance = 1 # 0 indexed
  kill_pcp_broker(master)
  run_pcp_broker(master, broker_instance)
end

step 'On each agent, test that a new association has occurred' do
  agents.each_with_index do |agent|
    assert(is_associated?(master, "pcp://#{agent}/agent"),
           "Agent identity pcp://#{agent}/agent for agent host #{agent} does not appear in pcp-broker's client inventory")
  end
end
