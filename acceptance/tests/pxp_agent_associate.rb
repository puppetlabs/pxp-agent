test_name 'C93807 - Associate pxp-agent with a PCP broker'

agent1 = agents[0]

def pxp_service_command(agent, service_state)
  agent.platform.start_with?('windows') ?
    on(agent, "net #{service_state} pxp-agent") :
    on(agent, "service pxp-agent #{service_state}")
end

log_file = ''
agent1.platform.start_with?('windows') ?
  log_file = '/cygdrive/c/ProgramData/PuppetLabs/pxp-agent/var/log/pxp-agent.log' :
  log_file = '/var/log/puppetlabs/pxp-agent/pxp-agent.log'

step 'Stop pxp-agent if it is currently running'
pxp_service_command(agent1, 'stop')

step 'Clear existing logs so we don\'t match an existing association entry'
on(agent1, "rm -rf #{log_file}")

step 'Start pxp-agent service'
pxp_service_command(agent1, 'start')

step 'Allow 10 seconds after service start-up for association to complete'
sleep(10)

success = "INFO.*Successfully established a WebSocket connection with the PCP broker"
assert(on(agent1, "grep -e '#{success}' #{log_file} | wc -l | { read total; test $total -eq 1; };"),
       "Expected connection message '#{success}' did not appear exactly once in logs")
