require 'pxp-agent/bolt_pxp_module_helper.rb'

test_name 'run_command task' do

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

  step 'Run a command (`echo`) on agent hosts' do
    agents.each do |agent|
      cmd = agent.platform =~ /windows/ ? 'write-host hello' : 'echo hello'
      run_successful_command(master, [agent], cmd) do |stdout|
        assert_equal('hello', stdout.chomp)
      end
    end
  end # test step

  step 'Responses that create large output which exceedes max-message-size error' do
    agents.each do |agent|
      # Escaping quotes for powershell turns out to be more confusing than just
      # getting away from quotes by coercing an integer to a string
      win_cmd = '$x = 1; $y = $x.ToString() * 65 * 1012 * 1012; Write-Output($y)'
      nix_cmd = '/opt/puppetlabs/puppet/bin/ruby -e "puts \'a\'* 65 * 1012 * 1012"'
      cmd = agent.platform =~ /windows/ ? win_cmd : nix_cmd
      run_errored_command(master, [agent], cmd) do |stdout|
        assert_match(/exceeded max-message-size/, stdout)
      end
    end
  end # test step
end # test
