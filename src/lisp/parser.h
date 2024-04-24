#ifndef PARSE_H_
#define PARSE_H_

#include "lisp/value.h"

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
    uLispParser() {}

    String unescape(String str);

    void skip_whitespace(String& s, int& ptr);

    // TODO add more, e.g.
    // is_voltage
    // is_freq
    bool is_symbol(String&, int);
    bool is_comment(String&, int);
    bool is_quote(String&, int);
    bool is_list(String&, int);
    bool is_vector(String&, int);
    bool is_map(String&, int);
    bool is_midinote(String&, int); // M
    bool is_freq(String&, int); // Hz
    bool is_fraction(String&, int); // e.g. 3/8 - NOTE should be combinable with others, e.g. voltage

    Value parse(String& s, int& ptr);
    Value parse(String s);
};

// Utils

#endif // PARSE_H_
