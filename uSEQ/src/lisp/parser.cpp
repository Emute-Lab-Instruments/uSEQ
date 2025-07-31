#include "parser.h"
#include "interpreter.h"
#include <cstring>

String uLispParser::unescape(const char* str)
{
    String result = "";
    for (size_t i = 0; str[i] != '\0'; i++)
    {
        if (str[i] == '\\' && str[i + 1] != '\0')
        {
            i++;
            switch (str[i])
            {
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
        }
        else
        {
            result += str[i];
        }
    }
    return result;
}

void uLispParser::skip_whitespace(const char* s, int& ptr)
{
    while (s[ptr] != '\0' && (isspace(static_cast<unsigned char>(s[ptr])) ||
                               s[ptr] == '\n' || s[ptr] == ','))
    {
        ptr++;
    }
}

// Is this character a valid lisp symbol character
bool uLispParser::is_symbol(const char* s, int ptr)
{
    char ch = s[ptr];
    return ch != '\0' && (isdigit(static_cast<unsigned char>(ch)) ||
                           isalpha(static_cast<unsigned char>(ch)) ||
                           ispunct(static_cast<unsigned char>(ch))) &&
           ch != '(' && ch != ')' && ch != '[' && ch != ']' && ch != '"' &&
           ch != '\'';
}

bool uLispParser::is_comment(const char* s, int ptr) { return s[ptr] == ';'; }
bool uLispParser::is_quote(const char* s, int ptr) { return s[ptr] == '\''; }
bool uLispParser::is_list(const char* s, int ptr) { return s[ptr] == '('; }
bool uLispParser::is_vector(const char* s, int ptr) { return s[ptr] == '['; }
bool uLispParser::is_map(const char* s, int ptr) { return s[ptr] == '{'; }
bool uLispParser::is_midinote(const char* s, int ptr) { return s[ptr] == 'M'; }

// Parse a single value and increment the pointer
// to the beginning of the next value to parse.
Value uLispParser::parse(const char* s, int& ptr)
{
    // if (user_interaction)
    // {
    //     println("\n");
    //     println("\n");
    //     println("(inner parse)");
    //     println(s.substring(ptr));
    //     println("\n");
    //     println("\n");
    // }

    skip_whitespace(s, ptr);

    // Skip comments
    size_t len = strlen(s);
    while (s[ptr] != '\0' && is_comment(s, ptr))
    {
        // If this is a comment
        int work_ptr = ptr;
        // Skip to the end of the line
        while (s[work_ptr] != '\n' && work_ptr < int(len) && s[work_ptr] != '\0')
        {
            work_ptr++;
        }
        ptr = work_ptr;
        skip_whitespace(s, ptr);

        // If we're at the end of the string, return an empty value
        if (ptr >= int(len) || s[ptr] == '\0')
            return Value();
    }

    // Parse the value
    if (s[ptr] == '\0')
    {
        // TODO should this return some kind of error?
        // parsing an empty string shouldn't be the same
        // as parsing "nil"
        return Value();
    }
    else if (is_quote(s, ptr))
    {

        // println("is quote");
        //  If this is a quote
        ptr++;
        return Value::quote(parse(s, ptr));
    }
    else if (is_list(s, ptr))
    {

        // println("is list");
        // Consume the opening '(' and skip initial whitespace
        skip_whitespace(s, ++ptr);

        Value result = Value(std::vector<Value>());

        while (s[ptr] != ')' && ptr < int(len) && s[ptr] != '\0')
        {
            Value res = parse(s, ptr);
            if (res.is_error())
            {
                result = Value::error();
                break;
            }
            else
            {
                result.push(res);
            }
            skip_whitespace(s, ptr);
        }

        skip_whitespace(s, ++ptr);
        return result;
    }
    else if (is_vector(s, ptr))
    {
        // println("is vector");
        //  If this is a list
        skip_whitespace(s, ++ptr);

        Value result;
        std::vector<Value> vec;

        while (s[ptr] != ']' && ptr < int(len) && s[ptr] != '\0')
        {
            // skip_whitespace(s, ++ptr);
            Value res = parse(s, ptr);
            if (res.is_error())
            {
                return Value::error();
            }
            else
            {
                vec.push_back(res);
            }
            skip_whitespace(s, ptr);
        }
        ptr++;

        // skip_whitespace(s, ++ptr);
        return Value::vector(vec);
    }
    else if (isdigit(s[ptr]) || (s[ptr] == '-' && isdigit(s[ptr + 1])) ||
             (s[ptr] == '.' && isdigit(s[ptr + 1])))
    {

        // println("is digit");
        //  If this is a number
        bool negate = s[ptr] == '-';
        if (negate)
            ptr++;

        int save_ptr = ptr;
        while (isdigit(s[ptr]) || s[ptr] == '.')
            ptr++;
        String n(s + save_ptr, ptr - save_ptr);
        skip_whitespace(s, ptr);

        if (strchr(n.c_str(), '.') != nullptr)
            return Value((negate ? -1 : 1) * atof(n.c_str()));
        else
            return Value((negate ? -1 : 1) * atoi(n.c_str()));
    }
    else if (s[ptr] == '"')
    {

        // println("is string");
        //  If this is a string
        int n = 1;
        while (s[ptr + n] != '"')
        {
            if (ptr + n >= int(len))
            {
                println(MALFORMED_PROGRAM);
                return Value::error();
            }

            if (s[ptr + n] == '\\')
                n++;
            n++;
        }

        String x(s + ptr + 1, n - 1);
        ptr += n + 1;
        skip_whitespace(s, ptr);

        // Iterate over the characters in the string, and
        // replace escaped characters with their intended values.
        x = unescape(x.c_str());
        return Value::string(x);
    }
    else if (s[ptr] == '@')
    {
        // println("is @");
        ptr++;
        skip_whitespace(s, ptr);
        return Value();
    }
    else if (is_symbol(s, ptr))
    {
        // println("is symbol");
        //  If this is a string
        int n = 0;
        while (is_symbol(s, ptr + n))
        {
            n++;
        }

        String x(s + ptr, n);
        ptr += n;
        skip_whitespace(s, ptr);
        return Value::atom(x);
    }
    else
    {
        println(MALFORMED_PROGRAM);
        return Value::error();
        // throw std::runtime_error(MALFORMED_PROGRAM);
    }
}

static bool is_empty_string(const char* s) { return s == nullptr || s[0] == '\0'; }

// Parse an entire program and get its list of expressions.
Value uLispParser::parse(const char* code)
{
    // dbg(s);
    //

    // println("Parse: ");
    // println(code);
    // for (int i = 0; i < code.length(); i++)
    // {
    //     println(String((uint)code[i]));

    //     if (code[i] == '\n')
    //     {
    //         println("newline at: " + String(i));
    //     }
    // }

    int i = 0, last_i = -1;
    bool error = false;

    if (is_empty_string(code))
    {
        return Value();
    }

    // code = code.replace("\n", " ");

    Value result;
    // An implicit "do" is wrapped around the whole program
    // std::vector<Value> list = { Value::atom("do") };
    std::vector<Value> list = {};

    // While the parser is making progress (while the pointer is moving right)
    // and the pointer hasn't reached the end of the string,
    size_t len = strlen(code);
    while (last_i != i && i <= int(len - 1))
    {
        // Parse another expression and add it to the list.
        last_i     = i;
        Value item = parse(code, i);
        if (item.is_error())
        {
            error = true;
            break;
        }

        list.push_back(item);
    }

    // If the whole string wasn't parsed, the program must be bad.
    if (i < int(len))
    {
        println("parse: ");
        println(MALFORMED_PROGRAM);
        error = true;
    }

    if (error)
    {
        result = Value::error();
    }
    else
    {
        // if there were more than one top-level forms in the
        // provided code, wrap them all in an implicit 'do'
        // (which evaluates them all and returns the last value)
        if (list.size() > 1)
        {
            list.insert(list.begin(), Value::atom("do"));
            result = Value(list);
        }
        else
        {
            result = list[0];
        }
    }

    // dbg(result.debug());
    return result;
}
