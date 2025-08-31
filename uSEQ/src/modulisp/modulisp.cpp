#include "modulisp.h"

// Constructor
ModuLisp::ModuLisp() {
    // Create the interpreter with default settings
    interpreter = std::make_unique<ModuLispInterpreter>();
    
    // Initialize the interpreter
    interpreter->init();
    interpreter->init_builtinfuncs();
}

// Destructor
ModuLisp::~ModuLisp() = default;

// Main API: send LISP code and get response
ModuLisp::Response ModuLisp::send(const std::string& code) {
    // Convert std::string to Arduino String
    String arduino_code(code.c_str());
    
    // Parse and evaluate the code
    Value result = interpreter->eval_v(arduino_code);
    
    // Wrap the result in a Response object
    return Response(result);
}