require 'pxp-agent/bolt_pxp_module_helper.rb'

test_name 'run_command task' do

  tag 'audit:high',      # module validation: no other venue exists to test
      'audit:acceptance'

  suts = agents.reject { |host| host['roles'].include?('master') }

  step 'Ensure each agent host has pxp-agent running and associated' do
    suts.each do |agent|
      on agent, puppet('resource service pxp-agent ensure=stopped')
      # don't capture /tmp/test.out in pxp-agent.log
      create_remote_file(agent, pxp_agent_config_file(agent), pxp_config_hocon_using_puppet_certs(master, agent, loglevel: "info"))
      on agent, puppet('resource service pxp-agent ensure=running')

      assert(is_associated?(master, "pcp://#{agent}/agent"),
             "Agent #{agent} with PCP identity pcp://#{agent}/agent should be associated with pcp-broker")
    end
  end

  step 'Run a command (`echo`) on agent hosts' do
    suts.each do |agent|
      cmd = agent.platform =~ /windows/ ? 'write-host hello' : 'echo hello'
      run_successful_command(master, [agent], cmd) do |stdout|
        assert_equal('hello', stdout.chomp)
      end
    end
  end # test step

  step 'Responses that create large output which exceeds max-message-size error' do
    suts.each do |agent|
      # This test takes a bunch of resources to process correctly (and
      # not produce false positive failures). Platforms like MacOS, AIX
      # and older windows have a hard time reliably running the test
      # so we just confine the platforms to the standard Linuxes
      if agent.platform =~ /(el|ubuntu)/
        teardown do
          on(agent, 'rm /tmp/test.out')
        end
        on(agent, '/opt/puppetlabs/puppet/bin/ruby -e "puts \'a\'* 70 * 1024 * 1024" > /tmp/test.out')
        cmd = 'cat /tmp/test.out'
        run_errored_command(master, [agent], cmd) do |stdout|
          assert_match(/exceeded max-message-size/, stdout)
        end
      end
    end
  end # test step
end # test
