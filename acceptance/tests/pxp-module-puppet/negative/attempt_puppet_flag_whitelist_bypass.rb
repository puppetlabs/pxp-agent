require 'pxp-agent/test_helper.rb'

test_name 'C100245 - attempt bypass of flag whitelist with tab character' do

  confine :except, :platform => ['windows']

  step 'pick test host' do
    @test_host = agents.first 
    skip_test 'No compatible agents available to run test' unless @test_host
    skip_test 'Test not windows compatible' if @test_host.platform =~ /windows/
  end

  step 'set up executable file' do
    @executable_path = File.join(@test_host.system_temp_path, 'run_me')
    @test_file = [@test_host.system_temp_path, (0...20).map { ('a'..'z').to_a[rand(26)] }.join].join('/')

    executable_contents =<<-EOS
echo 'ALL YOUR BASE BELONG TO US NOW' >> #{@test_file}
    EOS

    create_remote_file(@test_host, @executable_path, executable_contents)
    on(@test_host, "chmod 777 #{@executable_path}")
  end

  teardown do
    on(@test_host, "rm -f #{@executable_path}", :accept_all_exit_codes => true)
    on(@test_host, "rm -f #{@test_file}", :accept_all_exit_codes => true)
  end

  step 'Ensure host has pxp-agent running and associated' do
    on @test_host, puppet('resource service pxp-agent ensure=stopped')
    create_remote_file(@test_host, pxp_agent_config_file(agent), pxp_config_json_using_puppet_certs(master, agent).to_s)
    on @test_host, puppet('resource service pxp-agent ensure=running')

    assert(is_associated?(master, "pcp://#{@test_host}/agent"),
           "Agent #{@test_host} with PCP identity pcp://#{@test_host}/agent should be associated with pcp-broker")
  end

  step "Send an rpc_blocking_request to all agents" do
    target_identities = ["pcp://#{@test_host}/agent"]
    begin
      rpc_request(master, target_identities,
                                       'pxp-module-puppet', 'run',
                                       {:env => [], :flags => ['--noop',
                                                               '--onetime',
                                                               '--no-daemonize',
                                                               "\t--postrun_command #{@executable_path}"]
                                       })
    rescue => exception
      raise("Exception occurred when trying to run Puppet on the test agent: #{exception.message}")
    end
  end

  step 'check if postrun command was run' do
    on(@test_host, "test -f #{@test_file}", :accept_all_exit_codes => true) do |result|
      assert_equal(1, result.exit_code, 'Flag whitelist failed to prevent remote code execution')
    end
  end
end # test
