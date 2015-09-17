require 'puppet/acceptance/install_utils'
extend Puppet::Acceptance::InstallUtils

test_name 'Install packages and git project repos'

step 'Install puppet-agent' do
  agent_version    = ENV['AGENT_VERSION']    ||= 'nightly'
  repo_configs_dir = 'repo-configs'

  hosts.each do |host|
    install_repos_on(host, 'puppet-agent', agent_version, repo_configs_dir)
    host.add_env_var('PATH', '/opt/puppetlabs/bin:$PATH')
  end
end

AGENT_PACKAGES = {
  :redhat => [
    'puppet-agent',
  ],
  :debian => [
    'puppet-agent',
  ],
}

# For now, install puppet on everything
# TODO (JS) - install Puppet server on master
install_packages_on(hosts, AGENT_PACKAGES)

step 'Install pxp-module-puppet on all hosts'
repositories = [
  {:name    => 'pxp-module-puppet',
   :sha     => ENV['PXP_MODULE_PUPPET_VER'],
   :private => true,
  },
]
repositories.each do |repo|
  hosts.each do |host|
    step "Install #{repo[:name]} repo"
    github_server = 'github.com'
    github_protocol = repo[:private] ? 'git@' : 'https://'
    repo[:path] = build_git_url(repo[:name],repo[:fork],github_server,github_protocol)
    repo[:rev]  = repo[:sha] || repo[:branch]
    install_from_git(host, SourcePath, repo)
  end
end

step 'Ensure puppet user and group added to master' do
  on master, puppet('resource user  puppet   ensure=present')
  on master, puppet('resource group puppet   ensure=present')
end

step 'start all the things' do
  on master, puppet("config set server #{master} --section agent")
  on master, puppet('master')
end
