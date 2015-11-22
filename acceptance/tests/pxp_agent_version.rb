test_name 'C93805 - pxp-agent - Versioning test'

agent1 = agents[0]

step 'cd into pxp-agent bin folder and check the version'
pxp_agent_folder = ''
if agent1.platform.start_with?('windows')
  if agent1[:ruby_arch] == 'x64'
    ruby_arch = /-64/
  else
    ruby_arch = /-32/
  end
  if agent1.platform =~ ruby_arch
    pxp_agent_folder = "/cygdrive/c/Program\\ Files/Puppet\\ Labs/Puppet/pxp-agent/bin"
  else
    pxp_agent_folder = "/cygdrive/c/Program\\ Files\\ \\(x86\\)/Puppet\\ Labs/Puppet/pxp-agent/bin"
  end
else
  pxp_agent_folder = "/opt/puppetlabs/puppet/bin"
end

on(agent1, "cd #{pxp_agent_folder} && ./pxp-agent --version") do |result|
  assert(/[0-9\.]*/ =~ result.stdout, "Version number should be numbers and periods but was \"#{result.stdout.to_s}\"")
end
