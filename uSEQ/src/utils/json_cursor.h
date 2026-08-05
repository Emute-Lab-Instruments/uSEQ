#pragma once

#include <cstddef>
#include <climits>
#include <cstdlib>
#include <cstring>

namespace useq::json {

enum class Kind : unsigned char {
    Invalid,
    String,
    Number,
    Object,
    Array,
    Boolean,
    Null,
};

struct Value {
    const char* data = nullptr;
    size_t size = 0;
    Kind kind = Kind::Invalid;

    explicit operator bool() const { return data != nullptr; }
};

namespace detail {

inline bool is_space(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

inline const char* skip_space(const char* cursor, const char* end) {
    while (cursor < end && is_space(*cursor)) ++cursor;
    return cursor;
}

inline const char* scan_string(const char* cursor, const char* end) {
    if (cursor >= end || *cursor != '"') return nullptr;
    ++cursor;
    bool escaped = false;
    while (cursor < end) {
        const char c = *cursor++;
        if (escaped) {
            escaped = false;
        } else if (c == '\\') {
            escaped = true;
        } else if (c == '"') {
            return cursor;
        } else if (static_cast<unsigned char>(c) < 0x20) {
            return nullptr;
        }
    }
    return nullptr;
}

inline const char* scan_compound(const char* cursor, const char* end) {
    const char opening = *cursor;
    const char closing = opening == '{' ? '}' : ']';
    size_t depth = 0;
    while (cursor < end) {
        if (*cursor == '"') {
            cursor = scan_string(cursor, end);
            if (!cursor) return nullptr;
            continue;
        }
        if (*cursor == opening) {
            ++depth;
        } else if (*cursor == closing) {
            if (--depth == 0) return cursor + 1;
        } else if (*cursor == '{' || *cursor == '[') {
            const char* nested = scan_compound(cursor, end);
            if (!nested) return nullptr;
            cursor = nested;
            continue;
        }
        ++cursor;
    }
    return nullptr;
}

inline bool scan_value(const char* cursor, const char* end,
                       Value& value, const char*& after) {
    cursor = skip_space(cursor, end);
    if (cursor >= end) return false;
    const char* value_end = nullptr;
    Kind kind = Kind::Invalid;
    if (*cursor == '"') {
        kind = Kind::String;
        value_end = scan_string(cursor, end);
    } else if (*cursor == '{') {
        kind = Kind::Object;
        value_end = scan_compound(cursor, end);
    } else if (*cursor == '[') {
        kind = Kind::Array;
        value_end = scan_compound(cursor, end);
    } else {
        value_end = cursor;
        while (value_end < end && !is_space(*value_end) &&
               *value_end != ',' && *value_end != '}' && *value_end != ']') {
            ++value_end;
        }
        if (value_end == cursor) return false;
        const size_t length = static_cast<size_t>(value_end - cursor);
        if (length == 4 && std::memcmp(cursor, "true", 4) == 0) {
            kind = Kind::Boolean;
        } else if (length == 5 && std::memcmp(cursor, "false", 5) == 0) {
            kind = Kind::Boolean;
        } else if (length == 4 && std::memcmp(cursor, "null", 4) == 0) {
            kind = Kind::Null;
        } else {
            kind = Kind::Number;
        }
    }
    if (!value_end) return false;
    value = {cursor, static_cast<size_t>(value_end - cursor), kind};
    after = value_end;
    return true;
}

} // namespace detail

inline bool copy_string(const Value& value, char* out, size_t capacity,
                        bool* truncated = nullptr) {
    if (truncated) *truncated = false;
    if (!out || capacity == 0 || value.kind != Kind::String || value.size < 2) {
        if (out && capacity) out[0] = '\0';
        return false;
    }
    const char* cursor = value.data + 1;
    const char* end = value.data + value.size - 1;
    size_t written = 0;
    bool overflow = false;
    while (cursor < end) {
        char c = *cursor++;
        if (c == '\\') {
            if (cursor >= end) return false;
            switch (*cursor++) {
                case '"': c = '"'; break;
                case '\\': c = '\\'; break;
                case '/': c = '/'; break;
                case 'b': c = '\b'; break;
                case 'f': c = '\f'; break;
                case 'n': c = '\n'; break;
                case 'r': c = '\r'; break;
                case 't': c = '\t'; break;
                default: return false; // Unicode escapes are not in the wire subset.
            }
        }
        if (written + 1 < capacity) out[written++] = c;
        else overflow = true;
    }
    out[written] = '\0';
    if (truncated) *truncated = overflow;
    return !overflow;
}

inline bool number(const Value& value, double& result) {
    if (value.kind != Kind::Number || value.size == 0 || value.size >= 64) {
        return false;
    }
    char buffer[64];
    std::memcpy(buffer, value.data, value.size);
    buffer[value.size] = '\0';
    char* after = nullptr;
    result = std::strtod(buffer, &after);
    return after == buffer + value.size;
}

inline bool integer(const Value& value, int& result) {
    double parsed = 0.0;
    if (!number(value, parsed)) return false;
    if (parsed != parsed) return false; // NaN
    if (parsed < static_cast<double>(INT_MIN) ||
        parsed > static_cast<double>(INT_MAX)) return false;
    const int converted = static_cast<int>(parsed);
    if (static_cast<double>(converted) != parsed) return false;
    result = converted;
    return true;
}

inline bool boolean(const Value& value, bool& result) {
    if (value.kind != Kind::Boolean) return false;
    result = value.size == 4;
    return true;
}

class ObjectCursor {
public:
    ObjectCursor(const char* json, size_t size)
        : begin_(json), cursor_(json), end_(json ? json + size : nullptr) {
        if (!json) return;
        cursor_ = detail::skip_space(cursor_, end_);
        if (cursor_ >= end_ || *cursor_ != '{') return;
        ++cursor_;
        valid_ = true;
    }

    explicit ObjectCursor(const Value& object)
        : ObjectCursor(object.data, object.size) {
        if (object.kind != Kind::Object) valid_ = false;
    }

    bool next(Value& key, Value& value) {
        if (!valid_ || finished_) return false;
        cursor_ = detail::skip_space(cursor_, end_);
        if (cursor_ < end_ && *cursor_ == '}') {
            finished_ = true;
            ++cursor_;
            return false;
        }
        const char* key_end = detail::scan_string(cursor_, end_);
        if (!key_end) return fail();
        key = {cursor_, static_cast<size_t>(key_end - cursor_), Kind::String};
        cursor_ = detail::skip_space(key_end, end_);
        if (cursor_ >= end_ || *cursor_ != ':') return fail();
        ++cursor_;
        const char* after = nullptr;
        if (!detail::scan_value(cursor_, end_, value, after)) return fail();
        cursor_ = detail::skip_space(after, end_);
        if (cursor_ < end_ && *cursor_ == ',') {
            ++cursor_;
        } else if (cursor_ >= end_ || *cursor_ != '}') {
            return fail();
        }
        return true;
    }

    bool valid() const { return valid_; }
    bool finished() const { return finished_; }

    bool find(const char* name, Value& value) {
        if (!name || !valid_ || !begin_ || !end_) return false;
        ObjectCursor scan(begin_, static_cast<size_t>(end_ - begin_));
        Value key;
        char decoded[80];
        while (scan.next(key, value)) {
            if (copy_string(key, decoded, sizeof(decoded)) &&
                std::strcmp(decoded, name) == 0) {
                return true;
            }
        }
        return false;
    }

private:
    bool fail() {
        valid_ = false;
        return false;
    }

    const char* begin_ = nullptr;
    const char* cursor_ = nullptr;
    const char* end_ = nullptr;
    bool valid_ = false;
    bool finished_ = false;
};

class ArrayCursor {
public:
    explicit ArrayCursor(const Value& array)
        : cursor_(array.data), end_(array.data ? array.data + array.size : nullptr) {
        if (array.kind != Kind::Array || !cursor_ || cursor_ >= end_ ||
            *cursor_ != '[') return;
        ++cursor_;
        valid_ = true;
    }

    bool next(Value& value) {
        if (!valid_ || finished_) return false;
        cursor_ = detail::skip_space(cursor_, end_);
        if (cursor_ < end_ && *cursor_ == ']') {
            finished_ = true;
            ++cursor_;
            return false;
        }
        const char* after = nullptr;
        if (!detail::scan_value(cursor_, end_, value, after)) return fail();
        cursor_ = detail::skip_space(after, end_);
        if (cursor_ < end_ && *cursor_ == ',') {
            ++cursor_;
        } else if (cursor_ >= end_ || *cursor_ != ']') {
            return fail();
        }
        return true;
    }

    bool valid() const { return valid_; }
    bool finished() const { return finished_; }

private:
    bool fail() {
        valid_ = false;
        return false;
    }
    const char* cursor_ = nullptr;
    const char* end_ = nullptr;
    bool valid_ = false;
    bool finished_ = false;
};

} // namespace useq::json
