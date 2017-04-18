require 'pxp-agent/config_helper'
require 'pxp-agent/test_helper'

test_name 'Start pxp-agent daemon with pidfile present' do
  applicable_agents = agents.select { |agent| agent['platform'] !~ /aix/ && agent['platform'] !~ /osx/ }
  unless applicable_agents.length > 0 then
    skip_test('OSX and AIX hosts use --foreground')
  end

  applicable_agents = applicable_agents.reject do |agent|
    on(agent, 'service pxp-agent status', :accept_all_exit_codes => true)
    stdout =~ /systemd/
  end
  unless applicable_agents.length > 0 then
    skip_test('systemd hosts use --foreground')
  end

  teardown do
    applicable_agents.each do |agent|
      on(agent, puppet('resource service pxp-agent ensure=running'))
    end
  end

  step 'Ensure each agent host has pxp-agent service running and enabled' do
    applicable_agents.each do |agent|
      on agent, puppet('resource service pxp-agent ensure=stopped')
      create_remote_file(agent, pxp_agent_config_file(agent), pxp_config_json_using_puppet_certs(master, agent).to_s)
      on agent, puppet('resource service pxp-agent ensure=running enable=true')

      assert(is_associated?(master, "pcp://#{agent}/agent"),
             "Agent #{agent} with PCP identity pcp://#{agent}/agent should be associated with pcp-broker")
    end
  end

  applicable_agents.each do |agent|
    pids = []

    step 'Start new pxp-agent service in daemon mode' do
      pids = get_process_pids(agent, 'pxp-agent')
      assert_equal(1, pids.count, 'Only one pxp-agent process should be running')

      alternate_log_dir = agent.tmpdir('alternate_log')
      alternate_log_file = File.join(alternate_log_dir, 'pxp-agent.log')
      pxp_agent_bindir = agent['privatebindir'].gsub('sys/ruby', 'pxp-agent')
      libs_bindir = agent['privatebindir'].gsub('sys/ruby', 'puppet')
      bindirs = "#{pxp_agent_bindir}:#{libs_bindir}:#{agent['privatebindir']}"
      on(agent, "env PATH=\"#{bindirs}:${PATH}\" pxp-agent --logfile #{alternate_log_file}", :acceptable_exit_codes => [1])

      # Check log file before process. Once it contains the message, the new pxp-agent should be stopped.
      if windows?(agent)
        expect_file_on_host_to_contain(agent, alternate_log_file, /Unable to acquire process lock/)
      else
        expect_file_on_host_to_contain(agent, alternate_log_file, /Already running /)
      end

      assert_equal(pids, get_process_pids(agent, 'pxp-agent'), 'Only the original pxp-agent process should be running')
    end

    step 'Kill -9 old pxp-agent service and start service' do
      if windows?(agent)
        on agent, "taskkill /F /pid #{pids[0]}"
      else
        on agent, "kill -9 #{pids[0]}"
      end

      on(agent, puppet('resource service pxp-agent ensure=running'))
      assert(is_associated?(master, "pcp://#{agent}/agent"))
      refute_equal(pids, get_process_pids(agent, 'pxp-agent'), 'A new pxp-agent process should be running')
    end
  end
end
