require 'pxp-agent/task_helper.rb'

test_name 'run echo task' do

  step 'Ensure each agent host has pxp-agent running and associated' do
    agents.each do |agent|
      on agent, puppet('resource service pxp-agent ensure=stopped')
      create_remote_file(agent, pxp_agent_config_file(agent), pxp_config_json_using_puppet_certs(master, agent).to_s)
      on agent, puppet('resource service pxp-agent ensure=running')

      assert(is_associated?(master, "pcp://#{agent}/agent"),
             "Agent #{agent} with PCP identity pcp://#{agent}/agent should be associated with pcp-broker")
    end
  end

  step 'Create echo task on agent hosts' do
    agents.each do |agent|
      if agent['platform'] =~ /win/
        task_body = '@echo %PT_message%'
      else
        task_body = "#!/bin/sh\necho $PT_message"
      end

      @sha256 = create_task_on(agent, 'echo', 'init.bat', task_body)
    end
  end

  step 'Run echo task on agent hosts' do
    run_successful_task(master, agents, 'echo', 'init.bat', @sha256, {:message => 'hello'}) do |stdout|
      assert_equal('hello', stdout.strip, "Output did not contain 'hello'")
    end
  end # test step
end # test
