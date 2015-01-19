#include "test/test.h"

#include "src/data_container.h"
#include "src/errors.h"
#include "src/modules/ping.h"

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/format.hpp>

#include <vector>

extern std::string ROOT_PATH;

namespace CthunAgent {

static const std::string ping_action { "ping" };

static const std::string ping_txt =
    "{  \"data\" : {"
    "       \"module\" : \"ping\","
    "       \"action\" : \"ping\","
    "       \"params\" : {}"
    "   },"
    "   \"hops\" : []"
    "}";
static const Message msg { ping_txt };

static const DataContainer input {};

TEST_CASE("Modules::Ping::call_action", "[modules]") {
    Modules::Ping ping_module {};

    SECTION("the ping module is correctly named") {
        REQUIRE(ping_module.module_name == "ping");
    }

    SECTION("the ping module has the ping action") {
        REQUIRE(ping_module.actions.find(ping_action) != ping_module.actions.end());
    }

    SECTION("it can call the ping action") {
        REQUIRE_NOTHROW(ping_module.call_action(ping_action, msg, input));
    }

    SECTION("it should execute the ping action correctly") {
        auto result = ping_module.call_action(ping_action, msg, input);
        REQUIRE(result.toString().find("agent_timestamp"));
        REQUIRE(result.toString().find("time_to_agent"));
    }
}

TEST_CASE("Modules::Ping::ping_action", "[modules]") {
    Modules::Ping ping_module {};

    boost::format ping_format {
        "{  \"data\" : {"
        "       \"module\" : \"ping\","
        "       \"action\" : \"ping\","
        "       \"params\" : {"
        "           \"sender_timestamp\" : \"%1%\""  // string
        "       }"
        "   },"
        "   \"hops\" : %2%"  // vector<DataContainer>
        "}"
    };
    boost::format input_format { "{ \"sender_timestamp\" : \"%1%\"}" };

    SECTION("it should take into account the sender_timestamp entry") {
        boost::posix_time::ptime current_date_microseconds =
            boost::posix_time::microsec_clock::local_time();
        auto current_date_milliseconds =
            std::to_string(
                current_date_microseconds.time_of_day().total_milliseconds() - 42);

        Message ping_msg { (ping_format % current_date_milliseconds % "[]").str() };
        DataContainer ping_input { (input_format % current_date_milliseconds).str() };

        auto result = ping_module.ping_action(ping_msg, ping_input);

        REQUIRE(current_date_milliseconds
                == result.get<std::string>("sender_timestamp"));
        REQUIRE(std::stoi(result.get<std::string>("sender_timestamp")) >= 42);
    }

    SECTION("it should copy an empty hops entry") {
        Message ping_msg { (ping_format % "" % "[]").str() };
        DataContainer ping_input { (input_format % "").str() };
        auto result = ping_module.ping_action(ping_msg, ping_input);
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

    SECTION("it should copy the hops entry when msg passed through a single server") {
        hops_str += " ]";

        Message ping_msg { (ping_format % "" % hops_str).str() };
        DataContainer ping_input { (input_format % "").str() };

        auto result = ping_module.ping_action(ping_msg, ping_input);
        auto hops = result.get<std::vector<DataContainer>>("request_hops");

        REQUIRE(hops.size() == 4);
        REQUIRE(hops[3].get<std::string>("time") == "007");
    }

    SECTION("it should copy the hops entry when msg passed through multiple servers") {
        hops_str += ", ";
        hops_str += (hop_format % "server_B" % "020" % "accepted").str() + ", ";
        hops_str += (hop_format % "server_B" % "022" % "accept-to-queue").str() + ", ";
        hops_str += (hop_format % "server_B" % "024" % "accept-to-mesh").str() + ", ";
        hops_str += (hop_format % "server_B" % "026" % "deliver").str();
        hops_str += " ]";

        Message ping_msg { (ping_format % "" % hops_str).str() };
        DataContainer ping_input { (input_format % "").str() };

        auto result = ping_module.ping_action(ping_msg, ping_input);
        auto hops = result.get<std::vector<DataContainer>>("request_hops");

        REQUIRE(hops.size() == 8);
        REQUIRE(hops[5].get<std::string>("stage") == "accept-to-queue");
        REQUIRE(hops[6].get<std::string>("time") == "024");
    }
}

}  // namespace CthunAgent
