require 'pxp-agent/bolt_pxp_module_helper.rb'

test_name 'run ruby task' do

  tag 'audit:high',      # module validation: no other venue exists to test
      'audit:acceptance'

  step 'Ensure each agent host has pxp-agent running and associated' do
    agents.each do |agent|
      on agent, puppet('resource service pxp-agent ensure=stopped')
      create_remote_file(agent, pxp_agent_config_file(agent), pxp_config_hocon_using_puppet_certs(master, agent))
      on agent, puppet('resource service pxp-agent ensure=running')

      assert(is_associated?(master, "pcp://#{agent}/agent"),
             "Agent #{agent} with PCP identity pcp://#{agent}/agent should be associated with pcp-broker")
    end
  end

  step 'Create ruby task on agent hosts' do
    agents.each do |agent|
      task_body = "#!/opt/puppetlabs/puppet/bin/ruby\nputs STDIN.gets\nputs ENV['PT_data']"
      @sha256 = create_task_on(agent, 'echo', 'init.rb', task_body)
    end
  end

  step 'Run ruby task on agent hosts' do
    files = [task_file_entry('init.rb', @sha256)]
    run_successful_task(master, agents, 'echo', files, input: {:data => [1, 2, 3]}) do |stdout|
      json, data = stdout.delete("\r").split("\n")
      assert_equal({"data" => [1,2,3], "_task" => "echo"}, JSON.parse(json), "Output did not contain 'data'")
      assert_equal('[1,2,3]', data, "Output did not contain 'data'")
    end
  end # test step
end # test
