require 'pxp-agent/test_helper.rb'

test_name 'C93807 - Associate pxp-agent with a PCP broker'

teardown do
  agents.each do |agent|
    on agent, puppet('resource service pxp-agent ensure=stopped')
    create_remote_file(agent, pxp_agent_config_file(agent), pxp_config_hocon_using_puppet_certs(master, agent))
    on agent, puppet('resource service pxp-agent ensure=running')
    unless windows?(agent)
      teardown_squid_proxy(agent)
    end
  end
end

agents.each do |agent|

  create_remote_file(agent, pxp_agent_config_file(agent), pxp_config_hocon_using_puppet_certs(master, agent))

  step 'Stop pxp-agent if it is currently running' do
    on agent, puppet('resource service pxp-agent ensure=stopped')
  end

  step 'Assert that the agent is not listed in pcp-broker inventory' do
    assert(is_not_associated?(master, "pcp://#{agent}/agent"),
           "Agent identity pcp://#{agent}/agent for agent host #{agent} appears in pcp-broker's client inventory " \
           "but pxp-agent service is supposed to be stopped")
  end

  step 'Start pxp-agent service' do
    on agent, puppet('resource service pxp-agent ensure=running')
  end

  step 'Assert that agent is listed in pcp-broker inventory' do
    assert(is_associated?(master, "pcp://#{agent}/agent"),
           "Agent identity pcp://#{agent}/agent for agent host #{agent} does not appear in pcp-broker's client inventory")
  end

  step 'Assert that agent can communicate with PCP-broker through proxy' do
    # only run squid for linux
    unless windows?(agent)
      squid_log = "/var/log/squid/access.log"
      setup_squid_proxy(agent)
      proxy = "http://#{agent}:3128"
      # restart agent to use proxy
      on(agent, puppet('resource service pxp-agent ensure=stopped'))
      assert(is_not_associated?(master, "pcp://#{agent}/agent"),
             "Agent identity pcp://#{agent}/agent for agent host #{agent} appears in pcp-broker's client inventory " \
             "but pxp-agent service is supposed to be stopped")
      
      create_remote_file(agent, pxp_agent_config_file(agent), pxp_config_hocon_using_puppet_certs(master, agent, broker_proxy: proxy))
      on(agent, puppet('resource service pxp-agent ensure=running'))

      # now stop agent so that the log is available in proxy access log (only shows up when connection is "done")
      on(agent, puppet('resource service pxp-agent ensure=stopped'))
      # prove the connection went through proxy 
      on(agent, "cat #{squid_log}") do |result|
        assert_match(/CONNECT #{master}/, result.stdout, 'Proxy logs did not indicate use of the proxy.' )
      end
      # restart the agent and make sure associated (prove that it connected with master)
      on(agent, puppet('resource service pxp-agent ensure=running'))
      assert(is_associated?(master, "pcp://#{agent}/agent"),
       "Agent identity pcp://#{agent}/agent for agent host #{agent} does not appear in pcp-broker's client inventory")
    end
  end
end
