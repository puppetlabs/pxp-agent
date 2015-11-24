require 'pxp-agent/config_helper.rb'
require 'json'

step 'Copy test certs to agents' do
  agents.each do |agent|
    scp_to(agent, '../test-resources', 'test-resources')
  end
end

step 'Create pxp-agent config file on each agent' do
  agents.each_with_index do |agent, i|
    create_remote_file(agent, pxp_agent_config_file(agent), pxp_config_json_using_test_certs(master, agent, i+1).to_s)
  end
end
