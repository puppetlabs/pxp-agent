require 'puppet/acceptance/install_utils'
extend Puppet::Acceptance::InstallUtils
require 'pxp-agent/test_helper'

step 'Install build dependencies for broker' do
  BROKER_DEP_PACKAGES = {
    :redhat => [
      'git',
      'java-1.8.0-openjdk-devel',
    ],
  }
  install_packages_on(master, BROKER_DEP_PACKAGES, :check_if_exists => true)
end

NUM_BROKERS = 2
have_broker_replica = NUM_BROKERS > 1

step 'Clone pcp-broker to broker_hosts' do
  clone_git_repo_on(master, GIT_CLONE_FOLDER,
    extract_repo_info_from(build_git_url('pcp-broker', nil, nil, 'https')))

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
           'curl -O https://raw.githubusercontent.com/technomancy/leiningen/stable/bin/lein'
end

step 'Run lein once so it sets itself up' do
  on master, 'chmod a+x /usr/bin/lein && export LEIN_ROOT=ok && /usr/bin/lein'
end

step 'Run lein deps to download dependencies' do
  # 'lein tk' will download dependencies automatically, but downloading them will take
  # some time and will eat into the polling period we allow for the broker to start
  for i in 0..NUM_BROKERS-1
    on master, "cd #{GIT_CLONE_FOLDER}/pcp-broker#{i}; export LEIN_ROOT=ok; lein deps"
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
