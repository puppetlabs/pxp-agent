require 'pxp-agent/task_helper.rb'
require 'pxp-agent/config_helper.rb'
require 'puppet/acceptance/environment_utils.rb'

test_name 'task downloads correct implementation' do

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

  filename = 'init_rb.rb'
  step 'Create the downloadable tasks on the master' do
    # Make the static content path
    env_name = File.basename(__FILE__, '.*')
    @static_content_path = File.join(environmentpath, mk_tmp_environment(env_name))

    # Create the task
    task_body = "#!/opt/puppetlabs/puppet/bin/ruby\nputs ENV['PT_message']"
    taskpath = "#{@static_content_path}/#{filename}"
    create_remote_file(master, taskpath, task_body)
    on master, "chmod 1777 #{taskpath}"
    @sha256 = Digest::SHA256.hexdigest(task_body + "\n")
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

  step 'Download and run the task on agent hosts' do
    agents.each do |agent|
      tasks_cache = get_tasks_cache(agent)
      on agent, "rm -rf #{tasks_cache}/#{@sha256}"
      assert_match(/ensure => 'absent'/, on(agent, puppet("resource file #{tasks_cache}/#{@sha256}")).stdout)
    end

    files = [file_entry(filename, @sha256, "/task-files/#{filename}"), file_entry('invalid', 'wrong')]
    metadata = { implementations: [
      { name: filename, requirements: ['puppet-agent'] },
      { name: 'invalid' }
    ]}
    run_successful_task(master, agents, 'echo', files, input: {:message => 'hello'}, metadata: metadata) do |stdout|
      assert_equal('hello', stdout.strip, "Output did not contain 'hello'")
    end
  end
end # test
