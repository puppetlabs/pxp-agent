#include <pxp-agent/modules/apply.hpp>
#include <pxp-agent/util/bolt_helpers.hpp>
#include <pxp-agent/configuration.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/tokenizer.hpp>
#include <leatherman/json_container/json_container.hpp>
#include <leatherman/file_util/file.hpp>
#include <leatherman/locale/locale.hpp>

namespace fs = boost::filesystem;
namespace lth_file = leatherman::file_util;
namespace lth_jc   = leatherman::json_container;
namespace lth_loc  = leatherman::locale;

namespace PXPAgent {
namespace Modules {

    static const std::string apply_ACTION { "apply" };
    static const std::string apply_ACTION_INPUT_SCHEMA { R"(
    {
        "type": "object",
        "properties": {
            "catalog": {
                "type": "object"
            },
            "apply_options": {
                "type": "object"
            }
        },
        "required": ["catalog", "apply_options"]
    }
    )" };

    static const std::string prep_ACTION { "prep" };

    // TODO: Actually manage this file in packaging (or perhaps fetch it from puppetserver).
    // For now just copy it to the cache dir each time unless there is already a copy
    static const std::string APPLY_RUBY_SHIM {
R"(#! /opt/puppetlabs/puppet/bin/ruby
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
server_list = args['master_uris'].map { |uri| URI.parse(uri).host }.join(',')

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

  # Now fully isolate again
  Puppet.settings.send(:clear_everything_for_tests)
  Puppet.initialize_settings(cli_base)
  # Avoid extraneous output
  Puppet[:report] = false

  # apply_settings will always be a hash (even if it empty) who's keys are puppet settings
  # For example: `noop` or `show_diff`. (only applies to apply action)
  # CODEREVIEW: is it safe to just add this under the rest of the 'apply' action?
  args['apply_options'].each { |setting, value| Puppet[setting.to_sym] = value } if args['action'] == 'apply'

  Puppet[:default_file_terminus] = :file_server
  # This happens implicitly when running the Configurer, but we make it explicit here. It creates the
  # directories we configured earlier.
  Puppet.settings.use(:main)

  # Given we set up an empty env, it will be 'production' in this case
  # Note we *do not* want to set this to the environment we are applying the catalog for
  env = Puppet.lookup(:environments).get('production')

  # Ensure custom facts are available for provider suitability tests
  facts = Puppet::Node::Facts.indirection.find(SecureRandom.uuid, environment: env)

  if args['action'] == 'apply'

    report = Puppet::Transaction::Report.new

    overrides = { current_environment: env,
                  loaders: Puppet::Pops::Loaders.new(env) }

    Puppet.override(overrides) do
      catalog = Puppet::Resource::Catalog.from_data_hash(args['catalog'])
      catalog.environment = env.name.to_s
      catalog.environment_instance = env
      Puppet::Pops::Evaluator::DeferredResolver.resolve_and_replace(facts, catalog)

      catalog = catalog.to_ral

      configurer = Puppet::Configurer.new
      configurer.run(catalog: catalog, report: report, pluginsync: false)
    end

    puts JSON.pretty_generate(report.to_data_hash)
    exit_code = report.exit_status != 1
  else
    puts JSON.pretty_generate(facts.values)
  end
ensure
  begin
    FileUtils.remove_dir(puppet_root)
  rescue Errno::ENOTEMPTY => e
    STDERR.puts("Could not cleanup temporary directory: #{e}")
  end
end

exit exit_code
)" };

    Apply::Apply(const fs::path& exec_prefix,
                 const std::vector<std::string>& master_uris,
                 const std::string& ca,
                 const std::string& crt,
                 const std::string& key,
                 const std::string& crl,
                 const std::string& proxy,
                 uint32_t download_connect_timeout,
                 uint32_t download_timeout,
                 std::shared_ptr<ModuleCacheDir> module_cache_dir,
                 std::shared_ptr<ResultsStorage> storage) :
        BoltModule { exec_prefix, std::move(storage), std::move(module_cache_dir) },
        Purgeable { module_cache_dir_->purge_ttl_ },
        master_uris_ { master_uris },
        ca_ { ca },
        crt_ { crt },
        key_ { key },
        crl_ { crl },
        proxy_ { proxy },
        download_connect_timeout_ { download_connect_timeout },
        download_timeout_ { download_timeout }
    {
        module_name = "apply";
        actions.push_back(apply_ACTION);
        actions.push_back(prep_ACTION);
        PCPClient::Schema apply_input_schema { apply_ACTION, lth_jc::JsonContainer { apply_ACTION_INPUT_SCHEMA } };
        input_validator_.registerSchema(apply_input_schema);

        PCPClient::Schema prep_input_schema { prep_ACTION };
        prep_input_schema.addConstraint("environment", PCPClient::TypeConstraint::String, true);
        input_validator_.registerSchema(prep_input_schema);

        PCPClient::Schema apply_output_schema { apply_ACTION };
        results_validator_.registerSchema(apply_output_schema);


        PCPClient::Schema prep_output_schema { prep_ACTION };
        results_validator_.registerSchema(prep_output_schema);
    }

    // This is copied here for now until we decide how to manage the ruby shim
    // NIX_DIR_PERMS is defined in pxp-agent/configuration
    #define NIX_DOWNLOADED_FILE_PERMS NIX_DIR_PERMS

    Util::CommandObject Apply::buildCommandObject(const ActionRequest& request)
    {
        if (crl_ == "") {
          throw Configuration::Error { lth_loc::format("ssl-crl setting is requried for apply") };
        }
        // Shared
        auto action = request.action();
        auto params = request.params();
        params.set<std::string>("ca", ca_);
        params.set<std::string>("crt", crt_);
        params.set<std::string>("key", key_);
        params.set<std::string>("crl", crl_);
        params.set<std::string>("proxy", proxy_);

        const fs::path& results_dir { request.resultsDir() };

        const std::string ruby_shim_cache_dir = "apply_ruby_shim";
        const fs::path& cache_dir = module_cache_dir_->createCacheDir(ruby_shim_cache_dir);

        std::string plugin_cache_name;

        const std::string ruby_shim_script_name = "apply_ruby_shim.rb";
        auto apply_ruby_shim_path = cache_dir / ruby_shim_script_name;

        if (!fs::exists(apply_ruby_shim_path)) {
          lth_file::atomic_write_to_file(APPLY_RUBY_SHIM, apply_ruby_shim_path.string(), NIX_DOWNLOADED_FILE_PERMS, std::ios::binary);
        }

        if (action == "apply") {
            plugin_cache_name = params.get<std::string>({"catalog", "environment"});
            params.set<std::string>("environment", params.get<std::string>({"catalog", "environment"}));
            params.set<std::string>("action", "apply");
        } else {
            plugin_cache_name = params.get<std::string>("environment");
            params.set<std::string>("action", "prep");
        }

        const auto plugin_cache = module_cache_dir_->createCacheDir(plugin_cache_name);
        params.set<std::string>("plugin_cache", plugin_cache.string());
        params.set<std::vector<std::string>>("master_uris", master_uris_);
        Util::CommandObject cmd {
            "",  // Executable will be detremined by findExecutableAndArguments
            {},  // No args for invoking shim
            {},  // Shim expects catalog on stdin
            params.toString(),  // JSON encoded string to be passed to shim on stdin
            [results_dir](size_t pid) {
                auto pid_file = (results_dir / "pid").string();
                lth_file::atomic_write_to_file(std::to_string(pid) + "\n", pid_file,
                                            NIX_FILE_PERMS, std::ios::binary);
            }  // PID Callback
        };
        Util::findExecutableAndArguments(apply_ruby_shim_path, cmd);

        return cmd;
    }

    unsigned int Apply::purge(
        const std::string& ttl,
        std::vector<std::string> ongoing_transactions,
        std::function<void(const std::string& dir_path)> purge_callback)
    {
        if (purge_callback == nullptr)
            purge_callback = &Purgeable::defaultDirPurgeCallback;
        return module_cache_dir_->purgeCache(ttl, ongoing_transactions, purge_callback);
    }

}  // namespace Modules
}  // namespace PXPAgent
