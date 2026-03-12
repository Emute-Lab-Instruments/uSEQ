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

TEST_CASE("Hello config advertises serial inputs and outputs", "[hardware][json][contract]")
{
    const String config_json =
        useq::protocol::build_hello_io_config_json(4, 3);

    REQUIRE(config_json.indexOf("\"inputs\"") >= 0);
    REQUIRE(config_json.indexOf("\"outputs\"") >= 0);
    REQUIRE(config_json.indexOf("\"name\":\"ssin1\"") >= 0);
    REQUIRE(config_json.indexOf("\"name\":\"ssin4\"") >= 0);
    REQUIRE(config_json.indexOf("\"name\":\"time\"") >= 0);
    REQUIRE(config_json.indexOf("\"name\":\"s1\"") >= 0);
    REQUIRE(config_json.indexOf("\"name\":\"s2\"") >= 0);
}

TEST_CASE("Stream-config captures output enablement and cadence",
          "[hardware][json][contract]")
{
    const auto request = useq::protocol::parse_stream_config_request(
        "{\"type\":\"stream-config\",\"maxRateHz\":30,"
        "\"channels\":["
        "{\"id\":1,\"name\":\"ssin1\",\"direction\":\"input\",\"enabled\":true,\"maxRateHz\":30},"
        "{\"id\":7,\"name\":\"time\",\"direction\":\"output\",\"enabled\":false,\"maxRateHz\":30},"
        "{\"id\":8,\"name\":\"s1\",\"direction\":\"output\",\"enabled\":true,\"maxRateHz\":15}"
        "],\"requestId\":\"req-stream\"}");

    REQUIRE(request.has_value());
    REQUIRE(request->max_rate_hz == 30);
    REQUIRE(request->channels.size() == 3);
    REQUIRE(request->channels[0].direction == "input");
    REQUIRE(request->channels[1].name == "time");
    REQUIRE(request->channels[1].direction == "output");
    REQUIRE(request->channels[1].enabled == false);
    REQUIRE(request->channels[2].name == "s1");
    REQUIRE(request->channels[2].max_rate_hz == 15);
}

TEST_CASE("uSEQ applies output stream-config to time and serial slots",
          "[hardware][json][contract]")
{
    uSEQ device;

    REQUIRE(device.__test_apply_stream_config(
        "{\"type\":\"stream-config\",\"maxRateHz\":40,"
        "\"channels\":["
        "{\"id\":1,\"name\":\"time\",\"direction\":\"output\",\"enabled\":false,\"maxRateHz\":40},"
        "{\"id\":2,\"name\":\"s1\",\"direction\":\"output\",\"enabled\":true,\"maxRateHz\":40}"
        "],\"requestId\":\"req-stream\"}"));

    REQUIRE(device.__test_is_serial_output_enabled(1) == false);
    REQUIRE(device.__test_is_serial_output_enabled(2) == true);
    REQUIRE(device.__test_is_serial_output_enabled(3) == false);
    REQUIRE(device.__test_get_serial_output_rate_limit_micros() == 25000);
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
