require 'pxp-agent/test_helper.rb'
require 'puppet/acceptance/environment_utils'

test_name 'Run Puppet while a Puppet Agent run is in-progress, wait for completion' do
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
    default:  { exec { '/bin/sleep 10': } }
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

  step 'Send two requests to all agents, the first with a long-running task' do
    target_identities = []
    agents.each do |agent|
      target_identities << "pcp://#{agent}/agent"
    end

    provisional_responses = rpc_non_blocking_request(master, target_identities,
                                                     'pxp-module-puppet', 'run',
                                                     {:flags => ['--onetime',
                                                                 '--no-daemonize',
                                                                 '--environment', environment_name]})

    responses = nil # Declare here so not local to begin/rescue below
    begin
      responses = rpc_blocking_request(master, target_identities,
                                      'pxp-module-puppet', 'run',
                                      {:flags => ['--onetime',
                                                  '--no-daemonize']})
    rescue => exception
      fail("Exception occurred when trying to run Puppet on all agents: #{exception.message}")
    end

    agents.each_with_index do |agent|
      identity = "pcp://#{agent}/agent"

      step "Check response to non-blocking request for #{agent}" do
        # The first runs should be done, because the 2nd run shouldn't have even started until
        # the first finished.
        assert_equal("http://puppetlabs.com/rpc_provisional_response",
                     provisional_responses[identity][:envelope][:message_type],
                     "Did not receive expected rpc_provisional_response in reply to non-blocking request")
        transaction_id = provisional_responses[identity][:data]["transaction_id"]

        query_responses = rpc_blocking_request(master, [identity],
                                               'status', 'query', {:transaction_id => transaction_id})
        action_result = query_responses[identity][:data]["results"]

        puppet_run_result = nil
        rpc_action_status = action_result['status']
        if (action_result.has_key?('stdout') && (action_result['stdout'] != ""))
          puppet_run_result = JSON.parse(action_result['stdout'])['status']
        end

        if (!puppet_run_result)
          fail("Run puppet non-blocking transaction did not contain stdout of puppet run")
        else
          assert_equal("success", rpc_action_status, "PXP run puppet action did not have expected 'success' result")
        end
      end

      step "Check response to blocking request for #{agent}" do
        action_result = responses[identity][:data]["results"]
        assert(action_result.has_key?('status'), "Results for pxp-module-puppet run on #{agent} should contain a 'status' field")
        assert_equal('unchanged', action_result['status'], "Result of pxp-module-puppet run on #{agent} should be 'unchanged'")
      end
    end
  end
end
