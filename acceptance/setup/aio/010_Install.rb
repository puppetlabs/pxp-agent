require 'puppet/acceptance/common_utils'
require 'beaker/dsl/install_utils'
extend Beaker::DSL::InstallUtils

test_name "Install Packages"

DEFAULT_BUILDS_URL = 'http://builds.delivery.puppetlabs.net'
dev_builds_url = ENV['DEV_BUILDS_URL'] || DEFAULT_BUILDS_URL

step "Install puppet-agent..." do
  if dev_builds_url == 'http://builds.delivery.puppetlabs.net'
    install_from_build_data_url('puppet-agent', "#{dev_builds_url}/puppet-agent/#{ENV['SHA']}/artifacts/#{ENV['SHA']}.yaml")
  else
    hosts.each do |host|
      install_puppetlabs_dev_repo(host, 'puppet-agent', ENV['SHA'], nil, :dev_builds_url => dev_builds_url)
      host.install_package('puppet-agent')
    end
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
    if ENV['SERVER_VERSION'] && ENV['SERVER_VERSION'] != 'latest' && dev_builds_url == DEFAULT_BUILDS_URL
      install_from_build_data_url('puppetserver', "#{dev_builds_url}/puppetserver/#{ENV['SERVER_VERSION']}/artifacts/#{ENV['SERVER_VERSION']}.yaml", master)
    else
      dev_builds_url = 'https://nightlies.puppetlabs.com' if dev_builds_url == DEFAULT_BUILDS_URL
      server_version = ENV['SERVER_VERSION'] || 'latest'
      install_puppetlabs_dev_repo(master, 'puppetserver', server_version, nil, :dev_builds_url => dev_builds_url)
      master.install_package('puppetserver')
    end
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
