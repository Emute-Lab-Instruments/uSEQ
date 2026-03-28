#ifndef SIGNAL_ENGINE_DIAGNOSTICS_H
#define SIGNAL_ENGINE_DIAGNOSTICS_H

#include "types.h"

namespace sig {

// ── Diagnostic Types ────────────────────────────────────────────────────────
// Compatible with the existing diagnostic.h types but using static strings
// (no heap allocation for messages).

enum class DiagnosticSeverity : uint8_t { Hint, Warning, Error };

enum class DiagnosticCategory : uint8_t {
    Syntax,        // parse errors
    UndefinedName, // unknown symbol
    Arity,         // wrong argument count
    Type,          // wrong argument type
    Boundary,      // side effect in signal context
    Arithmetic,    // division by zero, NaN
    Runtime,       // loop budget, recursion depth
    Overflow       // value out of range
};

struct Diagnostic {
    DiagnosticSeverity severity  = DiagnosticSeverity::Error;
    DiagnosticCategory category  = DiagnosticCategory::Runtime;
    uint16_t span_start          = 0;
    uint16_t span_len            = 0;
    const char* message          = nullptr; // static string literal
    const char* suggestion       = nullptr; // static string literal
};

const char* severity_to_cstr(DiagnosticSeverity s);
const char* category_to_cstr(DiagnosticCategory c);

// ── Fuzzy matching ──────────────────────────────────────────────────────────
// Returns the SymbolID of the closest match, or INVALID_ID if none is close enough.
SymbolID find_fuzzy_match(SymbolID unknown_sym);

} // namespace sig

#endif // SIGNAL_ENGINE_DIAGNOSTICS_H
