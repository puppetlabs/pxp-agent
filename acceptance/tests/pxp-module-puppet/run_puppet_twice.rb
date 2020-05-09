require 'pxp-agent/test_helper.rb'
require 'puppet/acceptance/environment_utils'

SECONDS_TO_SLEEP = 500 # The test will use SIGALARM to end this as soon as required
STATUS_QUERY_MAX_RETRIES = 60
STATUS_QUERY_INTERVAL_SECONDS = 1

test_name 'Run Puppet while a Puppet Agent run is in-progress, wait for completion' do

  tag 'audit:high',      # module validation: no other venue exists to test
      'audit:acceptance'

  extend Puppet::Acceptance::EnvironmentUtils

  env_name = test_file_name = File.basename(__FILE__, '.*')
  environment_name = mk_tmp_environment(env_name)

  teardown do
    stop_sleep_process(agents, SECONDS_TO_SLEEP, true)
  end

  step 'On master, create a new environment that will result in a slow run' do
    site_manifest = "#{environmentpath}/#{environment_name}/manifests/site.pp"
    create_sleep_manifest(master, site_manifest, SECONDS_TO_SLEEP)
  end

  step 'Ensure each agent host has pxp-agent running and associated' do
    agents.each do |agent|
      on agent, puppet('resource service pxp-agent ensure=stopped')
      create_remote_file(agent, pxp_agent_config_file(agent), pxp_config_hocon_using_puppet_certs(master, agent))
      on agent, puppet('resource service pxp-agent ensure=running')
      assert(is_associated?(master, "pcp://#{agent}/agent"),
             "Agent #{agent} with PCP identity pcp://#{agent}/agent should be associated with pcp-broker")
    end
  end

  step 'Ensure puppet is not currently running as a service (PCP-632)' do
    on(agents, puppet('resource service puppet ensure=stopped enable=false'))
  end

  target_identities = []
  agents.each do |agent|
    target_identities << "pcp://#{agent}/agent"
  end

  transaction_ids = []
  step 'Run Puppet on agents with long-running catalog' do
    transaction_ids = start_puppet_non_blocking_request(master, target_identities, environment_name)
  end

  step 'Wait until Puppet starts executing' do
    agents.each do |agent|
      wait_for_sleep_process(agent, SECONDS_TO_SLEEP)
    end
  end

  transaction_ids_2 = []
  step 'Run Puppet on agents again' do
    transaction_ids_2 = start_puppet_non_blocking_request(master, target_identities)
  end

  # Wait for the 2nd puppet agent process to exit
  #
  # Passing case: 2nd puppet process starts, exits on puppet_already_running error, pxp-module-puppet waits to retry.
  # Failing case: 2nd puppet process starts, exits on puppet_already_running error, pxp-module-puppet gives up.
  #
  # If the test continued without waiting for the 2nd instance to exit, the 1st agent instance might exit before the
  # 2nd agent instance was fully started; causing a false positive test result.
  #
  # Because beaker's sampling of the puppet agent PIDs might begin before the 2nd puppet agent appears, OR
  # it might begin after the 2nd puppet agent has already shut down; this step needs to use a heuristic approach.
  # It succeeds if either:
  #   a. It observes 2 PIDs on one sample, then only 1 PID on the subsequent sample
  #   b. It observes 1 PID for at least 10 concurrent samples
  #
  step 'Wait for only one puppet agent PID to exist' do
    agents.each do |agent|

      satisfied = false
      pid_counting_attempts = 0
      MAX_PID_COUNTING_ATTEMPTS = 60
      concurrent_samples_of_1_pid = 0
      REQUIRED_SAMPLES_OF_1_PID = 10
      at_least_two_pids_observed = false

      while pid_counting_attempts < MAX_PID_COUNTING_ATTEMPTS do
        puppet_agent_pids = get_process_pids(agent, 'puppet agent')
        if puppet_agent_pids.length == 1 then
          concurrent_samples_of_1_pid += 1
          if at_least_two_pids_observed then
            logger.debug "Determined that 2nd agent just stopped, there were 2 pids and now there is only 1."
            satisfied = true
          end
          if !satisfied && concurrent_samples_of_1_pid >= REQUIRED_SAMPLES_OF_1_PID then
            logger.debug "Assuming 2nd agent has stopped due to #{concurrent_samples_of_1_pid} concurrent "\
                         "pid checks that only returned 1 running agent pid"
            satisfied = true
          end
        elsif puppet_agent_pids.length >= 2 then
          concurrent_samples_of_1_pid = 0
          at_least_two_pids_observed = true
        else
          # If there are 0 pids then something has gone wrong
          raise("Test relies on there being at least 1  puppet-agent pid, "\
                "but somehow 0 pids were detected")
        end
        break if satisfied
        pid_counting_attempts += 1
        sleep 1
      end

      unless satisfied then
        fail("After #{MAX_PID_COUNTING_ATTEMPTS} times checking, could not determine that the 2nd agent had stopped")
      end
    end
  end

  step 'Signal sleep process to end so 1st Puppet run will complete' do
    stop_sleep_process(agents, SECONDS_TO_SLEEP)
  end

  target_identities.zip(transaction_ids).each do |identity, transaction_id|
    step "Check response to 1st non-blocking request for #{identity}" do
      check_puppet_non_blocking_response(identity, transaction_id,
                                         STATUS_QUERY_MAX_RETRIES, STATUS_QUERY_INTERVAL_SECONDS,
                                         'changed', environment_name)
    end
  end

  target_identities.zip(transaction_ids_2).each do |identity, transaction_id, transaction_id_2|
    step "Check response to 2nd non-blocking request for #{identity}" do
      check_puppet_non_blocking_response(identity, transaction_id,
                                         STATUS_QUERY_MAX_RETRIES, STATUS_QUERY_INTERVAL_SECONDS,
                                         'unchanged')
    end
  end
end
