#ifndef PARSE_H_
#define PARSE_H_

#include "value.h"

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

class uLispParser {
public:
  uLispParser() {}

  static const String unescape(const String str);

  static void skip_whitespace(const String &s, int &ptr);

  // TODO add more, e.g.
  // is_voltage
  // is_freq
  static bool is_symbol(const String &, int);
  static bool is_comment(const String &, int);
  static bool is_quote(const String &, int);
  static bool is_list(const String &, int);
  static bool is_vector(const String &, int);
  static bool is_map(const String &, int);
  static bool is_midinote(const String &, int); // M
  static bool is_freq(const String &, int);     // Hz
  // e.g. 3/8 - NOTE should be combinable with others, e.g. voltage
  static bool is_fraction(const String &, int);

  static Value parse(String s, int &ptr);
  static Value parse(String s);
};

// Utils

#endif // PARSE_H_
