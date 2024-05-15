#ifndef LOG_H_
#define LOG_H_

#include "string.h"

void print(String s);
void println(String s);

void debug(String s);

void error(String s);
// void error_num_args_incorrect(String function_name, int num_expected,
//                               int num_received);

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
