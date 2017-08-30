require 'pxp-agent/test_helper.rb'
require 'pxp-agent/config_helper.rb'
require 'puppet/acceptance/environment_utils'

SECONDS_TO_SLEEP = 500 # The test will use SIGALARM to end this as soon as required
STATUS_QUERY_MAX_RETRIES = 50
STATUS_QUERY_INTERVAL_SECONDS = 2

test_name 'Run Puppet while a Puppet Agent run is in-progress, wait for it to be killed' do

  extend Puppet::Acceptance::EnvironmentUtils

  env_name = test_file_name = File.basename(__FILE__, '.*')
  environment_name = mk_tmp_environment(env_name)

  step 'On master, create a new environment that will result in a slow run' do
    site_manifest = "#{environmentpath}/#{environment_name}/manifests/site.pp"
    create_sleep_manifest(master, site_manifest, SECONDS_TO_SLEEP)
  end

  step 'Ensure each agent host has pxp-agent running and associated' do
    agents.each do |agent|
      on agent, puppet('resource service pxp-agent ensure=stopped')
      create_remote_file(agent, pxp_agent_config_file(agent), pxp_config_json_using_puppet_certs(master, agent).to_s)
      on agent, puppet('resource service pxp-agent ensure=running')
      assert(is_associated?(master, "pcp://#{agent}/agent"),
             "Agent #{agent} with PCP identity pcp://#{agent}/agent should be associated with pcp-broker")
    end
  end

  step 'Start long-running Puppet agent jobs' do

    agents.each do |agent|
      on agent, puppet('agent', '--test', '--environment', environment_name, '--server', "#{master}",
                       '</dev/null >/dev/null 2>&1 & echo $!')
    end
  end

  step 'Wait until Puppet starts executing' do
    agents.each do |agent|
      wait_for_sleep_process(agent, SECONDS_TO_SLEEP)
    end
  end

  target_identities = []
  agents.each do |agent|
    target_identities << "pcp://#{agent}/agent"
  end

  transaction_ids = []
  step 'Run Puppet on agents' do
    transaction_ids = start_puppet_non_blocking_request(master, target_identities)
  end

  teardown do
    # Make sure we stop sleep processes when the test finishes, to avoid leaving stranded processes running.
    stop_sleep_process(agents, SECONDS_TO_SLEEP)
  end

  agents.each do |agent|
    step "Kill the first agent run on #{agent}" do
      lockfile = nil
      on agent, puppet('agent', '--configprint', 'agent_catalog_run_lockfile') do |result|
        lockfile = result.stdout.chomp
      end

      assert(agent.file_exist?(lockfile), 'First agent run completed before non-blocking-request returned')
      pid = nil
      on agent, "cat #{lockfile}" do |result|
        pid = result.stdout.chomp
      end

      if windows?(agent)
        on agent, "taskkill /F /pid #{pid}"
      else
        on agent, "kill -9 #{pid}"
      end
    end
  end

  # Note this will take 30+ seconds, as pxp-module-puppet waits that long to retry.
  target_identities.zip(transaction_ids).each do |identity, transaction_id|
    step "Check response to blocking request for #{identity}" do
      check_puppet_non_blocking_response(identity, transaction_id,
                                         STATUS_QUERY_MAX_RETRIES, STATUS_QUERY_INTERVAL_SECONDS,
                                         'unchanged')
    end
  end
end
