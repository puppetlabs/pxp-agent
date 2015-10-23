require 'puppet/acceptance/install_utils'
extend Puppet::Acceptance::InstallUtils
require 'beaker/dsl/install_utils'
extend Beaker::DSL::InstallUtils

test_name "Install Packages"

sha = ENV['SUITE_COMMIT']

step "Install repositories on target machines..." do
  repo_configs_dir = 'repo_configs'
  logger.debug('about to install repo for puppet-agent from ' + sha.to_s + ' ' + repo_configs_dir.to_s)
  hosts.each do |host|
    install_repos_on(host, 'puppet-agent', sha, repo_configs_dir)
  end
end

PACKAGES = {
  :redhat => [
    'puppet'
  ],
  :debian => [
    'puppet'
  ],
}

# Install puppet on all hosts including master
install_packages_on(hosts, PACKAGES)

step 'Install build dependencies on master'

MASTER_PACKAGES = {
  :redhat => [
    'git',
    'java-1.8.0-openjdk-devel',
  ],
}
install_packages_on(master, MASTER_PACKAGES, :check_if_exists => true)

step 'Install MSIs on any Windows agents'
agents.each do |agent|
  if agent.platform.start_with?('windows')
    logger.info "Installing Puppet agent msi #{sha} on #{agent}"
    install_puppet_agent_dev_repo_on(agent, :version => sha)
  end
end
