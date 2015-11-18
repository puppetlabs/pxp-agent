require 'pxp-agent/config_helper.rb'
require 'json'

step 'Copy test certs to agents'
agents.each do |agent|
  scp_to(agent, '../test-resources', 'test-resources')
end

step 'Create config file'
agents.each_with_index do |agent, i|
  create_remote_file(agent, pxp_agent_config_file(agent), pxp_config_json_using_test_certs(master, agent, i+1).to_s)
end
