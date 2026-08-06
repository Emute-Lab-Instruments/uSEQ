#pragma once

#include "../utils/json_cursor.h"
#include "useq_protocol_schema.generated.h"

#include <cstdio>
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

inline bool field_kind_matches(const useq::json::Value& value,
                               useq::protocol::FieldKind kind)
{
    using useq::json::Kind;
    using useq::protocol::FieldKind;
    switch (kind) {
        case FieldKind::String: return value.kind == Kind::String;
        case FieldKind::Number: {
            double parsed = 0.0;
            return useq::json::number(value, parsed);
        }
        case FieldKind::Integer: {
            int parsed = 0;
            return useq::json::integer(value, parsed);
        }
        case FieldKind::Boolean: return value.kind == Kind::Boolean;
        case FieldKind::Object: return value.kind == Kind::Object;
        case FieldKind::Array: return value.kind == Kind::Array;
        case FieldKind::Scalar:
            return value.kind == Kind::String || value.kind == Kind::Number ||
                   value.kind == Kind::Boolean;
    }
    return false;
}

inline const char* field_kind_name(useq::protocol::FieldKind kind)
{
    using useq::protocol::FieldKind;
    switch (kind) {
        case FieldKind::String: return "string";
        case FieldKind::Number: return "number";
        case FieldKind::Integer: return "integer";
        case FieldKind::Boolean: return "boolean";
        case FieldKind::Object: return "object";
        case FieldKind::Array: return "array";
        case FieldKind::Scalar: return "scalar";
    }
    return "value";
}

inline bool validate_request(const char* json, size_t json_len,
                             const useq::protocol::RequestDefinition& request,
                             char* error, size_t error_size)
{
    useq::json::ObjectCursor object(json, json_len);
    if (!object.valid()) {
        std::snprintf(error, error_size, "request must be a JSON object");
        return false;
    }

    useq::json::Value value;
    if (request.request_id == useq::protocol::RequestIdPolicy::Required &&
        (!object.find("requestId", value) ||
         value.kind != useq::json::Kind::String)) {
        std::snprintf(error, error_size,
                      "%s requires string field 'requestId'", request.type);
        return false;
    }
    if (object.find("requestId", value) &&
        value.kind != useq::json::Kind::String) {
        std::snprintf(error, error_size,
                      "%s field 'requestId' must be string", request.type);
        return false;
    }

    for (size_t i = 0; i < request.field_count; ++i) {
        const auto& field = request.fields[i];
        if (!object.find(field.name, value)) {
            if (field.required) {
                std::snprintf(error, error_size, "%s requires field '%s'",
                              request.type, field.name);
                return false;
            }
            continue;
        }
        if (!field_kind_matches(value, field.kind)) {
            std::snprintf(error, error_size, "%s field '%s' must be %s",
                          request.type, field.name, field_kind_name(field.kind));
            return false;
        }
    }
    return true;
}

inline const useq::protocol::RequestDefinition* validate_protocol_request(
    const char* json, size_t json_len, char* error, size_t error_size)
{
    char type[32] = {};
    if (extract_string(json, json_len, "type", type, sizeof(type)) == 0) {
        std::snprintf(error, error_size, "missing string field 'type'");
        return nullptr;
    }
    const auto* request = useq::protocol::find_request(type);
    if (!request) {
        std::snprintf(error, error_size, "unknown request type '%s'", type);
        return nullptr;
    }
    return validate_request(json, json_len, *request, error, error_size)
        ? request
        : nullptr;
}

} // namespace firmware::serial_protocol_detail
