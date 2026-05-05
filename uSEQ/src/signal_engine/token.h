#ifndef SIGNAL_ENGINE_TOKEN_H
#define SIGNAL_ENGINE_TOKEN_H

#include "types.h"
#include "diagnostics.h"

namespace sig {

// ── Token Types ─────────────────────────────────────────────────────────────

enum class TokenKind : uint8_t {
    LParen, RParen, LBracket, RBracket,
    Number, Symbol, String,
    Eof, Error
};

struct Token {
    TokenKind kind    = TokenKind::Eof;
    uint8_t pad       = 0;
    uint16_t span_start = 0;
    uint16_t span_len   = 0;
    union {
        double number;
        SymbolID symbol;
        struct { uint32_t offset; uint16_t length; } string;
    };

    Token() : number(0.0) {}
};
// sizeof(Token) == 16 bytes

// ── Token Stream ────────────────────────────────────────────────────────────

struct TokenStream {
    Token tokens[MAX_TOKENS];
    uint16_t count = 0;
    uint16_t pos   = 0;

    Token peek() const;
    Token consume();
    bool expect(TokenKind kind);
    void rewind(uint16_t position);
    bool at_end() const;

    // Zero-allocation tokenizer.
    // Returns number of tokens written. Parse errors are appended to `errors`.
    static uint16_t tokenize(const char* source, uint32_t length,
                             Token* out, uint16_t max_tokens,
                             Diagnostic* errors, uint8_t* error_count);
};

} // namespace sig

#endif // SIGNAL_ENGINE_TOKEN_H
