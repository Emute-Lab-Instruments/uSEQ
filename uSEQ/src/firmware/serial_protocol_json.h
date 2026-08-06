#pragma once

#include "../utils/json_cursor.h"

#include <cstring>

namespace firmware::serial_protocol_detail {

inline size_t extract_string(const char* json, size_t json_len,
                             const char* field_name,
                             char* dst, size_t dst_size,
                             bool* truncated = nullptr)
{
    useq::json::ObjectCursor object(json, json_len);
    useq::json::Value value;
    if (!object.find(field_name, value) ||
        !useq::json::copy_string(value, dst, dst_size, truncated)) {
        return 0;
    }
    return strlen(dst);
}

inline int extract_int(const char* json, size_t json_len,
                       const char* field_name, int default_value = 0)
{
    useq::json::ObjectCursor object(json, json_len);
    useq::json::Value value;
    int parsed = default_value;
    return object.find(field_name, value) && useq::json::integer(value, parsed)
        ? parsed
        : default_value;
}

} // namespace firmware::serial_protocol_detail
