#include "serial_protocol.h"
#include "serial_protocol_json.h"
#include "build_info.h"
#include "i2c_network.h"
#include "../modulisp/lisp/symbol_intern.h"
#include "../signal_engine/executor.h"
#include "../utils/json_builder.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace firmware {

using serial_protocol_detail::extract_int;
using serial_protocol_detail::extract_string;

void SerialProtocol::handle_hello(const char* /*payload*/, size_t /*len*/)
{
    m_json_mode = true;

    // Build IO config — mirrors useq::protocol::build_hello_io_config_json
    JsonBuilder inputs_arr;
    inputs_arr.array_begin_unkeyed();
    for (int i = 0; i < num_serial_ins; ++i)
    {
        char name[16];
        snprintf(name, sizeof(name), "ssin%d", i + 1);
        inputs_arr.object_begin()
            .field("index", i + 1)
            .field("name", name)
            .object_end();
    }
    inputs_arr.array_end();

    JsonBuilder outputs_arr;
    outputs_arr.array_begin_unkeyed();
    for (int i = 0; i < num_serial_outs; ++i)
    {
        char name[16];
        if (i == 0)
            snprintf(name, sizeof(name), "time");
        else
            snprintf(name, sizeof(name), "s%d", i);

        outputs_arr.object_begin()
            .field("index", i + 1)
            .field("name", name)
            .object_end();
    }
    outputs_arr.array_end();

    JsonBuilder config;
    config.object_begin()
        .field_raw("inputs", inputs_arr.build())
        .field_raw("outputs", outputs_arr.build())
        .object_end();

    JsonBuilder response;
    response.object_begin()
        .field("type", "response")
        .field("success", true)
        .field("mode", "json")
        .field("fw", build_info::VERSION)
        .field("protocol", build_info::PROTOCOL_VERSION)
        .field("target", build_info::HARDWARE_TARGET)
        .field_raw("capabilities", build_info::CAPABILITIES_JSON)
#ifdef ENABLE_I2C_NETWORKING
        .field_raw("modules", build_modules_json())
#endif
        .field_raw("config", config.build())
        .field("requestId", m_request_id)
        .object_end();

    write_json_str(response.build().c_str());
}

#ifdef ENABLE_I2C_NETWORKING
String SerialProtocol::build_modules_json() const
{
    JsonBuilder modules;
    modules.array_begin_unkeyed();
    if (i2c_network != nullptr && i2c_network->host_mode)
    {
        for (uint8_t index = 0; index < i2c_network->expander_count; ++index)
        {
            const ExpanderDescriptor& descriptor = i2c_network->expanders[index];
            modules.object_begin()
                .field("kind", "output-expander")
                .field("address", static_cast<int>(descriptor.address))
                .field("identityStatus", descriptor.factory_identity_valid
                                             ? "verified"
                                             : "unidentified-prototype")
                .field("firmware", descriptor.firmware_version)
                .field("target", descriptor.firmware_target)
                .field("connectedVia", "i2c")
                .field("protocol", static_cast<int>(descriptor.protocol_version));
            if (descriptor.factory_identity_valid)
            {
                const FactoryIdentity& identity = descriptor.factory_identity;
                modules.field("product", identity.product)
                    .field("hardwareRevision", identity.hardware_revision)
                    .field("assemblyVariant", identity.assembly_variant)
                    .field("batch", identity.batch)
                    .field("serial", identity.serial)
                    .field("manufactured", identity.manufacture_date)
                    .field("mcu", mcu_family_name(identity.mcu_family))
                    .field("updateTransport",
                           (identity.feature_bits & factory_feature::USB_UPDATE) != 0
                               ? "usb"
                               : "i2c-only")
                    .field("autoUpdateSafe",
                           (identity.feature_bits & factory_feature::USB_UPDATE) != 0);
            }
            else
            {
                modules.field("product", "unidentified-prototype")
                    .field("updateTransport", "i2c-only")
                    .field("autoUpdateSafe", false);
            }
            modules.object_end();
        }
    }
    modules.array_end();
    return modules.build();
}

void SerialProtocol::handle_rescan_modules(const char* /*payload*/, size_t /*len*/)
{
    if (i2c_network != nullptr && i2c_network->host_mode)
        i2c_network->scan_for_expanders();

    JsonBuilder response;
    response.object_begin()
        .field("type", "response")
        .field("success", i2c_network != nullptr && i2c_network->host_mode)
        .field_raw("modules", build_modules_json())
        .field("requestId", m_request_id)
        .object_end();
    write_json_str(response.build().c_str());
}
#endif

void SerialProtocol::handle_ping(const char* /*payload*/, size_t /*len*/)
{
    JsonBuilder b;
    b.object_begin()
        .field("type", "response")
        .field("success", true)
        .field("console", "")
        .field("text", "")
        .field_null("meta")
        .field("requestId", m_request_id)
        .object_end();

    write_json_str(b.build().c_str());
}

void SerialProtocol::handle_stream_config(const char* payload, size_t len)
{
    // Parse maxRateHz (cap at 100Hz)
    int max_rate = extract_int(payload, len, "maxRateHz", 0);
    if (max_rate > 100) max_rate = 100;
    if (max_rate > 0)
    {
        stream_rate_limit_us =
            static_cast<unsigned long>(1000000UL / static_cast<unsigned long>(max_rate));
        if (stream_rate_limit_us == 0) stream_rate_limit_us = 1;
    }

    // Reset all channels, then re-populate from the config
    // Channel 0 (time) is always present
    for (size_t i = 0; i < MAX_STREAM_CHANNELS; ++i)
        stream_channels[i] = StreamChannel{};
    stream_channels[0].enabled = true;
    stream_channels[0].source = StreamSource::Time;
    num_stream_channels = 1;

    useq::json::ObjectCursor request(payload, len);
    useq::json::Value channels;
    if (request.find("channels", channels) &&
        channels.kind == useq::json::Kind::Array) {
        useq::json::ArrayCursor entries(channels);
        useq::json::Value entry;
        while (entries.next(entry) && num_stream_channels < MAX_STREAM_CHANNELS) {
            if (entry.kind != useq::json::Kind::Object) continue;
            useq::json::ObjectCursor channel(entry);
            useq::json::Value field;
            bool enabled = true;
            if (channel.find("enabled", field)) {
                useq::json::boolean(field, enabled);
            }
            if (!enabled) continue;

            char name[16] = {};
            char direction[8] = {};
            if (channel.find("name", field))
                useq::json::copy_string(field, name, sizeof(name));
            if (channel.find("direction", field))
                useq::json::copy_string(field, direction, sizeof(direction));
            if (strcmp(name, "time") == 0) continue;

            StreamChannel& stream = stream_channels[num_stream_channels];
            stream.enabled = true;
            if (strncmp(direction, "input", 5) == 0) {
                stream.source = StreamSource::Input;
                if (strcmp(name, "ssin1") == 0) stream.source_idx = 8;
                else if (strcmp(name, "ssin2") == 0) stream.source_idx = 9;
                stream.on_change_only = true;
            } else {
                stream.source = StreamSource::Output;
                if (name[0] == 's' && name[1] >= '1' && name[1] <= '8' &&
                    name[2] == '\0') {
                    stream.source_idx = 16 + (name[1] - '1');
                }
            }
            ++num_stream_channels;
        }
    }

    // Acknowledge
    JsonBuilder b;
    b.object_begin()
        .field("type", "response")
        .field("success", true)
        .field("console", "")
        .field("text", "")
        .field_null("meta")
        .field("requestId", m_request_id)
        .object_end();

    write_json_str(b.build().c_str());
}

void SerialProtocol::handle_set_failure_mode(const char* payload, size_t len)
{
    // §5.10: configure the runtime non-finite failure policy
    // (failure-model.md §3). `mode` is "lkg" (default — non-finite at an
    // output root falls back to the last-known-good value) or "zero"
    // (legacy — every non-finite node result clamps to 0.0).
    char mode_buf[8] = {};
    extract_string(payload, len, "mode", mode_buf, sizeof(mode_buf));

    bool ok = true;
    if (strcmp(mode_buf, "lkg") == 0)
        sig::set_failure_mode(sig::FailureMode::LkgFallback);
    else if (strcmp(mode_buf, "zero") == 0)
        sig::set_failure_mode(sig::FailureMode::ZeroSquash);
    else
        ok = false;

    JsonBuilder b;
    b.object_begin()
        .field("type", "response")
        .field("success", ok)
        .field("console", "")
        .field("text", ok ? "" : "set-failure-mode: mode must be \"lkg\" or \"zero\"")
        .field_null("meta")
        .field("requestId", m_request_id)
        .object_end();
    write_json_str(b.build().c_str());
}

void SerialProtocol::handle_set_live_inputs(const char* payload, size_t len)
{
    int applied = 0;
    useq::json::ObjectCursor request(payload, len);
    useq::json::Value slots_value;
    if (engine && request.find("slots", slots_value) &&
        slots_value.kind == useq::json::Kind::Object) {
        useq::json::ObjectCursor slots(slots_value);
        useq::json::Value key;
        useq::json::Value encoded;
        while (slots.next(key, encoded)) {
            char id[sig::MAX_LIVE_SLOT_ID] = {};
            bool id_truncated = false;
            if (!useq::json::copy_string(
                    key, id, sizeof(id), &id_truncated) || id_truncated) {
                continue;
            }
            const int16_t slot_index = engine->pool.find_live_slot(id);
            if (slot_index < 0) continue; // Unknown IDs are dropped per §5.8.

            double value = 0.0;
            bool value_valid = false;
            if (encoded.kind == useq::json::Kind::Number) {
                value_valid = useq::json::number(encoded, value) &&
                              std::isfinite(value);
            } else if (encoded.kind == useq::json::Kind::Boolean) {
                bool flag = false;
                value_valid = useq::json::boolean(encoded, flag);
                value = flag ? 1.0 : 0.0;
            } else if (encoded.kind == useq::json::Kind::String &&
                       engine->pool.live_slots[slot_index].variant ==
                           sig::NodePool::SlotVariant::Keyword) {
                char option[sig::MAX_LIVE_SLOT_OPTION_LEN] = {};
                if (useq::json::copy_string(encoded, option, sizeof(option))) {
                    const auto& slot = engine->pool.live_slots[slot_index];
                    for (uint8_t index = 0; index < slot.options_count; ++index) {
                        if (strncmp(slot.options[index], option,
                                    sig::MAX_LIVE_SLOT_OPTION_LEN) == 0) {
                            value = static_cast<double>(index);
                            value_valid = true;
                            break;
                        }
                    }
                }
            }
            if (value_valid) {
                engine->pool.set_live_slot_value_by_index(
                    static_cast<uint16_t>(slot_index), value);
                ++applied;
            }
        }
    }

    // If the request included a requestId, emit the ack response (§5.8).
    // If no requestId, this is fire-and-forget — no response emitted.
    if (m_request_id[0] != '\0')
    {
        JsonBuilder b;
        b.object_begin()
            .field("type", "response")
            .field("requestId", m_request_id)
            .field("success", true)
            .field("applied", applied)
            .object_end();
        write_json_str(b.build().c_str());
    }
}

// ── get-state handler (state-sync.md §2) ────────────────────────────────

static const char* output_name_for_index(uint16_t idx) {
    static char buf[4];
    char prefix;
    uint16_t num;
    if (idx < 8)       { prefix = 'a'; num = idx; }
    else if (idx < 16) { prefix = 'd'; num = idx - 8; }
    else if (idx < 24) { prefix = 's'; num = idx - 16; }
    else return nullptr;
    buf[0] = prefix;
    buf[1] = (char)('1' + num);
    buf[2] = '\0';
    return buf;
}

void SerialProtocol::handle_get_state(const char* /*payload*/, size_t /*len*/)
{
    if (!engine)
    {
        JsonBuilder b;
        b.object_begin()
            .field("requestId", m_request_id)
            .field("success", false)
            .field("text", "no engine attached")
            .object_end();
        write_json_str(b.build().c_str());
        return;
    }

    char numbuf[32];

    // Build the state sub-object first, then embed it in the response.
    JsonBuilder state;
    state.object_begin();

    // transport
    {
        JsonBuilder tr;
        tr.object_begin();
        tr.field("playing", engine->state.is_playing);
        snprintf(numbuf, sizeof(numbuf), "%.15g", engine->state.time_offset);
        tr.field_raw("timeOffset", String(numbuf));
        tr.object_end();
        state.field_raw("transport", tr.build());
    }

    // time (not stored in engine — caller provides via tick)
    state.field_raw("time", "0");

    // cells — only emit non-empty cells
    {
        JsonBuilder cells;
        cells.object_begin();
        for (size_t i = 0; i < sig::MAX_CELLS; i++) {
            const auto& cell = engine->cells.cells[i];
            if (cell.kind == sig::CellKind::Empty) continue;

            const String& name = getSymbolString((uint32_t)i);
            if (name.length() == 0) continue;

            JsonBuilder c;
            c.object_begin();
            switch (cell.kind) {
                case sig::CellKind::Number:
                    c.field("type", "number");
                    snprintf(numbuf, sizeof(numbuf), "%.15g", cell.value);
                    c.field_raw("value", String(numbuf));
                    break;
                case sig::CellKind::Data: {
                    c.field("type", "data");
                    uint16_t dlen = 0;
                    const double* dptr = engine->cells.get_data_table(cell.data_table_id, dlen);
                    String arr = "[";
                    for (uint16_t d = 0; d < dlen; d++) {
                        if (d > 0) arr += ",";
                        snprintf(numbuf, sizeof(numbuf), "%.15g", dptr[d]);
                        arr += numbuf;
                    }
                    arr += "]";
                    c.field_raw("values", arr);
                    break;
                }
                case sig::CellKind::Callable: {
                    c.field("type", "callable");
                    const auto& info = engine->cells.callables[i];
                    if (info.source_length > 0) {
                        const char* src = engine->arena.read(info.source_offset);
                        if (src) {
                            String src_str(src);
                            if (src_str.length() > info.source_length)
                                src_str = src_str.substring(0, info.source_length);
                            c.field("source", src_str.c_str());
                        }
                    }
                    break;
                }
                case sig::CellKind::Nil:
                    c.field("type", "nil");
                    break;
                default:
                    break;
            }
            c.object_end();
            cells.field_raw(name.c_str(), c.build());
        }
        cells.object_end();
        state.field_raw("cells", cells.build());
    }

    // outputs — only emit active outputs
    {
        JsonBuilder outs;
        outs.object_begin();
        for (uint16_t i = 0; i < sig::MAX_OUTPUTS; i++) {
            const auto& slot = engine->pool.outputs[i];
            if (slot.root_node == sig::NODE_NONE && !engine->output_sources[i].has_source) continue;
            const char* oname = output_name_for_index(i);
            if (!oname) continue;

            JsonBuilder o;
            o.object_begin();
            // source text
            if (engine->output_sources[i].has_source) {
                const char* src = engine->arena.read(engine->output_sources[i].arena_offset);
                if (src) {
                    String src_str(src);
                    uint32_t slen = engine->output_sources[i].arena_length;
                    if (src_str.length() > slen)
                        src_str = src_str.substring(0, slen);
                    o.field("source", src_str.c_str());
                }
            }
            const char* health = sig::output_health_to_cstr(
                sig::output_health(engine->pool, i));
            o.field("health", health);
            snprintf(numbuf, sizeof(numbuf), "%.15g", slot.lkg_value);
            o.field_raw("lkgValue", String(numbuf));
            o.object_end();
            outs.field_raw(oname, o.build());
        }
        outs.object_end();
        state.field_raw("outputs", outs.build());
    }

    // stateSlots
    {
        String arr = "[";
        for (uint16_t i = 0; i < engine->pool.state_slot_count; i++) {
            if (i > 0) arr += ",";
            JsonBuilder s;
            s.object_begin();
            snprintf(numbuf, sizeof(numbuf), "state_%u", (unsigned)i);
            s.field("id", numbuf);
            snprintf(numbuf, sizeof(numbuf), "%.15g", engine->pool.state_values[i]);
            s.field_raw("value", String(numbuf));
            s.object_end();
            arr += s.build();
        }
        arr += "]";
        state.field_raw("stateSlots", arr);
    }

    // liveSlots
    {
        String arr = "[";
        for (uint16_t i = 0; i < engine->pool.live_slot_count; i++) {
            if (i > 0) arr += ",";
            const auto& slot = engine->pool.live_slots[i];
            JsonBuilder l;
            l.object_begin();
            l.field("id", slot.id);
            snprintf(numbuf, sizeof(numbuf), "%.15g", slot.value);
            l.field_raw("value", String(numbuf));
            snprintf(numbuf, sizeof(numbuf), "%.15g", slot.min_val);
            l.field_raw("min", String(numbuf));
            snprintf(numbuf, sizeof(numbuf), "%.15g", slot.max_val);
            l.field_raw("max", String(numbuf));
            l.object_end();
            arr += l.build();
        }
        arr += "]";
        state.field_raw("liveSlots", arr);
    }

    state.object_end();

    JsonBuilder b;
    b.object_begin()
        .field("requestId", m_request_id)
        .field("success", true)
        .field("type", "state-snapshot")
        .field_raw("state", state.build())
        .object_end();

    write_json_str(b.build().c_str());
}

} // namespace firmware
