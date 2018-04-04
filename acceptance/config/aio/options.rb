{
  :type => 'aio',
  :is_puppetserver => true,
  :puppetservice => 'puppetserver',
  :'puppetserver-confdir' => '/etc/puppetlabs/puppetserver/conf.d',
  :pre_suite => [
    "setup/common/045_SetPuppetServerOnAgents.rb",
    "setup/common/050_Setup_Broker.rb",
    "setup/common/060_Setup_PCP_Client.rb",
  ],
  :post_suite => [
    'teardown/common/099_Archive_Logs.rb',
  ],
}
