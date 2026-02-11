#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "../uSEQ/src/ports/mocks/MockClock.h"
#include "../uSEQ/src/ports/mocks/MockIo.h"
#include "../uSEQ/src/uSEQ.h"

#include "../uSEQ/src/utils/log.h"

TEST_CASE("JSON protocol requests are decoded and routed safely",
          "[routing][protocol][json]")
{
    MockClock clock;
    MockIo io;
    uSEQ device(&clock, nullptr, &io);
    device.init();

    // Keep expectations deterministic across sections.
    device.__test_get_defs().clear();
    device.__test_get_def_exprs().clear();
    Protocol::disable_json_mode();
    error_msg_q.clear();

    SECTION("Valid eval request executes code and leaves protocol idle")
    {
        const String payload =
            "{\"type\":\"eval\",\"code\":\"(define json_route_var 37)\","
            "\"requestId\":\"req-1\"}";

        REQUIRE(device.__test_handle_json_serial_request(payload));
        REQUIRE_FALSE(Protocol::request_active());
        REQUIRE(error_msg_q.empty());

        auto it = device.__test_get_defs().find(String("json_route_var"));
        REQUIRE(it != device.__test_get_defs().end());
        REQUIRE(it->second.is_int());
        REQUIRE(it->second.as_int() == 37);
    }

    SECTION("Type defaults to eval when omitted and escaped JSON string decodes")
    {
        const String payload =
            "{\"code\":\"(define json_route_text \\\"line\\\\nnext\\\")\","
            "\"requestId\":\"req-2\"}";

        REQUIRE(device.__test_handle_json_serial_request(payload));
        REQUIRE_FALSE(Protocol::request_active());
        REQUIRE(error_msg_q.empty());

        auto it = device.__test_get_defs().find(String("json_route_text"));
        REQUIRE(it != device.__test_get_defs().end());
        REQUIRE(it->second.is_string());
        REQUIRE(it->second.as_string() == String("line\nnext"));
    }

    SECTION("Malformed request (missing code) does not mutate environment")
    {
        const size_t before_defs = device.__test_get_defs().size();
        REQUIRE(device.__test_handle_json_serial_request("{\"type\":\"eval\"}"));
        REQUIRE_FALSE(Protocol::request_active());
        REQUIRE(device.__test_get_defs().size() == before_defs);
    }

    SECTION("Unsupported request type is rejected before evaluation")
    {
        const String payload =
            "{\"type\":\"ping\",\"code\":\"(define should_not_run 1)\","
            "\"requestId\":\"req-3\"}";

        REQUIRE(device.__test_handle_json_serial_request(payload));
        REQUIRE_FALSE(Protocol::request_active());
        REQUIRE(device.__test_get_defs().find(String("should_not_run")) ==
                device.__test_get_defs().end());
    }
}
