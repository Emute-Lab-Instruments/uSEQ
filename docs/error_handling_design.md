# ModuLisp Error Handling Design

## Overview

This document describes the comprehensive error handling system designed for ModuLisp, a live coding language for creative audio and visual programming. The system prioritizes user-friendly error reporting for non-programmers who are using the language for artistic expression.

## Design Philosophy

The error handling system is designed with these principles:

1. **Clear Communication**: Error messages should be immediately understandable
2. **Helpful Suggestions**: Every error should suggest how to fix it
3. **Context Awareness**: Show where the error occurred with visual markers
4. **Progressive Disclosure**: Simple message first, detailed help available
5. **Learning Support**: Include examples to teach correct usage
6. **Non-Intimidating**: Avoid technical jargon when possible

## Response Class API

The `ModuLisp::Response` class encapsulates all evaluation results:

```cpp
class Response {
    // Factory methods
    static Response success(const Value& value);
    static Response error(const std::string& message, ErrorType type);
    
    // Status checking
    bool is_success() const;
    bool is_error() const;
    
    // Error information
    ErrorType get_error_type() const;
    const std::string& get_error_message() const;
    const std::string& get_detailed_error() const;
    
    // Location tracking
    int get_line() const;
    int get_column() const;
    
    // Help and suggestions
    const std::string& get_suggestion() const;
    const std::vector<std::string>& get_did_you_mean() const;
    const std::vector<std::string>& get_examples() const;
    
    // Context
    const std::string& get_context_snippet() const;
    const std::vector<std::string>& get_stack_trace() const;
    
    // Warnings (non-fatal)
    void add_warning(const std::string& warning);
    const std::vector<std::string>& get_warnings() const;
    
    // Output formatting
    std::string format_for_terminal() const;
    std::string to_json() const;
    std::string to_log_format() const;
};
```

## Error Categories

### 1. Syntax Errors
- **Unbalanced parentheses**: Shows exactly where the missing/extra paren is
- **Unclosed strings**: Points to the start of the unclosed string
- **Invalid characters**: Explains what characters are allowed

Example:
```
Error: line 1, column 40
Missing closing parenthesis
(defn make-beat [phase] (sin (* phase 2)
                                        ^-- Expected ')' here
Suggestion: Add ')' at the end of your expression
```

### 2. Undefined References
- **Function typos**: Uses Levenshtein distance to suggest similar functions
- **Variable typos**: Suggests similar variable names
- **Case sensitivity**: Detects case mismatches

Example:
```
Error: line 1, column 2
Function 'sni' is not defined
Did you mean:
  • sin
Suggestion: Did you mean 'sin'? (sine wave function)
```

### 3. Argument Errors
- **Wrong arity**: Shows expected vs actual argument count
- **Type mismatches**: Explains what type was expected
- **Missing required arguments**: Shows usage examples

Example:
```
Error: line 1, column 1
Function 'sin' expects exactly 1 argument, but got 2
Suggestion: Usage: (sin phase)
Example: (sin 0.5) returns sine wave value at phase 0.5
```

### 4. Runtime Errors
- **Division by zero**: Common in envelope calculations
- **Index out of bounds**: Shows valid range
- **Stack overflow**: Suggests adding base case
- **Timeouts**: Suggests optimization

Example:
```
Error: line 1, column 12
Index 5 is out of bounds for list of size 4
Suggestion: Valid indices are 0 to 3. Use (count notes) to check list size.
```

### 5. Live Coding Specific
- **Audio parameters**: Frequency, amplitude ranges
- **MIDI values**: 0-127 range with note names
- **Timing errors**: Cannot schedule in the past
- **Pattern errors**: Sequence and pattern issues

Example:
```
Error: line 1, column 12
MIDI note 128 is out of range (0-127)
Suggestion: MIDI notes range from 0 (C-1) to 127 (G9).
Middle C (C4) is 60.
Example: (note-on 60) plays middle C
```

## Smart Features

### Typo Detection
The system uses Levenshtein distance algorithm to find similar function and variable names:
- Distance ≤ 1: Very likely typo (shown first)
- Distance ≤ 2: Possible alternatives
- Case-only differences: Prioritized

### Context-Aware Suggestions
Different contexts provide different suggestions:
- **Audio context**: Prioritizes audio functions (oscillator, filter, etc.)
- **Pattern context**: Prioritizes sequencing functions
- **Visual context**: Prioritizes graphics functions

### Pattern Recognition
Common beginner mistakes are detected:
- Missing quotes on strings: `(println Hello)` → suggests `"Hello"`
- Forgot brackets on lists: `(def notes (60 62))` → suggests `[60 62]`
- Wrong equality operator: Helps with `=` vs `==` confusion

### Progressive Error Messages
Each error has multiple levels of detail:
1. **Simple message**: One-line explanation
2. **Suggestion**: How to fix it
3. **Detailed error**: Full explanation with context
4. **Examples**: Working code examples
5. **Stack trace**: For debugging (when relevant)

## Warning System

Non-critical issues that don't stop execution:
- **Deprecated functions**: Suggests modern alternatives
- **Performance warnings**: Large computations
- **Unused variables**: Helps clean up code
- **Unusual values**: BPM too fast/slow, etc.

Example:
```
Success: 120
Warnings:
  • Function 'set-tempo' is deprecated. Use 'set-bpm' instead.
```

## Output Formats

### Terminal Format
Color-coded with ANSI escape sequences:
- Red for errors
- Yellow for warnings and suggestions
- Context snippet with error markers

### JSON Format
Structured for editor integration:
```json
{
  "error": true,
  "type": "UNDEFINED_FUNCTION",
  "message": "Function 'sni' is not defined",
  "line": 1,
  "column": 2,
  "suggestions": ["sin"],
  "suggestion": "Did you mean 'sin'?",
  "context": "(sni 0.5)\n ^^^^^"
}
```

### Log Format
Compact single-line for logging:
```
ERROR UNDEFINED_FUNCTION 1:2 Function 'sni' is not defined
```

## Testing Strategy

The test suite (`test_error_handling.cpp`) covers:
1. All error types with realistic live coding scenarios
2. Edge cases and boundary conditions
3. Multi-line code with proper line tracking
4. Suggestion accuracy and relevance
5. Warning system behavior
6. Output format correctness

## Implementation Notes

### Helper Functions
- `levenshtein_distance()`: Calculate edit distance between strings
- `find_similar_names()`: Find typo suggestions
- `looks_like_string_literal()`: Detect missing quotes
- `get_context_lines()`: Extract code around error
- `format_error_marker()`: Create visual error indicators

### Future Enhancements
1. **Machine Learning**: Learn from common user mistakes
2. **Interactive Fixes**: Offer to apply suggested fixes
3. **Error Recovery**: Continue evaluation after errors
4. **Chained Errors**: Show related errors together
5. **Localization**: Multi-language error messages

## Usage Example

```cpp
ModuLispInterpreter interp;
auto response = interp.evaluate("(sni 0.5)");

if (response.is_error()) {
    // Terminal output
    std::cout << response.format_for_terminal();
    
    // Or JSON for editor
    send_to_editor(response.to_json());
    
    // Or check specific fields
    if (!response.get_did_you_mean().empty()) {
        auto_complete(response.get_did_you_mean()[0]);
    }
}
```

## Benefits for Users

1. **Faster Learning**: Clear examples teach correct usage
2. **Less Frustration**: Helpful suggestions reduce trial and error
3. **Better Understanding**: Context shows exactly where problems are
4. **Confidence Building**: Non-intimidating messages encourage experimentation
5. **Performance Awareness**: Warnings help optimize code

This error handling system transforms ModuLisp from a potentially frustrating experience into an educational and supportive environment for creative expression.