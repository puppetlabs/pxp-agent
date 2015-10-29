test_name ''

agent1 = agents[0]
if agent1.platform.start_with?('windows')
  conf_dir = '/cygdrive/c/ProgramData/PuppetLabs/pxp-agent/etc'
  home_dir = '/home/Administrator'
  log_file = '/cygdrive/c/ProgramData/PuppetLabs/pxp-agent/var/log/pxp-agent.log'
else
  conf_dir = '/etc/puppetlabs/pxp-agent'
  home_dir = '/root'
  log_file = '/var/log/puppetlabs/pxp-agent/pxp-agent.log'
end

# On teardown, restore configuration file
teardown do
  if agent1.file_exist?("#{conf_dir}/pxp-agent.conf.orig")
    on agent1, puppet('resource service pxp-agent ensure=stopped')
    on(agent1, "mv #{conf_dir}/pxp-agent.conf.orig #{conf_dir}/pxp-agent.conf")
  end
end

step 'Create folder on agent to store invalid certs'
on(agent1, 'mkdir -p ~/invalid_certs ~/invalid_certs/certs ~/invalid_certs/private_keys')

step 'On master, generate some new certs'
if master.file_exist?('/tmp/ssl')
 on(master, 'rm -rf /tmp/ssl/*')
end
on master, puppet('master --ssldir /tmp/ssl'), :acceptable_exit_codes => [0,74]
on master, puppet('cert --ssldir /tmp/ssl generate client01.example.com')

step 'Copy certs from master to agent'
# TODO (JS) BKR-608 - currently cannot scp directly between 2 hosts,
#                     so need to do it in two steps.
`mkdir -p tmp/C94729 tmp/C94729/certs tmp/C94729/private_keys`
scp_from(master, '/tmp/ssl/certs/client01.example.com.pem', 'tmp/C94729/certs/')
scp_from(master, '/tmp/ssl/private_keys/client01.example.com.pem', 'tmp/C94729/private_keys/')
# TODO (JS) BKR-610 - beaker's scp_to method is buggy with the ~ alias,
#                     so need to give full home dir path
scp_to(agent1, "tmp/C94729/certs/client01.example.com.pem", "#{home_dir}/invalid_certs/certs/")
scp_to(agent1, "tmp/C94729/private_keys/client01.example.com.pem", "#{home_dir}/invalid_certs/private_keys/")
on(agent1, "chmod 644 #{home_dir}/invalid_certs/private_keys/client01.example.com.pem")

step 'Stop pxp-agent service and back-up the valid config file'
on agent1, puppet('resource service pxp-agent ensure=stopped')
on(agent1, "cp #{conf_dir}/pxp-agent.conf " \
           "#{conf_dir}/pxp-agent.conf.orig")

step "Wipe pxp-agent log"
on(agent1, "rm -rf #{log_file}")

step "Change pxp-agent config to use a cert that doesn't match private key"
on(agent1, "sed -i 's/\\(.*\\\"ssl-cert\\\".*\\)test-resources.*ssl/\\1invalid_certs/g' #{conf_dir}/pxp-agent.conf")

step 'C94730 - Attempt to run pxp-agent with mismatching SSL cert and private key'
on agent1, puppet('resource service pxp-agent ensure=running')
retry_on(agent1, "grep 'failed to load private key' #{log_file}", {:max_retries => 30,
                                                                   :retry_interval => 1})
assert(on(agent1, "grep 'pxp-agent will start unconfigured' #{log_file}"),
       "pxp-agent should log that is will start unconfigured")
on agent1, puppet('resource service pxp-agent') do |result|
  assert_match(/running/, result.stdout, "pxp-agent service should be running (unconfigured)")
end

step "Stop service and wipe log"
on agent1, puppet('resource service pxp-agent ensure=stopped')
on(agent, "rm -rf #{log_file}")

step "Change pxp-agent config so the cert and key match but not the right ca"
on(agent1, "sed -i 's/\\(.*\\\"ssl-key\\\".*\\)test-resources.*ssl/\\1invalid_certs/g' #{conf_dir}/pxp-agent.conf")
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
