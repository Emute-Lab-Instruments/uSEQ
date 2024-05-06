#ifndef ARDUINO

// #include "lisp/value.h"
// #include "uSEQ.h"
// #include "lisp/interpreter.h"
#include <iostream>

int main()
{
    std::cout << "Hello world!" << std::endl;
    // uSEQ useq(0, 0, 1, 1);
    // Interpreter i;
    // std::cout << "Parsing..." << std::endl;
    // String result = i.eval("(+ 1 2)");

    // useq.init();
    // std::cout << useq.parse("(+ 1 2)").as_string().c_str() << std::endl;
    // useq.run(); // blocks until a crash or until user requests exit

    // std::cout << result.c_str() << std::endl;
    return 0;
}

#endif
