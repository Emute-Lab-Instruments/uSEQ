#include "diagnostics.h"
#include "cell_store.h"
#include "../modulisp/lisp/symbol_intern.h"
#include <cstring>
#include <algorithm>

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

// ── Levenshtein distance (single-row DP, max symbol length 63) ──────────────

static int levenshtein(const char* s1, int len1, const char* s2, int len2) {
    if (len1 == 0) return len2;
    if (len2 == 0) return len1;
    constexpr int MAX_LEN = 64;
    if (len2 >= MAX_LEN) len2 = MAX_LEN - 1;
    int row[MAX_LEN];
    for (int i = 0; i <= len2; i++) row[i] = i;
    for (int i = 1; i <= len1; i++) {
        int prev = i - 1;
        row[0] = i;
        for (int j = 1; j <= len2; j++) {
            int temp = row[j];
            if (s1[i - 1] == s2[j - 1])
                row[j] = prev;
            else
                row[j] = 1 + std::min({prev, row[j], row[j - 1]});
            prev = temp;
        }
    }
    return row[len2];
}

// Well-known built-in symbol names that should be suggested even when
// they are not in the cell table (temporal phasors, operators, etc.)
static const char* const builtin_names[] = {
    "beat", "bar", "phrase", "section", "beat-num", "bar-num",
    "bpm", "beats-per-bar", "bars-per-phrase", "phrases-per-section",
    "sin", "cos", "tan", "abs", "floor", "ceil", "sqrt", "neg", "frac",
    "usin", "ucos", "bi-to-uni", "uni-to-bi",
    "min", "max", "pow", "expt", "mod", "pulse", "clamp", "lerp", "scale",
    "tri", "sqr", "step", "gates", "trigs", "euclid", "eu",
    "seq", "from-list", "interp", "flatseq", "dm", "range", "gatesw",
    "random", "index-rand", "loop-at", "rpulse", "rstep", "ridx", "rwarp",
    "fast", "slow", "offset", "shift",
    "if", "let", "do", "for", "while", "fn", "lambda",
    "not", "and", "or",
    "define", "def", "defn", "set", "input", "t",
    nullptr
};

SymbolID find_fuzzy_match(SymbolID unknown_sym, const CellStore& cells) {
    auto& si = SymbolIntern::getInstance();
    const String& name = si.getString(unknown_sym);
    if (name.length() == 0) return SymbolIntern::INVALID_ID;

    const char* name_cstr = name.c_str();
    int name_len = (int)name.length();

    SymbolID best = SymbolIntern::INVALID_ID;
    int best_dist = 3; // threshold: only suggest if distance < 3

    // Search defined cells
    for (uint16_t i = 1; i < MAX_CELLS; i++) {
        if (cells.cells[i].kind == CellKind::Empty) continue;
        const String& candidate = si.getString((SymbolID)i);
        if (candidate.length() == 0) continue;
        int d = levenshtein(name_cstr, name_len,
                            candidate.c_str(), (int)candidate.length());
        if (d > 0 && d < best_dist) {
            best_dist = d;
            best = (SymbolID)i;
        }
    }

    // Search well-known built-in names
    for (int k = 0; builtin_names[k] != nullptr; k++) {
        const char* cand = builtin_names[k];
        int cand_len = (int)strlen(cand);
        int d = levenshtein(name_cstr, name_len, cand, cand_len);
        if (d > 0 && d < best_dist) {
            best_dist = d;
            best = si.intern(cand);
        }
    }

    return best;
}

} // namespace sig
