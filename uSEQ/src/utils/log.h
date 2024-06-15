#ifndef LOG_H_
#define LOG_H_

#include "serial_message.h"
#include "string.h"
#include <optional>
#include <vector>

// enum class PrintJobType
// {
//     PRINT,
//     PRINTLN
// };

// struct ErrorMsg
// {
//     String msg;
//     PrintJobType type;
//     String relevant_atom;
// };

// struct uSEQ_Message
// {
//     enum class Type
//     {
//         USER_INFO,
//         USER_WARNING,
//         USER_ERROR,
//         RUNTIME_INFO,
//         RUNTIME_WARNING,
//         RUNTIME_ERROR,
//     };

//     Type type;
//     String str;

//     // Constructor accepting an lvalue reference
//     uSEQ_Message(Type type, const String& s) : type(type), str(s) {}

//     // Constructor accepting an rvalue reference
//     uSEQ_Message(Type type, String&& s) : type(type), str(std::move(s)) {}
// };

// class uSEQ_MessageQ
// {
// public:
//     void push(uSEQ_Message&& message) { messages.push_back(std::move(message)); }

//     void clear() { messages.clear(); }
//     const uSEQ_Message& first() { return messages[0]; }
//     const uSEQ_Message& last() { return messages[messages.size() - 1]; }

// private:
//     std::vector<uSEQ_Message> messages;
// };

extern std::vector<String> error_msg_q;

// void print_now(const String& s);
// void println_now(const String& s);

void message_editor(const String& s);

void print(const String& s);
void println(const String& s);

// void serve_message(const uSEQ_Message& msg);

// void execute_print_q_index(size_t index);

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
// NOTE: for later
// struct NumArgs
// {
//     enum class Type
//     {
//         EqualTo,
//         AtLeast,
//         AtMost,
//         Between
//     };

//     Type comp;
//     int num;
//     int num2;

//     NumArgs(Type type, int n, int n2 = 0) : comp(type), num(n), num2(n2) {}
// };

// struct ArgTypeRequirements
// {
//     enum class Type
//     {
//         AllArgs,
//         SpecificArg,
//         ArgRange
//     };

//     Type which_arg;

// };

// void error_wrong_arg_type(const String& function_name, const NumArgs&
// requirements,
//                           int num_received);

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
