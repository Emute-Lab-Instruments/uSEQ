#ifndef SIGNAL_ENGINE_DIAGNOSTICS_H
#define SIGNAL_ENGINE_DIAGNOSTICS_H

#include "types.h"

namespace sig {

// Forward declaration
struct CellStore;

// ── Diagnostic Types ────────────────────────────────────────────────────────
// Compatible with the existing diagnostic.h types but using borrowed,
// static-lifetime strings (no heap allocation for messages). Producers MUST
// NOT pass stack buffers or temporary String storage: Diagnostic is copied by
// value across builder/eval/active-health boundaries without copying text.

enum class DiagnosticSeverity : uint8_t { Warning, Error };

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
    const char* message          = nullptr; // borrowed static-lifetime text
    const char* suggestion       = nullptr; // borrowed static-lifetime text
};

const char* severity_to_cstr(DiagnosticSeverity s);
const char* category_to_cstr(DiagnosticCategory c);

// ── Fuzzy matching ──────────────────────────────────────────────────────────
// Returns the SymbolID of the closest match, or INVALID_ID if none is close enough.
// Searches defined cells and well-known built-in symbols.
SymbolID find_fuzzy_match(SymbolID unknown_sym, const CellStore& cells);

} // namespace sig

#endif // SIGNAL_ENGINE_DIAGNOSTICS_H
