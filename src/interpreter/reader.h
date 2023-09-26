#ifndef READER_H_
#define READER_H_

#include "string.h"

class Value;
// Parse a single value and increment the pointer
// to the beginning of the next value to parse.
Value parse(String &s, int &ptr) {
  skip_whitespace(s, ptr);

  // Skip comments
  while (s[ptr] == ';') {
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
    return Value();
  } else if (s[ptr] == '\'') {
    // If this is a quote
    ptr++;
    return Value::quote(parse(s, ptr));

  } else if (s[ptr] == '(') {
    // If this is a list
    skip_whitespace(s, ++ptr);

    Value result = Value(std::vector<Value>());

    while (s[ptr] != ')')
      result.push(parse(s, ptr));

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

  } else if (is_symbol(s[ptr])) {
    // If this is a string
    int n = 0;
    while (is_symbol(s[ptr + n])) {
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

// Parse an entire program and get its list of expressions.
std::vector<Value> parse(String s) {
  int i = 0, last_i = -1;
  std::vector<Value> result;
  bool error = false;
  // While the parser is making progress (while the pointer is moving right)
  // and the pointer hasn't reached the end of the string,
  while (last_i != i && i <= int(s.length() - 1)) {
    // Parse another expression and add it to the list.
    last_i = i;
    Value token = parse(s, i);
    if (token.is_error()) {
      error = true;
      break;
    }
    result.push_back(token);
  }

  // If the whole string wasn't parsed, the program must be bad.
  if (i < int(s.length())) {
    print("parse: ");
    println(MALFORMED_PROGRAM);
    error = true;
  }
  if (error) {
    result.clear();
  }
  // Return the list of values parsed.
  return result;
}

#endif // READER_H_
