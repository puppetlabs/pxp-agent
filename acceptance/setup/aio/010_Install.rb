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
    'puppet-agent'
  ],
}

# Install puppet on all hosts including master
install_packages_on(hosts, PACKAGES)

step 'Install MSIs on any Windows agents'
agents.each do |agent|
  if agent.platform.start_with?('windows')
    logger.info "Installing Puppet agent msi #{sha} on #{agent}"
    install_puppet_agent_dev_repo_on(agent, :version => sha)
    logger.info 'Prevent Puppet Service from Running'
    on(agent, puppet('resource service puppet ensure=stopped enable=false'))
    logger.info 'Vendored Ruby needs to be on PATH for pxp-agent to load libraries'
    # export needs sed'd to first line as bashrc exits for non-interaction shells near top of file
    (agent.is_x86_64? && (agent[:ruby_arch] == "x86")) ?
      export_path = "\$PATH\":\/cygdrive\/c\/Program\ Files\ \(x86\)\/Puppet\ Labs\/Puppet\/sys\/ruby\/bin\"" :
      export_path = "\$PATH\":\/cygdrive\/c\/Program\ Files\/Puppet\ Labs\/Puppet\/sys\/ruby\/bin\""
    on(agent, "sed -i '1iexport\ PATH=#{export_path}' ~/.bashrc")
  end
end
