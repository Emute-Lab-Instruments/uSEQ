#ifndef LOG_H_
#define LOG_H_

#include "string.h"
#include "serial_message.h"



// void print(String s);
void println(String s);

void debug(String s);

void error(String s);
void runtime_error(String s);

void user_warning(const String& s);

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

void error_wrong_num_args(const String& function_name, int num_received,
                          NumArgsComparison comp, int num, int num2);

void error_arg_is_error(const String& function_name, int num,
                        const String& received_val_str);

void error_wrong_all_pred(const String& function_name, int num,
                          const String& expected_str,
                          const String& received_val_str);

void error_wrong_specific_pred(const String& function_name, int num,
                               const String& expected_str,
                               const String& received_val_str);

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
