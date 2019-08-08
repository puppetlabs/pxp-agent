require 'pxp-agent/bolt_pxp_module_helper.rb'
require 'pxp-agent/config_helper.rb'
require 'puppet/acceptance/environment_utils.rb'
require 'json'

PUPPETSERVER_CONFIG_FILE = '/etc/puppetlabs/puppetserver/conf.d/webserver.conf'

def test_file_destination(agent)
  windows?(agent) ? "C:/Windows/Temp/testing_file#{rand(10000000)}.txt" : "/opt/testing_file#{rand(1000)}.txt"
end

def test_dir_destination(agent)
  windows?(agent) ? "C:/Windows/Temp/test_dir_destination#{rand(10000000)}" : "/opt/test_dir_destination#{rand(1000)}.txt"
end

suts = agents.reject { |host| host['roles'].include?('master') }

def clean_files(agent, files)
  tasks_cache = get_tasks_cache(agent)
  assert_match(/ensure => 'absent'/, on(agent, puppet("resource file #{tasks_cache}/#{@sha256} ensure=absent force=true")).stdout)
  files.each do |file|
    assert_match(/ensure => 'absent'/, on(agent, puppet("resource file #{file} ensure=absent force=true")).stdout)
  end
end

def test_file_resource_exists(agent, file, type)
  assert_match(/ensure\s*=>\s*'#{type}',/, on(agent, puppet("resource file #{file}")).stdout)
end

def test_file_resource_does_not_exist(agent, file)
  assert_match(/ensure => 'absent'/, on(agent, puppet("resource file #{file}")).stdout)
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
    filename = 'testing_file.txt'
    filepath = "#{@static_content_path}/#{filename}"
    create_remote_file(master, filepath, file_body)
    on master, "chmod 1777 #{filepath}"
    @source_file = "/download-test-files/#{filename}"
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

  step 'execute successful download_file with files,symlinks,directories' do
    suts.each do |agent|
      test_dir = test_dir_destination(agent)
      test_symlink = test_file_destination(agent)
      test_files = [
        test_file_destination(agent),
        test_file_destination(agent),
        test_file_destination(agent)
      ]
      request = []
      test_files.each do |file|
        request << download_file_entry(@sha256, @source_file, '', file, 'file')
      end
      request << download_file_entry('', '', '', test_dir, 'directory')
      request << download_file_entry('', '', test_files[0], test_symlink, 'symlink')
      run_successful_download(master,
                              agent,
                              request)
      test_files.each do |file|
        test_file_resource_exists(agent, file, 'file')
      end
      test_file_resource_exists(agent, test_dir, 'directory')
      test_file_resource_exists(agent, test_symlink, 'link')
      teardown {
        clean_files(agent, test_files)
        assert_match(/ensure => 'absent'/, on(agent, puppet("resource file #{test_dir} ensure=absent force=true")).stdout)
        assert_match(/ensure => 'absent'/, on(agent, puppet("resource file #{test_symlink} ensure=absent force=true")).stdout)
      }
    end
  end

  step 'execute download_file for a file with a destination directory that doesnt exist yet' do
    suts.each do |agent|
      test_dir = test_dir_destination(agent)
      test_file = test_dir + "/testing_file#{rand(10000000)}.txt"
      run_successful_download(master,
                              agent,
                              [download_file_entry(@sha256, @source_file, '', test_file, 'file')])
      test_file_resource_exists(agent, test_file, 'file')
      teardown {
        clean_files(agent, [test_file])
        # Remove the test directory too.
        assert_match(/ensure => 'absent'/, on(agent, puppet("resource file #{test_dir} ensure=absent force=true")).stdout)
      }
    end
  end

  step 'correctly report failed download_file' do
    suts.each do |agent|
      test_file = test_file_destination(agent)
      run_errored_download(
        master,
        agent,
        [download_file_entry('NOTREALSHA256',
                            "/download-test-files/not-real-file",
                            '',
                            test_file,
                            'file')]
      ) do |stdout|
        assert_match('puppetlabs.pxp-agent/execution-error', JSON.parse(stdout)['_error']['kind'])
      end

      test_file_resource_does_not_exist(agent, test_file)
      teardown {
        clean_files(agent, [test_file])
      }
    end
  end

  step 'Correctly fail if download succeeds but SHA256 does not match after download' do
    suts.each do |agent|
      test_file = test_file_destination(agent)
      run_errored_download(
        master,
        agent,
        [download_file_entry('NOTREALSHA256', @source_file, '', test_file, 'file')]
      ) do |stdout|
        assert_match('puppetlabs.pxp-agent/execution-error', JSON.parse(stdout)['_error']['kind'])
      end
      test_file_resource_does_not_exist(agent, test_file)
      teardown {
        clean_files(agent, [test_file])
      }
    end
  end
end
