#include "parser.h"
#include "error_context.h"
#include "value.h"

const String uLispParser::unescape(const String str) const
{
    String result = "";
    for (unsigned int i = 0; i < str.length(); i++)
    {
        if (str[i] == '\\' && i + 1 < str.length())
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

void uLispParser::skip_whitespace(const String& s, int& ptr) const
{
    while (isspace(s[ptr]) || s[ptr] == '\n' || s[ptr] == ',')
    {
        ptr++;
    }
}

// Is this character a valid lisp symbol character
bool uLispParser::is_symbol(const String& s, int ptr) const
{
    char ch = s[ptr];
    return (isdigit(ch) || isalpha(ch) || ispunct(ch)) && ch != '(' && ch != ')' &&
           ch != '[' && ch != ']' && ch != '{' && ch != '}' && ch != '"' &&
           ch != '\'' && ch != ';' && ch != ',' && ch != ' ' && ch != '\t' &&
           ch != '\n';
}

bool uLispParser::is_comment(const String& s, int ptr) const
{
    return s[ptr] == ';';
}
bool uLispParser::is_quote(const String& s, int ptr) const { return s[ptr] == '\''; }
bool uLispParser::is_list(const String& s, int ptr) const { return s[ptr] == '('; }
bool uLispParser::is_vector(const String& s, int ptr) const { return s[ptr] == '['; }
bool uLispParser::is_map(const String& s, int ptr) const { return s[ptr] == '{'; }
bool uLispParser::is_midinote(const String& s, int ptr) const
{
    if (s[ptr] != 'M')
        return false;

    // Check if there's at least one digit following 'M'
    int pos = ptr + 1;
    if (pos >= s.length() || !isdigit(s[pos]))
        return false;

    // Parse the number part to ensure it's valid
    int number = 0;
    while (pos < s.length() && isdigit(s[pos]))
    {
        number = number * 10 + (s[pos] - '0');
        pos++;
    }

    // MIDI notes should be 0-127
    return number >= 0 && number <= 127;
}

// Parse a single value and increment the pointer
// to the beginning of the next value to parse.
Value uLispParser::parse(String s, int& ptr) const
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
    while (is_comment(s, ptr))
    {
        // If this is a comment
        int work_ptr = ptr;
        // Skip to the end of the line
        while (s[work_ptr] != '\n' && work_ptr < int(s.length()))
        {
            work_ptr++;
        }
        ptr = work_ptr;
        skip_whitespace(s, ptr);

        // If we're at the end of the string, return an empty value
        if (ptr >= int(s.length()))
            return Value();
    }

    // Parse the value
    if (s == "")
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

        while (s[ptr] != ')')
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

        while (s[ptr] != ']')
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
    else if (is_map(s, ptr))
    {
        // Parse map as a list: {key1 value1 key2 value2} -> (key1 value1 key2
        // value2)
        std::vector<Value> list;
        ptr++; // Skip opening brace
        skip_whitespace(s, ptr);

        while (ptr < int(s.length()) && s[ptr] != '}')
        {
            Value v = parse(s, ptr);
            if (v.is_error())
                return v;
            list.push_back(v);
            skip_whitespace(s, ptr);
        }

        if (ptr >= int(s.length()))
        {
            if (m_error_manager)
            {
                m_error_manager->report_syntax_error(
                    "Unmatched closing parenthesis");
            }
            return Value::error();
        }

        ptr++; // Skip closing brace
        skip_whitespace(s, ptr);
        return Value(list);
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
        String n = s.substring(save_ptr, ptr);
        skip_whitespace(s, ptr);

        if (n.indexOf('.') != -1)
            // return Value((negate? -1 : 1) * atof(n.c_str()));
            return Value((negate ? -1 : 1) * atof(n.c_str()));
        else
            return Value((negate ? -1 : 1) * atoi(n.c_str()));
    }
    else if (s[ptr] == '\"')
    {

        // println("is string");
        //  If this is a string
        int n = 1;
        while (s[ptr + n] != '\"')
        {
            if (ptr + n >= int(s.length()))
            {
                if (m_error_manager)
                {
                    m_error_manager->report_syntax_error(
                        "Unexpected end of input, expected closing quote");
                }
                return Value::error();
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
        // Optimized symbol parsing - avoid repeated is_symbol calls
        int start = ptr;
        const int len = s.length();

        // Fast inner loop with bounds check
        while (ptr < len) {
            char ch = s[ptr];
            // Inline symbol check for speed
            switch (ch) {
                case '(': case ')': case '[': case ']': case '{': case '}':
                case '"': case '\'': case ';': case ',': case ' ': case '\t': case '\n':
                    goto symbol_done;
                default:
                    if (ch >= '!' && ch <= '~') {
                        ptr++;
                    } else {
                        goto symbol_done;
                    }
            }
        }
        symbol_done:

        String x = s.substring(start, ptr);
        skip_whitespace(s, ptr);
        return Value::atom(x);
    }
    else
    {
        if (m_error_manager)
        {
            m_error_manager->report_syntax_error(
                "Invalid input - cannot parse token");
        }
        return Value::error();
    }
}

bool is_empty_string(const String& s) { return s == ""; }

// Parse an entire program and get its list of expressions.
Value uLispParser::parse(String code) const
{
    // dbg(s);
    //

    // println("Parse: ");
    // println(code);
    // for (unsigned int i = 0; i < code.length(); i++)
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
    while (last_i != i && i <= int(code.length() - 1))
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

        // Skip any trailing comments after parsing an expression
        skip_whitespace(code, i);
        while (is_comment(code, i))
        {
            // Skip to the end of the line
            while (i < int(code.length()) && code[i] != '\n')
            {
                i++;
            }
            if (i < int(code.length()) && code[i] == '\n')
            {
                i++; // Skip the newline
            }
            skip_whitespace(code, i);
        }
    }

    // If the whole string wasn't parsed, the program must be bad.
    if (i < int(code.length()))
    {
        if (m_error_manager)
        {
            m_error_manager->report_syntax_error(
                "Malformed program - unexpected end of input");
        }
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

// Missing instance methods implementation
bool uLispParser::is_freq(const String& s, int ptr) const
{
    // Check for Hz suffix - could be extended for more frequency units
    int pos = ptr;
    while (pos < s.length() && (isdigit(s[pos]) || s[pos] == '.'))
    {
        pos++;
    }
    return pos < s.length() - 1 && s[pos] == 'H' && s[pos + 1] == 'z';
}

bool uLispParser::is_fraction(const String& s, int ptr) const
{
    // Check for pattern like "3/8"
    int pos = ptr;
    while (pos < s.length() && isdigit(s[pos]))
    {
        pos++;
    }
    if (pos >= s.length() || s[pos] != '/')
        return false;
    pos++;
    while (pos < s.length() && isdigit(s[pos]))
    {
        pos++;
    }
    return pos > ptr + 2; // Must have at least "1/1"
}

// Static methods for backward compatibility
const String uLispParser::unescape_static(const String str)
{
    uLispParser parser;
    return parser.unescape(str);
}

void uLispParser::skip_whitespace_static(const String& s, int& ptr)
{
    uLispParser parser;
    parser.skip_whitespace(s, ptr);
}

bool uLispParser::is_symbol_static(const String& s, int ptr)
{
    uLispParser parser;
    return parser.is_symbol(s, ptr);
}

bool uLispParser::is_comment_static(const String& s, int ptr)
{
    uLispParser parser;
    return parser.is_comment(s, ptr);
}

bool uLispParser::is_quote_static(const String& s, int ptr)
{
    uLispParser parser;
    return parser.is_quote(s, ptr);
}

bool uLispParser::is_list_static(const String& s, int ptr)
{
    uLispParser parser;
    return parser.is_list(s, ptr);
}

bool uLispParser::is_vector_static(const String& s, int ptr)
{
    uLispParser parser;
    return parser.is_vector(s, ptr);
}

bool uLispParser::is_map_static(const String& s, int ptr)
{
    uLispParser parser;
    return parser.is_map(s, ptr);
}

bool uLispParser::is_midinote_static(const String& s, int ptr)
{
    uLispParser parser;
    return parser.is_midinote(s, ptr);
}

bool uLispParser::is_freq_static(const String& s, int ptr)
{
    uLispParser parser;
    return parser.is_freq(s, ptr);
}

bool uLispParser::is_fraction_static(const String& s, int ptr)
{
    uLispParser parser;
    return parser.is_fraction(s, ptr);
}

Value uLispParser::parse_static(String s, int& ptr)
{
    uLispParser parser;
    return parser.parse(s, ptr);
}

Value uLispParser::parse_static(String s)
{
    uLispParser parser;
    return parser.parse(s);
}
