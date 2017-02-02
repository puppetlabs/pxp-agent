step 'Ensure puppet is not running or enabled as a service' do
  # This step should not be needed as puppet should be stopped/disabled on install
  # But pxp-agent tests should not fail if the installer gets this wrong. e.g. PA-654
  on(agents, puppet('resource service puppet ensure=stopped enable=false'))
  on(agents, puppet('config set daemonize false --section agent'))
end
