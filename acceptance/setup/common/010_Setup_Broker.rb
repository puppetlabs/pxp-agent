step 'Clone pcp-broker to master'
on master, puppet('resource package git ensure=present')
on master, 'git clone https://github.com/puppetlabs/pcp-broker.git'

step 'Install Java'
on master, puppet('resource package java ensure=present')
step 'Download lein bootstrap'
on master, 'cd /usr/bin && '\
           'curl -O https://raw.githubusercontent.com/technomancy/leiningen/stable/bin/lein'
step 'Run lein once so it sets itself up'
on master, 'chmod a+x /usr/bin/lein && export LEIN_ROOT=ok && /usr/bin/lein'

step 'Run pcp-broker in trapperkeeper in background (return immediately)'
on master, 'cd ~/pcp-broker; export LEIN_ROOT=ok; lein tk </dev/null >/dev/null 2>&1 &'
