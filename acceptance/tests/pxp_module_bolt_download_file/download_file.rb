require 'pxp-agent/bolt_pxp_module_helper.rb'
require 'pxp-agent/config_helper.rb'
require 'puppet/acceptance/environment_utils.rb'

PUPPETSERVER_CONFIG_FILE = '/etc/puppetlabs/puppetserver/conf.d/webserver.conf'

def test_file_destination(agent)
  windows?(agent) ? 'C:/testing_file.txt' : '/opt/testing_file.txt'
end

suts = agents.reject { |host| host['roles'].include?('master') }

def clean_files(agents)
  agents.each do |agent|
    tasks_cache = get_tasks_cache(agent)
    assert_match(/ensure => 'absent'/, on(agent, puppet("resource file #{tasks_cache}/#{@sha256} ensure=absent force=true")).stdout)
    assert_match(/ensure => 'absent'/, on(agent, puppet("resource file #{test_file_destination(agent)} ensure=absent force=true")).stdout)
  end
end

def test_file_exists(agent)
  assert_match(/ensure\s*=>\s*'file',/, on(agent, puppet("resource file #{test_file_destination(agent)}")).stdout)
end

def test_file_does_not_exist(agent)
  assert_match(/ensure => 'absent'/, on(agent, puppet("resource file #{test_file_destination(agent)}")).stdout)
end

test_name 'download file tests' do
  extend Puppet::Acceptance::EnvironmentUtils
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
    # Create the file
    file_body = '## TESTING BODY ##'
    @filename = 'testing_file.txt'
    filepath = "#{@static_content_path}/#{@filename}"
    create_remote_file(master, filepath, file_body)
    on master, "chmod 1777 #{filepath}"
    @sha256 = Digest::SHA256.hexdigest(file_body + "\n")
  end

  step 'Setup the static task file mount on puppetserver' do
    on master, puppet('resource service puppetserver ensure=stopped')
    hocon_file_edit_in_place_on(master, PUPPETSERVER_CONFIG_FILE) do |host, doc|
      doc.set_config_value("webserver.static-content", Hocon::ConfigValueFactory.from_any_ref([{
        "path" => "/download-test-files",
        "resource" => @static_content_path
      }]))
    end
    on master, puppet('resource service puppetserver ensure=running')
  end

  step 'execute successful download_file' do
    clean_files(suts)
    suts.each do |agent|
      run_successful_download(master,
                              agent,
                              [download_file_entry(@sha256, "/download-test-files/#{@filename}", test_file_destination(agent))])
      test_file_exists(agent)
    end
  end

  step 'correctly report failed download_file' do
    clean_files(suts)
    suts.each do |agent|
      run_failed_download(master,
                          agent,
                          [download_file_entry('NOTREALSHA256', "/download-test-files/not-real-file", test_file_destination(agent))])
      test_file_does_not_exist(agent)
    end
  end

  step 'Correctly fail if download succeeds but SHA256 does not match after download' do
    clean_files(suts)
    suts.each do |agent|
      run_failed_download(master,
                          agent,
                          [download_file_entry('NOTREALSHA256', "/download-test-files/#{@filename}", test_file_destination(agent))])
      test_file_does_not_exist(agent)
    end
  end
end
