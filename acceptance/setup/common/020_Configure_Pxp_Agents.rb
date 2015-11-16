require 'pxp-agent/config_helper.rb'

step 'Copy test certs to agents'
agents.each do |agent|
  scp_to(agent, '../test-resources', 'test-resources')
end

step 'Create config file'
agents.each_with_index do |agent, i|
  config = <<OUTPUT
{
  "broker-ws-uri" : "#{broker_ws_uri(master)}",
  "ssl-key" : "#{ssl_key_file(agent, i+1)}",
  "ssl-ca-cert" : "#{ssl_ca_file(agent)}",
  "ssl-cert" : "#{ssl_cert_file(agent, i+1)}"
}
OUTPUT
  create_remote_file(agent, pxp_agent_config_file(agent), config)
end
