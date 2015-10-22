test_name 'C93807 - Associate pxp-agent with a PCP broker'

agent1 = agents[0]

log_file = ''
agent1.platform.start_with?('windows') ?
  log_file = '/cygdrive/c/ProgramData/PuppetLabs/pxp-agent/var/log/pxp-agent.log' :
  log_file = '/var/log/puppetlabs/pxp-agent/pxp-agent.log'

step 'Stop pxp-agent if it is currently running'
on agent1, puppet('resource service pxp-agent ensure=stopped')

step 'Clear existing logs so we don\'t match an existing association entry'
on(agent1, "rm -rf #{log_file}")

step 'Start pxp-agent service'
on agent1, puppet('resource service pxp-agent ensure=running')

step 'Allow 10 seconds after service start-up for association to complete'
sleep(10)

success = /INFO.*Successfully established a WebSocket connection with the PCP broker/
on(agent1, "cat #{log_file}") do |result|
  log_contents = result.stdout
  assert_match(success, log_contents,
               "Did not match expected association message '#{success.to_s}' " \
               "in actual pxp-agent.log contents '#{log_contents}'")
end
