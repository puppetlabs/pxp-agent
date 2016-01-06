{
  :log_level     => 'info',
  :type          => 'packages',
  :forge_host    => 'forge-aio01-petest.puppetlabs.com',
  :load_path     => './lib/',
  :repo_proxy    => true,
  :add_el_extras => true,
  :'puppetserver-confdir' => '/etc/puppetserver/conf.d',
  :pre_suite     => [
    'setup/aio/010_Install.rb',
    'setup/aio/021_InstallAristaModule.rb',
    'setup/common/010_Setup_Broker.rb',
    'setup/common/020_Configure_Pxp_Agents.rb',],
  :ssh           => {
    :keys => ["id_rsa_acceptance", "#{ENV['HOME']}/.ssh/id_rsa-acceptance",
              "id_rsa", "#{ENV['HOME']}/.ssh/id_rsa"],
  },
}
