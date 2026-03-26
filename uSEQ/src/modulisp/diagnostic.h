// Diagnostic data model for parser and compiler error reporting.
// See docs/ERROR_HANDLING_SPEC.md §2.1 (SourceSpan) and §2.2 (Diagnostic).
#pragma once

#include "../utils/string.h"
#include <cstdint>

struct SourceSpan {
    uint16_t start = 0; // character offset from start of source text
    uint16_t end = 0;   // exclusive end offset
    // {0,0} means "no span information"
};

static_assert(sizeof(SourceSpan) == 4, "SourceSpan must be 4 bytes");

enum class DiagnosticSeverity : uint8_t {
    Hint,    // style suggestion, not an error
    Warning, // something suspicious but not broken
    Error    // expression will not work
};

enum class DiagnosticCategory : uint8_t {
    Syntax,        // parse errors: unmatched parens, bad tokens
    UndefinedName, // unknown function or variable
    Arity,         // wrong number of arguments
    Type,          // wrong type of argument
    Boundary,      // side-effect in signal context, eval in signal
    Arithmetic,    // division by zero, non-finite result
    Runtime,       // loop budget, call depth, intrinsic failure
    Overflow       // value out of expected range
};

struct Diagnostic {
    DiagnosticSeverity severity = DiagnosticSeverity::Error;
    DiagnosticCategory category = DiagnosticCategory::Runtime;
    SourceSpan span;
    String message;
    String suggestion;   // may be empty
    String example;      // may be empty
    String triggered_by; // symbol name for dependency-triggered errors, empty otherwise
};

inline const char* severity_to_cstr(DiagnosticSeverity s) {
    switch (s) {
    case DiagnosticSeverity::Hint:    return "hint";
    case DiagnosticSeverity::Warning: return "warning";
    case DiagnosticSeverity::Error:   return "error";
    }
    return "error";
}

inline const char* category_to_cstr(DiagnosticCategory c) {
    switch (c) {
    case DiagnosticCategory::Syntax:        return "syntax";
    case DiagnosticCategory::UndefinedName: return "undefinedName";
    case DiagnosticCategory::Arity:         return "arity";
    case DiagnosticCategory::Type:          return "type";
    case DiagnosticCategory::Boundary:      return "boundary";
    case DiagnosticCategory::Arithmetic:    return "arithmetic";
    case DiagnosticCategory::Runtime:       return "runtime";
    case DiagnosticCategory::Overflow:      return "overflow";
    }
    return "runtime";
}
