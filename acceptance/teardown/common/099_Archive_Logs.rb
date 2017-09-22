require 'pxp-agent/test_helper'

step 'Archive files created during tests' do
  ## Copy file(s) from master
  archive_file_from(master, '/var/log/puppetlabs')

  ## Copy file(s) from agents
  agents.each do |agent|
    if agent['platform'] =~ /cisco_nexus/
      # On Cisco, we don't login as root
      on agent, "chmod -R 777 #{logdir(agent)}"
    end
    archive_file_from(agent, logdir(agent))
  end
end
