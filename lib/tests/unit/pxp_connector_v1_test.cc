#include "../common/mock_connector.hpp"

#include <pxp-agent/pxp_connector_v1.hpp>

#include <catch.hpp>

using namespace PXPAgent;

TEST_CASE("PXPConnectorV1::PXPConnectorV1", "[agent]") {
    SECTION("successfully instantiates with valid arguments") {
        REQUIRE_NOTHROW(PXPConnectorV1{AGENT_CONFIGURATION});
    };
}
