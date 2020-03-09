require 'pxp-agent/test_helper.rb'
require 'pxp-agent/bolt_pxp_module_helper.rb'
require 'puppet/acceptance/environment_utils.rb'
require 'digest/sha2'

test_name 'When certs have been revoked in CRL' do
  extend Puppet::Acceptance::EnvironmentUtils

  # init
  win_agents, nix_agents = agents.partition { |agent| windows?(agent) }
  PUPPETSERVER_CONFIG_FILE = '/etc/puppetlabs/puppetserver/conf.d/webserver.conf'

  empty_crl = '/etc/puppetlabs/puppet/ssl/crl.pem'
  revoked_crl = '/tmp/ssl-revoked/crl.pem'

  # Ensure empty CRL is restored on every agent
  teardown do
    agents.each do |agent|
      on(agent, puppet('resource service pxp-agent ensure=stopped'))
      pxp_config = pxp_config_hash_using_puppet_certs(master, agent)
      crl_path = pxp_config['ssl-crl']
      scp_from(master, empty_crl, 'tmp')
      scp_to(agent, 'tmp/crl.pem', crl_path)
      create_remote_file(agent, pxp_agent_config_file(agent), to_hocon(pxp_config))
      on(agent, puppet('resource service pxp-agent ensure=running'))
    end
  end

  step 'Create a CRL with the master/broker cert revoked' do
    on(master, 'yes | cp -r /etc/puppetlabs/puppet/ssl /tmp/ssl-bkp')
    original_crl = on(master, 'cat /etc/puppetlabs/puppet/ssl/crl.pem').stdout
    original_crl_hash = Digest::SHA256.hexdigest(original_crl)
    on(master, "/opt/puppetlabs/bin/puppetserver ca revoke --certname #{master.hostname}")
    on(master, 'openssl crl -in /etc/puppetlabs/puppet/ssl/crl.pem -noout -text')
    # It can take several seconds for the crl file to be updated once the cert is revoked
    wait_for_crl = true
    while wait_for_crl
      updated_crl = on(master, 'cat /etc/puppetlabs/puppet/ssl/crl.pem').stdout
      updated_crl_hash = Digest::SHA256.hexdigest(updated_crl)
      wait_for_crl = updated_crl_hash == original_crl_hash
    end
    on(master, 'yes | cp -r /etc/puppetlabs/puppet/ssl /tmp/ssl-revoked')
    on(master, "yes | cp /tmp/ssl-bkp/ca/ca_crl.pem /etc/puppetlabs/puppet/ssl/ca/ca_crl.pem")
    on(master, "yes | cp /tmp/ssl-bkp/ca/infra_crl.pem /etc/puppetlabs/puppet/ssl/ca/infra_crl.pem")
    on(master, "yes | cp /tmp/ssl-bkp/crl.pem /etc/puppetlabs/puppet/ssl/crl.pem")
  end


  step 'Assert that agent will not connect to PCP-broker with revoked cert' do
    agents.each do |agent|
      # Stop pxp-service to break (possibly) existing websocket connection
      on(agent, puppet('resource service pxp-agent ensure=stopped'))
      assert(is_not_associated?(master, "pcp://#{agent}/agent"),
             "Agent identity pcp://#{agent}/agent for agent host #{agent} appears in pcp-broker's client inventory " \
             "but pxp-agent service is supposed to be stopped")

      # Configure agent to use CRL that contains a revocation for broker/master cert
      pxp_config = pxp_config_hash_using_puppet_certs(master, agent)
      crl_path = pxp_config['ssl-crl']
      scp_from(master, revoked_crl, 'tmp')
      scp_to(agent, 'tmp/crl.pem', crl_path)
      create_remote_file(agent, pxp_agent_config_file(agent), to_hocon(pxp_config))
      # Start the service and assert that the agent does not connect to broker with revoked cert
      on(agent, puppet('resource service pxp-agent ensure=running'))
      assert(is_not_associated?(master, "pcp://#{agent}/agent"),
       "Agent identity pcp://#{agent}/agent for agent host #{agent} appears in pcp-broker's client inventory " \
       "but pxp-agent service is supposed to be stopped")
      # Now revert to using empty CRL and assert the agent re-connects
      on(agent, puppet('resource service pxp-agent ensure=stopped'))
      scp_from(master, empty_crl, 'tmp')
      scp_to(agent, 'tmp/crl.pem', crl_path)
      create_remote_file(agent, pxp_agent_config_file(agent), pxp_config_hocon_using_puppet_certs(master, agent))
      on(agent, puppet('resource service pxp-agent ensure=running'))
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
    on(master, puppet('resource service puppetserver ensure=stopped'))
    hocon_file_edit_in_place_on(master, PUPPETSERVER_CONFIG_FILE) do |host, doc|
      doc.set_config_value("webserver.static-content", Hocon::ConfigValueFactory.from_any_ref([{
        "path" => "/task-files",
        "resource" => @static_content_path
      }]))
    end
    on(master, puppet('resource service puppetserver ensure=running'))
  end

  step 'Connect each agent to a PCP broker with an empty CRL, then once websocket is established swap in revoked' do
    agents.each do |agent|
      # Connect to broker in empty CRL
      on(agent, puppet('resource service pxp-agent ensure=stopped'))
      pxp_config = pxp_config_hash_using_puppet_certs(master, agent)
      create_remote_file(agent, pxp_agent_config_file(agent), to_hocon(pxp_config))
      on(agent, puppet('resource service pxp-agent ensure=running'))
      assert(is_associated?(master, "pcp://#{agent}/agent"),
             "Agent #{agent} with PCP identity pcp://#{agent}/agent should be associated with pcp-broker")
      # Now that we are connected to PCP broker, add revoked CRL to test CURL client CRL verification
      crl_path = pxp_config['ssl-crl']
      scp_from(master, revoked_crl, 'tmp')
      scp_to(agent, 'tmp/crl.pem', crl_path)
      create_remote_file(agent, pxp_agent_config_file(agent), to_hocon(pxp_config))
    end
  end

  step 'Task download honors CRL' do
    test_cases = {}
    if win_agents.length > 0
      test_cases.merge!(win_agents => ["init.bat", @win_sha256])
    end
    if nix_agents.length > 0
      test_cases.merge!(nix_agents => ["init", @nix_sha256])
    end

    test_cases.each do |agents, (filename, sha256)|
      # Ensure that the task file does not exist beforehand so we know that
      # if it runs successfully in the later assertions, then it was downloaded.
      agents.each do |agent|
        tasks_cache = get_tasks_cache(agents.first)
        on(agent, "rm -rf #{tasks_cache}/#{sha256}")
        assert_match(/ensure\s+=> 'absent'/, on(agent, puppet("resource file #{tasks_cache}/#{sha256}")).stdout)
      end

      files = [task_file_entry(filename, sha256, "/task-files/#{filename}")]
      run_errored_task(master, agents, 'echo', files, input: {:message => 'hello'}) do |description|
        assert_match(/certificate revoked/, description)
      end

      # Now revert to using empty CRL and assert the agent re-connects
      agents.each do |agent|
        on(agent, puppet('resource service pxp-agent ensure=stopped'))
        pxp_config = pxp_config_hash_using_puppet_certs(master, agent)
        crl_path = pxp_config['ssl-crl']
        create_remote_file(agent, pxp_agent_config_file(agent), to_hocon(pxp_config))
        scp_from(master, empty_crl, 'tmp')
        scp_to(agent, 'tmp/crl.pem', crl_path)
        on(agent, puppet('resource service pxp-agent ensure=running'))
        assert(is_associated?(master, "pcp://#{agent}/agent"),
               "Agent #{agent} with PCP identity pcp://#{agent}/agent should be associated with pcp-broker")
      end
    end
  end

  step 'Do not set ssl-crl setting and esure master/broker connection still works' do
    agents.each do |agent|
      # Connect to broker with pxp-agent.conf missing 'ssl-crl' setting
      on(agent, puppet('resource service pxp-agent ensure=stopped'))
      pxp_config = pxp_config_hash_using_puppet_certs(master, agent)
      pxp_config.delete('ssl-crl')
      create_remote_file(agent, pxp_agent_config_file(agent), to_hocon(pxp_config))
      on(agent, puppet('resource service pxp-agent ensure=running'))
      assert(is_associated?(master, "pcp://#{agent}/agent"),
             "Agent #{agent} with PCP identity pcp://#{agent}/agent should be associated with pcp-broker")
    end
  end

  step 'Download and run the task on agent hosts' do
    test_cases = {}
    if win_agents.length > 0
      test_cases.merge!(win_agents => ["init.bat", @win_sha256])
    end
    if nix_agents.length > 0
      test_cases.merge!(nix_agents => ["init", @nix_sha256])
    end

    test_cases.each do |agents, (filename, sha256)|
      # Ensure that the task file does not exist beforehand so we know that
      # if it runs successfully in the later assertions, then it was downloaded.
      agents.each do |agent|
        tasks_cache = get_tasks_cache(agents.first)
        on agent, "rm -rf #{tasks_cache}/#{sha256}"
        assert_match(/ensure\s+=> 'absent'/, on(agent, puppet("resource file #{tasks_cache}/#{sha256}")).stdout)
      end

      files = [task_file_entry(filename, sha256, "/task-files/#{filename}")]
      run_successful_task(master, agents, 'echo', files, input: {:message => 'hello'}) do |stdout|
        assert_equal('hello', stdout.strip, "Output did not contain 'hello'")
      end
    end
  end
end
