require 'pxp-agent/bolt_pxp_module_helper.rb'

test_name 'run_command task' do
  step 'Ensure each agent host has pxp-agent running and associated' do
    agents.each do |agent|
      on agent, puppet('resource service pxp-agent ensure=stopped')
      create_remote_file(agent, pxp_agent_config_file(agent), pxp_config_hocon_using_puppet_certs(master, agent))
      on agent, puppet('resource service pxp-agent ensure=running')

      assert(is_associated?(master, "pcp://#{agent}/agent"),
             "Agent #{agent} with PCP identity pcp://#{agent}/agent should be associated with pcp-broker")
    end
  end

  step 'Run a command (`echo`) on agent hosts' do
    agents.each do |agent|
      cmd = agent.platform =~ /windows/ ? 'write-host hello' : 'echo hello'
      run_successful_command(master, [agent], cmd) do |stdout|
        assert_equal('hello', stdout.chomp)
      end
    end
  end # test step
end # test
