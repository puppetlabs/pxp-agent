require 'pxp-agent/task_helper.rb'
require 'pxp-agent/config_helper.rb'
require 'puppet/acceptance/environment_utils.rb'

test_name 'task download' do

  tag 'audit:high',      # module validation: no other venue exists to test
      'audit:acceptance'

  extend Puppet::Acceptance::EnvironmentUtils

  PUPPETSERVER_CONFIG_FILE = windows?(master) ?
    '/cygdrive/c/ProgramData/PuppetLabs/puppetserver/etc/conf.d/webserver.conf'
  : '/etc/puppetlabs/puppetserver/conf.d/webserver.conf'

  step 'Ensure each agent host has pxp-agent running and associated' do
    agents.each do |agent|
      on agent, puppet('resource service pxp-agent ensure=stopped')
      create_remote_file(agent, pxp_agent_config_file(agent), pxp_config_hocon_using_puppet_certs(master, agent))
      on agent, puppet('resource service pxp-agent ensure=running')

      assert(is_associated?(master, "pcp://#{agent}/agent"),
             "Agent #{agent} with PCP identity pcp://#{agent}/agent should be associated with pcp-broker")
    end
  end

  step 'Create the downloadable tasks on the master' do
    # Make the static content path
    env_name = File.basename(__FILE__, '.*')
    @static_content_path = File.join(environmentpath, mk_tmp_environment(env_name))

    # Create the tasks
    nix_task_body = "#!/bin/sh\n#Comment to change sha\necho $PT_message"
    win_task_body = "@REM Comment to change sha\n@echo %PT_message%"
    @nix_sha256, @win_sha256 = { "init" => nix_task_body, "init.bat" => win_task_body }.map do |filename, task_body|
      taskpath = "#{@static_content_path}/#{filename}"
      create_remote_file(master, taskpath, task_body)
      on master, "chmod 1777 #{taskpath}"
      Digest::SHA256.hexdigest(task_body + "\n")
    end
  end

  step 'Setup the static task file mount on puppetserver' do
    on master, puppet('resource service puppetserver ensure=stopped')
    hocon_file_edit_in_place_on(master, PUPPETSERVER_CONFIG_FILE) do |host, doc|
      doc.set_config_value("webserver.static-content", Hocon::ConfigValueFactory.from_any_ref([{
        "path" => "/task-files",
        "resource" => @static_content_path
      }]))
    end
    on master, puppet('resource service puppetserver ensure=running')
  end

  win_agents, nix_agents = agents.partition { |agent| windows?(agent) }

  step 'Download and run the task on agent hosts' do
    test_cases = { win_agents => ["init.bat", @win_sha256], nix_agents => ["init", @nix_sha256] }

    test_cases.each do |agents, (filename, sha256)|
      # Ensure that the task file does not exist beforehand so we know that
      # if it runs successfully in the later assertions, then it was downloaded.
      agents.each do |agent|
        tasks_cache = get_tasks_cache(agents.first)
        on agent, "rm -rf #{tasks_cache}/#{sha256}"
        assert_match(/ensure => 'absent'/, on(agent, puppet("resource file #{tasks_cache}/#{sha256}")).stdout)
      end

      files = [file_entry(filename, sha256, "/task-files/#{filename}")]
      run_successful_task(master, agents, 'echo', files, input: {:message => 'hello'}, ) do |stdout|
        assert_equal('hello', stdout.strip, "Output did not contain 'hello'")
      end
    end
  end

  step 'Ensure that the correct permissions were used when creating the tasks-cache/<sha> and tasks-cache/<sha>/<filename> directories' do
    test_cases = { win_agents => ["init.bat", @win_sha256, '755'], nix_agents => ["init", @nix_sha256, '750'] }

    test_cases.each do |agents, (filename, sha256, expected_mode)|
      agents.each do |agent|
        tasks_cache = get_tasks_cache(agent)
        [sha256, "#{sha256}/#{filename}"].each do |file|
          mode = get_mode(agent, "#{tasks_cache}/#{file}")
          assert_equal(expected_mode, mode, "Expected permissions to be #{expected_mode}")
        end
      end
    end
  end

  step 'Return a PXP-error when a SHA version conflict occurs between the downloaded task file and the provided SHA 256' do
    test_cases = { win_agents => ["init", @win_sha256, "init.bat"], nix_agents => ["init.bat", @nix_sha256, "init"] }

    test_cases.each do |agents, (filename, sha256, expected_file)|
      files = [file_entry(filename, sha256, "/task-files/#{filename}")]
      run_pxp_errored_task(master, agents, 'echo', files, input: {:message => 'hello'}) do |description|
        assert_match(/The downloaded \"#{filename}\"'s sha differs from the provided sha/, description, 'Expected SHA version conflict was not detected')
      end

      # Ensure things were properly cleaned up. Note that
      # there is a task-file in the tasks-cache/<sha>
      # because of the previous successful download test,
      # but this should be the only file.
      agents.each do |agent|
        tasks_cache = get_tasks_cache(agents.first)
        assert_equal(expected_file, on(agent, "ls #{tasks_cache}/#{sha256}").stdout.chomp, 'tasks-cache/<sha> directory was not properly cleaned up')
      end
    end
  end

  step 'Return a PXP-error when a 400+ response is returned from the server containing the status code and the response body' do
    # Ensure that the endpoint does result in a 404 status before performing
    # the corresponding test.
    assert_match(/Error 404 Not Found/, on(master, "curl -k https://#{master}:8140/task-files/non_existent_task").stdout.chomp)

    files = [file_entry('some_file', '1234', "/task-files/non_existent_task")]
    run_pxp_errored_task(master, agents, 'echo', files) do |description|
      assert_match(/Error:?\s+404/i, description, 'Expected 404 HTTP status was not detected')
    end

    # Ensure things were properly cleaned up
    agents.each do |agent|
      tasks_cache = get_tasks_cache(agent)
      assert_equal("", on(agent, "ls #{tasks_cache}/1234").stdout.chomp, 'tasks-cache/<sha> directory was not properly cleaned up')
    end
  end # test step
end # test
