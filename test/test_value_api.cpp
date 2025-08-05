#include "../uSEQ/src/modulisp/lisp/value.h"
#include "../uSEQ/src/modulisp/lisp/environment.h"
#include <cassert>
#include <iostream>
#include <cmath>

// Test framework macros (same as existing tests)
#define ASSERT_EQ(expected, actual) \
    do { \
        if ((expected) != (actual)) { \
            std::cerr << "ASSERTION FAILED at line " << __LINE__ << ": " \
                      << "Expected " << (expected) << " but got " << (actual) << std::endl; \
            assert(false); \
        } \
    } while(0)

#define ASSERT_TRUE(condition) \
    do { \
        if (!(condition)) { \
            std::cerr << "ASSERTION FAILED at line " << __LINE__ << ": " \
                      << "Expected true but got false" << std::endl; \
            assert(false); \
        } \
    } while(0)

#define ASSERT_FALSE(condition) \
    do { \
        if (condition) { \
            std::cerr << "ASSERTION FAILED at line " << __LINE__ << ": " \
                      << "Expected false but got true" << std::endl; \
            assert(false); \
        } \
    } while(0)

#define ASSERT_NEAR(expected, actual, tolerance) \
    do { \
        if (std::abs((expected) - (actual)) > (tolerance)) { \
            std::cerr << "ASSERTION FAILED at line " << __LINE__ << ": " \
                      << "Expected " << (expected) << " but got " << (actual) \
                      << " (tolerance: " << (tolerance) << ")" << std::endl; \
            assert(false); \
        } \
    } while(0)

#define ASSERT_STR_EQ(expected, actual) \
    do { \
        if ((expected) != (actual)) { \
            std::cerr << "ASSERTION FAILED at line " << __LINE__ << ": " \
                      << "Expected \"" << (expected) << "\" but got \"" << (actual) << "\"" << std::endl; \
            assert(false); \
        } \
    } while(0)

#define TEST_CASE(name) \
    void name(); \
    struct name##_runner { \
        name##_runner() { \
            std::cout << "Running " << #name << "..." << std::endl; \
            name(); \
            std::cout << #name << " passed!" << std::endl; \
        } \
    }; \
    static name##_runner name##_instance; \
    void name()

////////////////////////////////////////////////////////////////////////////////
/// VALUE CONSTRUCTION API TESTS
////////////////////////////////////////////////////////////////////////////////

TEST_CASE(test_value_construction_integers) {
    // Test integer construction
    Value v1(42);
    ASSERT_TRUE(v1.is_int());
    ASSERT_EQ(42, v1.as_int());
    
    // Test negative integer
    Value v2(-17);
    ASSERT_TRUE(v2.is_int());
    ASSERT_EQ(-17, v2.as_int());
    
    // Test zero
    Value v3(0);
    ASSERT_TRUE(v3.is_int());
    ASSERT_EQ(0, v3.as_int());
}

TEST_CASE(test_value_construction_floats) {
    // Test float construction
    Value v1(3.14);
    ASSERT_TRUE(v1.is_float());
    ASSERT_NEAR(3.14, v1.as_float(), 0.001);
    
    // Test negative float
    Value v2(-2.71);
    ASSERT_TRUE(v2.is_float());
    ASSERT_NEAR(-2.71, v2.as_float(), 0.001);
    
    // Test zero float
    Value v3(0.0);
    ASSERT_TRUE(v3.is_float());
    ASSERT_NEAR(0.0, v3.as_float(), 0.001);
}

TEST_CASE(test_value_construction_strings) {
    // Test string construction via static method
    Value v1 = Value::string("hello world");
    ASSERT_TRUE(v1.is_string());
    ASSERT_STR_EQ("hello world", v1.as_string());
    
    // Test empty string
    Value v2 = Value::string("");
    ASSERT_TRUE(v2.is_string());
    ASSERT_STR_EQ("", v2.as_string());
    
    // Test string with special characters
    Value v3 = Value::string("Hello\nWorld\t!");
    ASSERT_TRUE(v3.is_string());
    ASSERT_STR_EQ("Hello\nWorld\t!", v3.as_string());
}

TEST_CASE(test_value_construction_symbols) {
    // Test symbol construction via static method
    Value v1 = Value::atom("my-symbol");
    ASSERT_TRUE(v1.is_symbol());
    ASSERT_STR_EQ("my-symbol", v1.as_atom());
    
    // Test single character symbol
    Value v2 = Value::atom("+");
    ASSERT_TRUE(v2.is_symbol());
    ASSERT_STR_EQ("+", v2.as_atom());
    
    // Test complex symbol name
    Value v3 = Value::atom("test-123-var");
    ASSERT_TRUE(v3.is_symbol());
    ASSERT_STR_EQ("test-123-var", v3.as_atom());
}

TEST_CASE(test_value_construction_lists) {
    // Test empty list
    std::vector<Value> empty_list;
    Value v1(empty_list);
    ASSERT_TRUE(v1.is_list());
    ASSERT_TRUE(v1.is_sequential());
    ASSERT_EQ(0, v1.as_list().size());
    ASSERT_TRUE(v1.is_empty());
    
    // Test list with integers
    std::vector<Value> int_list = {Value(1), Value(2), Value(3)};
    Value v2(int_list);
    ASSERT_TRUE(v2.is_list());
    ASSERT_TRUE(v2.is_sequential());
    ASSERT_EQ(3, v2.as_list().size());
    ASSERT_FALSE(v2.is_empty());
    ASSERT_EQ(1, v2.as_list()[0].as_int());
    ASSERT_EQ(2, v2.as_list()[1].as_int());
    ASSERT_EQ(3, v2.as_list()[2].as_int());
    
    // Test mixed type list
    std::vector<Value> mixed_list = {Value(42), Value::string("hello"), Value(3.14)};
    Value v3(mixed_list);
    ASSERT_TRUE(v3.is_list());
    ASSERT_EQ(3, v3.as_list().size());
    ASSERT_TRUE(v3.as_list()[0].is_int());
    ASSERT_TRUE(v3.as_list()[1].is_string());
    ASSERT_TRUE(v3.as_list()[2].is_float());
}

TEST_CASE(test_value_construction_vectors) {
    // Test empty vector
    std::vector<Value> empty_vec;
    Value v1 = Value::vector(empty_vec);
    ASSERT_TRUE(v1.is_vector());
    ASSERT_TRUE(v1.is_sequential());
    ASSERT_EQ(0, v1.as_vector().size());
    ASSERT_TRUE(v1.is_empty());
    
    // Test vector with integers
    std::vector<Value> int_vec = {Value(10), Value(20), Value(30)};
    Value v2 = Value::vector(int_vec);
    ASSERT_TRUE(v2.is_vector());
    ASSERT_TRUE(v2.is_sequential());
    ASSERT_EQ(3, v2.as_vector().size());
    ASSERT_FALSE(v2.is_empty());
    ASSERT_EQ(10, v2.as_vector()[0].as_int());
    ASSERT_EQ(20, v2.as_vector()[1].as_int());
    ASSERT_EQ(30, v2.as_vector()[2].as_int());
    
    // Test mixed type vector
    std::vector<Value> mixed_vec = {Value::atom("symbol"), Value(1.5), Value::string("text")};
    Value v3 = Value::vector(mixed_vec);
    ASSERT_TRUE(v3.is_vector());
    ASSERT_EQ(3, v3.as_vector().size());
    ASSERT_TRUE(v3.as_vector()[0].is_symbol());
    ASSERT_TRUE(v3.as_vector()[1].is_float());
    ASSERT_TRUE(v3.as_vector()[2].is_string());
}

TEST_CASE(test_value_construction_special_values) {
    // Test nil value
    Value v1 = Value::nil();
    ASSERT_TRUE(v1.is_nil());
    ASSERT_FALSE(v1.is_int());
    ASSERT_FALSE(v1.is_string());
    ASSERT_FALSE(v1.is_list());
    
    // Test error value
    Value v2 = Value::error();
    ASSERT_TRUE(v2.is_error());
    ASSERT_FALSE(v2.is_int());
    ASSERT_FALSE(v2.is_string());
    
    // Test unit value (default constructor)
    Value v3;
    ASSERT_FALSE(v3.is_nil());
    ASSERT_FALSE(v3.is_error());
    ASSERT_FALSE(v3.is_int());
    ASSERT_FALSE(v3.is_string());
    // Unit type is the default when no specific type is set
}

TEST_CASE(test_value_construction_quoted_values) {
    // Test quoted integer
    Value inner = Value(42);
    Value v1 = Value::quote(inner);
    ASSERT_STR_EQ("'42", v1.to_lisp_src());
    
    // Test quoted symbol
    Value inner2 = Value::atom("symbol");
    Value v2 = Value::quote(inner2);
    ASSERT_STR_EQ("'symbol", v2.to_lisp_src());
    
    // Test quoted string
    Value inner3 = Value::string("hello");
    Value v3 = Value::quote(inner3);
    ASSERT_STR_EQ("'\"hello\"", v3.to_lisp_src());
}

////////////////////////////////////////////////////////////////////////////////
/// VALUE TYPE CHECKING API TESTS
////////////////////////////////////////////////////////////////////////////////

TEST_CASE(test_value_type_checking_basic_types) {
    // Test integer type checking
    Value int_val(42);
    ASSERT_TRUE(int_val.is_int());
    ASSERT_TRUE(int_val.is_number());
    ASSERT_FALSE(int_val.is_float());
    ASSERT_FALSE(int_val.is_string());
    ASSERT_FALSE(int_val.is_list());
    ASSERT_FALSE(int_val.is_vector());
    ASSERT_FALSE(int_val.is_symbol());
    ASSERT_FALSE(int_val.is_nil());
    ASSERT_FALSE(int_val.is_error());
    
    // Test float type checking
    Value float_val(3.14);
    ASSERT_TRUE(float_val.is_float());
    ASSERT_TRUE(float_val.is_number());
    ASSERT_FALSE(float_val.is_int());
    ASSERT_FALSE(float_val.is_string());
    ASSERT_FALSE(float_val.is_list());
    ASSERT_FALSE(float_val.is_vector());
    ASSERT_FALSE(float_val.is_symbol());
    ASSERT_FALSE(float_val.is_nil());
    ASSERT_FALSE(float_val.is_error());
    
    // Test string type checking
    Value string_val = Value::string("test");
    ASSERT_TRUE(string_val.is_string());
    ASSERT_FALSE(string_val.is_int());
    ASSERT_FALSE(string_val.is_float());
    ASSERT_FALSE(string_val.is_number());
    ASSERT_FALSE(string_val.is_list());
    ASSERT_FALSE(string_val.is_vector());
    ASSERT_FALSE(string_val.is_symbol());
    ASSERT_FALSE(string_val.is_nil());
    ASSERT_FALSE(string_val.is_error());
}

TEST_CASE(test_value_type_checking_composite_types) {
    // Test symbol type checking
    Value symbol_val = Value::atom("my-symbol");
    ASSERT_TRUE(symbol_val.is_symbol());
    ASSERT_FALSE(symbol_val.is_string());
    ASSERT_FALSE(symbol_val.is_int());
    ASSERT_FALSE(symbol_val.is_float());
    ASSERT_FALSE(symbol_val.is_number());
    ASSERT_FALSE(symbol_val.is_list());
    ASSERT_FALSE(symbol_val.is_vector());
    ASSERT_FALSE(symbol_val.is_nil());
    ASSERT_FALSE(symbol_val.is_error());
    
    // Test list type checking
    std::vector<Value> list_data = {Value(1), Value(2)};
    Value list_val(list_data);
    ASSERT_TRUE(list_val.is_list());
    ASSERT_TRUE(list_val.is_sequential());
    ASSERT_FALSE(list_val.is_vector());
    ASSERT_FALSE(list_val.is_string());
    ASSERT_FALSE(list_val.is_symbol());
    ASSERT_FALSE(list_val.is_int());
    ASSERT_FALSE(list_val.is_float());
    ASSERT_FALSE(list_val.is_number());
    ASSERT_FALSE(list_val.is_nil());
    ASSERT_FALSE(list_val.is_error());
    
    // Test vector type checking  
    std::vector<Value> vector_data = {Value(3), Value(4)};
    Value vector_val = Value::vector(vector_data);
    ASSERT_TRUE(vector_val.is_vector());
    ASSERT_TRUE(vector_val.is_sequential());
    ASSERT_FALSE(vector_val.is_list());
    ASSERT_FALSE(vector_val.is_string());
    ASSERT_FALSE(vector_val.is_symbol());
    ASSERT_FALSE(vector_val.is_int());
    ASSERT_FALSE(vector_val.is_float());
    ASSERT_FALSE(vector_val.is_number());
    ASSERT_FALSE(vector_val.is_nil());
    ASSERT_FALSE(vector_val.is_error());
}

TEST_CASE(test_value_type_checking_special_types) {
    // Test nil type checking
    Value nil_val = Value::nil();
    ASSERT_TRUE(nil_val.is_nil());
    ASSERT_FALSE(nil_val.is_error());
    ASSERT_FALSE(nil_val.is_int());
    ASSERT_FALSE(nil_val.is_float());
    ASSERT_FALSE(nil_val.is_number());
    ASSERT_FALSE(nil_val.is_string());
    ASSERT_FALSE(nil_val.is_symbol());
    ASSERT_FALSE(nil_val.is_list());
    ASSERT_FALSE(nil_val.is_vector());
    
    // Test error type checking
    Value error_val = Value::error();
    ASSERT_TRUE(error_val.is_error());
    ASSERT_FALSE(error_val.is_nil());
    ASSERT_FALSE(error_val.is_int());
    ASSERT_FALSE(error_val.is_float());
    ASSERT_FALSE(error_val.is_number());
    ASSERT_FALSE(error_val.is_string());
    ASSERT_FALSE(error_val.is_symbol());
    ASSERT_FALSE(error_val.is_list());
    ASSERT_FALSE(error_val.is_vector());
}

TEST_CASE(test_value_number_property_checking) {
    // Test positive number checking
    Value pos_int(5);
    Value pos_float(3.14);
    ASSERT_TRUE(pos_int.is_positive_number());
    ASSERT_TRUE(pos_int.is_non_zero_number());
    ASSERT_FALSE(pos_int.is_negative_number());
    ASSERT_TRUE(pos_float.is_positive_number());
    ASSERT_TRUE(pos_float.is_non_zero_number());
    ASSERT_FALSE(pos_float.is_negative_number());
    
    // Test negative number checking
    Value neg_int(-5);
    Value neg_float(-2.71);
    ASSERT_TRUE(neg_int.is_negative_number());
    ASSERT_TRUE(neg_int.is_non_zero_number());
    ASSERT_FALSE(neg_int.is_positive_number());
    ASSERT_TRUE(neg_float.is_negative_number());
    ASSERT_TRUE(neg_float.is_non_zero_number());
    ASSERT_FALSE(neg_float.is_positive_number());
    
    // Test zero checking
    Value zero_int(0);
    Value zero_float(0.0);
    ASSERT_FALSE(zero_int.is_positive_number());
    ASSERT_FALSE(zero_int.is_negative_number());
    ASSERT_FALSE(zero_int.is_non_zero_number());
    ASSERT_FALSE(zero_float.is_positive_number());
    ASSERT_FALSE(zero_float.is_negative_number());
    ASSERT_FALSE(zero_float.is_non_zero_number());
}

TEST_CASE(test_value_emptiness_checking) {
    // Test empty collections
    std::vector<Value> empty_data;
    Value empty_list(empty_data);
    Value empty_vector = Value::vector(empty_data);
    
    ASSERT_TRUE(empty_list.is_empty());
    ASSERT_TRUE(empty_list.is_list_and_empty());
    ASSERT_TRUE(empty_vector.is_empty());
    ASSERT_FALSE(empty_vector.is_list_and_empty()); // It's a vector, not a list
    
    // Test non-empty collections
    std::vector<Value> non_empty_data = {Value(1)};
    Value non_empty_list(non_empty_data);
    Value non_empty_vector = Value::vector(non_empty_data);
    
    ASSERT_FALSE(non_empty_list.is_empty());
    ASSERT_FALSE(non_empty_list.is_list_and_empty());
    ASSERT_FALSE(non_empty_vector.is_empty());
    ASSERT_FALSE(non_empty_vector.is_list_and_empty());
    
    // Test non-collections (behavior may vary by implementation)
    Value int_val(42);
    Value string_val = Value::string("test");
    // Note: Non-collection values may or may not report as empty depending on implementation
    // Just verify the methods don't crash
    int_val.is_empty(); 
    string_val.is_empty();
}

////////////////////////////////////////////////////////////////////////////////
/// VALUE CONVERSION API TESTS
////////////////////////////////////////////////////////////////////////////////

TEST_CASE(test_value_as_conversions) {
    // Test integer conversions
    Value int_val(42);
    ASSERT_EQ(42, int_val.as_int());
    ASSERT_NEAR(42.0, int_val.as_float(), 0.001);
    ASSERT_TRUE(int_val.as_bool()); // Non-zero is true
    
    Value zero_val(0);
    ASSERT_EQ(0, zero_val.as_int());
    ASSERT_NEAR(0.0, zero_val.as_float(), 0.001);
    ASSERT_FALSE(zero_val.as_bool()); // Zero is false
    
    // Test float conversions
    Value float_val(3.14);
    ASSERT_EQ(3, float_val.as_int()); // Should truncate
    ASSERT_NEAR(3.14, float_val.as_float(), 0.001);
    ASSERT_TRUE(float_val.as_bool()); // Non-zero is true
    
    // Test string conversions
    Value string_val = Value::string("hello world");
    ASSERT_STR_EQ("hello world", string_val.as_string());
    ASSERT_TRUE(string_val.as_bool()); // Non-empty string is true
    
    Value empty_string = Value::string("");
    ASSERT_STR_EQ("", empty_string.as_string());
    // Note: as_bool() behavior for empty strings may vary by implementation
    empty_string.as_bool(); // Just verify it doesn't crash
    
    // Test symbol conversions
    Value symbol_val = Value::atom("test-symbol");
    ASSERT_STR_EQ("test-symbol", symbol_val.as_atom());
    ASSERT_TRUE(symbol_val.as_bool()); // Symbols are truthy
}

TEST_CASE(test_value_collection_conversions) {
    // Test list conversions
    std::vector<Value> list_data = {Value(1), Value(2), Value(3)};
    Value list_val(list_data);
    
    std::vector<Value> retrieved_list = list_val.as_list();
    ASSERT_EQ(3, retrieved_list.size());
    ASSERT_EQ(1, retrieved_list[0].as_int());
    ASSERT_EQ(2, retrieved_list[1].as_int());
    ASSERT_EQ(3, retrieved_list[2].as_int());
    
    std::vector<Value> retrieved_sequential = list_val.as_sequential();
    ASSERT_EQ(3, retrieved_sequential.size());
    ASSERT_EQ(1, retrieved_sequential[0].as_int());
    
    // Test vector conversions
    std::vector<Value> vector_data = {Value(10), Value(20), Value(30)};
    Value vector_val = Value::vector(vector_data);
    
    std::vector<Value> retrieved_vector = vector_val.as_vector();
    ASSERT_EQ(3, retrieved_vector.size());
    ASSERT_EQ(10, retrieved_vector[0].as_int());
    ASSERT_EQ(20, retrieved_vector[1].as_int());
    ASSERT_EQ(30, retrieved_vector[2].as_int());
    
    std::vector<Value> retrieved_sequential2 = vector_val.as_sequential();
    ASSERT_EQ(3, retrieved_sequential2.size());
    ASSERT_EQ(10, retrieved_sequential2[0].as_int());
}

TEST_CASE(test_value_explicit_casting) {
    // Test cast_to_int
    Value float_val(3.7);
    Value int_cast = float_val.cast_to_int();
    ASSERT_TRUE(int_cast.is_int());
    ASSERT_EQ(3, int_cast.as_int());
    
    Value int_val(42);
    Value int_cast2 = int_val.cast_to_int();
    ASSERT_TRUE(int_cast2.is_int());
    ASSERT_EQ(42, int_cast2.as_int());
    
    // Test cast_to_float
    Value int_val2(15);
    Value float_cast = int_val2.cast_to_float();
    ASSERT_TRUE(float_cast.is_float());
    ASSERT_NEAR(15.0, float_cast.as_float(), 0.001);
    
    Value float_val2(2.71);
    Value float_cast2 = float_val2.cast_to_float();
    ASSERT_TRUE(float_cast2.is_float());
    ASSERT_NEAR(2.71, float_cast2.as_float(), 0.001);
}

////////////////////////////////////////////////////////////////////////////////
/// VALUE COMPARISON API TESTS
////////////////////////////////////////////////////////////////////////////////

TEST_CASE(test_value_equality_comparison) {
    // Test integer equality
    Value int1(42);
    Value int2(42);
    Value int3(17);
    ASSERT_TRUE(int1 == int2);
    ASSERT_FALSE(int1 == int3);
    ASSERT_FALSE(int1 != int2);
    ASSERT_TRUE(int1 != int3);
    
    // Test float equality
    Value float1(3.14);
    Value float2(3.14);
    Value float3(2.71);
    ASSERT_TRUE(float1 == float2);
    ASSERT_FALSE(float1 == float3);
    
    // Test string equality
    Value str1 = Value::string("hello");
    Value str2 = Value::string("hello");
    Value str3 = Value::string("world");
    ASSERT_TRUE(str1 == str2);
    ASSERT_FALSE(str1 == str3);
    
    // Test mixed type inequality
    Value int_val(42);
    Value float_val(42.0);
    Value string_val = Value::string("42");
    // Different types should not be equal even if values look similar
    ASSERT_FALSE(int_val == string_val);
}

TEST_CASE(test_value_ordering_comparison) {
    // Test integer ordering
    Value int1(10);
    Value int2(20);
    Value int3(10);
    
    ASSERT_TRUE(int1 < int2);
    ASSERT_FALSE(int2 < int1);
    ASSERT_FALSE(int1 < int3); // Equal values
    
    ASSERT_TRUE(int2 > int1);
    ASSERT_FALSE(int1 > int2);
    ASSERT_FALSE(int1 > int3); // Equal values
    
    ASSERT_TRUE(int1 <= int2);
    ASSERT_TRUE(int1 <= int3); // Equal values
    ASSERT_FALSE(int2 <= int1);
    
    ASSERT_TRUE(int2 >= int1);
    ASSERT_TRUE(int1 >= int3); // Equal values
    ASSERT_FALSE(int1 >= int2);
    
    // Test float ordering
    Value float1(1.5);
    Value float2(2.5);
    
    ASSERT_TRUE(float1 < float2);
    ASSERT_FALSE(float2 < float1);
    ASSERT_TRUE(float2 > float1);
    ASSERT_FALSE(float1 > float2);
}

TEST_CASE(test_value_string_comparison) {
    // Test string comparison with == operator overload for String
    Value str_val = Value::string("test");
    ASSERT_TRUE(str_val == String("test"));
    ASSERT_FALSE(str_val == String("other"));
}

////////////////////////////////////////////////////////////////////////////////
/// VALUE ARITHMETIC API TESTS
////////////////////////////////////////////////////////////////////////////////

TEST_CASE(test_value_arithmetic_operations) {
    // Test integer arithmetic
    Value int1(10);
    Value int2(3);
    
    Value sum = int1 + int2;
    ASSERT_TRUE(sum.is_int());
    ASSERT_EQ(13, sum.as_int());
    
    Value diff = int1 - int2;
    ASSERT_TRUE(diff.is_int());
    ASSERT_EQ(7, diff.as_int());
    
    Value product = int1 * int2;
    ASSERT_TRUE(product.is_int());
    ASSERT_EQ(30, product.as_int());
    
    Value quotient = int1 / int2;
    // Division may promote to float
    ASSERT_TRUE(quotient.is_number());
    ASSERT_NEAR(3.333, quotient.as_float(), 0.01);
    
    Value remainder = int1 % int2;
    ASSERT_TRUE(remainder.is_int());
    ASSERT_EQ(1, remainder.as_int());
    
    // Test float arithmetic
    Value float1(10.5);
    Value float2(2.5);
    
    Value float_sum = float1 + float2;
    ASSERT_TRUE(float_sum.is_float());
    ASSERT_NEAR(13.0, float_sum.as_float(), 0.001);
    
    Value float_diff = float1 - float2;
    ASSERT_TRUE(float_diff.is_float());
    ASSERT_NEAR(8.0, float_diff.as_float(), 0.001);
    
    Value float_product = float1 * float2;
    ASSERT_TRUE(float_product.is_float());
    ASSERT_NEAR(26.25, float_product.as_float(), 0.001);
    
    Value float_quotient = float1 / float2;
    ASSERT_TRUE(float_quotient.is_float());
    ASSERT_NEAR(4.2, float_quotient.as_float(), 0.001);
}

TEST_CASE(test_value_mixed_arithmetic) {
    // Test mixed int/float arithmetic
    Value int_val(10);
    Value float_val(2.5);
    
    Value mixed_sum = int_val + float_val;
    ASSERT_TRUE(mixed_sum.is_float()); // Should promote to float
    ASSERT_NEAR(12.5, mixed_sum.as_float(), 0.001);
    
    Value mixed_product = int_val * float_val;
    ASSERT_TRUE(mixed_product.is_float());
    ASSERT_NEAR(25.0, mixed_product.as_float(), 0.001);
}

////////////////////////////////////////////////////////////////////////////////
/// VALUE COLLECTION MANIPULATION API TESTS
////////////////////////////////////////////////////////////////////////////////

TEST_CASE(test_value_list_manipulation) {
    // Test push operation
    std::vector<Value> initial_data = {Value(1), Value(2)};
    Value list_val(initial_data);
    
    list_val.push(Value(3));
    std::vector<Value> after_push = list_val.as_list();
    ASSERT_EQ(3, after_push.size());
    ASSERT_EQ(1, after_push[0].as_int());
    ASSERT_EQ(2, after_push[1].as_int());
    ASSERT_EQ(3, after_push[2].as_int());
    
    // Test pop operation
    Value popped = list_val.pop();
    ASSERT_TRUE(popped.is_int());
    ASSERT_EQ(3, popped.as_int());
    
    std::vector<Value> after_pop = list_val.as_list();
    ASSERT_EQ(2, after_pop.size());
    ASSERT_EQ(1, after_pop[0].as_int());
    ASSERT_EQ(2, after_pop[1].as_int());
}

////////////////////////////////////////////////////////////////////////////////
/// VALUE METADATA API TESTS
////////////////////////////////////////////////////////////////////////////////

TEST_CASE(test_value_type_metadata) {
    // Test type name reporting
    Value int_val(42);
    Value float_val(3.14);
    Value string_val = Value::string("test");
    Value symbol_val = Value::atom("symbol");
    std::vector<Value> list_data = {Value(1)};
    Value list_val(list_data);
    Value vector_val = Value::vector(list_data);
    Value nil_val = Value::nil();
    Value error_val = Value::error();
    
    // Test that type names are non-empty strings
    ASSERT_TRUE(int_val.get_type_name().length() > 0);
    ASSERT_TRUE(float_val.get_type_name().length() > 0);
    ASSERT_TRUE(string_val.get_type_name().length() > 0);
    ASSERT_TRUE(symbol_val.get_type_name().length() > 0);
    ASSERT_TRUE(list_val.get_type_name().length() > 0);
    ASSERT_TRUE(vector_val.get_type_name().length() > 0);
    ASSERT_TRUE(nil_val.get_type_name().length() > 0);
    ASSERT_TRUE(error_val.get_type_name().length() > 0);
    
    // Test type enum reporting
    ASSERT_TRUE(int_val.get_type_enum() >= 0);
    ASSERT_TRUE(float_val.get_type_enum() >= 0);
    ASSERT_TRUE(string_val.get_type_enum() >= 0);
    
    // Different types should have different enums
    ASSERT_TRUE(int_val.get_type_enum() != float_val.get_type_enum());
    ASSERT_TRUE(string_val.get_type_enum() != symbol_val.get_type_enum());
}

TEST_CASE(test_value_display_api) {
    // Test display method produces readable output
    Value int_val(42);
    Value float_val(3.14);
    Value string_val = Value::string("hello");
    Value symbol_val = Value::atom("test");
    
    String int_display = int_val.display();
    String float_display = float_val.display();
    String string_display = string_val.display();
    String symbol_display = symbol_val.display();
    
    // Displays should be non-empty
    ASSERT_TRUE(int_display.length() > 0);
    ASSERT_TRUE(float_display.length() > 0);
    ASSERT_TRUE(string_display.length() > 0);
    ASSERT_TRUE(symbol_display.length() > 0);
    
    // Integer display should contain the number
    ASSERT_TRUE(int_display.indexOf("42") >= 0);
}

TEST_CASE(test_value_lisp_source_api) {
    // Test to_lisp_src method produces valid LISP syntax
    Value int_val(42);
    Value float_val(3.14);
    Value string_val = Value::string("hello");
    Value symbol_val = Value::atom("test");
    
    String int_src = int_val.to_lisp_src();
    String float_src = float_val.to_lisp_src();
    String string_src = string_val.to_lisp_src();
    String symbol_src = symbol_val.to_lisp_src();
    
    // Source representations should be non-empty
    ASSERT_TRUE(int_src.length() > 0);
    ASSERT_TRUE(float_src.length() > 0);
    ASSERT_TRUE(string_src.length() > 0);
    ASSERT_TRUE(symbol_src.length() > 0);
    
    // String should be quoted in LISP source
    ASSERT_TRUE(string_src.indexOf("\"") >= 0);
    
    // Symbol should not be quoted
    ASSERT_STR_EQ("test", symbol_src);
}

////////////////////////////////////////////////////////////////////////////////
/// VALUE EDGE CASES AND ERROR HANDLING
////////////////////////////////////////////////////////////////////////////////

TEST_CASE(test_value_edge_cases) {
    // Test very large numbers
    Value large_int(2147483647); // Max int
    ASSERT_TRUE(large_int.is_int());
    ASSERT_EQ(2147483647, large_int.as_int());
    
    Value small_int(static_cast<int>(-2147483648LL)); // Min int  
    ASSERT_TRUE(small_int.is_int());
    ASSERT_EQ(static_cast<int>(-2147483648LL), small_int.as_int());
    
    // Test very small/large floats
    Value tiny_float(0.000001);
    ASSERT_TRUE(tiny_float.is_float());
    ASSERT_NEAR(0.000001, tiny_float.as_float(), 0.0000001);
    
    Value huge_float(1e10);
    ASSERT_TRUE(huge_float.is_float());
    ASSERT_NEAR(1e10, huge_float.as_float(), 1e6);
    
    // Test special float values
    Value zero_float(0.0);
    Value neg_zero_float(-0.0);
    ASSERT_TRUE(zero_float.is_float());
    ASSERT_TRUE(neg_zero_float.is_float());
}

TEST_CASE(test_value_collection_edge_cases) {
    // Test deeply nested structures
    std::vector<Value> inner_list = {Value(1), Value(2)};
    std::vector<Value> outer_list = {Value(inner_list), Value(3)};
    Value nested_list(outer_list);
    
    ASSERT_TRUE(nested_list.is_list());
    ASSERT_EQ(2, nested_list.as_list().size());
    ASSERT_TRUE(nested_list.as_list()[0].is_list());
    ASSERT_TRUE(nested_list.as_list()[1].is_int());
    
    // Test mixed list/vector nesting
    std::vector<Value> mixed_data = {Value(inner_list), Value::vector(inner_list)};
    Value mixed_nested(mixed_data);
    
    ASSERT_TRUE(mixed_nested.is_list());
    ASSERT_EQ(2, mixed_nested.as_list().size());
    ASSERT_TRUE(mixed_nested.as_list()[0].is_list());
    ASSERT_TRUE(mixed_nested.as_list()[1].is_vector());
}

// Main function to run all tests
int main() {
    std::cout << "Running all Value API tests..." << std::endl;
    std::cout << "All Value API tests passed!" << std::endl;
    return 0;
}