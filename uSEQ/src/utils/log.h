#ifndef LOG_H_
#define LOG_H_

#include "serial_message.h"
#include "string.h"
#include <optional>
#include <vector>

extern std::vector<String> error_msg_q;

void message_editor(const String& s);
void println(const String& s);

namespace Protocol
{
void enable_json_mode();
void disable_json_mode();
bool json_mode_enabled();

void begin_request(const String& request_id);
void finish_request();
bool request_active();
void append_request_text(const String& line);
String consume_request_text();

void send_json_response(bool success, const String& text,
                        const std::optional<String>& meta,
                        const String& request_id);
void send_json_error(const String& request_id, const String& message);
void send_raw_json(const String& payload);
} // namespace Protocol

void debug(String s);

void report_error(const String& s);
void report_generic_error(const String& s);
void report_runtime_error(const String& s);
void report_user_warning(const String& s);

enum class UserError
{
    WrongNumArgs,
    SpecificArgType,
    AllArgsType
};

enum class NumArgsComparison
{
    EqualTo,
    AtLeast,
    AtMost,
    Between
};

void report_error_wrong_num_args(const String& function_name, int num_received,
                                 NumArgsComparison comp, int num, int num2);

void report_error_arg_is_error(const String& function_name, int num,
                               const String& received_val_str);

void report_error_wrong_all_pred(const String& function_name, int num,
                                 const String& expected_str,
                                 const String& received_val_str);

void report_error_wrong_specific_pred(const String& function_name, int num,
                                      const String& expected_str,
                                      const String& received_val_str);

void report_error_atom_not_defined(const String& atom);

void report_custom_function_error(const String& function_name, const String& msg);

int free_heap();

#if USEQ_DEBUG

#include <unordered_set>

class DebugLogger
{
public:
    DebugLogger(String name);
    ~DebugLogger();

    // These are the same, log is more explicit
    void log(const String& message);
    void operator()(const String& message);

    static bool print_free_heap;
    static std::unordered_set<String> mutes;
    static std::unordered_set<String> solos;

private:
    String m_name;
    void dbg_print(const String& s);

    int start_heap = 0;
    String addIndentAfterNewlines(const String& input);

    static int m_level;
    static String m_spaces;
    static void update_spaces();
    static void inc_level();
    static void dec_level();
};

#endif // USEQ_DEBUG

#endif // LOG_H_
