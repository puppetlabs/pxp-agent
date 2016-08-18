require 'pxp-agent/test_helper.rb'
require 'puppet/acceptance/environment_utils'

test_name 'C98107 - Run puppet with non-ASCII characters in Puppet code' do
  extend Puppet::Acceptance::EnvironmentUtils

  env_name = test_file_name = File.basename(__FILE__, '.*')
  environment_name = mk_tmp_environment(env_name)

  step 'On master, create a new environment that includes a non-ASCII character and will result in Changed' do
    site_manifest = "#{environmentpath}/#{environment_name}/manifests/site.pp"
    create_remote_file(master, site_manifest, <<-SITEPP)
node default {
  notify {'☃':}
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

  step "Send an rpc_blocking_request to all agents" do
    target_identities = []
    agents.each do |agent|
      target_identities << "pcp://#{agent}/agent"
    end
    responses = nil # Declare here so not local to begin/rescue below
    begin
      responses = rpc_blocking_request(master, target_identities,
                                      'pxp-module-puppet', 'run',
                                      {:env => [], :flags => ['--environment', environment_name]})
    rescue => exception
      fail("Exception occurred when trying to run Puppet on all agents: #{exception.message}")
    end
    agents.each_with_index do |agent|
      step "Check Run Puppet response for #{agent}" do
        identity = "pcp://#{agent}/agent"
        assert(responses[identity][:data].has_key?("results"),
              "Agent #{agent} received a response for rpc_blocking_request but it is missing a 'results' entry: #{responses[identity]}")
        action_result = responses[identity][:data]["results"]
        # The test's pass/fail criteria is only the value of 'status'. However, if something goes wrong and Puppet needs to default
        # the environment to 'production' and results in 'unchanged' then it's better to fail specifically on the environment.
        assert(action_result.has_key?('environment'), "Results for pxp-module-puppet run on #{agent} should contain an 'environment' field")
        assert_equal(environment_name, action_result['environment'], "Result of pxp-module-puppet run on #{agent} should run with the "\
                                                                     "#{environment_name} environment")
        assert(action_result.has_key?('status'), "Results for pxp-module-puppet run on #{agent} should contain a 'status' field")
        assert_equal('changed', action_result['status'], "Result of pxp-module-puppet run on #{agent} should be 'changed'")
      end
    end
  end
end
