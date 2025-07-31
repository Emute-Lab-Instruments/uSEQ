#ifndef ARDUINO

#include "uSEQ.h"
#include "lisp/value.h"
#include "hardware_includes.h"
#include <iostream>
#include <string>
#include <sstream>

// Define extern variables for desktop build stubs
PIO pio0 = nullptr;
PIO pio1 = nullptr;
const pwm_program_struct pwm_program = {};
SerialStub Serial;

void print_banner() {
    std::cout << "uSEQ Standalone LISP Interpreter\n";
    std::cout << "Type expressions to evaluate, or 'quit' to exit.\n";
    std::cout << "Use 'help' for basic commands.\n\n";
}

void print_help() {
    std::cout << "Commands:\n";
    std::cout << "  help  - Show this help message\n";
    std::cout << "  quit  - Exit the interpreter\n";
    std::cout << "  exit  - Exit the interpreter\n";
    std::cout << "\nExamples:\n";
    std::cout << "  (+ 1 2 3)     ; Addition\n";
    std::cout << "  (* 5 6)       ; Multiplication\n";
    std::cout << "  (list 1 2 3)  ; Create a list\n";
    std::cout << "  (quote hello) ; Quote a symbol\n\n";
}

int main(int argc, char* argv[]) {
    // Initialize uSEQ instance
    uSEQ useq_instance;
    useq_instance.init();
    
    // Handle command line arguments
    if (argc > 1) {
        // Execute code from command line arguments
        std::stringstream code_stream;
        for (int i = 1; i < argc; i++) {
            code_stream << argv[i];
            if (i < argc - 1) code_stream << " ";
        }
        
        String code = String(code_stream.str().c_str());
        try {
            String result = useq_instance.eval(code);
            std::cout << result.c_str() << std::endl;
        } catch (...) {
            std::cerr << "Error evaluating expression" << std::endl;
            return 1;
        }
        return 0;
    }
    
    // Interactive REPL mode
    print_banner();
    
    std::string input;
    std::cout << "> ";
    
    while (std::getline(std::cin, input)) {
        // Trim whitespace
        input.erase(0, input.find_first_not_of(" \t\n\r"));
        input.erase(input.find_last_not_of(" \t\n\r") + 1);
        
        // Handle special commands
        if (input.empty()) {
            std::cout << "> ";
            continue;
        }
        
        if (input == "quit" || input == "exit") {
            std::cout << "Goodbye!\n";
            break;
        }
        
        if (input == "help") {
            print_help();
            std::cout << "> ";
            continue;
        }
        
        // Evaluate LISP expression
        try {
            String code = String(input.c_str());
            String result = useq_instance.eval(code);
            std::cout << result.c_str() << std::endl;
        } catch (const std::exception& e) {
            std::cout << "Error: " << e.what() << std::endl;
        } catch (...) {
            std::cout << "Error: Unknown exception occurred" << std::endl;
        }
        
        std::cout << "> ";
    }
    
    return 0;
}

#endif
