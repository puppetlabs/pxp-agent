require 'pxp-agent/test_helper.rb'

test_name 'C94777 - Ensure pxp-agent functions after agent host restart' do

  applicable_agents = agents.select { |agent| agent['platform'] !~ /aix/ && !agent['roles'].include?('master') }
  unless applicable_agents.length > 0 then
    skip_test('All agent hosts are AIX and QENG-3629 prevents AIX hosts being restarted')
  end

  applicable_agents = applicable_agents.select { |agent| agent['platform'] !~ /cisco_nexus/}
  unless applicable_agents.length > 0 then
    skip_test('PCP-508 - CiscoNX hosts cannot be restarted')
  end

  applicable_agents = applicable_agents.select { |agent| agent['platform'] != "eos-4-i386"}
  unless applicable_agents.length > 0 then
    skip_test('RE-7673 - Arista hosts cannot be restarted')
  end  

  step 'Ensure each agent host has pxp-agent service running and enabled' do
    applicable_agents.each do |agent|
      on agent, puppet('resource service pxp-agent ensure=stopped')
      create_remote_file(agent, pxp_agent_config_file(agent), pxp_config_json_using_puppet_certs(master, agent).to_s)
      on agent, puppet('resource service pxp-agent ensure=running enable=true')
      show_pcp_logs_on_failure do
        assert(is_associated?(master, "pcp://#{agent}/agent"),
               "Agent #{agent} with PCP identity pcp://#{agent}/agent should be associated with pcp-broker")
      end
    end
  end

  step "restart each agent" do
    applicable_agents.each do |agent|
      agent.reboot
      # BKR-812
      timeout = 30
      begin
        Timeout.timeout(timeout) do
          until agent.up?
            sleep 1
          end
        end
      rescue Timeout::Error => e
        raise "Agent did not come back up within #{timeout} seconds."
      end

      step "wait until pxp-agent is back up and associated on #{agent}" do
        opts = {
          :max_retries => 30,
          :retry_interval => 1,
        }
        retry_on(agent, puppet('resource service pxp-agent | grep running'), opts)
        assert(is_associated?(master, "pcp://#{agent}/agent"),
               "Agent #{agent} should be associated with pcp-broker following host reboot")
      end
    end
  end

  step "Send an rpc_blocking_request to all agents after reboot" do
    target_identities = []
    applicable_agents.each do |agent|
      target_identities << "pcp://#{agent}/agent"
    end
    responses = nil # Declare here so scoped above the begin/rescue below
    begin
      responses = rpc_blocking_request(master, target_identities,
                                      'pxp-module-puppet', 'run',
                                      {:env => [], :flags => ['--noop',
                                                              '--onetime',
                                                              '--no-daemonize']
                                      })
    rescue => exception
      fail("Exception occurred when trying to run Puppet on all agents: #{exception.message}")
    end
    applicable_agents.each do |agent|
      step "Check Run Puppet response for #{agent}" do
        identity = "pcp://#{agent}/agent"
        response = responses[identity]
        assert(response.has_key?(:envelope), "Response from PCP for #{agent} is missing :envelope")
        assert(response[:envelope].has_key?(:message_type), "Response from PCP for #{agent} is missing "\
                                                            ":message_type in :envelope")
        assert_equal('http://puppetlabs.com/rpc_blocking_response',
                     response[:envelope][:message_type],
                     "Received a message from pcp-broker for #{agent} but it wasn't of "\
                     "type http://puppetlabs.com/rpc_blocking_response")
      end # Step for this agent
    end # iterating on each agent
  end # test step
end # test
