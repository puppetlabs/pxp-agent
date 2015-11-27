require 'pxp-agent/config_helper.rb'

test_name 'C93805 - pxp-agent - Versioning test'

agents.each do |agent|
  step 'cd into pxp-agent bin folder and check the version' do
    on(agent, "cd #{pxp_agent_dir(agent)} && ./pxp-agent --version") do |result|
      assert(/[0-9\.]*/ =~ result.stdout, "Version number should be numbers and periods but was \"#{result.stdout.to_s}\"")
    end
  end
end
