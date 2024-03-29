#!/opt/puppetlabs/puppet/bin/ruby
# frozen_string_literal: true

require 'fileutils'
require 'json'
require 'puppet'
require 'puppet/configurer'
require 'securerandom'
require 'tempfile'
require 'uri'

## TODO: Option to read from a file is for debugging. Only read from stdin in production.
args = JSON.parse(ARGV[0] ? File.read(ARGV[0]) : STDIN.read)

# Create temporary directories for all core Puppet settings so we don't clobber
# existing state or read from puppet.conf. Also create a temporary modulepath.
# Additionally include rundir, which gets its own initialization.
puppet_root = Dir.mktmpdir
moduledir = File.join(puppet_root, 'modules')
Dir.mkdir(moduledir)
plugin_cache = args['plugin_cache']
plugin_dest = File.join(plugin_cache, 'plugins')
pluginfact_dest = File.join(plugin_cache, 'pluginfacts')
# Use isolated directories in puppet_root
setting_keys = Puppet::Settings::REQUIRED_APP_SETTINGS.append(:rundir)
cli_base = (setting_keys).flat_map do |setting|
  ["--#{setting}", File.join(puppet_root, setting.to_s.chomp('dir'))]
end
cli_base.concat([
  '--modulepath',
  moduledir,
  '--plugindest',
  plugin_dest,
  '--pluginfactdest',
  pluginfact_dest
])
# There will always be at least a single master URI
# TODO: make sure comma separated is what --server_list expects
server_list = args['primary_uris'].map { |uri| URI.parse(uri).host }.join(',')

# These settings are required for communication with puppetserver. This is primarily for
# pluginsync but it is also required if a catalog requires files from modules served by 
# puppetserver (Puppet[:default_file_terminus] = rest by default)
cli_settings_pluginsync = [
  '--localcacert',
  args['ca'],
  '--hostcert',
  args['crt'],
  '--hostprivkey',
  args['key'],
  '--hostcrl',
  args['crl'],
  '--server_list',
  server_list
]

# Break apart proxy and extract individual settings
proxy_flags = {
  user: '--http_proxy_user',
  password: '--http_proxy_password',
  port: '--http_proxy_port',
  host: '--http_proxy_host'
}
# Proxy will always be a string (even if its empty)
parsed_proxy = URI.parse(args['proxy'])
proxy_flags.each do |uri_method, setting_flag|
  val = parsed_proxy.send(uri_method)
  cli_settings_pluginsync << setting_flag << val unless val.nil?
end

exit_code = 0
begin

  Puppet.initialize_settings(cli_base + cli_settings_pluginsync)

  remote_env_for_plugins = Puppet::Node::Environment.remote(args['environment'])
  Puppet[:environment] = remote_env_for_plugins.name.to_s
  downloader = Puppet::Configurer::Downloader.new(
    "plugin",
    Puppet[:plugindest],
    Puppet[:pluginsource],
    Puppet[:pluginsignore],
    remote_env_for_plugins
  )
  downloader.evaluate

  source_permissions = Puppet::Util::Platform.windows? ? :ignore : :use
  plugin_fact_downloader = Puppet::Configurer::Downloader.new(
    "pluginfacts",
    Puppet[:pluginfactdest],
    Puppet[:pluginfactsource],
    Puppet[:pluginsignore],
    remote_env_for_plugins,
    source_permissions
  )
  plugin_fact_downloader.evaluate

  # Append the newe paths to the load path (copying them over to the new vardir takes time and seems unneeded)
  $LOAD_PATH << plugin_dest << pluginfact_dest

  # Avoid extraneous output
  Puppet[:report] = false

  # apply_settings will always be a hash (even if it empty) who's keys are puppet settings
  # For example: `noop` or `show_diff`. (only applies to apply action)
  args['apply_options'].each { |setting, value| Puppet[setting.to_sym] = value } if args['action'] == 'apply'

  # This happens implicitly when running the Configurer, but we make it explicit here. It creates the
  # directories we configured earlier.
  Puppet.settings.use(:main)

  # Ensure custom facts are available for provider suitability tests
  facts = Puppet::Node::Facts.indirection.find(SecureRandom.uuid, environment: remote_env_for_plugins)

  if args['action'] == 'apply'

    report = Puppet::Transaction::Report.new

    overrides = { current_environment: remote_env_for_plugins,
                  loaders: Puppet::Pops::Loaders.new(remote_env_for_plugins) }

    Puppet.override(overrides) do
      catalog = Puppet::Resource::Catalog.from_data_hash(args['catalog'])
      catalog.environment = remote_env_for_plugins.name.to_s
      catalog.environment_instance = remote_env_for_plugins
      Puppet::Pops::Evaluator::DeferredResolver.resolve_and_replace(facts, catalog)

      catalog = catalog.to_ral

      configurer = Puppet::Configurer.new
      configurer.run(catalog: catalog, report: report, pluginsync: false)
    end

    puts JSON.pretty_generate(report.to_data_hash)
    exit_code = report.exit_status != 1
  else
    facts.name = facts.values['clientcert']
    puts facts.values.to_json
  end
ensure
  begin
    FileUtils.remove_dir(puppet_root)
  rescue Errno::ENOTEMPTY => e
    STDERR.puts("Could not cleanup temporary directory: #{e}")
  end
end

exit exit_code
