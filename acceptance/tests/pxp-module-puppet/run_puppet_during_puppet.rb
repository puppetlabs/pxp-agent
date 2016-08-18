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
  step 'Check for lockfile on agents' do
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
  end

  target_identities = []
  agents.each do |agent|
    target_identities << "pcp://#{agent}/agent"
  end
  responses = nil # Declare here so not local to begin/rescue below

  step "Run Puppet on agents" do
    begin
      responses = rpc_blocking_request(master, target_identities,
                                       'pxp-module-puppet', 'run',
                                       {:flags => ['--onetime',
                                                   '--no-daemonize']})
    rescue => exception
      fail("Exception occurred when trying to run Puppet on all agents: #{exception.message}")
    end
  end

  target_identities.each do |identity|
    step "Check response to blocking request for #{identity}" do
      action_result = responses[identity][:data]["results"]
      assert(action_result.has_key?('status'), "Results for pxp-module-puppet run on #{identity} should contain a 'status' field")
      assert_equal('unchanged', action_result['status'], "Result of pxp-module-puppet run on #{identity} should be 'unchanged'")
    end
  end
end
