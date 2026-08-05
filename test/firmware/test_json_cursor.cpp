#define CATCH_CONFIG_MAIN
#include "../catch.hpp"

#include "utils/json_cursor.h"

#include <cmath>
#include <cstring>

using namespace useq::json;

TEST_CASE("JSON cursor finds bounded top-level fields") {
    const char json[] = R"({"nested":{"name":"wrong"},"name":"right","n":-12})";
    ObjectCursor object(json, sizeof(json) - 1);
    Value value;
    char text[16];
    REQUIRE(object.find("name", value));
    REQUIRE(copy_string(value, text, sizeof(text)));
    REQUIRE(std::strcmp(text, "right") == 0);
    REQUIRE(object.find("n", value));
    int number_value = 0;
    REQUIRE(integer(value, number_value));
    REQUIRE(number_value == -12);
}

TEST_CASE("JSON cursor iterates arrays and object values without allocation") {
    const char json[] = R"({"items":[{"enabled":true},{"enabled":false}]})";
    ObjectCursor root(json, sizeof(json) - 1);
    Value items;
    REQUIRE(root.find("items", items));
    ArrayCursor array(items);
    Value item;
    int count = 0;
    while (array.next(item)) {
        ObjectCursor entry(item);
        Value enabled;
        bool flag = false;
        REQUIRE(entry.find("enabled", enabled));
        REQUIRE(boolean(enabled, flag));
        REQUIRE(flag == (count == 0));
        ++count;
    }
    REQUIRE(array.valid());
    REQUIRE(array.finished());
    REQUIRE(count == 2);
}

TEST_CASE("JSON cursor rejects malformed and truncated input") {
    const char malformed[] = R"({"x":"unterminated})";
    ObjectCursor object(malformed, sizeof(malformed) - 1);
    Value key;
    Value value;
    REQUIRE_FALSE(object.next(key, value));
    REQUIRE_FALSE(object.valid());

    const char valid[] = R"({"long":"abcdef"})";
    ObjectCursor valid_object(valid, sizeof(valid) - 1);
    REQUIRE(valid_object.find("long", value));
    char tiny[4];
    bool truncated = false;
    REQUIRE_FALSE(copy_string(value, tiny, sizeof(tiny), &truncated));
    REQUIRE(truncated);
    REQUIRE(std::strcmp(tiny, "abc") == 0);
}

TEST_CASE("JSON cursor number parser consumes the whole token") {
    const char json[] = R"({"ok":1.25e2,"bad":1x,"nan":NaN})";
    ObjectCursor object(json, sizeof(json) - 1);
    Value value;
    double parsed = 0.0;
    REQUIRE(object.find("ok", value));
    REQUIRE(number(value, parsed));
    REQUIRE(parsed == 125.0);
    REQUIRE(object.find("bad", value));
    REQUIRE_FALSE(number(value, parsed));
    REQUIRE(object.find("nan", value));
    REQUIRE(number(value, parsed));
    REQUIRE(std::isnan(parsed));
}

TEST_CASE("JSON cursor remains bounded across malformed byte fuzz") {
    unsigned int state = 0xC0FFEEu;
    char input[128];
    for (int iteration = 0; iteration < 2000; ++iteration) {
        state = state * 1664525u + 1013904223u;
        const size_t length = 1 + (state % (sizeof(input) - 1));
        for (size_t index = 0; index < length; ++index) {
            state = state * 1664525u + 1013904223u;
            input[index] = static_cast<char>(0x20 + (state % 0x5f));
        }
        input[0] = '{';
        ObjectCursor object(input, length);
        Value key;
        Value value;
        size_t entries = 0;
        while (object.next(key, value)) {
            REQUIRE(++entries <= length / 2);
        }
    }
}
