#include <iostream>
#include <cassert>
#include "../uSEQ/src/interfaces/test_helpers.h"

using namespace test_helpers;

void testBasicEvaluation() {
    std::cout << "Testing basic evaluation with minimal interfaces..." << std::endl;
    
    QuickInterpreter interp;
    
    // Test simple arithmetic
    auto result = interp.evalToString("(+ 1 2)");
    assert(result == "3");
    
    float numResult = interp.evalToNumber("(+ 10 20)");
    assert(test_utils::floatEquals(numResult, 30.0f));
    
    assert(interp.evalIsNumber("42"));
    assert(interp.evalIsString("\"hello\""));
    assert(interp.evalIsList("(list 1 2 3)"));
    
    std::cout << "  \u2713 Basic evaluation works" << std::endl;
}

void testUtilityFunctions() {
    std::cout << "Testing utility functions..." << std::endl;
    
    QuickInterpreter interp;
    
    // Test expression builders
    auto addExpr = test_utils::makeAddExpr(5.0f, 7.0f);
    float result = interp.evalToNumber(addExpr);
    assert(test_utils::floatEquals(result, 12.0f));
    
    auto listExpr = test_utils::makeListExpr(1.0f, 2.0f, 3.0f);
    size_t listSize = interp.evalListSize(listExpr);
    assert(listSize == 3);
    
    auto ifExpr = test_utils::makeIfExpr("t", "1", "2");
    float ifResult = interp.evalToNumber(ifExpr);
    assert(test_utils::floatEquals(ifResult, 1.0f));
    
    std::cout << "  \u2713 Utility functions work" << std::endl;
}

void testListProcessing() {
    std::cout << "Testing list processing with minimal interfaces..." << std::endl;
    
    QuickInterpreter interp;
    
    size_t listSize = interp.evalListSize("(list 10 20 30)");
    assert(listSize == 3);
    
    assert(interp.evalIsList("(list 1 2 3)"));
    assert(!interp.evalIsList("42"));
    
    // Test list operations
    float firstResult = interp.evalToNumber("(first (list 10 20 30))");
    assert(test_utils::floatEquals(firstResult, 10.0f));
    
    std::cout << "  \u2713 List processing works" << std::endl;
}

int main() {
    std::cout << "\n=== Testing with Minimal Interfaces ===" << std::endl;
    std::cout << "This test demonstrates faster compilation by using minimal interfaces." << std::endl;
    std::cout << "Tests avoid including heavy implementation headers.\n" << std::endl;
    
    try {
        testBasicEvaluation();
        testUtilityFunctions();
        testListProcessing();
        
        std::cout << "\n\u2713 All tests using minimal interfaces passed!" << std::endl;
        std::cout << "The test compiled faster by avoiding heavy headers." << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n\u2717 Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}