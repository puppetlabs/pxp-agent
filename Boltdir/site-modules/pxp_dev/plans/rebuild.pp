plan pxp_dev::rebuild (
  TargetSpec $target_agents,
  String $target_host_type,
  Optional[Boolean] $run_cmake = false,
  Optional[String] $winrm_pass = undef,
) {
  $pxp_root = pxp_root()
  $build_host = get_target('build_host')
  $build_location = strip(run_command('find /var/tmp/ -maxdepth 2 -name pxp-agent', $build_host).first().value()['stdout'])
  if $target_host_type =~ /^win/ {
    upload_file($pxp_root, file::join($build_location, 'pxp-agent'), $build_host)
  } else {
    $build_hostname = $build_host.host()
    run_command("rsync ${pxp_root} ${build_hostname}:${build_location}", localhost)
  }
  run_task('pxp_dev::run_pxp_make', $build_host, run_cmake => $run_cmake)
  run_plan('pxp_dev::upload_to_agent', target_agents => $target_agents, target_host_type => $target_agents, winrm_pass => $winrm_pass)
}
