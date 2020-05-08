require 'pxp-agent/bolt_pxp_module_helper.rb'
require 'pxp-agent/config_helper.rb'

suts = agents.reject { |host| host['roles'].include?('master') }

def clean_files(agent)
  tasks_cache = get_tasks_cache(agent)
  assert_match(/ensure\s+=> 'absent'/, on(agent, puppet("resource file #{tasks_cache}/#{@sha256} ensure=absent force=true")).stdout)
end

test_name 'run script tests' do

  tag 'audit:high',      # module validation: no other venue exists to test
      'audit:acceptance'

  test_env = 'bolt'

  step 'Ensure each agent host has pxp-agent running and associated' do
    agents.each do |agent|
      on agent, puppet('resource service pxp-agent ensure=stopped')
      create_remote_file(agent, pxp_agent_config_file(agent), pxp_config_hocon_using_puppet_certs(master, agent))
      on agent, puppet('resource service pxp-agent ensure=running')

      assert(is_associated?(master, "pcp://#{agent}/agent"),
             "Agent #{agent} with PCP identity pcp://#{agent}/agent should be associated with pcp-broker")
    end
  end

  # Spec testing can cover most of the functionality for running a script,
  # we only really require one test to ensure the script module is correctly
  # connected through the agent and is able to execute on a message actually
  # sent from a broker.
  step 'Execute a script run' do
    fixtures = File.absolute_path('files')
    fixture_env = File.join(fixtures, 'environments', test_env)

    suts.each do |agent|
      if windows?(agent)
        test_file = 'win_script.ps1'
      else
        test_file = 'unix_script.sh'
      end
      test_source = "/#{test_env}/#{test_file}"
      test_sha = Digest::SHA256.file(File.join(fixture_env, test_file)).hexdigest

      run_successful_script(master, agent, script_file_entry(test_file, test_sha, test_source), ['ARGS_TEST']) do |std_out|
        assert(std_out.include?("TEST SCRIPT: ARGS_TEST"))
      end

      teardown {
        clean_files(agent)
      }
    end
  end
end
