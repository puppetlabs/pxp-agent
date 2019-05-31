require 'pxp-agent/test_helper.rb'
require 'puppet/acceptance/environment_utils'

test_name 'umask inherited by puppet' do

  applicable_agents = agents.reject { |agent| windows?(agent) }
  unless applicable_agents.length > 0 then
    skip_test('Windows agents have no concept of umask')
  end

  extend Puppet::Acceptance::EnvironmentUtils

  env_name = File.basename(__FILE__, '.*')
  environment_name = mk_tmp_environment(env_name)
  test_file = '/tmp/umask_acceptance_test'

  step 'On master, create a new environment that creates a file' do
    site_manifest = "#{environmentpath}/#{environment_name}/manifests/site.pp"
    create_remote_file(master, site_manifest, <<-SITEPP)
node default {
  exec {'touch #{test_file}':
    path => ['/bin', '/usr/bin'],
  }
}
SITEPP
    on(master, "chmod 644 #{site_manifest}")
  end

  teardown do
    applicable_agents.each do |agent|
      on agent, puppet('resource service pxp-agent ensure=stopped')

      if agent.file_exist?('/tmp/pxp-agent.default')
        on agent, 'mv /tmp/pxp-agent.default /etc/default/pxp-agent'
      end
      if agent.file_exist?('/tmp/pxp-agent.sysconfig')
        on agent, 'mv /tmp/pxp-agent.sysconfig /etc/sysconfig/pxp-agent'
      end
      if agent.file_exist?('/etc/systemd/system/pxp-agent.service.d')
        on agent, 'rm -rf /etc/systemd/system/pxp-agent.service.d'
      end

      on agent, "rm -f #{test_file}"

      on agent, puppet('resource service pxp-agent ensure=running')

      assert(is_associated?(master, "pcp://#{agent}/agent"),
             "Agent #{agent} with PCP identity pcp://#{agent}/agent should be associated with pcp-broker")
    end
  end

  step 'Ensure each agent host has pxp-agent running and associated with umask' do
    applicable_agents.each do |agent|
      on agent, puppet('resource service pxp-agent ensure=stopped')

      if agent.file_exist?('/etc/default/pxp-agent')
        on agent, 'mv /etc/default/pxp-agent /tmp/pxp-agent.default'
        on agent, 'echo umask 222 > /etc/default/pxp-agent'
      end
      if agent.file_exist?('/etc/sysconfig/pxp-agent')
        on agent, 'mv /etc/sysconfig/pxp-agent /tmp/pxp-agent.sysconfig'
        on agent, 'echo umask 222 > /etc/sysconfig/pxp-agent'
      end

      on(agent, 'service pxp-agent status', :accept_all_exit_codes => true)
      if stdout =~ /systemd/ || stderr =~ /not found/
        on agent, 'mkdir /etc/systemd/system/pxp-agent.service.d'
        create_remote_file(agent, '/etc/systemd/system/pxp-agent.service.d/override.conf', "[Service]\nUMask=0222")
      end

      create_remote_file(agent, pxp_agent_config_file(agent), pxp_config_hocon_using_puppet_certs(master, agent))
      on agent, puppet('resource service pxp-agent ensure=running')

      assert(is_associated?(master, "pcp://#{agent}/agent"),
             "Agent #{agent} with PCP identity pcp://#{agent}/agent should be associated with pcp-broker")
    end
  end

  step "Send an rpc_blocking_request to all agents" do
    target_identities = []
    applicable_agents.each do |agent|
      target_identities << "pcp://#{agent}/agent"
    end
    responses = nil # Declare here so not local to begin/rescue below
    begin
      responses = rpc_blocking_request(master, target_identities,
                                      'pxp-module-puppet', 'run',
                                      {:env => [], :flags => ['--environment', environment_name]})
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
        assert_equal(identity,
                     response[:envelope][:sender],
                     "Received the expected rpc_blocking_response for #{agent} "\
                     "but not from the expected identity")
      end # Step for this agent

      step "Verify file permissions" do
        mode = get_mode(agent, test_file)
        expected_mode =
          case agent['platform']
          when /osx/
            # MacOS seems to ignore umask
            '644'
          when /solaris/
            # Solaris has no identifiable way to modify umask for smf services
            '644'
          when /aix/
            # TODO: set a umask for an AIX service
            # Doing so appears non-trivial, so for now asserts default.
            '644'
          else
            '444'
          end

        assert_equal(expected_mode, mode, 'umask should prevent making file writable')
      end
    end # iterating on each agent
  end # test step
end # test
