require 'json'

test_name 'Run Puppet when agent already running'

puppet_module_executable = File.join(SourcePath, 'pxp-agent', 'modules', 'pxp-module-puppet')

agent1 = agents[0]
agent_pid = 0

teardown do
  if (agent_pid != 0)
    step 'Kill the hanging Puppet agent'
    on(agent1, "kill -9 #{agent_pid}")
  end
end

step 'Start and pause puppet agent'
on(agent1, "cat << EOF > /tmp/pause_puppet.sh\n" +
                        "#!/bin/sh\n\n" +
                        "puppet agent -t </dev/null >/dev/null 2>&1 &\n" +
                        "sleep 0.5\n" +
                        "pid=\\`pidof /opt/puppetlabs/puppet/bin/ruby\\`\n" +
                        "kill -STOP \\$pid\n" +
                        "echo \\$pid > /opt/puppetlabs/puppet/cache/state/agent_catalog_run.lock\n" +
                        "echo \\$pid\n" +
                        "kill -9 $$\nEOF")

on(agent1, 'chmod a+x /tmp/pause_puppet.sh')
on(agent1, 'nohup /tmp/pause_puppet.sh &') do |result|
  agent_pid = result.stdout.chomp
end
assert_match(/[0-9]+/, agent_pid, 'Did not get a numeric pid for an already running puppet agent!')

step "Call puppet run at CLI"

params_and_config = {"params" => {"env" => [],
                                  "flags" => ["--noop"]},
                     "config" => {"puppet_bin" => "/opt/puppetlabs/bin/puppet"}}
run_command = "/opt/puppetlabs/puppet/bin/ruby #{puppet_module_executable} run"

on(agent1, 'chmod a+x %s' % [puppet_module_executable])
on(agent1, "echo '#{params_and_config.to_json}' | #{run_command}", :acceptable_exit_codes => [1]) do |result|
  begin
    resultJson = JSON.parse(result.stdout.chomp)
  rescue JSON::ParserError => e
    fail("Failed to parse stdout response from puppet run command as JSON. Exception as string is: '#{e.to_s}'")
  end
  assert(resultJson['exitcode'] == -1, "exitcode in result JSON was not -1")
  assert(resultJson['error'] == 'Puppet agent is already performing a run', "error in result JSON was not the expected error 'Puppet agent is already performing a run'")
  assert("unknown" == resultJson['status'], 'run status was not "unknown" (should be unknown due to no report yaml generated)')
end
