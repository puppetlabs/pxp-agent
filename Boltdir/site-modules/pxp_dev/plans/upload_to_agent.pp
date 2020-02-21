plan pxp_dev::upload_to_agent (
  TargetSpec $target_agents,
  String $target_host_type,
  Optional[String] $winrm_pass = undef,
) {
  $status = run_task('service', $target_agents, action => 'status', name => 'pxp-agent')
  $running_agents = $status.filter |$status_result| {
    $status_result.value()['enabled'] == 'enabled'
  }
  # Stop any running pxp-agents before replacing the executable
  run_task('service', $running_agents, action => 'stop', name => 'pxp-agent')

  $build_host = get_target('build_host')
  run_task('pxp_dev::upload_from_builder_to_agent', $build_host, agents => $target_agents)

  # restart any previously stopped agents
  run_task('service', $running_agents, action => 'start', name => 'pxp-agent')
}
