require 'pxp-agent/test_helper.rb'


STATUS_QUERY_MAX_RETRIES = 60
STATUS_QUERY_INTERVAL_SECONDS = 1

test_name 'C99777 - two runs with same transaction_id' do

  tag 'audit:high',
      'audit:acceptance'

  step 'Ensure each agent host has pxp-agent running and associated' do
    agents.each do |agent|
      on agent, puppet('resource service pxp-agent ensure=stopped')
      create_remote_file(agent, pxp_agent_config_file(agent), pxp_config_hocon_using_puppet_certs(master, agent))
      on agent, puppet('resource service pxp-agent ensure=running')
      assert(is_associated?(master, "pcp://#{agent}/agent"),
             "Agent #{agent} with PCP identity pcp://#{agent}/agent should be associated with pcp-broker")
    end
  end

  target_identities = agents.map {|agent| "pcp://#{agent}/agent"}
  transaction_ids = []
  run_results = []

  step "run puppet to generate a transaction_id" do
    transaction_ids = start_puppet_non_blocking_request(master, target_identities)
    target_identities.zip(transaction_ids).each do |identity, transaction_id|
      run_results << check_puppet_non_blocking_response(identity, transaction_id, STATUS_QUERY_MAX_RETRIES, STATUS_QUERY_INTERVAL_SECONDS, "unchanged")
    end
  end

  step "restart pxp" do
    agents.each do |agent|
      on agent, puppet('resource service pxp-agent ensure=stopped')
      create_remote_file(agent, pxp_agent_config_file(agent), pxp_config_hocon_using_puppet_certs(master, agent))
      on agent, puppet('resource service pxp-agent ensure=running')
      assert(is_associated?(master, "pcp://#{agent}/agent"),
             "Agent #{agent} with PCP identity pcp://#{agent}/agent should be associated with pcp-broker")
    end
  end

  step "rerun with same transaction_id, expect provisional response and status queryable" do
    target_identities.zip(transaction_ids, run_results).each do |identity, transaction_id, first_run_result|
      # This should fail!
      response = rpc_request(master, [identity],
                             'pxp-module-puppet', 'run',
                             {:flags => ['--onetime',
                                         '--no-daemonize',
                                         '--environment', 'production']},
                                         false,
                                         transaction_id)[identity]

      assert_equal(response[:envelope][:message_type], 'http://puppetlabs.com/rpc_provisional_response', 'Did not receive rpc provisional response')
      second_run_result = check_puppet_non_blocking_response(identity, transaction_id, STATUS_QUERY_MAX_RETRIES, STATUS_QUERY_INTERVAL_SECONDS, "unchanged")
      assert_equal(first_run_result, second_run_result, 'Run results were not identical, Puppet may have run again')
    end
  end
end # test
