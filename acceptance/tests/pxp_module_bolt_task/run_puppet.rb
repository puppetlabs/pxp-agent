require 'pxp-agent/bolt_pxp_module_helper.rb'

test_name 'run puppet task' do

  step 'Ensure each agent host has pxp-agent running and associated' do
    agents.each do |agent|
      on agent, puppet('resource service pxp-agent ensure=stopped')
      create_remote_file(agent, pxp_agent_config_file(agent), pxp_config_hocon_using_puppet_certs(master, agent))
      on agent, puppet('resource service pxp-agent ensure=running')

      assert(is_associated?(master, "pcp://#{agent}/agent"),
             "Agent #{agent} with PCP identity pcp://#{agent}/agent should be associated with pcp-broker")
    end
  end

  step 'Create puppet task on agent hosts' do
    agents.each do |agent|
      # Note: shebang is hard
      # https://stackoverflow.com/questions/9988125/shebang-pointing-to-script-also-having-shebang-is-effectively-ignored
      # https://unix.stackexchange.com/questions/63979/shebang-line-with-usr-bin-env-command-argument-fails-on-linux
      if agent['platform'] =~ /osx/
        shebang = '#!/opt/puppetlabs/puppet/bin/ruby /opt/puppetlabs/puppet/bin/puppet apply'
      elsif agent['platform'] =~ /el-5|solaris-|aix-/
        shebang = '#!/usr/bin/env puppet-apply'
        create_remote_file(agent, '/usr/bin/puppet-apply', <<-EOF)
#!/bin/sh
exec /opt/puppetlabs/puppet/bin/puppet apply "$@"
EOF
        on agent, 'chmod +x /usr/bin/puppet-apply'
        teardown { on agent, 'rm -f /usr/bin/puppet-apply' }
      else
        shebang = '#!/opt/puppetlabs/bin/puppet apply'
      end

      task_body = "#{shebang}\nnotify { 'hello': }"

      @sha256 = create_task_on(agent, 'hello', 'init.pp', task_body)
    end
  end

  step 'Run puppet task on agent hosts' do
    files = [file_entry('init.pp', @sha256)]
    run_successful_task(master, agents, 'hello', files, input: {:data => [1, 2, 3]}) do |stdout|
      assert_match(/Notify\[hello\]\/message: defined 'message' as 'hello'/, stdout, "Output did not contain 'hello'")
    end
  end # test step
end # test
