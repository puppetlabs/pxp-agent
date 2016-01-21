require 'pxp-agent/test_helper.rb'

step 'Clone pcp-broker to master' do
  on master, puppet('resource package git ensure=present')
  on master, 'git clone https://github.com/puppetlabs/pcp-broker.git'
  on master, 'cd ~/pcp-broker ; git checkout 602c003'
end

step 'Install Java' do
  on master, puppet('resource package java ensure=present')
end

step 'Download lein bootstrap script' do
  on master, 'cd /usr/bin && '\
             'curl -O https://raw.githubusercontent.com/technomancy/leiningen/stable/bin/lein'
end

step 'Run lein once so it sets itself up' do
  on master, 'chmod a+x /usr/bin/lein && export LEIN_ROOT=ok && /usr/bin/lein'
end

step 'Run lein deps to download dependencies' do
  # 'lein tk' will download dependencies automatically, but downloading them will take
  # some time and will eat into the polling period we allow for the broker to start
  on master, 'cd ~/pcp-broker; export LEIN_ROOT=ok; lein deps'
end

step "Run pcp-broker in trapperkeeper in background and wait for port" do
  run_pcp_broker(master)
end
