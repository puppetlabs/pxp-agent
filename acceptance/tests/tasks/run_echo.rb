require 'pxp-agent/task_helper.rb'

test_name 'run echo task' do

  step 'Ensure each agent host has pxp-agent running and associated' do
    agents.each do |agent|
      on agent, puppet('resource service pxp-agent ensure=stopped')
      create_remote_file(agent, pxp_agent_config_file(agent), pxp_config_hocon_using_puppet_certs(master, agent))
      on agent, puppet('resource service pxp-agent ensure=running')

      assert(is_associated?(master, "pcp://#{agent}/agent"),
             "Agent #{agent} with PCP identity pcp://#{agent}/agent should be associated with pcp-broker")
    end
  end

  step 'Create and run echo task on agent hosts' do
    win_agents, nix_agents = agents.partition { |agent| windows?(agent) }
    [[win_agents, '@echo %PT_message%'], [nix_agents, "#!/bin/sh\necho $PT_message"]].each do |targets, task_body|
      targets.each do |agent|
        @sha256 = create_task_on(agent, 'echo', 'init.bat', task_body)
      end
      files = [file_entry('init.bat', @sha256)]
      run_successful_task(master, targets, 'echo', files, {:message => 'hello'}) do |stdout|
        assert_equal('hello', stdout.strip, "Output did not contain 'hello'")
      end
    end
  end
end # test
