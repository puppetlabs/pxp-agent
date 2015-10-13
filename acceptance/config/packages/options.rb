{
  :type          => 'packages',
  :forge_host    => 'forge-aio01-petest.puppetlabs.com',
  :load_path     => './lib/',
  :repo_proxy    => true,
  :add_el_extras => true,
  #:preserve_hosts => 'always',
  :'puppetserver-confdir' => '/etc/puppetserver/conf.d',
  :pre_suite     => [
    'setup/packages/010_Install.rb',],
  :ssh           => {
    :keys => ["id_rsa_acceptance", "#{ENV['HOME']}/.ssh/id_rsa-acceptance",
              "id_rsa", "#{ENV['HOME']}/.ssh/id_rsa"],
  },
}
