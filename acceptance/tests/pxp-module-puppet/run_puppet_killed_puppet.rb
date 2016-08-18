require 'pxp-agent/test_helper.rb'
require 'pxp-agent/config_helper.rb'
require 'puppet/acceptance/environment_utils'

STATUS_QUERY_MAX_RETRIES = 60
STATUS_QUERY_INTERVAL_SECONDS = 1

test_name 'Run Puppet while a Puppet Agent run is in-progress, wait for it to be killed' do
  extend Puppet::Acceptance::EnvironmentUtils

  env_name = test_file_name = File.basename(__FILE__, '.*')
  environment_name = mk_tmp_environment(env_name)

  step 'On master, create a new environment that will result in a slow run' do
    site_manifest = "#{environmentpath}/#{environment_name}/manifests/site.pp"
    create_remote_file(master, site_manifest, <<-SITEPP)
node default {
  case $::osfamily {
    'windows': { exec { 'sleep':
                        command => 'true',
                        unless  => 'sleep 10', #PUP-5806
                        path    => 'C:\\cygwin64\\bin',} }
    default:  { exec { '/bin/sleep 30': } }
  }
}
SITEPP
    on(master, "chmod 644 #{site_manifest}")
  end

  step 'Ensure each agent host has pxp-agent running and associated' do
    agents.each do |agent|
      on agent, puppet('resource service pxp-agent ensure=stopped')
      create_remote_file(agent, pxp_agent_config_file(agent), pxp_config_json_using_puppet_certs(master, agent).to_s)
      on agent, puppet('resource service pxp-agent ensure=running')
      show_pcp_logs_on_failure do
        assert(is_associated?(master, "pcp://#{agent}/agent"),
               "Agent #{agent} with PCP identity pcp://#{agent}/agent should be associated with pcp-broker")
      end
    end
  end

  lockfiles = []
  agents.each do |agent|
    step "Get lockfile location for #{agent}" do
      on agent, puppet('agent', '--configprint', 'agent_catalog_run_lockfile') do |result|
        lockfiles << result.stdout.chomp
      end
    end
  end

  agents.each do |agent|
    step "Start a long-running Puppet agent job on #{agent}" do
      on agent, puppet('agent', '--test', '--environment', environment_name, '--server', "#{master}",
                       '>/dev/null & echo $!')
    end
  end

  # TODO: switch to checking all agents after each sleep
  agents.zip(lockfiles).each do |agent, lockfile|
    step "Check for lockfile on #{agent}" do
      lockfile_exists = false
      for i in 1..50
        lockfile_exists |= agent.file_exist?(lockfile)
        if lockfile_exists
          break
        end
        sleep 0.2
      end
      assert(lockfile_exists, 'Agent run did not generate a lock file')
    end
  end

  target_identities = []
  agents.each do |agent|
    target_identities << "pcp://#{agent}/agent"
  end

  transaction_ids = []
  step "Run Puppet on #{agent}" do
    provisional_responses = rpc_non_blocking_request(master, target_identities,
                                                     'pxp-module-puppet', 'run',
                                                     {:flags => ['--onetime',
                                                                 '--no-daemonize']})
    target_identities.each do |identity|
      assert_equal("http://puppetlabs.com/rpc_provisional_response",
                   provisional_responses[identity][:envelope][:message_type],
                   "Did not receive expected rpc_provisional_response in reply to non-blocking request")
      transaction_ids << provisional_responses[identity][:data]["transaction_id"]
    end
  end

  # Ensure time has passed for pxp-module-puppet to begin waiting
  sleep 3

  agents.zip(lockfiles).each do |agent, lockfile|
    step "Kill the first agent run on #{agent}" do
      assert(agent.file_exist?(lockfile), 'First agent run completed before non-blocking-request returned')
      if windows?(agent)
        on agent, "taskkill /pid `cat #{lockfile}`"
      else
        on agent, "kill -9 `cat #{lockfile}`"
      end
    end
  end

  target_identities.zip(transaction_ids).each do |identity, transaction_id|
    step "Check response to blocking request for #{identity}" do
      puppet_run_result = nil
      query_attempts = 0
      until (query_attempts == STATUS_QUERY_MAX_RETRIES || puppet_run_result) do
        query_responses = rpc_blocking_request(master, [identity],
                                               'status', 'query', {:transaction_id => transaction_id})
        action_result = query_responses[identity][:data]["results"]
        if (action_result.has_key?('stdout') && (action_result['stdout'] != ""))
          rpc_action_status = action_result['status']
          puppet_run_result = JSON.parse(action_result['stdout'])['status']
        end
        query_attempts += 1
        if (!puppet_run_result)
          sleep STATUS_QUERY_INTERVAL_SECONDS
        end
      end
      if (!puppet_run_result)
        fail("Run puppet non-blocking transaction did not contain stdout of puppet run after #{query_attempts} attempts " \
             "and #{query_attempts * STATUS_QUERY_INTERVAL_SECONDS} seconds")
      else
        assert_equal("success", rpc_action_status, "PXP run puppet action did not have expected 'success' result")
        assert_equal("unchanged", puppet_run_result, "Puppet run did not have expected result of 'unchanged'")
      end
    end
  end
end
