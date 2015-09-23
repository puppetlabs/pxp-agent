test_name 'Setup environment'
# this file and most of the setup scripts copied from the puppet repo

step 'Ensure Git and Ruby and Java'

require 'puppet/acceptance/install_utils'
extend Puppet::Acceptance::InstallUtils
require 'beaker/dsl/install_utils'
extend Beaker::DSL::InstallUtils

MASTER_PACKAGES = {
  :redhat => [
    'git',
  ],
}

step 'Install build dependencies on master'
install_packages_on(master, MASTER_PACKAGES, :check_if_exists => true)

AGENT_PACKAGES = {
  :redhat => [
    'git',
  ],
  :debian8 => [
    'inetutils-syslogd',
    'git',
  ],
  :windows => [
    'git',
    'ruby',
  ]
}

step 'Install dependencies on agents'
install_packages_on(agents, AGENT_PACKAGES, :check_if_exists => true)

step 'Allow git clones via ssh to "unknown" host at github.com; required to clone private repos.'
on hosts, "echo #{GitHubSig} >> $HOME/.ssh/known_hosts"
# TODO: might not need the below with the above?
on master, 'echo -e "Host github.com\n\tStrictHostKeyChecking no\n" >> ~/.ssh/config'
