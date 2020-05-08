require 'pxp-agent/test_helper.rb'
require 'pxp-agent/task_helper.rb'

test_name 'Connect via proxy' do

  tag 'audit:medium',   # proxy validation not user critical
      'audit:acceptance'

  # skip amazon6/7 tests
  agents.each do |agent|
    if agent['template'] =~ /amazon/
      skip_test 'Amazon test instances not configured for proxy test setup'
    end
  end

  # init
  test_env = 'bolt'
  squid_log = "/var/log/squid/access.log"
  proxy = setup_squid_proxy(master)
  non_master_agents = agents.reject { |host| host['roles'].include?('master') }
  win_agents, nix_agents = non_master_agents.partition { |agent| windows?(agent) }

  # if there is a failure make sure to reset agent to NOT use proxy
  teardown do
    agents.each do |agent|
      on agent, puppet('resource service pxp-agent ensure=stopped')
      create_remote_file(agent, pxp_agent_config_file(agent), pxp_config_hocon_using_puppet_certs(master, agent))
      on agent, puppet('resource service pxp-agent ensure=running')
    end
    teardown_squid_proxy(master)
  end

  step 'Assert that agent can communicate with PCP-broker through proxy' do
    agents.each do |agent|
      # restart agent to use proxy
      on(agent, puppet('resource service pxp-agent ensure=stopped'))
      assert(is_not_associated?(master, "pcp://#{agent}/agent"),
             "Agent identity pcp://#{agent}/agent for agent host #{agent} appears in pcp-broker's client inventory " \
             "but pxp-agent service is supposed to be stopped")
      create_remote_file(agent, pxp_agent_config_file(agent), pxp_config_hocon_using_puppet_certs(master, agent, broker_proxy: proxy))
      on(agent, puppet('resource service pxp-agent ensure=running'))
      # now stop agent so that the log is available in proxy access log (only shows up when connection is "done")
      on(agent, puppet('resource service pxp-agent ensure=stopped'))
      # prove the connection went through proxy
      on(master, "cat #{squid_log}") do |result|
        assert_match(/CONNECT #{master}/, result.stdout, 'Proxy logs did not indicate use of the proxy.' )
      end
      # restart the agent and make sure associated (prove that it connected with master)
      on(agent, puppet('resource service pxp-agent ensure=running'))
      assert(is_associated?(master, "pcp://#{agent}/agent"),
       "Agent identity pcp://#{agent}/agent for agent host #{agent} does not appear in pcp-broker's client inventory")
      clear_squid_log(master)
    end
  end

  step 'get shas for fixture files' do
    fixtures = File.absolute_path('files')
    fixture_env = File.join(fixtures, 'environments', test_env)
    @nix_sha256, @win_sha256 = { "init" => '', "init.bat" => '' }.map do |filename,|
      Digest::SHA256.file(File.join(fixture_env, filename)).hexdigest
    end
  end

  step 'Ensure each agent host has pxp-agent running with proxy configured and associated' do
    agents.each do |agent|
      on agent, puppet('resource service pxp-agent ensure=stopped')
      create_remote_file(agent, pxp_agent_config_file(agent), pxp_config_hocon_using_puppet_certs(master, agent, master_proxy: proxy))
      on agent, puppet('resource service pxp-agent ensure=running')

      assert(is_associated?(master, "pcp://#{agent}/agent"),
             "Agent #{agent} with PCP identity pcp://#{agent}/agent should be associated with pcp-broker")
    end
  end

  step 'Download and run the task on agent hosts via proxy' do
    test_cases = {}
    if win_agents.length > 0
      test_cases.merge!(win_agents => ["init.bat", @win_sha256])
    end
    if nix_agents.length > 0
      test_cases.merge!(nix_agents => ["init", @nix_sha256])
    end

    test_cases.each do |agents, (filename, sha256)|
      # Ensure that the task file does not exist beforehand so we know that
      # if it runs successfully in the later assertions, then it was downloaded.
      agents.each do |agent|
        tasks_cache = get_tasks_cache(agents.first)
        on agent, "rm -rf #{tasks_cache}/#{sha256}"
        assert_match(/ensure\s+=> 'absent'/, on(agent, puppet("resource file #{tasks_cache}/#{sha256}")).stdout)
      end
      # download task through the web proxy
      run_successful_task(master, agents, 'echo', filename, sha256, {:message => 'hello'}, "/#{test_env}/#{filename}") do |stdout|
        assert_equal('hello', stdout.strip, "Output did not contain 'hello'")
      end

      agents.each do |agent|
        # stop agent to ensure log is generated in proxy access log
        on(agent, puppet('resource service pxp-agent ensure=stopped'))
      end
      # each agent should have two entries in squid proxy log
      on(master, "cat #{squid_log}") do |result|
        agents.each do |agent|
          agent_ip = agent.ip
          match = result.stdout.split("\n").select { |line| line =~ /#{agent_ip}/ }
          assert_equal(2, match.length)
        end
        assert_match(/CONNECT #{master}/, result.stdout, 'Proxy logs did not indicate use of the proxy.' )
      end
      clear_squid_log(master)
    end
  end
end
