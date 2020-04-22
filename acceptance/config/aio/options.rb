{
  :type => 'aio',
  :is_puppetserver => true,
  :'use-service' => true, # use service scripts for start/stop
  :puppetservice => 'puppetserver',
  :'puppetserver-confdir' => '/etc/puppetlabs/puppetserver/conf.d',
  :pre_suite => [
    "setup/common/045_SetPuppetServerOnAgents.rb",
    "setup/common/050_Setup_Broker.rb",
    "setup/common/060_Setup_PCP_Client.rb",
    "setup/common/070_Setup_Server_fixtures.rb",
  ],
  :post_suite => [
    'teardown/common/099_Archive_Logs.rb',
  ],
}
