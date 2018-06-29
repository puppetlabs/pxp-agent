#include "../common/mock_connector.hpp"

#include <pxp-agent/pxp_connector_v2.hpp>

#include <catch.hpp>

using namespace PXPAgent;

TEST_CASE("PXPConnectorV2::PXPConnectorV2", "[agent]") {
    SECTION("successfully instantiates with valid arguments") {
        REQUIRE_NOTHROW(PXPConnectorV2{AGENT_CONFIGURATION});
    };
}
