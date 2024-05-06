#ifndef LOG_H_
#define LOG_H_

#include "string.h"

void print(String s);
void println(String s);

void debug(String s);
void error(String s);

int free_heap();

#if USEQ_DEBUG

class DEBUGGER
{
public:
    DEBUGGER(String name);
    ~DEBUGGER();
    void operator()(const String& message);
    void log(const String& message);

private:
    String name;
    void pr(const String& s);

    int start_heap = 0;
    String addIndentAfterNewline(const String& input);

    static int m_level;
    static String m_spaces;
    static void update_spaces();
    static void inc_level();
    static void dec_level();
    static bool print_free_heap;
};

#endif // USEQ_DEBUG

#endif // LOG_H_
