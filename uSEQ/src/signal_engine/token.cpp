#include "token.h"
#include "../modulisp/lisp/symbol_intern.h"
#include <cmath>
#include <cstring>
#include <cstdlib>

namespace sig {

// ── TokenStream methods ─────────────────────────────────────────────────────

Token TokenStream::peek() const {
    if (pos < count) return tokens[pos];
    Token eof;
    eof.kind = TokenKind::Eof;
    return eof;
}

Token TokenStream::consume() {
    if (pos < count) return tokens[pos++];
    Token eof;
    eof.kind = TokenKind::Eof;
    return eof;
}

bool TokenStream::expect(TokenKind kind) {
    if (pos < count && tokens[pos].kind == kind) {
        pos++;
        return true;
    }
    return false;
}

void TokenStream::rewind(uint16_t position) {
    pos = position;
}

bool TokenStream::at_end() const {
    return pos >= count || tokens[pos].kind == TokenKind::Eof;
}

// ── Tokenizer ───────────────────────────────────────────────────────────────

static bool is_symbol_char(char c) {
    if (c <= ' ') return false;
    switch (c) {
        case '(': case ')': case '[': case ']':
        case '"': case '\'': case ';': case ',':
            return false;
        default:
            return true;
    }
}

static bool is_digit(char c) { return c >= '0' && c <= '9'; }

// A14: strtod accepts hex (0x10), "inf" and "nan" forms — ModuLisp numeric
// literals are plain decimal only. Restrict number tokens to decimal
// characters so those forms tokenize as symbols (→ UndefinedName) instead of
// silently becoming numbers.
static bool is_plain_number_text(const char* s, uint32_t len) {
    for (uint32_t k = 0; k < len; k++) {
        char c = s[k];
        if (!(is_digit(c) || c == '.' || c == '-' || c == '+' ||
              c == 'e' || c == 'E'))
            return false;
    }
    return true;
}

uint16_t TokenStream::tokenize(const char* source, uint32_t length,
                                Token* out, uint16_t max_tokens,
                                Diagnostic* errors, uint8_t* error_count) {
    uint16_t count = 0;
    uint32_t i = 0;
    bool overflowed = false;
    TokenKind delimiter_stack[MAX_TOKENS] = {};
    uint16_t delimiter_depth = 0;

    auto emit = [&](Token t) {
        if (count < max_tokens) {
            out[count++] = t;
        } else {
            // Buffer full: record the overflow instead of silently truncating.
            overflowed = true;
        }
    };

    auto emit_error = [&](uint32_t pos, uint16_t len, const char* msg,
                          const char* suggestion = nullptr) {
        if (!error_count || *error_count >= 8) return;
        if (errors) {
            errors[*error_count] = {
                DiagnosticSeverity::Error, DiagnosticCategory::Syntax,
                (uint16_t)pos, len, msg, suggestion
            };
        }
        (*error_count)++;
    };

    // Token spans are represented by uint16_t. Reject a submission that
    // cannot be represented before scanning so no later cast can wrap a
    // diagnostic or source slice onto unrelated bytes.
    if (length > UINT16_MAX) {
        emit_error(0, UINT16_MAX,
                   "Program is too large for source-span tracking",
                   "Split the submission into smaller forms");
        Token eof;
        eof.kind = TokenKind::Eof;
        eof.span_start = UINT16_MAX;
        if (max_tokens > 0) out[count++] = eof;
        return count;
    }
    if (!source && length > 0) {
        emit_error(0, 0, "Source buffer is null");
        return count;
    }

    // The language source profile is ASCII. Preflight the complete byte
    // string, including comments and string literals, before emitting any
    // usable token. NUL is not whitespace and high bytes are never decoded
    // through implementation-defined signed-char behaviour.
    for (uint32_t j = 0; j < length; ++j) {
        const unsigned char byte =
            static_cast<unsigned char>(source[j]);
        if (byte == 0 || byte >= 0x80) {
            emit_error(j, 1,
                       byte == 0
                           ? "NUL is not allowed in source"
                           : "Source must contain ASCII bytes only");
            Token eof;
            eof.kind = TokenKind::Eof;
            eof.span_start = static_cast<uint16_t>(length);
            if (max_tokens > 0) out[count++] = eof;
            return count;
        }
    }

    while (i < length) {
        char c = source[i];

        // Skip whitespace
        if (c <= ' ') { i++; continue; }

        // Skip comments
        if (c == ';') {
            while (i < length && source[i] != '\n') i++;
            continue;
        }

        // Parens and brackets
        if (c == '(') {
            Token t; t.kind = TokenKind::LParen; t.span_start = (uint16_t)i; t.span_len = 1;
            emit(t);
            if (delimiter_depth < MAX_TOKENS) {
                delimiter_stack[delimiter_depth++] = TokenKind::RParen;
            } else {
                emit_error(i, 1, "Delimiter nesting is too deep");
            }
            i++;
            continue;
        }
        if (c == ')') {
            Token t; t.kind = TokenKind::RParen; t.span_start = (uint16_t)i; t.span_len = 1;
            emit(t);
            if (delimiter_depth == 0) {
                emit_error(i, 1, "Unexpected closing delimiter");
            } else if (delimiter_stack[delimiter_depth - 1] !=
                       TokenKind::RParen) {
                emit_error(i, 1, "Mismatched closing delimiter",
                           "Close '[' with ']'");
                delimiter_depth--;
            } else {
                delimiter_depth--;
            }
            i++;
            continue;
        }
        if (c == '[') {
            Token t; t.kind = TokenKind::LBracket; t.span_start = (uint16_t)i; t.span_len = 1;
            emit(t);
            if (delimiter_depth < MAX_TOKENS) {
                delimiter_stack[delimiter_depth++] = TokenKind::RBracket;
            } else {
                emit_error(i, 1, "Delimiter nesting is too deep");
            }
            i++;
            continue;
        }
        if (c == ']') {
            Token t; t.kind = TokenKind::RBracket; t.span_start = (uint16_t)i; t.span_len = 1;
            emit(t);
            if (delimiter_depth == 0) {
                emit_error(i, 1, "Unexpected closing delimiter");
            } else if (delimiter_stack[delimiter_depth - 1] !=
                       TokenKind::RBracket) {
                emit_error(i, 1, "Mismatched closing delimiter",
                           "Close '(' with ')'");
                delimiter_depth--;
            } else {
                delimiter_depth--;
            }
            i++;
            continue;
        }

        // String literal
        if (c == '"') {
            uint32_t start = i;
            i++; // skip opening quote
            uint32_t str_start = i;
            while (i < length && source[i] != '"') {
                if (source[i] == '\\' && i + 1 < length) i++; // skip escape
                i++;
            }
            if (i >= length) {
                emit_error((uint32_t)start, (uint16_t)(i - start),
                           "Unterminated string");
                break;
            }
            Token t;
            t.kind = TokenKind::String;
            t.span_start = (uint16_t)start;
            t.span_len = (uint16_t)(i + 1 - start);
            t.string.offset = str_start;
            t.string.length = (uint16_t)(i - str_start);
            emit(t);
            i++; // skip closing quote
            continue;
        }

        // Number (including negative numbers)
        if (is_digit(c) || (c == '-' && i + 1 < length && is_digit(source[i + 1])) ||
            (c == '.' && i + 1 < length && is_digit(source[i + 1]))) {
            uint32_t start = i;
            char* end_ptr = nullptr;
            double val = strtod(source + i, &end_ptr);
            if (end_ptr > source + i) {
                uint32_t num_end = (uint32_t)(end_ptr - source);
                // A14: the character after the number must not be a symbol
                // char ("2x" must tokenize as the symbol 2x, not number 2
                // followed by symbol x), and the literal itself must be plain
                // decimal (no 0x/inf/nan strtod forms). Otherwise fall
                // through to the symbol path.
                bool clean_tail = num_end >= length ||
                                  !is_symbol_char(source[num_end]);
                if (clean_tail &&
                    is_plain_number_text(source + start, num_end - start)) {
                    if (!std::isfinite(val)) {
                        emit_error(start, (uint16_t)(num_end - start),
                                   "Numeric literal is outside the finite binary64 range",
                                   "Use a smaller finite decimal literal");
                        Token t;
                        t.kind = TokenKind::Error;
                        t.span_start = (uint16_t)start;
                        t.span_len = (uint16_t)(num_end - start);
                        emit(t);
                        i = num_end;
                        continue;
                    }
                    Token t;
                    t.kind = TokenKind::Number;
                    t.span_start = (uint16_t)start;
                    t.span_len = (uint16_t)(num_end - start);
                    t.number = val;
                    emit(t);
                    i = num_end;
                    continue;
                }
            }
        }

        // Symbol
        if (is_symbol_char(c)) {
            uint32_t start = i;
            while (i < length && is_symbol_char(source[i])) i++;
            // Intern the symbol
            // We need a temporary null-terminated string
            char buf[256];
            uint32_t sym_len = i - start;
            if (sym_len >= sizeof(buf)) {
                uint16_t diag_len = sym_len > UINT16_MAX
                    ? UINT16_MAX : (uint16_t)sym_len;
                emit_error(start, diag_len, "Symbol is too long",
                           "Use a name shorter than 256 bytes");
                Token t;
                t.kind = TokenKind::Error;
                t.span_start = (uint16_t)start;
                t.span_len = diag_len;
                emit(t);
                continue;
            }
            memcpy(buf, source + start, sym_len);
            buf[sym_len] = '\0';

            // Check if it's actually a number (e.g., "-1" when preceded by
            // space). Restricted to plain decimal text (A14) so strtod's
            // inf/nan/hex forms stay symbols.
            char* end_ptr = nullptr;
            double val = strtod(buf, &end_ptr);
            if (end_ptr == buf + sym_len &&
                is_plain_number_text(buf, sym_len)) {
                if (!std::isfinite(val)) {
                    emit_error(start, (uint16_t)sym_len,
                               "Numeric literal is outside the finite binary64 range",
                               "Use a smaller finite decimal literal");
                    Token t;
                    t.kind = TokenKind::Error;
                    t.span_start = (uint16_t)start;
                    t.span_len = (uint16_t)sym_len;
                    emit(t);
                    continue;
                }
                Token t;
                t.kind = TokenKind::Number;
                t.span_start = (uint16_t)start;
                t.span_len = (uint16_t)sym_len;
                t.number = val;
                emit(t);
                continue;
            }

            Token t;
            t.kind = TokenKind::Symbol;
            t.span_start = (uint16_t)start;
            t.span_len = (uint16_t)sym_len;
            t.symbol = SymbolIntern::getInstance().intern(String(buf));
            emit(t);
            continue;
        }

        // Unknown character
        emit_error(i, 1, "Unexpected character");
        i++;
    }

    // Append EOF
    Token eof;
    eof.kind = TokenKind::Eof;
    eof.span_start = (uint16_t)length;
    eof.span_len = 0;
    emit(eof);

    // EOF is not an implicit closing delimiter.  The parser's expect() calls
    // are deliberately non-fatal for recovery, so validate the lexical
    // delimiter balance here before a malformed form can compile as valid.
    if (delimiter_depth > 0) {
        emit_error(length, 0, "Unclosed form",
                   "Add the missing closing delimiter(s)");
    }

    // If the token buffer overflowed we dropped tokens (including, possibly,
    // the EOF). Emit a clear diagnostic instead of silently truncating the
    // program — silent truncation produces baffling parse errors downstream.
    if (overflowed) {
        // Ensure the stream is still well-formed for the parser: force the
        // final slot to EOF so callers don't read past valid tokens.
        if (count > 0 && out[count - 1].kind != TokenKind::Eof) {
            out[count - 1] = eof;
        }
        emit_error(0, (uint16_t)(length > 0xFFFF ? 0xFFFF : length),
                   "Program too large (token limit exceeded)");
    }

    return count;
}

} // namespace sig
