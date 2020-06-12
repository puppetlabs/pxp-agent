#include <pxp-agent/modules/script.hpp>
#include <pxp-agent/util/bolt_helpers.hpp>
#include <pxp-agent/configuration.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/tokenizer.hpp>
#include <leatherman/json_container/json_container.hpp>
#include <leatherman/file_util/file.hpp>


namespace fs = boost::filesystem;
namespace lth_jc = leatherman::json_container;
namespace lth_file = leatherman::file_util;

namespace PXPAgent {
namespace Modules {

    static const std::string script_ACTION { "run" };
    static const std::string script_ACTION_INPUT_SCHEMA { R"(
    {
        "type": "object",
        "properties": {
            "script": {
                "type": "object",
                "properties": {
                    "filename": {
                        "type": "string"
                    },
                    "uri": {
                        "type": "object",
                        "properties": {
                            "path": {
                                "type": "string"
                            },
                            "params": {
                                "type": "object"
                            }
                        },
                        "required": ["path", "params"]
                    },
                    "sha256": {
                        "type": "string"
                    }
                },
                "required": ["filename", "uri", "sha256"]
            },
            "arguments": {
                "type": "array",
                "items": {
                    "type": "string"
                }
            }
        },
        "required": ["script", "arguments"]
    }
    )" };

    Script::Script(const fs::path& exec_prefix,
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
        download_connect_timeout_ { download_connect_timeout },
        download_timeout_ { download_timeout }
    {
        module_name = "script";
        actions.push_back(script_ACTION);

        PCPClient::Schema input_schema { script_ACTION, lth_jc::JsonContainer { script_ACTION_INPUT_SCHEMA } };
        PCPClient::Schema output_schema { script_ACTION };

        input_validator_.registerSchema(input_schema);
        results_validator_.registerSchema(output_schema);

        client_.set_ca_cert(ca);
        client_.set_client_cert(crt, key);
        client_.set_client_crl(crl);
        client_.set_supported_protocols(CURLPROTO_HTTPS);
        client_.set_proxy(proxy);
    }

    Util::CommandObject Script::buildCommandObject(const ActionRequest& request)
    {
        const auto params = request.params();
        auto script = params.get<lth_jc::JsonContainer>("script");
        auto arguments = params.get<std::vector<std::string>>("arguments");
        const fs::path& results_dir { request.resultsDir() };

        // get script from cache, download if necessary
        auto script_file = module_cache_dir_->getCachedFile(master_uris_,
                                                            download_connect_timeout_,
                                                            download_timeout_,
                                                            client_,
                                                            module_cache_dir_->createCacheDir(script.get<std::string>("sha256")),
                                                            script);
        Util::CommandObject cmd {
            "",         // Executable will be detremined by findExecutableAndArguments
            arguments,  // Arguments
            {},         // Environment
            "",         // Input
            [results_dir](size_t pid) {
                auto pid_file = (results_dir / "pid").string();
                lth_file::atomic_write_to_file(std::to_string(pid) + "\n", pid_file,
                                            NIX_FILE_PERMS, std::ios::binary);
            }  // PID Callback
        };
        Util::findExecutableAndArguments(script_file, cmd);

        return cmd;
    }

    unsigned int Script::purge(
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
