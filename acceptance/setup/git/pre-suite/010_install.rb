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

install_puppet_agent_on(hosts, {:default_action => 'gem_install'})

step 'Install pxp-agen on all hosts'
repositories = [
  {:name    => 'pxp-agent',
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

step 'Hosts: create puppet.conf'
hosts.each do |host|
  on host, puppet('config set reports "store"   --section main')
  on host, puppet("config set server #{master}           --section agent")
  on host, puppet('config set report true                --section agent')
end
on master, puppet("config set 'dns_alt_names' 'puppet,#{master.hostname}'       --section master")
on master, puppet('config set environmentpath /etc/puppetlabs/code/environments --section master')
on master, puppet('config set storeconfigs false                                 --section master')
#on master, puppet('config set storeconfigs_backend puppetdb                     --section master')
#on master, puppet('config set data_binding_terminus none                        --section master')
on master, puppet('master')

# TODO (JS) there should be a seperate cert-signing file
on(agents, puppet("agent --test --server #{master}"), :acceptable_exit_codes => [0,1])
on(master, puppet("cert sign --all"), :acceptable_exit_codes => [0,24])
