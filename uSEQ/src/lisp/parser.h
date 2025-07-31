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

  // Core interface working with raw C strings ------------------------------
  static String unescape(const char *str);

  static void skip_whitespace(const char *s, int &ptr);

  // TODO add more, e.g.
  // is_voltage
  // is_freq
  static bool is_symbol(const char *, int);
  static bool is_comment(const char *, int);
  static bool is_quote(const char *, int);
  static bool is_list(const char *, int);
  static bool is_vector(const char *, int);
  static bool is_map(const char *, int);
  static bool is_midinote(const char *, int); // M
  static bool is_freq(const char *, int);     // Hz
  // e.g. 3/8 - NOTE should be combinable with others, e.g. voltage
  static bool is_fraction(const char *, int);

  static Value parse(const char *s, int &ptr);
  static Value parse(const char *s);

  // Convenience wrappers for the custom String class -----------------------
  static inline String unescape(const String &str) { return unescape(str.c_str()); }
  static inline void skip_whitespace(const String &s, int &ptr) {
    skip_whitespace(s.c_str(), ptr);
  }
  static inline bool is_symbol(const String &s, int ptr) {
    return is_symbol(s.c_str(), ptr);
  }
  static inline bool is_comment(const String &s, int ptr) {
    return is_comment(s.c_str(), ptr);
  }
  static inline bool is_quote(const String &s, int ptr) {
    return is_quote(s.c_str(), ptr);
  }
  static inline bool is_list(const String &s, int ptr) {
    return is_list(s.c_str(), ptr);
  }
  static inline bool is_vector(const String &s, int ptr) {
    return is_vector(s.c_str(), ptr);
  }
  static inline bool is_map(const String &s, int ptr) {
    return is_map(s.c_str(), ptr);
  }
  static inline bool is_midinote(const String &s, int ptr) {
    return is_midinote(s.c_str(), ptr);
  }
  static inline bool is_freq(const String &s, int ptr) {
    return is_freq(s.c_str(), ptr);
  }
  static inline bool is_fraction(const String &s, int ptr) {
    return is_fraction(s.c_str(), ptr);
  }

  static inline Value parse(String s, int &ptr) {
    return parse(s.c_str(), ptr);
  }
  static inline Value parse(String s) { return parse(s.c_str()); }
};

// Utils

#endif // PARSE_H_
