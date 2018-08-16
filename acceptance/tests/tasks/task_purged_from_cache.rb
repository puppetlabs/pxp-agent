require 'pxp-agent/task_helper.rb'

test_name 'remove old task from pxp-agent cache' do

  task_name = 'foo'
  nix_cache_path = '/opt/puppetlabs/pxp-agent/tasks-cache/'
  win_cache_path = '/cygdrive/c/ProgramData/PuppetLabs/pxp-agent/tasks-cache/'

  step 'Remove contents of cache' do
    agents.each do |agent|
      agent_cache_path = (agent['platform'] =~ /win/) ? win_cache_path : nix_cache_path
      on(agent, "rm -rf #{agent_cache_path}")
    end
  end

  step 'Start agent with task-cache-dir-purge-ttl configured' do
    agents.each do |agent|
      on(agent, puppet('resource service pxp-agent ensure=stopped'))
      config_hash = pxp_config_hash_using_puppet_certs(master, agent)
      config_hash['task-cache-dir-purge-ttl'] = '1d'
      create_remote_file(agent, pxp_agent_config_file(agent), to_hocon(config_hash))
      on(agent, puppet('resource service pxp-agent ensure=running'))

      assert(is_associated?(master, "pcp://#{agent}/agent"),
             "Agent #{agent} with PCP identity pcp://#{agent}/agent should be associated with pcp-broker")
    end
  end

  step "Create #{task_name} task on agent hosts and run it to populate cache" do
    win_agents, nix_agents = agents.partition { |agent| windows?(agent) }
    [[win_agents, '@echo %PT_message%'], [nix_agents, "#!/bin/sh\necho $PT_message"]].each do |targets, task_body|
      targets.each do |agent|
        @sha256 = create_task_on(agent, task_name, 'init.bat', task_body)
      end
      files = [file_entry('init.bat', @sha256)]
      run_successful_task(master, targets, task_name, files, {:message => 'hello'}){}
    end
  end

  step 'Update access time on file and restart pxp-agent to trigger cache purge' do
    agents.each do |agent|
      agent_cache_path = (agent['platform'] =~ /win/) ? win_cache_path : nix_cache_path
      stamp = (DateTime.now - 4).strftime('%Y%m%d%H%M')
      on(agent, "find #{agent_cache_path} -type d") do |result|
        @target_directory_path = result.stdout.match(/#{Regexp.escape(agent_cache_path)}\/*(\w+)$/)[0]
      end
      on(agent, "touch -m -t #{stamp} #{@target_directory_path}")
      on(agent, puppet('resource service pxp-agent ensure=stopped'))
      on(agent, puppet('resource service pxp-agent ensure=running'))

      # retry until file is purged from cache
      begin
        Timeout::timeout(10) do
          until @success
            on(agent, "test -d #{@target_directory_path}", accept_all_exit_codes: true) do |result|
              @success = true if result.exit_code == 1
            end
          end
        end
      rescue TimeoutError
        nil
      ensure
        assert(@success, 'The cached file was not purged on restart of pxp-agent')
      end
    end
  end
end
