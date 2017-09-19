#include "../common/content_format.hpp"

#include <pxp-agent/action_response.hpp>

#include <cpp-pcp-client/protocol/chunks.hpp>

#include <leatherman/json_container/json_container.hpp>

#include <catch.hpp>

#include <vector>

namespace PXPAgent {

namespace lth_jc = leatherman::json_container;
using R_T = ActionResponse::ResponseType;

static const std::string DATA_TXT {
    (DATA_FORMAT % "\"04352987\""
                 % "\"module name\""
                 % "\"action name\""
                 % "{ \"some key\" : \"some value\" }").str() };

TEST_CASE("ActionResponse::ActionResponse", "[response]") {
    lth_jc::JsonContainer envelope { ENVELOPE_TXT };
    lth_jc::JsonContainer data { DATA_TXT };
    std::vector<lth_jc::JsonContainer> debug {};

    const PCPClient::ParsedChunks p_c { envelope, data, debug, 0 };
    auto req = ActionRequest(RequestType::Blocking, p_c);

    SECTION("successfully instantiates with valid arguments") {
        REQUIRE_NOTHROW(ActionResponse(ModuleType::Internal, req, "transaction_id"));
    }

    SECTION("successfully instantiates with valid metadata") {
        auto metadata = ActionResponse::getMetadataFromRequest(req);
        REQUIRE_NOTHROW(ActionResponse(ModuleType::External, RequestType::NonBlocking, {}, std::move(metadata)));
    }

    SECTION("throw a ActionResponse::Error if invalid metadata") {
        REQUIRE_THROWS_AS(ActionResponse(ModuleType::External, RequestType::NonBlocking, {}, {}),
                          ActionResponse::Error);
    }
}

TEST_CASE("ActionResponse::toJSON", "[response]") {
    lth_jc::JsonContainer envelope { ENVELOPE_TXT };
    lth_jc::JsonContainer data { DATA_TXT };
    std::vector<lth_jc::JsonContainer> debug {};

    const PCPClient::ParsedChunks p_c { envelope, data, debug, 0 };
    auto req = ActionRequest(RequestType::Blocking, p_c);

    SECTION("serializes results for blocking response") {
        auto results = lth_jc::JsonContainer{"{\"foo\": true}"};
        auto resp = ActionResponse(ModuleType::Internal, req);
        resp.setValidResultsAndEnd(std::move(results));
        REQUIRE(resp.toJSON(R_T::Blocking).toString() ==
                "{\"transaction_id\":\"04352987\",\"results\":{\"foo\":true}}");
    }

    SECTION("serializes results for nonblocking response") {
        auto results = lth_jc::JsonContainer{"{\"foo\": true}"};
        auto resp = ActionResponse(ModuleType::External, req);
        resp.setValidResultsAndEnd(std::move(results));
        REQUIRE(resp.toJSON(R_T::NonBlocking).toString() ==
                "{\"transaction_id\":\"04352987\",\"results\":{\"foo\":true}}");
    }

    SECTION("serializes errors in an rpc error") {
        auto resp = ActionResponse(ModuleType::External, req);
        resp.setBadResultsAndEnd("some failure");
        REQUIRE(resp.toJSON(R_T::RPCError).toString() ==
                "{\"transaction_id\":\"04352987\",\"id\":\"123456\",\"description\":\"some failure\"}");
    }

    SECTION("serializes output in a status response") {
        auto output = ActionOutput{0, "{\"foo\": true}", ""};
        auto metadata = ActionResponse::getMetadataFromRequest(req);
        auto resp = ActionResponse(ModuleType::External, RequestType::Blocking, output, std::move(metadata));

        auto results = lth_jc::JsonContainer{"{\"transaction_id\":\"123456\",\"status\":\"success\"}"};
        resp.setValidResultsAndEnd(std::move(results), "");

        REQUIRE(resp.toJSON(R_T::StatusOutput).toString() ==
                "{\"transaction_id\":\"04352987\",\"results\":{\"transaction_id\":\"\",\"exitcode\":0,\"status\":\"success\",\"stdout\":\"{\\\"foo\\\": true}\"}}");
    }

    SECTION("serializes errors if present in a status response") {
        auto output = ActionOutput{0, "{\"foo\": true}", ""};
        auto metadata = ActionResponse::getMetadataFromRequest(req);
        auto resp = ActionResponse(ModuleType::External, RequestType::Blocking, output, std::move(metadata));

        auto results = lth_jc::JsonContainer{"{\"transaction_id\":\"123456\",\"status\":\"failure\"}"};
        resp.setValidResultsAndEnd(std::move(results), "other");

        REQUIRE(resp.toJSON(R_T::StatusOutput).toString() ==
                "{\"transaction_id\":\"04352987\",\"results\":{\"transaction_id\":\"\",\"exitcode\":0,\"status\":\"failure\",\"stdout\":\"{\\\"_error\\\":{\\\"kind\\\":\\\"puppetlabs.pxp-agent/execution-error\\\",\\\"details\\\":{},\\\"msg\\\":\\\"other\\\"}}\"}}");
    }
}

}  // namespace PXPAgent
