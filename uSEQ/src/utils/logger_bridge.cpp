#include "logger_bridge.h"

namespace
{
ILogger* g_logger = nullptr;
}

void set_global_logger(ILogger* logger) { g_logger = logger; }
ILogger* get_global_logger() { return g_logger; }
