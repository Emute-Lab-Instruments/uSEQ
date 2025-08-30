#pragma once

#include "../ports/ILogger.h"

// Transitional global logger bridge to avoid large call-site churn.
// If set, println()/report_error() will also forward to this logger.
void set_global_logger(ILogger* logger);
ILogger* get_global_logger();

