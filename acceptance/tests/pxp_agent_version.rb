require 'pxp-agent/config_helper.rb'

test_name 'C93805 - pxp-agent - Versioning test'

agents.each do |agent|
  step 'cd into pxp-agent bin folder and check the version' do
    version_command = "#{pxp_agent_dir(agent)}/pxp-agent --version"
    # Vendored Ruby needs to be on path for Windows/cygwin.
    # Only do this for cygwin because other OSes (Cisco Nexus) do not handle setting env values inline.
    if windows?(agent) then
      puppet_bindir = '/cygdrive/c/Program Files (x86)/Puppet Labs/Puppet/puppet/bin:'+
        '/cygdrive/c/Program Files/Puppet Labs/Puppet/puppet/bin'
      version_command = "PATH=\"#{agent['privatebindir']}:#{puppet_bindir}:$PATH\"" + " " + version_command
    end
    on(agent, version_command) do |result|
      assert(/[0-9\.]*/ =~ result.stdout, "Version number should be numbers and periods but was \"#{result.stdout.to_s}\"")
    end
  end
end
