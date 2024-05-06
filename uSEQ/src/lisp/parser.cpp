#include "parser.h"

const String uLispParser::unescape(const String str) {
  String result = "";
  for (int i = 0; i < str.length(); i++) {
    if (str[i] == '\\' && i + 1 < str.length()) {
      i++;
      switch (str[i]) {
      case 'n':
        result += "\n";
        break;
      case '"':
        result += "\"";
        break;
      case 'r':
        result += "\r";
        break;
      case 't':
        result += "\t";
        break;
      default:
        result += str[i];
        break;
      }
    } else {
      result += str[i];
    }
  }
  return result;
}

void uLispParser::skip_whitespace(const String &s, int &ptr) {
  while (isspace(s[ptr])) {
    ptr++;
  }
}

// Is this character a valid lisp symbol character
bool uLispParser::is_symbol(const String &s, int ptr) {
  char ch = s[ptr];
  return (isdigit(ch) || isalpha(ch) || ispunct(ch)) && ch != '(' &&
         ch != ')' && ch != '"' && ch != '\'';
}

bool uLispParser::is_comment(const String &s, int ptr) { return s[ptr] == ';'; }
bool uLispParser::is_quote(const String &s, int ptr) { return s[ptr] == '\''; }
bool uLispParser::is_list(const String &s, int ptr) { return s[ptr] == '('; }
bool uLispParser::is_vector(const String &s, int ptr) { return s[ptr] == '['; }
bool uLispParser::is_map(const String &s, int ptr) { return s[ptr] == '{'; }
bool uLispParser::is_midinote(const String &s, int ptr) {
  return s[ptr] == 'M';
}

// Parse a single value and increment the pointer
// to the beginning of the next value to parse.
Value uLispParser::parse(String s, int &ptr) {

  // dbg(s);
  // dbg(String(ptr));

  skip_whitespace(s, ptr);

  // Skip comments
  while (is_comment(s, ptr)) {
    // If this is a comment
    int work_ptr = ptr;
    // Skip to the end of the line
    while (s[work_ptr] != '\n' && work_ptr < int(s.length())) {
      work_ptr++;
    }
    ptr = work_ptr;
    skip_whitespace(s, ptr);

    // If we're at the end of the string, return an empty value
    if (s.substring(ptr, ptr + s.length() - ptr - 1) == "")
      return Value();
  }

  // Parse the value
  if (s == "") {
    // TODO should this return some kind of error?
    // parsing an empty string shouldn't be the same
    // as parsing "nil"
    return Value();
  } else if (is_quote(s, ptr)) {
    // If this is a quote
    ptr++;
    return Value::quote(parse(s, ptr));
  } else if (is_list(s, ptr)) {
    // If this is a list
    skip_whitespace(s, ++ptr);

    Value result = Value(std::vector<Value>());

    while (s[ptr] != ')') {
      Value res = parse(s, ptr);
      if (res == Value::error()) {
        result = Value::error();
        break;
      } else {
        result.push(res);
      }
    }

    skip_whitespace(s, ++ptr);
    return result;
  } else if (isdigit(s[ptr]) || (s[ptr] == '-' && isdigit(s[ptr + 1]))) {
    // If this is a number
    bool negate = s[ptr] == '-';
    if (negate)
      ptr++;

    int save_ptr = ptr;
    while (isdigit(s[ptr]) || s[ptr] == '.')
      ptr++;
    String n = s.substring(save_ptr, save_ptr + ptr);
    skip_whitespace(s, ptr);

    if (n.indexOf('.') != -1)
      // return Value((negate? -1 : 1) * atof(n.c_str()));
      return Value((negate ? -1 : 1) * atof(n.c_str()));
    else
      return Value((negate ? -1 : 1) * atoi(n.c_str()));
  } else if (s[ptr] == '\"') {
    // If this is a string
    int n = 1;
    while (s[ptr + n] != '\"') {
      if (ptr + n >= int(s.length())) {
        print(MALFORMED_PROGRAM);
        println(" 1");
        return Value::error();
        // throw std::runtime_error(MALFORMED_PROGRAM);
      }

      if (s[ptr + n] == '\\')
        n++;
      n++;
    }

    String x = s.substring(ptr + 1, ptr + 1 + n - 1);
    ptr += n + 1;
    skip_whitespace(s, ptr);

    // Iterate over the characters in the string, and
    // replace escaped characters with their intended values.
    x = unescape(x);
    return Value::string(x);
  } else if (s[ptr] == '@') {
    ptr++;
    skip_whitespace(s, ptr);
    return Value();
  } else if (is_symbol(s, ptr)) {
    // If this is a string
    int n = 0;
    while (is_symbol(s, ptr + n)) {
      n++;
    }

    String x = s.substring(ptr, ptr + n);
    ptr += n;
    skip_whitespace(s, ptr);
    return Value::atom(x);
  } else {
    println(MALFORMED_PROGRAM);
    return Value::error();
    // throw std::runtime_error(MALFORMED_PROGRAM);
  }
}

bool is_empty_string(const String &s) { return s == ""; }

// Parse an entire program and get its list of expressions.
Value uLispParser::parse(String s) {

  // const char* st = s.c_str();

  // dbg(s);

  int i = 0, last_i = -1;
  bool error = false;

  if (is_empty_string(s)) {
    return Value();
  }

  Value result;
  // An implicit "do" is wrapped around the whole program
  // std::vector<Value> list = { Value::atom("do") };
  std::vector<Value> list = {};

  // While the parser is making progress (while the pointer is moving right)
  // and the pointer hasn't reached the end of the string,
  while (last_i != i && i <= int(s.length() - 1)) {
    // Parse another expression and add it to the list.
    last_i = i;
    Value item = parse(s, i);
    if (item.is_error()) {
      error = true;
      break;
    }

    list.push_back(item);
  }

  // If the whole string wasn't parsed, the program must be bad.
  if (i < int(s.length())) {
    print("parse: ");
    println(MALFORMED_PROGRAM);
    error = true;
  }

  if (error) {
    result = Value::error();
  } else {
    // if there were more than one top-level forms in the
    // provided code, wrap them all in an implicit 'do'
    // (which evaluates them all and returns the last value)
    if (list.size() > 1) {
      list.insert(list.begin(), Value::atom("do"));
      result = Value(list);
    } else {
      result = list[0];
    }
  }

  // dbg(result.debug());
  return result;
}
