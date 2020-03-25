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
                 std::shared_ptr<ResultsStorage> storage,
                 const fs::path& libexec_path) :
        BoltModule { exec_prefix, std::move(storage), std::move(module_cache_dir) },
        Purgeable { module_cache_dir_->purge_ttl_ },
        master_uris_ { master_uris },
        ca_ { ca },
        crt_ { crt },
        key_ { key },
        crl_ { crl },
        proxy_ { proxy },
        download_connect_timeout_ { download_connect_timeout },
        download_timeout_ { download_timeout },
        libexec_path_ { libexec_path }
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

        std::string plugin_cache_name;

        const std::string ruby_shim_script_name = "apply_ruby_shim.rb";
        auto apply_ruby_shim_path = libexec_path_ / ruby_shim_script_name;

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
