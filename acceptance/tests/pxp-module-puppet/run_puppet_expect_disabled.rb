require 'json'

test_name 'Attempt to run Puppet when agent is disabled'

puppet_module_executable = File.join(SourcePath, 'pxp-agent', 'modules', 'pxp-module-puppet')

agent1 = agents[0]

teardown do
  step 'ensure Puppet agent enabled for subsequent tests'
  on(agent1, puppet('agent --enable'))
end

step "Disable Puppet agent"
on(agent1, puppet('agent --disable'))

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
  assert(resultJson['error'] == 'Puppet agent is disabled', "error in result JSON was not the expected error 'Puppet agent is disabled'")
  assert("unknown" == resultJson['status'], 'run status was not "unknown" (should be unknown due to no report yaml generated)')
end
