require 'pxp-agent/bolt_pxp_module_helper.rb'
require 'pxp-agent/config_helper.rb'
require 'json'


def test_file_destination(agent)
  windows?(agent) ? "C:/Windows/Temp/testing_file#{rand(10000000)}.txt" : "/opt/testing_file#{rand(1000)}.txt"
end

def test_dir_destination(agent)
  windows?(agent) ? "C:/Windows/Temp/test_dir_destination#{rand(10000000)}" : "/opt/test_dir_destination#{rand(1000)}.txt"
end

suts = agents.reject { |host| host['roles'].include?('master') }

def clean_files(agent, files)
  tasks_cache = get_tasks_cache(agent)
  assert_match(/ensure\s+=> 'absent'/, on(agent, puppet("resource file #{tasks_cache}/#{@sha256} ensure=absent force=true")).stdout)
  files.each do |file|
    assert_match(/ensure\s+=> 'absent'/, on(agent, puppet("resource file #{file} ensure=absent force=true")).stdout)
  end
end

def test_file_resource_exists(agent, file, type)
  assert_match(/ensure\s+=> '#{type}',/, on(agent, puppet("resource file #{file}")).stdout)
end

def test_file_resource_does_not_exist(agent, file)
  assert_match(/ensure\s+=> 'absent'/, on(agent, puppet("resource file #{file}")).stdout)
end

test_name "Bolt module - download file tests" do

  tag 'audit:high',      # module validation: no other venue exists to test
      'audit:acceptance'

  test_env = 'bolt'

  step 'Ensure each agent host has pxp-agent running and associated' do
    agents.each do |agent|
      on agent, puppet('resource service pxp-agent ensure=stopped')
      create_remote_file(agent, pxp_agent_config_file(agent), pxp_config_hocon_using_puppet_certs(master, agent))
      on agent, puppet('resource service pxp-agent ensure=running')

      assert(is_associated?(master, "pcp://#{agent}/agent"),
             "Agent #{agent} with PCP identity pcp://#{agent}/agent should be associated with pcp-broker")
    end
  end

  step 'execute successful download file with files,symlinks,directories' do
    fixtures = File.absolute_path('files')
    fixture_env = File.join(fixtures, 'environments', test_env)
    filename = 'testing_file.txt'
    @sha256 = Digest::SHA256.file(File.join(fixture_env, filename)).hexdigest
    @source_file = "/#{test_env}/#{filename}"

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
        assert_match(/ensure\s+=> 'absent'/, on(agent, puppet("resource file #{test_dir} ensure=absent force=true")).stdout)
        assert_match(/ensure\s+=> 'absent'/, on(agent, puppet("resource file #{test_symlink} ensure=absent force=true")).stdout)
      }
    end
  end

  step 'execute download file for a file with a destination directory that doesnt exist yet' do
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
        assert_match(/ensure\s+=> 'absent'/, on(agent, puppet("resource file #{test_dir} ensure=absent force=true")).stdout)
      }
    end
  end

  step 'correctly report failed download file' do
    suts.each do |agent|
      test_file = test_file_destination(agent)
      run_errored_download(
        master,
        agent,
        [download_file_entry('NOTREALSHA256',
                            "/#{test_env}/not-real-file",
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
