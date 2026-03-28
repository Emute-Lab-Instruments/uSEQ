#include "diagnostics.h"
#include "../modulisp/lisp/symbol_intern.h"
#include <cstring>

namespace sig {

const char* severity_to_cstr(DiagnosticSeverity s) {
    switch (s) {
        case DiagnosticSeverity::Hint:    return "hint";
        case DiagnosticSeverity::Warning: return "warning";
        case DiagnosticSeverity::Error:   return "error";
    }
    return "error";
}

const char* category_to_cstr(DiagnosticCategory c) {
    switch (c) {
        case DiagnosticCategory::Syntax:        return "syntax";
        case DiagnosticCategory::UndefinedName:  return "undefinedName";
        case DiagnosticCategory::Arity:          return "arity";
        case DiagnosticCategory::Type:           return "type";
        case DiagnosticCategory::Boundary:       return "boundary";
        case DiagnosticCategory::Arithmetic:     return "arithmetic";
        case DiagnosticCategory::Runtime:        return "runtime";
        case DiagnosticCategory::Overflow:       return "overflow";
    }
    return "runtime";
}

SymbolID find_fuzzy_match(SymbolID /* unknown_sym */) {
    // TODO: implement Levenshtein distance fuzzy matching
    return SymbolIntern::INVALID_ID;
}

} // namespace sig
