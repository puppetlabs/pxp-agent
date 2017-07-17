require 'pxp-agent/task_helper.rb'

test_name 'run puppet task' do

  step 'Ensure each agent host has pxp-agent running and associated' do
    agents.each do |agent|
      on agent, puppet('resource service pxp-agent ensure=stopped')
      create_remote_file(agent, pxp_agent_config_file(agent), pxp_config_json_using_puppet_certs(master, agent).to_s)
      on agent, puppet('resource service pxp-agent ensure=running')

      assert(is_associated?(master, "pcp://#{agent}/agent"),
             "Agent #{agent} with PCP identity pcp://#{agent}/agent should be associated with pcp-broker")
    end
  end

  step 'Create puppet task on agent hosts' do
    agents.each do |agent|
      create_task_on(agent, 'hello', 'init.pp', <<-EOF)
#!/opt/puppetlabs/bin/puppet apply
notify { 'hello': }
EOF
    end
  end

  step 'Run puppet task on agent hosts' do
    run_task(master, agents, 'hello', {:data => [1, 2, 3]}) do |stdout|
      assert_match(/Notify\[hello\]\/message: defined 'message' as 'hello'/, stdout['output'], "Output did not contain 'hello'")
    end
  end # test step
end # test
