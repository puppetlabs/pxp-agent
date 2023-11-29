require 'pxp-agent/test_helper'

step 'Install build dependencies for broker' do
  # Assumes RedHat master
  master.install_package('make')
  master.install_package('git')
  master.install_package('java-11-openjdk-devel')
end

NUM_BROKERS = 2
have_broker_replica = NUM_BROKERS > 1

PCP_BROKER_FORK = ENV['PCP_BROKER_FORK'] || nil
PCP_BROKER_REF  = ENV['PCP_BROKER_REF'] || 'refs/tags/1.4.0'

step 'Terminate existing pcp-brokers if running' do
  kill_all_pcp_brokers(master)
end

step 'Clone pcp-broker to broker_hosts' do
  pcp_broker_url = build_git_url('pcp-broker', PCP_BROKER_FORK, nil, 'https')
  if PCP_BROKER_REF
    pcp_broker_url += '#' + PCP_BROKER_REF
  end

  clone_git_repo_on(master, GIT_CLONE_FOLDER, extract_repo_info_from(pcp_broker_url))

  step 'Replace PCP test certs with the Puppet certs of this SUT' do
    on(master, puppet('config print ssldir')) do |result|
      puppet_ssldir = result.stdout.chomp
      broker_ssldir = "#{GIT_CLONE_FOLDER}/pcp-broker/test-resources/ssl"
      on master, "\\cp #{puppet_ssldir}/certs/#{master}.pem #{broker_ssldir}/certs/broker.example.com.pem"
      on master, "\\cp #{puppet_ssldir}/private_keys/#{master}.pem #{broker_ssldir}/private_keys/broker.example.com.pem"
      on master, "\\cp #{puppet_ssldir}/ca/ca_crt.pem #{broker_ssldir}/ca/ca_crt.pem"
      on master, "\\cp #{puppet_ssldir}/ca/ca_crl.pem #{broker_ssldir}/ca/ca_crl.pem"
    end
  end

  on(master, "\\ln -s #{GIT_CLONE_FOLDER}/pcp-broker #{GIT_CLONE_FOLDER}/pcp-broker0")
  if have_broker_replica
    step 'create replica pcp-broker dir' do
      for i in 1..NUM_BROKERS-1
        on(master, "\\cp -a #{GIT_CLONE_FOLDER}/pcp-broker #{GIT_CLONE_FOLDER}/pcp-broker#{i}")
      end
    end
  end
end

step 'Download lein bootstrap' do
  on master, 'cd /usr/bin && '\
    'curl -O https://raw.githubusercontent.com/technomancy/leiningen/2.7.1/bin/lein'
end

step 'Run lein once so it sets itself up' do
  on master, 'chmod a+x /usr/bin/lein && export LEIN_ROOT=ok && /usr/bin/lein'
end

step 'Run lein deps to download dependencies' do
  # 'lein tk' will download dependencies automatically, but downloading them will take
  # some time and will eat into the polling period we allow for the broker to start
  for i in 0..NUM_BROKERS-1
    lein_command = "cd #{GIT_CLONE_FOLDER}/pcp-broker#{i}; export LEIN_ROOT=ok; lein with-profile #{LEIN_PROFILE} deps"
    if master[:gke_container]
      jvm_opts = ['-Xms2g -Xmx2g']
      lein_command = "export JVM_OPTS='#{jvm_opts.join(' ')}'; export LEIN_JVM_OPTS='#{jvm_opts.join(' ')}'; #{lein_command}"
    end
    on(master, lein_command)
  end
end

step "Run pcp-broker on master in trapperkeeper in background and wait for it to report status 'running'" do
  for i in 0..NUM_BROKERS-1
    broker_instance = i
    configure_pcp_broker(master,broker_instance)
  end
  broker_instance = 0 # 0 indexed
  run_pcp_broker(master, broker_instance)
end
