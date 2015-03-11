#include "test/test.h"

#include "src/errors.h"
#include "src/modules/ping.h"

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/format.hpp>

#include <vector>

extern std::string ROOT_PATH;

// TODO(ale): update this to cthun-client

namespace CthunAgent {

static const std::string ping_action { "ping" };

static const std::string ping_data_txt {
    "{  \"module\" : \"ping\","
    "   \"action\" : \"ping\","
    "   \"params\" : {}"
    "}"
};

static const std::string debug_txt { "{\"hops\" : []}" };
static const DataContainer debug_entry { debug_txt };
static const std::vector<DataContainer> debug { debug_entry };

static const ParsedContent content { DataContainer(),
                                     DataContainer(ping_data_txt),
                                     debug };

TEST_CASE("Modules::Ping::callAction", "[modules]") {
    Modules::Ping ping_module {};

    SECTION("the ping module is correctly named") {
        REQUIRE(ping_module.module_name == "ping");
    }

    SECTION("the ping module has the ping action") {
        REQUIRE(ping_module.actions.find(ping_action) != ping_module.actions.end());
    }

    SECTION("it can call the ping action") {
        REQUIRE_NOTHROW(ping_module.callAction(ping_action, content));
    }

    SECTION("it should return the request_hops entries") {
        auto result = ping_module.callAction(ping_action, content);
        REQUIRE(result.includes("request_hops"));
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
        std::vector<DataContainer> other_debug { DataContainer(debug_txt) };

        ParsedContent other_content { DataContainer(),
                                      DataContainer(data_txt),
                                      other_debug };

        auto result = ping_module.ping(other_content);
        REQUIRE(result.get<std::vector<DataContainer>>("request_hops").empty());
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
        std::vector<DataContainer> other_debug { DataContainer(debug_txt) };


        ParsedContent other_content { DataContainer(),
                                      DataContainer(data_txt),
                                      other_debug };

        auto result = ping_module.ping(other_content);
        auto hops = result.get<std::vector<DataContainer>>("request_hops");

        REQUIRE(hops.size() == 4);
        REQUIRE(hops[3].get<std::string>("time") == "007");
    }
}

}  // namespace CthunAgent
