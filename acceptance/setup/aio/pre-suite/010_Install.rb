require 'puppet/acceptance/common_utils'
require 'puppet/acceptance/install_utils'
extend Puppet::Acceptance::InstallUtils
require 'beaker/dsl/install_utils'
extend Beaker::DSL::InstallUtils

test_name "Install Packages"

step "Install repositories on target machines..." do
  sha = ENV['SHA']
  hosts.each do |host|
    install_repos_on(host, 'puppet-agent', sha, 'repo-configs')
  end
end

step "Install puppet-agent on hosts" do
  PACKAGES = {
    :redhat => [
      'puppet-agent',
    ],
    :debian => [
      'puppet-agent',
    ],
  }

  install_packages_on(hosts, PACKAGES)

  # make sure install is sane, beaker has already added puppet and ruby
  # to PATH in ~/.ssh/environment
  agents.each do |agent|
    on agent, puppet('--version')
    ruby = Puppet::Acceptance::CommandUtils.ruby_command(agent)
    on agent, "#{ruby} --version"
  end
end

# This step is unique to pxp-agent
step 'Add path for pxp-agent.exe on Windows' do
  agents.each do |agent|
    if agent.platform.start_with?('windows')
      (agent.is_x86_64? && (agent[:ruby_arch] == "x86")) ?
        export_path = "\$PATH\":\/cygdrive\/c\/Program\ Files\ \(x86\)\/Puppet\ Labs\/Puppet\/sys\/ruby\/bin\"" :
        export_path = "\$PATH\":\/cygdrive\/c\/Program\ Files\/Puppet\ Labs\/Puppet\/sys\/ruby\/bin\""
      # bashrc exits almost immediately for non-interaction shells, so our export needs sed'd to first line
      on(agent, "sed -i '1iexport\ PATH=#{export_path}' ~/.bashrc")
    end
  end
end

step 'Install build dependencies on master' do

 MASTER_PACKAGES = {
    :redhat => [
      'git',
      'java-1.8.0-openjdk-devel',
    ],
  }
  install_packages_on(master, MASTER_PACKAGES, :check_if_exists => true)
end
