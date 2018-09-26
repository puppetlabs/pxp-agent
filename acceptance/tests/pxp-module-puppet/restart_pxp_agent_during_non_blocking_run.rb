require 'pxp-agent/test_helper.rb'
require 'puppet/acceptance/environment_utils'

SECONDS_TO_SLEEP = 500 # The test will use SIGALARM to end this as soon as required
STATUS_QUERY_MAX_RETRIES = 60
STATUS_QUERY_INTERVAL_SECONDS = 5

test_name 'C94705 - Run Puppet (non-blocking request) and restart pxp-agent service during run' do

  extend Puppet::Acceptance::EnvironmentUtils

  applicable_agents = agents

  teardown do
    unless applicable_agents.empty? then
      stop_sleep_process(applicable_agents, SECONDS_TO_SLEEP, true)
    end
    sleep 3000
  end

  env_name = test_file_name = File.basename(__FILE__, '.*')
  environment_name = mk_tmp_environment(env_name)

  teardown do
    if (!applicable_agents.empty?)
      applicable_agents.each do |agent|
        # If puppet agent run has been left running by this test failing, terminate it
        if agent.file_exist?('/opt/puppetlabs/puppet/cache/state/agent_catalog_run.lock')
          on(agent 'kill -9 `cat /opt/puppetlabs/puppet/cache/state/agent_catalog_run.lock`')
        end
      end
    end
  end

  step 'On master, create a new environment that will result in a slow run' do
    site_manifest = "#{environmentpath}/#{environment_name}/manifests/site.pp"
    create_sleep_manifest(master, site_manifest, SECONDS_TO_SLEEP)
  end

  step 'Ensure each agent host has pxp-agent running and associated' do
    agents.each do |agent|
      on agent, puppet('resource service pxp-agent ensure=stopped')
      create_remote_file(agent, pxp_agent_config_file(agent), pxp_config_hocon_using_puppet_certs(master, agent))
      on agent, puppet('resource service pxp-agent ensure=running enable=true')

      assert(is_associated?(master, "pcp://#{agent}/agent"),
             "Agent #{agent} with PCP identity pcp://#{agent}/agent should be associated with pcp-broker")
    end
  end

  applicable_agents.each do |agent|
    agent_identity = "pcp://#{agent}/agent"
    transaction_id = nil
    step "Make a non-blocking puppet run request on #{agent}" do
      provisional_responses = rpc_non_blocking_request(master, [agent_identity],
                                      'pxp-module-puppet', 'run',
                                      {:env => [], :flags => ['--onetime',
                                                              '--no-daemonize',
                                                              "--environment", "#{environment_name}"]})
      assert_equal(provisional_responses[agent_identity][:envelope][:message_type],
                   "http://puppetlabs.com/rpc_provisional_response",
                   "Did not receive expected rpc_provisional_response in reply to non-blocking request")
      transaction_id = provisional_responses[agent_identity][:data]["transaction_id"]
    end

    step 'Wait to ensure that Puppet has time to execute manifest' do
      wait_for_sleep_process(agent, SECONDS_TO_SLEEP)
    end

    step "Restart pxp-agent service on #{agent}" do
      on agent, puppet('resource service pxp-agent ensure=stopped')
      on agent, puppet('resource service pxp-agent ensure=running')
      # Wait for the service to be reconnected
      unless is_associated?(master, agent_identity) then
        fail("Agent has not reconnected after #{PCP_INVENTORY_RETRIES} inventory queries")
      end
    end

    step 'Signal sleep process to end so Puppet run will complete' do
      stop_sleep_process(agent, SECONDS_TO_SLEEP)
    end

    step 'Check response of puppet run' do
      check_puppet_non_blocking_response(agent_identity, transaction_id,
                                         STATUS_QUERY_MAX_RETRIES, STATUS_QUERY_INTERVAL_SECONDS,
                                         'changed', environment_name)
    end
  end
end
