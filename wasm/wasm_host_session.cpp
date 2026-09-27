#include "wasm_wrapper_internal.h"
#include "../uSEQ/src/signal_engine/output_names.h"

sig::SignalEngine* g_engine = nullptr;
double g_hw_inputs[32] = {};
double g_current_time = 0.0;
double g_prev_tick_time = 0.0;
double g_authoritative_wall_frontier = 0.0;
bool g_has_authoritative_wall_frontier = false;
sig::Diagnostic g_last_diagnostics[16] = {};
uint8_t g_last_diagnostic_count = 0;
String s_last_error;
uint32_t g_cell_revision = 0;

void reset_host_session_state()
{
    reset_probe_session();
    reset_projection_session();
    g_current_time = 0.0;
    g_prev_tick_time = 0.0;
    g_authoritative_wall_frontier = 0.0;
    g_has_authoritative_wall_frontier = false;
    g_cell_revision++;
}

char* alloc_cstr(const char* text)
{
    size_t length = strlen(text);
    char* buffer = static_cast<char*>(malloc(length + 1));
    memcpy(buffer, text, length + 1);
    return buffer;
}

char* alloc_string(const String& text)
{
    char* buffer = static_cast<char*>(malloc(text.length() + 1));
    memcpy(buffer, text.c_str(), text.length() + 1);
    return buffer;
}

bool parse_output_names(const char* json, std::vector<String>& outputs)
{
    if (!json) return false;
    useq::json::Value array{json, strlen(json), useq::json::Kind::Array};
    useq::json::ArrayCursor entries(array);
    useq::json::Value entry;
    char name[16];
    while (entries.next(entry)) {
        if (!useq::json::copy_string(entry, name, sizeof(name))) return false;
        outputs.push_back(String(name));
    }
    return entries.valid() && entries.finished();
}

uint16_t resolve_output_name(const char* name)
{
    return sig::output_name_to_index(name);
}

bool format_output_name(uint16_t index, char (&name)[3])
{
    name[0] = name[1] = name[2] = 0;
    if (index < 8) {
        name[0] = 'a';
        name[1] = static_cast<char>('1' + index);
    } else if (index < 16) {
        name[0] = 'd';
        name[1] = static_cast<char>('1' + index - 8);
    } else if (index < 24) {
        name[0] = 's';
        name[1] = static_cast<char>('1' + index - 16);
    } else {
        return false;
    }
    return true;
}
