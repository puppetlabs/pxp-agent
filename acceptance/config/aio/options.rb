{
  :type => 'aio',
  :pre_suite => [
    'setup/common/000-delete-puppet-when-none.rb',
    'setup/aio/pre-suite/010_Install.rb',
    'setup/aio/021_InstallAristaModule.rb',
    'setup/common/010_Setup_Broker.rb',
  ],
}
