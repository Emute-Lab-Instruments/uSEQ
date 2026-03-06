#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "../uSEQ/src/uSEQ.h"

TEST_CASE("Non-eval JSON requests parse without code", "[hardware][json][parser]")
{
    SECTION("hello request")
    {
        auto request = useq::protocol::parse_json_request(
            "{\"type\":\"hello\",\"requestId\":\"req-hello\"}");

        REQUIRE(request.has_value());
        REQUIRE(request->type == "hello");
        REQUIRE(request->request_id == "req-hello");
        REQUIRE(request->code == "");
    }

    SECTION("ping request")
    {
        auto request = useq::protocol::parse_json_request(
            "{\"type\":\"ping\",\"requestId\":\"req-ping\"}");

        REQUIRE(request.has_value());
        REQUIRE(request->type == "ping");
        REQUIRE(request->request_id == "req-ping");
        REQUIRE(request->code == "");
    }

    SECTION("stream-config request")
    {
        auto request = useq::protocol::parse_json_request(
            "{\"type\":\"stream-config\",\"requestId\":\"req-stream\"}");

        REQUIRE(request.has_value());
        REQUIRE(request->type == "stream-config");
        REQUIRE(request->request_id == "req-stream");
        REQUIRE(request->code == "");
    }
}

TEST_CASE("Eval JSON requests still require code", "[hardware][json][parser]")
{
    SECTION("explicit eval without code is rejected")
    {
        auto request = useq::protocol::parse_json_request(
            "{\"type\":\"eval\",\"requestId\":\"req-eval\"}");

        REQUIRE_FALSE(request.has_value());
    }

    SECTION("missing type defaults to eval and still requires code")
    {
        auto request =
            useq::protocol::parse_json_request("{\"requestId\":\"req-default\"}");

        REQUIRE_FALSE(request.has_value());
    }

    SECTION("eval with code still parses")
    {
        auto request = useq::protocol::parse_json_request(
            "{\"type\":\"eval\",\"code\":\"(+ 1 2)\",\"requestId\":\"req-ok\"}");

        REQUIRE(request.has_value());
        REQUIRE(request->type == "eval");
        REQUIRE(request->code == "(+ 1 2)");
        REQUIRE(request->request_id == "req-ok");
    }
}
