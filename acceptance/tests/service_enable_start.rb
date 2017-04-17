test_name '(C94777) Validate that pxp-agent service can be enabled and started'

agents.each do |agent|
  step 'start service'
  on(agent, puppet('resource service pxp-agent ensure=running enable=true'))

  step 'validate that service is enabled and running'
  on(agent, puppet('resource service pxp-agent')) do |result|
    assert_match(/ensure\s+=>\s+'running'/, result.stdout, 'pxp-agent failed to start')
    assert_match(/enable\s+=>\s+'true'/, result.stdout, 'pxp-agent failed to enable')
  end
end
