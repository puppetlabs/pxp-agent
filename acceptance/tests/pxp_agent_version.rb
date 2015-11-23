require 'pxp-agent/config_helper.rb'

test_name 'C93805 - pxp-agent - Versioning test'

agent1 = agents[0]

step 'cd into pxp-agent bin folder and check the version'
on(agent1, "cd #{pxp_agent_dir(agent1)} && ./pxp-agent --version") do |result|
  assert(/[0-9\.]*/ =~ result.stdout, "Version number should be numbers and periods but was \"#{result.stdout.to_s}\"")
end
