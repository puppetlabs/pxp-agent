require 'pxp-agent/task_helper.rb'

test_name 'run multi file ruby task' do

  step 'Ensure each agent host has pxp-agent running and associated' do
    agents.each do |agent|
      on agent, puppet('resource service pxp-agent ensure=stopped')
      create_remote_file(agent, pxp_agent_config_file(agent), pxp_config_hocon_using_puppet_certs(master, agent))
      on agent, puppet('resource service pxp-agent ensure=running')

      assert(is_associated?(master, "pcp://#{agent}/agent"),
             "Agent #{agent} with PCP identity pcp://#{agent}/agent should be associated with pcp-broker")
    end
  end

  step 'Create ruby task on agent hosts' do
    agents.each do |agent|
      task_body = <<-'TASK'
#!/opt/puppetlabs/puppet/bin/ruby
require 'json'
file1 = File.read(File.join(__dir__, '..', '..', 'file1.txt'))
file2 =  File.read(File.join(ENV['PT__installdir'], 'dir', 'file2.txt'))
file3 =  File.read(File.join(ENV['PT__installdir'], 'dir', 'sub_dir', 'file3.txt'))
results = { "file1" => file1, "file2" => file2, "file3" => file3 }
puts results.to_json
      TASK
      file1_body = "file1"
      file2_body = "file2"
      file3_body = "file3"
      @sha256_task = create_task_on(agent, 'multi_file', 'multi_file.rb', task_body)
      @sha256_file1 = create_task_on(agent, 'multi_file', 'file1.txt', file1_body)
      @sha256_file2 = create_task_on(agent, 'multi_file', 'file2.txt', file2_body)
      @sha256_file3 = create_task_on(agent, 'multi_file', 'file3.txt', file3_body)
    end
  end

  step 'Run ruby task on agent hosts' do

    files = [file_entry('multi_file.rb', @sha256_task), file_entry('file1.txt', @sha256_file1), file_entry('dir/file2.txt', @sha256_file2), file_entry('dir/sub_dir/file3.txt', @sha256_file3)]
    metadata = { implementations: [
      { name: 'multi_file.rb', requirements: ['puppet-agent'], files: ['file1.txt']}], files: ['dir/']}
    run_successful_task(master, agents, 'multi_file', files, metadata: metadata ) do |stdout|

      json = stdout.delete("\r")
      assert_equal({"file1" => "file1\n", "file2" => "file2\n", "file3" => "file3\n"}, JSON.parse(json), "additional files not provided via '_installdir'")
    end
  end # test step
end # test
