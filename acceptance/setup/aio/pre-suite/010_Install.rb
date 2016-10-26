require 'puppet/acceptance/common_utils'
require 'puppet/acceptance/install_utils'
extend Puppet::Acceptance::InstallUtils
require 'beaker/dsl/install_utils'
extend Beaker::DSL::InstallUtils

test_name "Install Packages"

step "Install puppet-agent..." do
  opts = {
    :puppet_collection    => 'PC1',
    :puppet_agent_sha     => ENV['SHA'],
    :puppet_agent_version => ENV['SUITE_VERSION'] || ENV['SHA']
  }
  agents.each do |agent|
    next if agent == master # Avoid SERVER-528
    install_puppet_agent_dev_repo_on(agent, opts)
  end
end

MASTER_PACKAGES = {
  :redhat => [
    'puppetserver',
  ],
  :debian => [
    'puppetserver',
  ],
}

step "Install puppetserver..." do
  if ENV['SERVER_VERSION']
    install_puppetlabs_dev_repo(master, 'puppetserver', ENV['SERVER_VERSION'])
    install_puppetlabs_dev_repo(master, 'puppet-agent', ENV['SHA'])
    master.install_package('puppetserver')
  else
    # beaker can't install puppetserver from nightlies (BKR-673)
    repo_configs_dir = 'repo-configs'
    install_repos_on(master, 'puppetserver', 'nightly', repo_configs_dir)
    install_repos_on(master, 'puppet-agent', ENV['SHA'], repo_configs_dir)
    install_packages_on(master, MASTER_PACKAGES)
  end
end

step 'Make sure install is sane' do 
  # beaker has already added puppet and ruby to PATH in ~/.ssh/environment
  agents.each do |agent|
    on agent, puppet('--version')
    ruby = Puppet::Acceptance::CommandUtils.ruby_command(agent)
    on agent, "#{ruby} --version"
  end
end

step 'Ensure puppet is not running or enabled as a service' do
  # This step should not be needed as puppet should be stopped/disabled on install
  # But pxp-agent tests should not fail if the installer gets this wrong. e.g. PA-654
  on(agents, puppet('resource service puppet ensure=stopped enable=false'))
end
