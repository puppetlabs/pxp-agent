require 'pxp-agent/test_helper.rb'
require 'pxp-agent/task_helper.rb'
require 'puppet/acceptance/environment_utils.rb'

test_name 'Connect via proxy' do
  extend Puppet::Acceptance::EnvironmentUtils

  # skip amazon6/7 tests
  agents.each do |agent|
    if agent['template'] =~ /amazon/
      skip_test 'Amazon test instances not configured for proxy test setup'
    end
  end

  # init
  squid_log = "/var/log/squid/access.log"
  proxy = setup_squid_proxy(master)
  win_agents, nix_agents = agents.partition { |agent| windows?(agent) }
  PUPPETSERVER_CONFIG_FILE = '/etc/puppetlabs/puppetserver/conf.d/webserver.conf'

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

  step 'Create the downloadable tasks on the master' do
    # Make the static content path
    env_name = File.basename(__FILE__, '.*')
    @static_content_path = File.join(environmentpath, mk_tmp_environment(env_name))

    # Create the tasks
    nix_task_body = "#!/bin/sh\n#Comment to change sha\necho $PT_message"
    win_task_body = "@REM Comment to change sha\n@echo %PT_message%"
    @nix_sha256, @win_sha256 = { "init" => nix_task_body, "init.bat" => win_task_body }.map do |filename, task_body|
      taskpath = "#{@static_content_path}/#{filename}"
      create_remote_file(master, taskpath, task_body)
      on master, "chmod 1777 #{taskpath}"
      Digest::SHA256.hexdigest(task_body + "\n")
    end
  end

  step 'Setup the static task file mount on puppetserver' do
    on master, puppet('resource service puppetserver ensure=stopped')
    hocon_file_edit_in_place_on(master, PUPPETSERVER_CONFIG_FILE) do |host, doc|
      doc.set_config_value("webserver.static-content", Hocon::ConfigValueFactory.from_any_ref([{
        "path" => "/task-files",
        "resource" => @static_content_path
      }]))
    end
    on master, puppet('resource service puppetserver ensure=running')
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
        assert_match(/ensure => 'absent'/, on(agent, puppet("resource file #{tasks_cache}/#{sha256}")).stdout)
      end
      # download task through the web proxy
      run_successful_task(master, agents, 'echo', filename, sha256, {:message => 'hello'}, "/task-files/#{filename}") do |stdout|
        assert_equal('hello', stdout.strip, "Output did not contain 'hello'")
      end

      agents.each do |agent|
        # stop agent to ensure log is generated in proxy access log
        on(agent, puppet('resource service pxp-agent ensure=stopped'))
      end
      # each agent should have two entries in squid proxy log
      on(master, "cat #{squid_log}") do |result|
        assert_equal(result.stdout.split("\n").length, agents.length * 2)
        assert_match(/CONNECT #{master}/, result.stdout, 'Proxy logs did not indicate use of the proxy.' )
      end
      clear_squid_log(master)
    end
  end
end