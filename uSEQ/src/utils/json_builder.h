#ifndef JSON_BUILDER_H_
#define JSON_BUILDER_H_

#include "string.h"
#include <cstdio>

/**
 * @brief Lightweight fluent JSON builder using the project's String type.
 *
 * Hand-rolled string concatenation behind a clean API so callers don't
 * embed raw JSON punctuation.  If we ever adopt ArduinoJson or similar,
 * only this file needs to change.
 *
 * Usage:
 *   String json = JsonBuilder()
 *       .object_begin()
 *           .field("success", true)
 *           .field("mode", String("json"))
 *           .field("count", 42)
 *           .field_null("meta")
 *           .array_begin("items")
 *               .object_begin()
 *                   .field("index", 1)
 *                   .field("name", String("ssin1"))
 *               .object_end()
 *           .array_end()
 *       .object_end()
 *       .build();
 */
class JsonBuilder
{
public:
    JsonBuilder() { m_buf.reserve(128); }

    // ---- structure ----

    JsonBuilder& object_begin()
    {
        maybe_comma();
        m_buf += '{';
        push_scope();
        return *this;
    }

    JsonBuilder& object_end()
    {
        pop_scope();
        m_buf += '}';
        return *this;
    }

    JsonBuilder& array_begin(const String& key)
    {
        maybe_comma();
        append_key(key);
        m_buf += '[';
        push_scope();
        return *this;
    }

    JsonBuilder& array_begin_unkeyed()
    {
        maybe_comma();
        m_buf += '[';
        push_scope();
        return *this;
    }

    JsonBuilder& array_end()
    {
        pop_scope();
        m_buf += ']';
        return *this;
    }

    // ---- keyed fields ----

    JsonBuilder& field(const String& key, const String& value)
    {
        maybe_comma();
        append_key(key);
        m_buf += '"';
        append_escaped(value);
        m_buf += '"';
        return *this;
    }

    JsonBuilder& field(const String& key, const char* value)
    {
        return field(key, String(value));
    }

    JsonBuilder& field(const String& key, bool value)
    {
        maybe_comma();
        append_key(key);
        m_buf += value ? "true" : "false";
        return *this;
    }

    JsonBuilder& field(const String& key, int value)
    {
        maybe_comma();
        append_key(key);
        char numbuf[16];
        snprintf(numbuf, sizeof(numbuf), "%d", value);
        m_buf += numbuf;
        return *this;
    }

    JsonBuilder& field_null(const String& key)
    {
        maybe_comma();
        append_key(key);
        m_buf += "null";
        return *this;
    }

    /** Insert a pre-built JSON fragment as the value for @p key. */
    JsonBuilder& field_raw(const String& key, const String& raw_json)
    {
        maybe_comma();
        append_key(key);
        m_buf += raw_json;
        return *this;
    }

    // ---- terminal ----

    String build() const { return m_buf; }

private:
    static constexpr int MAX_DEPTH = 8;

    String m_buf;
    bool m_need_comma[MAX_DEPTH] = {};
    int m_depth                  = 0;

    void push_scope()
    {
        if (m_depth < MAX_DEPTH)
        {
            m_need_comma[m_depth] = false;
            ++m_depth;
        }
    }

    void pop_scope()
    {
        if (m_depth > 0)
        {
            --m_depth;
        }
        // After closing a scope, the parent scope needs a comma before the next element
        if (m_depth > 0)
        {
            m_need_comma[m_depth - 1] = true;
        }
    }

    void maybe_comma()
    {
        if (m_depth > 0 && m_need_comma[m_depth - 1])
        {
            m_buf += ',';
        }
        if (m_depth > 0)
        {
            m_need_comma[m_depth - 1] = true;
        }
    }

    void append_key(const String& key)
    {
        m_buf += '"';
        append_escaped(key);
        m_buf += "\":";
    }

    void append_escaped(const String& s)
    {
        for (unsigned int i = 0; i < s.length(); ++i)
        {
            char c = s[i];
            switch (c)
            {
            case '"':
                m_buf += "\\\"";
                break;
            case '\\':
                m_buf += "\\\\";
                break;
            case '\n':
                m_buf += "\\n";
                break;
            case '\r':
                m_buf += "\\r";
                break;
            case '\t':
                m_buf += "\\t";
                break;
            default:
                if (static_cast<unsigned char>(c) < 0x20)
                {
                    char ubuf[7];
                    snprintf(ubuf, sizeof(ubuf), "\\u%04X",
                             static_cast<unsigned char>(c));
                    m_buf += ubuf;
                }
                else
                {
                    m_buf += c;
                }
                break;
            }
        }
    }
};

#endif // JSON_BUILDER_H_
