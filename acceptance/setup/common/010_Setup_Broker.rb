pcp_broker_port = 8142
pcp_broker_minutes_to_start = 2

step 'Clone pcp-broker to master'
clone_git_repo_on(master, '/opt/puppet-git-repos',
    extract_repo_info_from(build_git_url('pcp-broker', nil, nil, 'git')))

step 'Download lein bootstrap'
on master, 'cd /usr/bin && '\
           'curl -O https://raw.githubusercontent.com/technomancy/leiningen/stable/bin/lein'
step 'Run lein once so it sets itself up'
on master, 'chmod a+x /usr/bin/lein && export LEIN_ROOT=ok && /usr/bin/lein'

step 'Run lein deps to download dependencies'
# 'lein tk' will download dependencies automatically, but downloading them will take
# some time and will eat into the polling period we allow for the broker to start
on master, 'cd /opt/puppet-git-repos/pcp-broker; export LEIN_ROOT=ok; lein deps'

step "Run pcp-broker in trapperkeeper in background and wait for port #{pcp_broker_port.to_s}"
on master, 'cd /opt/puppet-git-repos/pcp-broker; export LEIN_ROOT=ok; lein tk </dev/null >/var/log/pcp-broker.log 2>&1 &'
assert(port_open_within?(master, pcp_broker_port, 60 * pcp_broker_minutes_to_start),
       "pcp-broker port #{pcp_broker_port.to_s} not open within " \
       "#{pcp_broker_minutes_to_start.to_s} minutes of starting the broker")
