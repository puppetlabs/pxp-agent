require 'pxp-agent/test_helper.rb'
require 'json'

ENVIRONMENT_NAME = 'sleep'
MODULE_NAME = 'sleep'
SECONDS_TO_SLEEP = 500 # The test will use SIGALARM to end this as soon as required
STATUS_QUERY_MAX_RETRIES = 60
STATUS_QUERY_INTERVAL_SECONDS = 5

test_name 'C94705 - Run Puppet (non-blocking request) and restart pxp-agent service during run' do

  applicable_agents = agents.select { |agent| agent['platform'] !~ /win/}
  if applicable_agents.empty? then
    skip_test('Skipping - All agent hosts are Windows and are not applicable to this test (see PCP-276)')
  end

  master_environment_path = master.puppet['environmentpath']

  teardown do
    unless master_environment_path.to_s == ''
      retry_on(master, "rm -rf #{master_environment_path}/#{ENVIRONMENT_NAME}")
    end
    if (!applicable_agents.empty?)
      applicable_agents.each do |agent|
        # If puppet agent run has been left running by this test failing, terminate it
        if agent.file_exist?('/opt/puppetlabs/puppet/cache/state/agent_catalog_run.lock')
          on(agent 'kill -9 `cat /opt/puppetlabs/puppet/cache/state/agent_catalog_run.lock`')
        end
      end
    end
  end

  step 'On master, create a new environment with a module that will take some time to apply' do

    # Create the environment in a unique tmp folder and create a symbolic link to the Puppet environmentpath
    # The symbolic link will be deleted on teardown; but the tmp folder persists for debugging

    step 'Create a temp dir for the environment'
    tmp_environment_dir = master.tmpdir(ENVIRONMENT_NAME)
    on(master, "chmod 655 #{tmp_environment_dir}")
    on(master, "cp -r #{master_environment_path}/production/* #{tmp_environment_dir}")

    step 'Create the site manifest file'
    site_manifest = "#{tmp_environment_dir}/manifests/site.pp"
    create_remote_file(master, site_manifest, <<-SITEPP)
node default {
  include #{MODULE_NAME}
}
SITEPP
    on(master, "chmod 644 #{site_manifest}")

    step 'Create a module that will execute sleep on the agent'
    on(master, "mkdir -p #{tmp_environment_dir}/modules/#{MODULE_NAME}/manifests")
    module_manifest = "#{tmp_environment_dir}/modules/#{MODULE_NAME}/manifests/init.pp"
    create_remote_file(master, module_manifest, <<-MODULEPP)
class #{MODULE_NAME} {
  case $::osfamily {
    'windows': { exec { 'sleep':
                        command => 'true',
                        unless  => 'sleep #{SECONDS_TO_SLEEP}', #PUP-5806
                        path    => 'C:\\cygwin64\\bin',} }
    'Darwin':  { exec { 'sleep': command => '/bin/sleep #{SECONDS_TO_SLEEP} || /usr/bin/true', } }
    default:   { exec { 'sleep': command => '/bin/sleep #{SECONDS_TO_SLEEP} || /bin/true', } }
  }
}
MODULEPP
    on(master, "chmod 644 #{module_manifest}")

    step 'Link the environment\'s temp dir to the actual Puppet environmentpath'
    retry_on(master, "rm -rf #{master_environment_path}/#{ENVIRONMENT_NAME}") # Ensure folder does not pre-exist, or ln will create link inside it
    on(master, "ln -s #{tmp_environment_dir} #{master_environment_path}/#{ENVIRONMENT_NAME}")
  end

  step 'Ensure each agent host has pxp-agent running and associated' do
    agents.each do |agent|
      on agent, puppet('resource service pxp-agent ensure=stopped')
      create_remote_file(agent, pxp_agent_config_file(agent), pxp_config_json_using_puppet_certs(master, agent).to_s)
      on agent, puppet('resource service pxp-agent ensure=running enable=true')
      show_pcp_logs_on_failure do
        assert(is_associated?(master, "pcp://#{agent}/agent"),
               "Agent #{agent} with PCP identity pcp://#{agent}/agent should be associated with pcp-broker")
      end
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
                                                              "--environment", "'#{ENVIRONMENT_NAME}'"]})
      assert_equal(provisional_responses[agent_identity][:envelope][:message_type],
                   "http://puppetlabs.com/rpc_provisional_response",
                   "Did not receive expected rpc_provisional_response in reply to non-blocking request")
      transaction_id = provisional_responses[agent_identity][:data]["transaction_id"]
    end

    step 'Wait to ensure that Puppet has time to execute manifest' do
      begin
        retry_on(agent, "ps -ef | grep 'bin/sleep' | grep -v 'grep'", {:max_retries => 15, :retry_interval => 2})
      rescue
        fail("After triggering a puppet run on #{agent} the expected sleep process did not appear in process list")
      end
    end

    step "Restart pxp-agent service on #{agent}" do
      on agent, puppet('resource service pxp-agent ensure=stopped')
      on agent, puppet('resource service pxp-agent ensure=running')
    end

    step 'Signal sleep process to end so Puppet run will complete' do
      agent['platform'] =~ /osx/ ?
        command = "ps -e -o pid,comm | grep sleep | sed 's/^[^0-9]*//g' | cut -d\\  -f1" :
        command = "ps -ef | grep 'bin/sleep' | grep -v 'grep' | grep -v 'true' | sed 's/^[^0-9]*//g' | cut -d\\  -f1"
      on(agent, command) do |output|
        pid = output.stdout.chomp
        if (!pid || pid == '')
          fail('Did not find a pid for the sleep process holding up Puppet - cannot test PXP response if Puppet isn\'t sleeping')
        end
        agent['platform'] =~ /cisco_nexus/ ?
          on(agent, "kill -s ALRM #{pid}") :
          on(agent, "kill SIGALARM #{pid}")
      end
    end

    step 'Check response of puppet run' do
      puppet_run_result = nil
      query_attempts = 0
      until (query_attempts == STATUS_QUERY_MAX_RETRIES || puppet_run_result) do
        query_responses = rpc_blocking_request(master, [agent_identity],
                                        'status', 'query', {:transaction_id => transaction_id})
        action_result = query_responses[agent_identity][:data]["results"]
        if (action_result.has_key?('stdout') && (action_result['stdout'] != ""))
          rpc_action_status = action_result['status']
          puppet_run_result = JSON.parse(action_result['stdout'])['status']
          puppet_run_environment = JSON.parse(action_result['stdout'])['environment']
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
        assert_equal(rpc_action_status, "success", "PXP run puppet action did not have expected 'success' result")
        assert_equal(puppet_run_environment, ENVIRONMENT_NAME, "Puppet run did not use the expected environment")
        assert_equal(puppet_run_result, "changed", "Puppet run did not have expected result of 'changed'")
      end
    end
  end
end
