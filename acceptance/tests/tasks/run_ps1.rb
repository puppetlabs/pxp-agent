require 'pxp-agent/task_helper.rb'

test_name 'run powershell task' do

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
      create_task_on(agent, 'echo', 'init.ps1', <<-EOF)
foreach ($i in $input) { Write-Output $i }
Write-Output $env:PT_data
EOF
    end
  end

  step 'Run powershell task on Windows agent hosts' do
    run_task(master, windows_hosts, 'echo', 'init.ps1', {:data => [1, 2, 3]}) do |stdout|
      json, data = stdout.delete("\r").split("\n")
      assert_equal('{"data":[1,2,3]}', json, "Output did not contain 'data'")
      assert_equal('[1,2,3]', data, "Output did not contain 'data'")
    end
  end # test step
end # test
