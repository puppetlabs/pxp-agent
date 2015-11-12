test_name 'C93805 - pxp-agent - Versioning test'

def pxp_version_cmd(platform)
  if platform.start_with?('windows')
    # simple hack to ensure OpenSSL is found w/out modifying PATH
    "cd /cygdrive/c/Program\\ Files/Puppet\\ Labs/Puppet/sys/ruby/bin && ../../../pxp-agent/bin/pxp-agent --version"
  else
    'cd /opt/puppetlabs/puppet/bin && ./pxp-agent --version'
  end
end

step 'cd into pxp-agent bin folder and check the version'
agents.each do |agent|
  on(agent, pxp_version_cmd(agent.platform)) do |result|
    assert(/[0-9\.]*/ =~ result.stdout, "Version number should be numbers and periods but was \"#{result.stdout.to_s}\"")
  end
end
