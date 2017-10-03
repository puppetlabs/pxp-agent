require 'pxp-agent/task_helper.rb'
require 'json'

test_name 'run powershell task' do

  windows_hosts = hosts.select {|h| /windows/ =~ h[:platform]}
  if windows_hosts.empty?
    skip_test "No windows hosts to test powershell on"
  end

  step 'Ensure each agent host has pxp-agent running and associated' do
    windows_hosts.each do |agent|
      on agent, puppet('resource service pxp-agent ensure=stopped')
      create_remote_file(agent, pxp_agent_config_file(agent), pxp_config_hocon_using_puppet_certs(master, agent))
      on agent, puppet('resource service pxp-agent ensure=running')

      assert(is_associated?(master, "pcp://#{agent}/agent"),
             "Agent #{agent} with PCP identity pcp://#{agent}/agent should be associated with pcp-broker")
    end
  end

  fixtures = File.absolute_path('files')

  step 'Create powershell task on agent hosts' do
    task_body = File.read(File.join(fixtures, 'complex-task.ps1'))
    windows_hosts.each do |agent|
      @sha256 = create_task_on(agent, 'echo', 'init.ps1', task_body)
    end
  end

  step 'Run powershell task on Windows agent hosts' do
    task_input = JSON.parse(File.read(File.join(fixtures, 'complex-args.json')))
    task_output = File.read(File.join(fixtures, 'complex-output'))
    run_successful_task(master, windows_hosts, 'echo', 'init.ps1', @sha256, task_input) do |stdout|
      # Handle some known variations
      stdout.gsub!(/System\.Guid/, 'guid')
      stdout.gsub!(/System\.TimeSpan/, 'timespan')

      assert_equal(task_output, stdout, "Output did not match")
    end
  end # test step

  step 'Try erroring powershell task on agent hosts' do
    task_body = 'throw "Error trying to do a task"'
    windows_hosts.each do |agent|
      @sha256 = create_task_on(agent, 'echo', 'init.ps1', task_body)
    end

    run_failed_task(master, windows_hosts, 'echo', 'init.ps1', @sha256, {}) do |stderr|
      assert_match(/Error trying to do a task/, stderr.delete("\n"), 'stderr did not contain error text')
    end
  end

  step 'Try non-zero exit powershell task on agent hosts' do
    # run_failed_task expects exit 1
    task_body = 'exit 1'
    windows_hosts.each do |agent|
      @sha256 = create_task_on(agent, 'echo', 'init.ps1', task_body)
    end

    run_failed_task(master, windows_hosts, 'echo', 'init.ps1', @sha256, {}) do |stderr|
      assert_equal(nil, stderr)
    end
  end
end # test
