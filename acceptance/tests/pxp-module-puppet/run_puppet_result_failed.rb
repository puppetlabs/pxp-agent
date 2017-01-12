require 'pxp-agent/test_helper.rb'
require 'puppet/acceptance/environment_utils'

test_name 'C93063 - pxp-module-puppet run expect puppet failure' do

  agents.each do |agent|
    if agent.platform =~ /^cisco_ios_xr/
      skip_test 'PCP-685: Skip Cisco XR Platform'
    end
  end

  extend Puppet::Acceptance::EnvironmentUtils

  app_type = test_file_name = File.basename(__FILE__, '.*')
  tmp_environment  = mk_tmp_environment(app_type)

  step 'On master, create a new environment that will result in Failed' do
    site_manifest = File.join(environmentpath, tmp_environment, 'manifests', 'site.pp')
    create_remote_file(master, site_manifest, <<-SITEPP)
node default {
  exec{'nonesuchbinary':}
}
SITEPP
    on master, "chmod -R 755 #{File.join(environmentpath, tmp_environment)}"
  end

  step 'Ensure each agent host has pxp-agent running and associated' do
    agents.each do |agent|
      on agent, puppet('resource service pxp-agent ensure=stopped')
      create_remote_file(agent, pxp_agent_config_file(agent),
                         pxp_config_json_using_puppet_certs(master, agent).to_s)
      on agent, puppet('resource service pxp-agent ensure=running')
      assert(is_associated?(master, "pcp://#{agent}/agent"),
             "Agent #{agent} with PCP identity pcp://#{agent}/agent should be associated with pcp-broker")
    end
  end

  def assert_failed_agent_run(environment, action_result)
    assert(action_result.has_key?('status'),
           "Results for pxp-module-puppet run on #{agent} should contain a 'status' field")
    assert_equal(environment, action_result['environment'],
                 "Result of pxp-module-puppet run on #{agent} should run with the "\
                 "#{environment} environment")
    assert_equal('failed', action_result['status'],
                 "Result of pxp-module-puppet run on #{agent} in #{environment} should be 'failed'. was: "\
                 "'#{action_result["status"]}'")
    assert_equal(1, action_result['exitcode'],
                 "pxp-module-puppet on #{agent} in #{environment} should exit with 1. was: "\
                 "'#{action_result["exitcode"]}'")
    assert_equal('agent_exit_non_zero', action_result['error_type'],
                 "puppet-agent on #{agent} in #{environment} should exit non-0. was: "\
                 "'#{action_result["error_type"]}'")
  end

  def rpc_blocking_request_all_onetime(environment=nil)
    target_identities = []
    step "Send an rpc_blocking_request to all agents for #{environment}" do
      agents.each do |agent|
        target_identities << "pcp://#{agent}/agent"
      end
      responses = nil # Declare here so not local to begin/rescue below
      begin
        responses = rpc_blocking_request(master, target_identities,
                                         'pxp-module-puppet', 'run',
                                         {:env => [], :flags => ['--environment', environment,
                                                                 '--onetime']
        })
      rescue => exception
        fail("Exception occurred when trying to run Puppet on all agents: #{exception.message}")
      end
      return responses
    end
  end

  responses = rpc_blocking_request_all_onetime(tmp_environment)
  agents.each do |agent|
    step "Check Run Puppet response for #{agent}" do
      identity = "pcp://#{agent}/agent"
      action_result = responses[identity][:data]["results"]
      assert_failed_agent_run(tmp_environment, action_result)
    end
  end


  tmp_environment2  = mk_tmp_environment(app_type)

  step 'On master, create a new environment that will result in another type of Failed' do
    site_manifest = File.join(environmentpath, tmp_environment2, 'manifests', 'site.pp')
    create_remote_file(master, site_manifest, <<-SITEPP)
node default {
  fail('fail on purpose')
}
SITEPP
    on master, "chmod -R 755 #{File.join(environmentpath, tmp_environment2)}"
  end

  responses = rpc_blocking_request_all_onetime(tmp_environment2)
  agents.each do |agent|
    step "Check Run Puppet response for #{agent}" do
      identity = "pcp://#{agent}/agent"
      action_result = responses[identity][:data]["results"]
      assert_failed_agent_run(tmp_environment2, action_result)
    end
  end

end
