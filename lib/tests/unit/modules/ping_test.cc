#include <catch.hpp>

#include <cthun-agent/modules/ping.hpp>
#include <cthun-agent/errors.hpp>

#include <cthun-client/data_container/data_container.hpp>
#include <cthun-client/protocol/chunks.hpp>       // ParsedChunks

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/format.hpp>

#include <vector>
#include <algorithm>

extern std::string ROOT_PATH;

namespace CthunAgent {

static const std::string ping_action { "ping" };

static const std::string ping_data_txt {
    "{  \"module\" : \"ping\","
    "   \"action\" : \"ping\","
    "   \"params\" : {}"
    "}"
};

static const std::vector<CthunClient::DataContainer> debug {
    CthunClient::DataContainer { "{\"hops\" : []}" }
};

static const CthunClient::ParsedChunks parsed_chunks {
                    CthunClient::DataContainer(),
                    CthunClient::DataContainer(ping_data_txt),
                    debug,
                    0 };

TEST_CASE("Modules::Ping::executeAction", "[modules]") {
    Modules::Ping ping_module {};
    ActionRequest request { RequestType::Blocking, parsed_chunks };

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
        auto outcome = ping_module.executeAction(request);
        REQUIRE(outcome.results.includes("request_hops"));
    }
}

TEST_CASE("Modules::Ping::ping", "[modules]") {
    Modules::Ping ping_module {};

    boost::format data_format {
        "{  \"module\" : \"ping\","
        "   \"action\" : \"ping\","
        "   \"params\" : {"
        "       \"sender_timestamp\" : \"%1%\""  // string
        "   }"
        "}"
    };

    boost::format debug_format { "{ \"hops\" : %1% }" };  // vector<DataContainer>

    SECTION("it should copy an empty hops entry") {
        auto data_txt = (data_format % "").str();
        auto debug_txt = (debug_format % "[]").str();
        std::vector<CthunClient::DataContainer> other_debug {
            CthunClient::DataContainer { debug_txt }
        };

        CthunClient::ParsedChunks other_chunks {
                    CthunClient::DataContainer(),
                    CthunClient::DataContainer(ping_data_txt),
                    other_debug,
                    0 };
        ActionRequest other_request { RequestType::Blocking, other_chunks };

        auto result = ping_module.ping(other_request);
        auto hops = result.get<std::vector<CthunClient::DataContainer>>(
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

    SECTION("it should copy the hops entry when msg passed through a single server") {
        auto data_txt = (data_format % "").str();
        auto debug_txt = (debug_format % hops_str).str();
        std::vector<CthunClient::DataContainer> other_debug {
            CthunClient::DataContainer { debug_txt }
        };

        CthunClient::ParsedChunks other_chunks {
                CthunClient::DataContainer(),
                CthunClient::DataContainer(data_txt),
                other_debug,
                0 };
        ActionRequest other_request { RequestType::Blocking, other_chunks };

        auto result = ping_module.ping(other_request);
        auto hops = result.get<std::vector<CthunClient::DataContainer>>(
                        "request_hops");

        REQUIRE(hops.size() == 4);
        REQUIRE(hops[3].get<std::string>("time") == "007");
    }
}

}  // namespace CthunAgent
