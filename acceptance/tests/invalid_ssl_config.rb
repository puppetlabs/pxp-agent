test_name 'Attempt to start pxp-agent with invalid SSL config'

agent1 = agents[0]
if agent1.platform.start_with?('windows')
  conf_dir = '/cygdrive/c/ProgramData/PuppetLabs/pxp-agent/etc'
  home_dir = '/home/Administrator'
  log_file = '/cygdrive/c/ProgramData/PuppetLabs/pxp-agent/var/log/pxp-agent.log'
  sed_path_seperator = "\\\\\\\\"
else
  conf_dir = '/etc/puppetlabs/pxp-agent'
  home_dir = '/root'
  log_file = '/var/log/puppetlabs/pxp-agent/pxp-agent.log'
  sed_path_seperator = "\\\/"
end

# On teardown, restore configuration file
teardown do
  if agent1.file_exist?("#{conf_dir}/pxp-agent.conf.orig")
    on agent1, puppet('resource service pxp-agent ensure=stopped')
    on(agent1, "mv -f #{conf_dir}/pxp-agent.conf.orig #{conf_dir}/pxp-agent.conf")
  end
end

step 'Setup - Stop pxp-agent service and back-up the existing valid config file'
on agent1, puppet('resource service pxp-agent ensure=stopped')
on(agent1, "cp #{conf_dir}/pxp-agent.conf " \
           "#{conf_dir}/pxp-agent.conf.orig")

step "Setup - Wipe pxp-agent log"
on(agent1, "rm -rf #{log_file}")

step "Setup - Change pxp-agent config to use a cert that doesn't match private key"
on(agent1, "sed -i 's/\\(.*\\\"ssl-cert\\\".*test-resources.*\\)ssl/\\1ssl#{sed_path_seperator}alternative/g' #{conf_dir}/pxp-agent.conf")
on(agent1, "sed -i 's/\\(.*\\\"ssl-cert\\\".*\\)client01.example.com.pem/\\1client01.alt.example.com.pem/g' #{conf_dir}/pxp-agent.conf")

step 'C94730 - Attempt to run pxp-agent with mismatching SSL cert and private key'
expected_private_key_error='failed to load private key'
on agent1, puppet('resource service pxp-agent ensure=running')
# If the expected error does not appear in log within 30 seconds, then do an
# explicit assertion so we see the log contents in the test failure output 
begin
  retry_on(agent1, "grep '#{expected_private_key_error}' #{log_file}", {:max_retries => 30,
                                                                        :retry_interval => 1})
rescue
  on(agent1, "cat #{log_file}") do |result|
    assert_match(expected_private_key_error, result.stdout,
                "Expected error '#{expected_private_key_error}' did not appear in pxp-agent.log")
  end
end
assert(on(agent1, "grep 'pxp-agent will start unconfigured' #{log_file}"),
       "pxp-agent should log that is will start unconfigured")
on agent1, puppet('resource service pxp-agent') do |result|
  assert_match(/running/, result.stdout, "pxp-agent service should be running (unconfigured)")
end

step "Stop service and wipe log"
on agent1, puppet('resource service pxp-agent ensure=stopped')
on(agent, "rm -rf #{log_file}")

step "Change pxp-agent config so the cert and key match but they are of a different ca than the broker"
on(agent1, "sed -i 's/\\(.*\\\"ssl-key\\\".*test-resources.*\\)ssl/\\1ssl#{sed_path_seperator}alternative/g' #{conf_dir}/pxp-agent.conf")
on(agent1, "sed -i 's/\\(.*\\\"ssl-key\\\".*\\)client01.example.com.pem/\\1client01.alt.example.com.pem/g' #{conf_dir}/pxp-agent.conf")
on(agent1, "cat #{conf_dir}/pxp-agent.conf")

step 'C94729 - Attempt to run pxp-agent with SSL keypair from a different ca'
on agent1, puppet('resource service pxp-agent ensure=running')
retry_on(agent1, "grep 'TLS handshake failed' #{log_file}", {:max_retries => 30,
                                                             :retry_interval => 1})
assert(on(agent1, "grep 'retrying in' #{log_file}"),
       "pxp-agent should log that it will retry connection")
on agent1, puppet('resource service pxp-agent') do |result|
  assert_match(/running/, result.stdout, "pxp-agent service should be running (failing handshake)")
end
on agent1, puppet('resource service pxp-agent ensure=stopped')
on agent1, puppet('resource service pxp-agent') do |result|
  assert_match(/stopped/, result.stdout,
               "pxp-agent service should stop cleanly when it is running in a loop retrying invalid certs")
end
