require 'pxp-agent/config_helper.rb'
require 'pxp-agent/test_helper.rb'

test_name 'C97934 - agent should use next broker if primary is intentionally shutdown' do

  PRIMARY_BROKER_INSTANCE = 0
  REPLICA_BROKER_INSTANCE = 1
  teardown do
    kill_all_pcp_brokers(master)
    run_pcp_broker(master, PRIMARY_BROKER_INSTANCE)
  end

  step 'Ensure each agent host has pxp-agent configured with multiple uris, running and associated' do
    agents.each do |agent|
      on agent, puppet('resource service pxp-agent ensure=stopped')
      num_brokers = 2
      pxp_config = pxp_config_hash_using_puppet_certs(master, agent, num_brokers)
      create_remote_file(agent, pxp_agent_config_file(agent), pxp_config.to_json.to_s)
      retry_on(agent, "rm -rf #{logfile(agent)}")
      on agent, puppet('resource service pxp-agent ensure=running')
      show_pcp_logs_on_failure do
        assert_equal(master[:pcp_broker_instance], PRIMARY_BROKER_INSTANCE, "broker instance is not set correctly: #{master[:pcp_broker_instance]}")
        assert(is_associated?(master, "pcp://#{agent}/agent"),
               "Agent identity pcp://#{agent}/agent for agent host #{agent} does not appear in pcp-broker's (#{broker_ws_uri(master)}) client inventory after ~#{PCP_INVENTORY_RETRIES} seconds")
      end
    end
  end

  step 'Stop primary broker, start replica' do
    kill_all_pcp_brokers(master)
    run_pcp_broker(master, REPLICA_BROKER_INSTANCE)
  end

  step 'On each agent, test that a new association has occurred' do
    assert_equal(master[:pcp_broker_instance], REPLICA_BROKER_INSTANCE, "broker instance is not set correctly: #{master[:pcp_broker_instance]}")
    agents.each_with_index do |agent|
      assert(is_associated?(master, "pcp://#{agent}/agent"),
             "Agent identity pcp://#{agent}/agent for agent host #{agent} does not appear in pcp-broker's (#{broker_ws_uri(master)}) client inventory after ~#{PCP_INVENTORY_RETRIES} seconds")
    end
  end

end
