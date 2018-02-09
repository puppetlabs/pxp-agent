require 'puppet/acceptance/common_utils'
require 'beaker/dsl/install_utils'
extend Beaker::DSL::InstallUtils

test_name "Install Packages"

def install_from_local(hosts, package)
  hosts.each do |agent|
    tmpdir = agent.tmpdir('puppet_agent')

    step "Copy package to #{agent}" do
      scp_to(agent, package, tmpdir)
    end

    step "Install package on #{agent}" do
      agent.install_package("#{tmpdir}/#{File.basename(package)}")
    end
  end
end

step "Install puppet-agent..." do
  if ENV['PACKAGE']
    step "from local package #{ENV['PACKAGE']}" do
      install_from_local(agents, ENV['PACKAGE'])
    end
  else
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
    if ENV['SERVER_VERSION'].nil? || ENV['SERVER_VERSION'] == 'latest'
      server_version = 'latest'
      server_download_url = "http://ravi.puppetlabs.com"
    else
      server_version = ENV['SERVER_VERSION']
      server_download_url = "http://builds.delivery.puppetlabs.net"
    end
    install_puppetlabs_dev_repo(master, 'puppetserver', server_version, nil, :dev_builds_url => server_download_url)
    install_puppetlabs_dev_repo(master, 'puppet-agent', ENV['SHA'])
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
