require 'json'

test_name 'Check module works'

puppet_module_executable = File.join(SourcePath, 'pxp-module-puppet', 'modules', 'puppet')

step "Call puppet run at CLI"

params_and_config = {"params" => {"env" => [],
                                  "flags" => ["--noop"]},
                     "config" => {"puppet_bin" => "/opt/puppetlabs/bin/puppet"}}
run_command = "/opt/puppetlabs/puppet/bin/ruby #{puppet_module_executable} run"

on(master, 'chmod a+x %s' % [puppet_module_executable])
on(master, "echo '#{params_and_config.to_json}' | #{run_command}") do |result|
  begin
    resultJson = JSON.parse(result.stdout.chomp)
  rescue JSON::ParserError => e
    fail("Failed to parse stdout response from puppet run command as JSON. Exception as string is: '#{e.to_s}'")
  end
  assert(resultJson['exitcode'] == 0, "exitcode in result JSON was not 0")
  assert(resultJson['error'].empty?, "error in rsult JSON was not an empty string")
  assert(["unchanged", "success"].include?(resultJson['status'] ), 'run status was not "unchanged" or "success"')
  assert(resultJson['kind'] == 'apply', 'puppet action type was not "apply"')
end
