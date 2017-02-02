#include "../../common/content_format.hpp"

#include <pxp-agent/modules/ping.hpp>

#include <cpp-pcp-client/protocol/chunks.hpp>       // ParsedChunks

#include <leatherman/json_container/json_container.hpp>

#include <boost/date_time/posix_time/posix_time.hpp>

#include <catch.hpp>

#include <vector>
#include <algorithm>

namespace PXPAgent {

namespace lth_jc = leatherman::json_container;

static const std::string PING_ACTION { "ping" };

static const std::string PING_TXT {
    (DATA_FORMAT % "\"0563\""
                 % "\"ping\""
                 % "\"ping\""
                 % "{}").str() };

static const std::vector<lth_jc::JsonContainer> DEBUG {
    lth_jc::JsonContainer { "{\"hops\" : []}" }
};

static const PCPClient::ParsedChunks PARSED_CHUNKS {
                    lth_jc::JsonContainer(ENVELOPE_TXT),
                    lth_jc::JsonContainer(PING_TXT),
                    DEBUG,
                    0 };

TEST_CASE("Modules::Ping::executeAction", "[modules]") {
    Modules::Ping ping_module {};
    ActionRequest request { RequestType::Blocking, PARSED_CHUNKS };

    SECTION("the ping module is correctly named") {
        REQUIRE(ping_module.module_name == "ping");
    }

    SECTION("the ping module has the ping action") {
        auto found = std::find(ping_module.actions.begin(),
                               ping_module.actions.end(),
                               "ping");
        REQUIRE(found != ping_module.actions.end());
    }

    SECTION("it can call the ping action") {
        REQUIRE_NOTHROW(ping_module.executeAction(request));
    }

    SECTION("it should return the request_hops entries") {
        auto response = ping_module.executeAction(request);
        auto r = response.action_metadata.get<lth_jc::JsonContainer>("results");
        REQUIRE(r.includes("request_hops"));
    }
}

TEST_CASE("Modules::Ping::ping", "[modules]") {
    Modules::Ping ping_module {};

    boost::format data_format {
        "{  \"transaction_id\" : \"1234123412\","
        "   \"module\" : \"ping\","
        "   \"action\" : \"ping\","
        "   \"params\" : {"
        "       \"sender_timestamp\" : \"%1%\""  // string
        "   }"
        "}"
    };

    SECTION("it should respond when debug chunks are omitted") {
        auto data_txt = (data_format % "").str();
        PCPClient::ParsedChunks other_chunks {
                    lth_jc::JsonContainer(ENVELOPE_TXT),
                    lth_jc::JsonContainer(PING_TXT),
                    std::vector<lth_jc::JsonContainer>{},
                    0 };
        ActionRequest other_request { RequestType::Blocking, other_chunks };

        auto result = ping_module.ping(other_request);
        std::cout << result.toString() << std::endl;
        auto hops = result.get<std::vector<lth_jc::JsonContainer>>(
                        "request_hops");
        REQUIRE(hops.empty());
    }

    boost::format debug_format { "{ \"hops\" : %1% }" };  // vector<JsonContainer>

    SECTION("it should copy an empty hops entry") {
        auto data_txt = (data_format % "").str();
        auto debug_txt = (debug_format % "[]").str();
        std::vector<lth_jc::JsonContainer> other_debug {
            lth_jc::JsonContainer { debug_txt }
        };

        PCPClient::ParsedChunks other_chunks {
                    lth_jc::JsonContainer(ENVELOPE_TXT),
                    lth_jc::JsonContainer(PING_TXT),
                    other_debug,
                    0 };
        ActionRequest other_request { RequestType::Blocking, other_chunks };

        auto result = ping_module.ping(other_request);
        auto hops = result.get<std::vector<lth_jc::JsonContainer>>(
                        "request_hops");
        REQUIRE(hops.empty());
    }

    boost::format hop_format {
        "{  \"server\" : \"%1%\","
        "   \"time\"   : \"%2%\","
        "   \"stage\"  : \"%3%\""
        "}"
    };

    std::string hops_str { "[ " };
    hops_str += (hop_format % "server_A" % "001" % "accepted").str() + ", ";
    hops_str += (hop_format % "server_A" % "003" % "accept-to-queue").str() + ", ";
    hops_str += (hop_format % "server_A" % "005" % "accept-to-mesh").str() + ", ";
    hops_str += (hop_format % "server_A" % "007" % "deliver").str();
    hops_str += " ]";

    SECTION("it should copy the hops entry when msg passed through a single broker") {
        auto data_txt = (data_format % "").str();
        auto debug_txt = (debug_format % hops_str).str();
        std::vector<lth_jc::JsonContainer> other_debug {
            lth_jc::JsonContainer { debug_txt }
        };

        PCPClient::ParsedChunks other_chunks {
                lth_jc::JsonContainer(ENVELOPE_TXT),
                lth_jc::JsonContainer(data_txt),
                other_debug,
                0 };
        ActionRequest other_request { RequestType::Blocking, other_chunks };

        auto result = ping_module.ping(other_request);
        auto hops = result.get<std::vector<lth_jc::JsonContainer>>(
                        "request_hops");

        REQUIRE(hops.size() == 4u);
        REQUIRE(hops[3].get<std::string>("time") == "007");
    }
}

}  // namespace PXPAgent
