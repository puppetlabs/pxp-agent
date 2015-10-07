test_name 'C93805 - pxp-agent - Versioning test'

agent1 = agents[0]

step 'cd into pxp-agent bin folder and check the version'
pxp_agent_folder = ''
agent1.platform.start_with?('windows') ?
  pxp_agent_folder = "/cygdrive/c/Program\\ Files/Puppet\\ Labs/Puppet/pxp-agent/bin" :
  pxp_agent_folder = "/opt/puppetlabs/puppet/bin"
on(agent1, "cd #{pxp_agent_folder} && ./pxp-agent --version") do |result|
  assert(/[0-9\.]*/ =~ result.stdout, "Version number should be numbers and periods but was \"#{result.stdout.to_s}\"")
end
