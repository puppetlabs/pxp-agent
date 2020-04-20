require 'pxp-agent/config_helper.rb'

test_name 'Service Start stop/start, with configuration)'

@pxp_temp_file = '~/pxp-agent.conf'

# On teardown, restore configuration file on each agent
teardown do
  agents.each do |agent|
    # Create a valid config file
    create_remote_file(agent, pxp_agent_config_file(agent), pxp_config_hocon_using_puppet_certs(master, agent))

    # Ensure pxp-agent service is running
    # PUP-6702 - on Windows, the service may be in the Paused state
    # 'puppet resource service pxp-agent' will not work as expected
    # Workaround: stop/start it using net
    if(windows?(agent)) then
      on(agent, 'net stop pxp-agent', :accept_all_exit_codes => true) # Will error if already stopped, ignore
      on(agent, 'net start pxp-agent')
    end
    on(agent, puppet('resource service pxp-agent ensure=running'))

    # If the test left behind a temporary config file, remove it
    if agent.file_exist?(@pxp_temp_file)
      on(agent, "rm #{@pxp_temp_file}")
    end
  end
end

def stop_service
  on(@agent, puppet('resource service pxp-agent ensure=stopped'))
end

def start_service
  on(@agent, puppet('resource service pxp-agent ensure=running'))
end

def assert_stopped
  on(@agent, puppet('resource service pxp-agent ')) do |result|
    assert_match(/ensure\s+=> .stopped.,/, result.stdout,
                 "pxp-agent not in expected stopped state")
  end
end

def assert_running
  on(@agent, puppet('resource service pxp-agent ')) do |result|
    assert_match(/ensure\s+=> .running.,/, result.stdout,
                 "pxp-agent not in expected running state")
  end
end

def sysv?(platform)
  if platform.variant =~ /redhat|centos/ && platform.version.to_i < 7
    return true
  elsif platform.variant =~ /sles/ && platform.version.to_i < 12
    return true
  elsif platform.variant =~ /ubuntu/ && platform.version.to_i < 15
    return true
  else
    return false
  end
end

agents.each_with_index do |agent, i|
  @agent = agent

  create_remote_file(@agent, pxp_agent_config_file(@agent), pxp_config_hocon_using_puppet_certs(master, @agent))

  step 'C93070 - Service Start (from stopped, with configuration)' do
    stop_service
    assert_stopped
    start_service
    assert_running
  end

  step 'C93069 - Service Stop (from running, with configuration)' do
    stop_service
    assert_stopped
  end

  if sysv?(@agent['platform'])
    step '`service stop` stops all pxp-agent processes' do
      start_service
      assert_running
      on(@agent, "rm -rf /var/run/puppetlabs/pxp-agent.pid")
      on(@agent, "service pxp-agent restart")
      pids = on(@agent, "pgrep -u `whoami` -f /opt/puppetlabs/puppet/bin/pxp-agent").stdout.split("\n")
      assert_equal(1, pids.length, "Expected 1 pxp-agent process, got #{pids.length}")
    end
  end

  # Solaris service administration will prevent the service from starting
  # if it is un-configured because it has been defined as required.
  # See: https://github.com/puppetlabs/pxp-agent/blob/stable/ext/solaris/smf/pxp-agent.xml#L10-L12
  #
  # Therefore, the un-configured test steps need to be skipped on Solaris
  #
  # Also needs skipped on OSX, as the pxp-agent executable will exit but the service will
  # continue running and will re-execute pxp-agent every 10 seconds. Ref: PCP-305 
  #
  unless (@agent['platform'] =~ /solaris|osx/) then
    step 'Remove configuration' do
      stop_service
      on(@agent, "mv #{pxp_agent_config_file(@agent)} #{@pxp_temp_file}")
    end

    step 'C94686 - Service Start (from stopped, un-configured)' do
      start_service
      assert_stopped
    end

    step 'Restore configuration' do
      on(@agent, "mv #{@pxp_temp_file} #{pxp_agent_config_file(@agent)}")
    end
  end
end
