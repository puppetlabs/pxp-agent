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

step "Install puppetserver..." do
  if master[:hypervisor] == 'ec2' && master[:platform].match(/(?:el|centos|oracle|redhat|scientific)/)
    variant, version = master['platform'].to_array
    server_num = ENV['SERVER_VERSION'].to_i
    # nil or 'latest' will convert to 0
    if server_num > 0 && server_num < 5
      logger.info "EC2 master found: Installing public PC1 repo to satisfy puppetserver dependency."
      on(master, "rpm -Uvh https://yum.puppetlabs.com/puppetlabs-release-pc1-el-#{version}.noarch.rpm")
    else
      logger.info "EC2 master found: Installing public puppet5 repo to satisfy puppetserver dependency."
      on(master, "rpm -Uvh https://yum.puppetlabs.com/puppet5-release-el-#{version}.noarch.rpm")
    end
  else
    if ENV['SERVER_VERSION']
      install_puppetlabs_dev_repo(master, 'puppetserver', ENV['SERVER_VERSION'])
      install_puppetlabs_dev_repo(master, 'puppet-agent', ENV['SHA'])
    else
      # beaker can't install puppetserver from nightlies (BKR-673)
      repo_configs_dir = 'repo-configs'
      install_repos_on(master, 'puppetserver', 'nightly', repo_configs_dir)
      install_repos_on(master, 'puppet-agent', ENV['SHA'], repo_configs_dir)
    end
  end
  master.install_package('puppetserver')
end

step 'Make sure install is sane' do
  # beaker has already added puppet and ruby to PATH in ~/.ssh/environment
  agents.each do |agent|
    on agent, puppet('--version')
    ruby = Puppet::Acceptance::CommandUtils.ruby_command(agent)
    on agent, "#{ruby} --version"
  end
end
