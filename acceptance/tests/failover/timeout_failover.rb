require 'pxp-agent/config_helper.rb'
require 'pxp-agent/test_helper.rb'

test_name 'C97964 - agent should use next broker if primary is timing out' do

  PRIMARY_BROKER_INSTANCE = 0
  REPLICA_BROKER_INSTANCE = 1
  teardown do
    unblock_pcp_broker(master,PRIMARY_BROKER_INSTANCE)
    kill_all_pcp_brokers(master)
    run_pcp_broker(master,    PRIMARY_BROKER_INSTANCE)
  end

  step 'Ensure each agent host has pxp-agent configured with multiple uris, running and associated' do
    agents.each do |agent|
      on agent, puppet('resource service pxp-agent ensure=stopped')
      num_brokers = 2
      pxp_config = pxp_config_hash_using_puppet_certs(master, agent, num_brokers)
      # Should attempt to reconnect in ~10 seconds.
      pxp_config['allowed-keepalive-timeouts'] = 0
      pxp_config['ping-interval'] = 6
      create_remote_file(agent, pxp_agent_config_file(agent), to_hocon(pxp_config))
      reset_logfile(agent)
      on agent, puppet('resource service pxp-agent ensure=running')

      assert_equal(master[:pcp_broker_instance], PRIMARY_BROKER_INSTANCE, "broker instance is not set correctly: #{master[:pcp_broker_instance]}")
      assert(is_associated?(master, "pcp://#{agent}/agent"),
             "Agent identity pcp://#{agent}/agent for agent host #{agent} does not appear in pcp-broker's (#{broker_ws_uri(master)}) client inventory after ~#{PCP_INVENTORY_RETRIES} seconds")
    end
  end

  step 'Block primary broker, start replica' do
    block_pcp_broker(master,PRIMARY_BROKER_INSTANCE)
    run_pcp_broker(master,  REPLICA_BROKER_INSTANCE)
  end

  step 'On each agent, test that a new association has occurred' do
    assert_equal(master[:pcp_broker_instance], REPLICA_BROKER_INSTANCE, "broker instance is not set correctly: #{master[:pcp_broker_instance]}")
    agents.each do |agent|
      inventory_retries = 60
      assert(is_associated?(master, "pcp://#{agent}/agent", inventory_retries),
             "Agent identity pcp://#{agent}/agent for agent host #{agent} does not appear in pcp-broker's (#{broker_ws_uri(master)}) client inventory after ~#{inventory_retries} seconds")
    end
  end

  # We do *not* need to ensure we are not associated with the primary broker
  #   this requires the primary to receive socket close from the agent, which of course we have restricted above.
  #   After opening the port the broker may or may not receive the socket close making this test flaky
  #   In addition, we do not depend upon broker dis-association for any features.

end
