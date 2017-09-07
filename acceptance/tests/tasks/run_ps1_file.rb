require 'pxp-agent/task_helper.rb'

test_name 'run powershell task with file input' do

  windows_hosts = hosts.select {|h| /windows/ =~ h[:platform]}
  if windows_hosts.empty?
    skip_test "No windows hosts to test powershell on"
  end

  step 'Ensure each agent host has pxp-agent running and associated' do
    windows_hosts.each do |agent|
      on agent, puppet('resource service pxp-agent ensure=stopped')
      create_remote_file(agent, pxp_agent_config_file(agent), pxp_config_json_using_puppet_certs(master, agent).to_s)
      on agent, puppet('resource service pxp-agent ensure=running')

      assert(is_associated?(master, "pcp://#{agent}/agent"),
             "Agent #{agent} with PCP identity pcp://#{agent}/agent should be associated with pcp-broker")
    end
  end

  step 'Create powershell task on agent hosts' do
    windows_hosts.each do |agent|
      task_body = "Write-Output $(Get-Content $args[0])"
      @sha256 = create_task_on(agent, 'echo', 'init.ps1', task_body)
    end
  end

  step 'Run powershell task on Windows agent hosts' do
    run_successful_task(master, windows_hosts, 'echo', 'init.ps1', @sha256, {:data => [1, 2, 3]}, 'file') do |stdout|
      assert_equal({'data' => [1, 2, 3]}, stdout, "Output did not contain 'data'")
    end
  end # test step
end # test
