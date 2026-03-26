#ifndef PARSE_H_
#define PARSE_H_

#include "value.h"
#include "../diagnostic.h"
#include <vector>

// a -> symbol
// foo -> symbol
// 1+ -> symbol
// 1 -> number
// M1 -> MIDINOTE
// C4 -> MIDINOTE
// V4 -> VOLTAGE

////////////////////////////////////////////////////////////////////////////////
/// HELPER FUNCTIONS ///////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

class uLispParser
{
public:
    // Constructor that accepts a diagnostics vector for dependency injection
    explicit uLispParser(std::vector<Diagnostic>* diags) : m_diagnostics(diags) {}

    // Default constructor for compatibility (uses nullptr diagnostics)
    uLispParser() : m_diagnostics(nullptr) {}

    // Instance methods (no longer static)
    const String unescape(const String str) const;
    void skip_whitespace(const String& s, int& ptr) const;

    // Type checking methods
    bool is_symbol(const String&, int) const;
    bool is_comment(const String&, int) const;
    bool is_quote(const String&, int) const;
    bool is_list(const String&, int) const;
    bool is_vector(const String&, int) const;
    bool is_map(const String&, int) const;
    bool is_midinote(const String&, int) const; // M
    bool is_freq(const String&, int) const;     // Hz
    // e.g. 3/8 - NOTE should be combinable with others, e.g. voltage
    bool is_fraction(const String&, int) const;

    // Main parsing methods
    Value parse(String s, int& ptr) const;
    Value parse(String s) const;

    // Static methods for backward compatibility (will be deprecated)
    static const String unescape_static(const String str);
    static void skip_whitespace_static(const String& s, int& ptr);
    static bool is_symbol_static(const String&, int);
    static bool is_comment_static(const String&, int);
    static bool is_quote_static(const String&, int);
    static bool is_list_static(const String&, int);
    static bool is_vector_static(const String&, int);
    static bool is_map_static(const String&, int);
    static bool is_midinote_static(const String&, int);
    static bool is_freq_static(const String&, int);
    static bool is_fraction_static(const String&, int);
    static Value parse_static(String s, int& ptr);
    static Value parse_static(String s);

private:
    std::vector<Diagnostic>* m_diagnostics;
};

// Utils

#endif // PARSE_H_
