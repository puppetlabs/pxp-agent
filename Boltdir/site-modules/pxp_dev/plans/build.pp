plan pxp_dev::build (
  TargetSpec $target_agents,
  String $agent_ref,
  String $target_host_type,
  Optional[String] $winrm_pass = undef,
  Optional[Boolean] $no_bundler = false,
) {
  $pxp_root = pxp_root()
  $vanagon_builder_hostname = run_task('pxp_dev::build_deps_with_vanagon', localhost, agent_ref => $agent_ref, os_type => $target_host_type, no_bundler => $no_bundler).first().value()['build_host']
  if $target_host_type =~ /^win/ {
    $build_host_opts_hash = {'uri' => $vanagon_builder_hostname,
                             'name' => 'build_host',
                             'config' => {
                               'transport' => 'winrm',
                               'winrm' => {
                                 'user' => 'Administrator',
                                 'password' => $winrm_pass,
                                 'ssh' => false,
                               }
                             }
                           }
  } else {
    $build_host_opts_hash = {'uri' => $vanagon_builder_hostname,
                             'name' => 'build_host',
                             'config' => {
                               'transport' => 'ssh',
                               'ssh' => {
                                 'host-key-check' => false
                               }
                             }
                           }
  }
  $build_host = Target.new($build_host_opts_hash)
  replace_inventory($build_host_opts_hash, file::join($pxp_root, 'Boltdir', 'inventory.yaml'))

  $build_location = strip(run_command('dirname $(find /var/tmp/ -maxdepth 2 -name cpp-pcp-client)', $build_host).first().value()['stdout'])
  upload_file($pxp_root, file::join($build_location, 'pxp-agent'), $build_host)
  run_task('pxp_dev::run_pxp_make', $build_host, run_cmake => true)
  run_plan('pxp_dev::upload_to_agent',target_agents => $target_agents, target_host_type => $target_agents, winrm_pass => $winrm_pass)
}
