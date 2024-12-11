#ifndef ARDUINO

// #include "lisp/value.h"
// #include "uSEQ.h"
#include "lisp/interpreter.h"
#include <iostream>

int main()
{
    std::cout << "Hello world!" << std::endl;
    Interpreter i;
    std::cout << i.eval("(+ 1 2)").c_str() << std::endl;
    return 0;
}

#endif
