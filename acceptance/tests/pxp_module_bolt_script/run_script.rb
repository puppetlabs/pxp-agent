require 'pxp-agent/bolt_pxp_module_helper.rb'
require 'pxp-agent/config_helper.rb'
require 'puppet/acceptance/environment_utils.rb'

PUPPETSERVER_CONFIG_FILE = '/etc/puppetlabs/puppetserver/conf.d/webserver.conf'

suts = agents.reject { |host| host['roles'].include?('master') }

def clean_files(agent)
  tasks_cache = get_tasks_cache(agent)
  assert_match(/ensure\s+=> 'absent'/, on(agent, puppet("resource file #{tasks_cache}/#{@sha256} ensure=absent force=true")).stdout)
end

test_name 'run script tests' do
  extend Puppet::Acceptance::EnvironmentUtils
  step 'Ensure each agent host has pxp-agent running and associated' do
    agents.each do |agent|
      on agent, puppet('resource service pxp-agent ensure=stopped')
      create_remote_file(agent, pxp_agent_config_file(agent), pxp_config_hocon_using_puppet_certs(master, agent))
      on agent, puppet('resource service pxp-agent ensure=running')

      assert(is_associated?(master, "pcp://#{agent}/agent"),
             "Agent #{agent} with PCP identity pcp://#{agent}/agent should be associated with pcp-broker")
    end
  end

  step 'Create the downloadable scripts on the master' do
    # Make the static content path
    env_name = File.basename(__FILE__, '.*')
    @static_content_path = File.join(environmentpath, mk_tmp_environment(env_name))
    # Create a linux script
    unix_file_body = "#!/usr/bin/env bash\necho \"TEST SCRIPT: $1\"\nexit 0"
    unix_filename = 'unix_test.sh'
    win_file_body = "Write-Output \"TEST SCRIPT: $($Args[0])\"\nexit 0"
    win_filename = 'win_test.ps1'
    @unix_sha256, @win_sha256 = { unix_filename => unix_file_body, win_filename => win_file_body }.map do |filename, file_body|
      filepath = "#{@static_content_path}/#{filename}"
      create_remote_file(master, filepath, file_body)
      on master, "chmod 1777 #{filepath}"
      Digest::SHA256.hexdigest(file_body + "\n")
    end
    @win_source = "/download-test-files/#{win_filename}"
    @unix_source = "/download-test-files/#{unix_filename}"
  end

  step 'Setup the static script file mount on puppetserver' do
    on master, puppet('resource service puppetserver ensure=stopped')
    hocon_file_edit_in_place_on(master, PUPPETSERVER_CONFIG_FILE) do |host, doc|
      doc.set_config_value("webserver.static-content", Hocon::ConfigValueFactory.from_any_ref([{
        "path" => "/download-test-files",
        "resource" => @static_content_path
      }]))
    end
    on master, puppet('resource service puppetserver ensure=running')
  end


  # Spec testing can cover most of the functionality for running a script,
  # we only really require one test to ensure the script module is correctly
  # connected through the agent and is able to execute on a message actually
  # sent from a broker.
  step 'Execute a script run' do
    suts.each do |agent|
      if windows?(agent)
        test_file = 'win_test.ps1'
        test_source = @win_source
        test_sha = @win_sha256
      else
        test_file = 'unix_test.sh'
        test_source = @unix_source
        test_sha = @unix_sha256
      end

      run_successful_script(master, agent, script_file_entry(test_file, test_sha, test_source), ['ARGS_TEST']) do |std_out|
        assert(std_out.include?("TEST SCRIPT: ARGS_TEST"))
      end

      teardown {
        clean_files(agent)
      }
    end
  end
end
