#define CATCH_CONFIG_MAIN // This tells Catch to provide a main() - only do this in
                          // one cpp file
#include "catch.hpp"

#include "../uSEQ/src/modulisp/lisp/environment.h"
#include "../uSEQ/src/modulisp/lisp/value.h"
#include <cmath>

////////////////////////////////////////////////////////////////////////////////
/// VALUE CONSTRUCTION API TESTS
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("Value construction - integers", "[value][construction][integers]")
{
    // Test integer construction
    Value v1(42);
    REQUIRE(v1.is_int());
    REQUIRE(v1.as_int() == 42);

    // Test negative integer
    Value v2(-17);
    REQUIRE(v2.is_int());
    REQUIRE(v2.as_int() == -17);

    // Test zero
    Value v3(0);
    REQUIRE(v3.is_int());
    REQUIRE(v3.as_int() == 0);
}

TEST_CASE("Value construction - floats", "[value][construction][floats]")
{
    // Test float construction
    Value v1(3.14);
    REQUIRE(v1.is_float());
    REQUIRE(v1.as_float() == Approx(3.14).epsilon(0.001));

    // Test negative float
    Value v2(-2.71);
    REQUIRE(v2.is_float());
    REQUIRE(v2.as_float() == Approx(-2.71).epsilon(0.001));

    // Test zero float
    Value v3(0.0);
    REQUIRE(v3.is_float());
    REQUIRE(v3.as_float() == Approx(0.0).epsilon(0.001));
}

TEST_CASE("Value construction - strings", "[value][construction][strings]")
{
    // Test string construction via static method
    Value v1 = Value::string("hello world");
    REQUIRE(v1.is_string());
    REQUIRE(v1.as_string() == "hello world");

    // Test empty string
    Value v2 = Value::string("");
    REQUIRE(v2.is_string());
    REQUIRE(v2.as_string() == "");

    // Test string with special characters
    Value v3 = Value::string("Hello\nWorld\t!");
    REQUIRE(v3.is_string());
    REQUIRE(v3.as_string() == "Hello\nWorld\t!");
}

TEST_CASE("Value construction - symbols", "[value][construction][symbols]")
{
    // Test symbol construction via static method
    Value v1 = Value::atom("my-symbol");
    REQUIRE(v1.is_symbol());
    REQUIRE(v1.as_atom() == "my-symbol");

    // Test single character symbol
    Value v2 = Value::atom("+");
    REQUIRE(v2.is_symbol());
    REQUIRE(v2.as_atom() == "+");

    // Test complex symbol name
    Value v3 = Value::atom("test-123-var");
    REQUIRE(v3.is_symbol());
    REQUIRE(v3.as_atom() == "test-123-var");
}

TEST_CASE("Value construction - lists", "[value][construction][lists]")
{
    // Test empty list
    std::vector<Value> empty_list;
    Value v1(empty_list);
    REQUIRE(v1.is_list());
    REQUIRE(v1.is_sequential());
    REQUIRE(v1.as_list().size() == 0);
    REQUIRE(v1.is_empty());

    // Test list with integers
    std::vector<Value> int_list = { Value(1), Value(2), Value(3) };
    Value v2(int_list);
    REQUIRE(v2.is_list());
    REQUIRE(v2.is_sequential());
    REQUIRE(v2.as_list().size() == 3);
    REQUIRE_FALSE(v2.is_empty());
    REQUIRE(v2.as_list()[0].as_int() == 1);
    REQUIRE(v2.as_list()[1].as_int() == 2);
    REQUIRE(v2.as_list()[2].as_int() == 3);

    // Test mixed type list
    std::vector<Value> mixed_list = { Value(42), Value::string("hello"),
                                      Value(3.14) };
    Value v3(mixed_list);
    REQUIRE(v3.is_list());
    REQUIRE(v3.as_list().size() == 3);
    REQUIRE(v3.as_list()[0].is_int());
    REQUIRE(v3.as_list()[1].is_string());
    REQUIRE(v3.as_list()[2].is_float());
}

TEST_CASE("Value construction - vectors", "[value][construction][vectors]")
{
    // Test empty vector
    std::vector<Value> empty_vec;
    Value v1 = Value::vector(empty_vec);
    REQUIRE(v1.is_vector());
    REQUIRE(v1.is_sequential());
    REQUIRE(v1.as_vector().size() == 0);
    REQUIRE(v1.is_empty());

    // Test vector with integers
    std::vector<Value> int_vec = { Value(10), Value(20), Value(30) };
    Value v2                   = Value::vector(int_vec);
    REQUIRE(v2.is_vector());
    REQUIRE(v2.is_sequential());
    REQUIRE(v2.as_vector().size() == 3);
    REQUIRE_FALSE(v2.is_empty());
    REQUIRE(v2.as_vector()[0].as_int() == 10);
    REQUIRE(v2.as_vector()[1].as_int() == 20);
    REQUIRE(v2.as_vector()[2].as_int() == 30);

    // Test mixed type vector
    std::vector<Value> mixed_vec = { Value::atom("symbol"), Value(1.5),
                                     Value::string("text") };
    Value v3                     = Value::vector(mixed_vec);
    REQUIRE(v3.is_vector());
    REQUIRE(v3.as_vector().size() == 3);
    REQUIRE(v3.as_vector()[0].is_symbol());
    REQUIRE(v3.as_vector()[1].is_float());
    REQUIRE(v3.as_vector()[2].is_string());
}

TEST_CASE("Value construction - special values", "[value][construction][special]")
{
    // Test nil value
    Value v1 = Value::nil();
    REQUIRE(v1.is_nil());
    REQUIRE_FALSE(v1.is_int());
    REQUIRE_FALSE(v1.is_string());
    REQUIRE_FALSE(v1.is_list());

    // Test error value
    Value v2 = Value::error();
    REQUIRE(v2.is_error());
    REQUIRE_FALSE(v2.is_int());
    REQUIRE_FALSE(v2.is_string());

    // Test unit value (default constructor)
    Value v3;
    REQUIRE_FALSE(v3.is_nil());
    REQUIRE_FALSE(v3.is_error());
    REQUIRE_FALSE(v3.is_int());
    REQUIRE_FALSE(v3.is_string());
    // Unit type is the default when no specific type is set
}

TEST_CASE("Value construction - quoted values", "[value][construction][quotes]")
{
    // Test quoted integer
    Value inner = Value(42);
    Value v1    = Value::quote(inner);
    REQUIRE(v1.to_lisp_src() == "'42");

    // Test quoted symbol
    Value inner2 = Value::atom("symbol");
    Value v2     = Value::quote(inner2);
    REQUIRE(v2.to_lisp_src() == "'symbol");

    // Test quoted string
    Value inner3 = Value::string("hello");
    Value v3     = Value::quote(inner3);
    REQUIRE(v3.to_lisp_src() == "'\"hello\"");
}

////////////////////////////////////////////////////////////////////////////////
/// VALUE TYPE CHECKING API TESTS
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("Value type checking - basic types", "[value][type_checking][basic]")
{
    // Test integer type checking
    Value int_val(42);
    REQUIRE(int_val.is_int());
    REQUIRE(int_val.is_number());
    REQUIRE_FALSE(int_val.is_float());
    REQUIRE_FALSE(int_val.is_string());
    REQUIRE_FALSE(int_val.is_list());
    REQUIRE_FALSE(int_val.is_vector());
    REQUIRE_FALSE(int_val.is_symbol());
    REQUIRE_FALSE(int_val.is_nil());
    REQUIRE_FALSE(int_val.is_error());

    // Test float type checking
    Value float_val(3.14);
    REQUIRE(float_val.is_float());
    REQUIRE(float_val.is_number());
    REQUIRE_FALSE(float_val.is_int());
    REQUIRE_FALSE(float_val.is_string());
    REQUIRE_FALSE(float_val.is_list());
    REQUIRE_FALSE(float_val.is_vector());
    REQUIRE_FALSE(float_val.is_symbol());
    REQUIRE_FALSE(float_val.is_nil());
    REQUIRE_FALSE(float_val.is_error());

    // Test string type checking
    Value string_val = Value::string("test");
    REQUIRE(string_val.is_string());
    REQUIRE_FALSE(string_val.is_int());
    REQUIRE_FALSE(string_val.is_float());
    REQUIRE_FALSE(string_val.is_number());
    REQUIRE_FALSE(string_val.is_list());
    REQUIRE_FALSE(string_val.is_vector());
    REQUIRE_FALSE(string_val.is_symbol());
    REQUIRE_FALSE(string_val.is_nil());
    REQUIRE_FALSE(string_val.is_error());
}

TEST_CASE("Value type checking - composite types",
          "[value][type_checking][composite]")
{
    // Test symbol type checking
    Value symbol_val = Value::atom("my-symbol");
    REQUIRE(symbol_val.is_symbol());
    REQUIRE_FALSE(symbol_val.is_string());
    REQUIRE_FALSE(symbol_val.is_int());
    REQUIRE_FALSE(symbol_val.is_float());
    REQUIRE_FALSE(symbol_val.is_number());
    REQUIRE_FALSE(symbol_val.is_list());
    REQUIRE_FALSE(symbol_val.is_vector());
    REQUIRE_FALSE(symbol_val.is_nil());
    REQUIRE_FALSE(symbol_val.is_error());

    // Test list type checking
    std::vector<Value> list_data = { Value(1), Value(2) };
    Value list_val(list_data);
    REQUIRE(list_val.is_list());
    REQUIRE(list_val.is_sequential());
    REQUIRE_FALSE(list_val.is_vector());
    REQUIRE_FALSE(list_val.is_string());
    REQUIRE_FALSE(list_val.is_symbol());
    REQUIRE_FALSE(list_val.is_int());
    REQUIRE_FALSE(list_val.is_float());
    REQUIRE_FALSE(list_val.is_number());
    REQUIRE_FALSE(list_val.is_nil());
    REQUIRE_FALSE(list_val.is_error());

    // Test vector type checking
    std::vector<Value> vector_data = { Value(3), Value(4) };
    Value vector_val               = Value::vector(vector_data);
    REQUIRE(vector_val.is_vector());
    REQUIRE(vector_val.is_sequential());
    REQUIRE_FALSE(vector_val.is_list());
    REQUIRE_FALSE(vector_val.is_string());
    REQUIRE_FALSE(vector_val.is_symbol());
    REQUIRE_FALSE(vector_val.is_int());
    REQUIRE_FALSE(vector_val.is_float());
    REQUIRE_FALSE(vector_val.is_number());
    REQUIRE_FALSE(vector_val.is_nil());
    REQUIRE_FALSE(vector_val.is_error());
}

TEST_CASE("Value type checking - special types", "[value][type_checking][special]")
{
    // Test nil type checking
    Value nil_val = Value::nil();
    REQUIRE(nil_val.is_nil());
    REQUIRE_FALSE(nil_val.is_error());
    REQUIRE_FALSE(nil_val.is_int());
    REQUIRE_FALSE(nil_val.is_float());
    REQUIRE_FALSE(nil_val.is_number());
    REQUIRE_FALSE(nil_val.is_string());
    REQUIRE_FALSE(nil_val.is_symbol());
    REQUIRE_FALSE(nil_val.is_list());
    REQUIRE_FALSE(nil_val.is_vector());

    // Test error type checking
    Value error_val = Value::error();
    REQUIRE(error_val.is_error());
    REQUIRE_FALSE(error_val.is_nil());
    REQUIRE_FALSE(error_val.is_int());
    REQUIRE_FALSE(error_val.is_float());
    REQUIRE_FALSE(error_val.is_number());
    REQUIRE_FALSE(error_val.is_string());
    REQUIRE_FALSE(error_val.is_symbol());
    REQUIRE_FALSE(error_val.is_list());
    REQUIRE_FALSE(error_val.is_vector());
}

TEST_CASE("Value number property checking", "[value][number][properties]")
{
    // Test positive number checking
    Value pos_int(5);
    Value pos_float(3.14);
    REQUIRE(pos_int.is_positive_number());
    REQUIRE(pos_int.is_non_zero_number());
    REQUIRE_FALSE(pos_int.is_negative_number());
    REQUIRE(pos_float.is_positive_number());
    REQUIRE(pos_float.is_non_zero_number());
    REQUIRE_FALSE(pos_float.is_negative_number());

    // Test negative number checking
    Value neg_int(-5);
    Value neg_float(-2.71);
    REQUIRE(neg_int.is_negative_number());
    REQUIRE(neg_int.is_non_zero_number());
    REQUIRE_FALSE(neg_int.is_positive_number());
    REQUIRE(neg_float.is_negative_number());
    REQUIRE(neg_float.is_non_zero_number());
    REQUIRE_FALSE(neg_float.is_positive_number());

    // Test zero checking
    Value zero_int(0);
    Value zero_float(0.0);
    REQUIRE_FALSE(zero_int.is_positive_number());
    REQUIRE_FALSE(zero_int.is_negative_number());
    REQUIRE_FALSE(zero_int.is_non_zero_number());
    REQUIRE_FALSE(zero_float.is_positive_number());
    REQUIRE_FALSE(zero_float.is_negative_number());
    REQUIRE_FALSE(zero_float.is_non_zero_number());
}

TEST_CASE("Value emptiness checking", "[value][emptiness]")
{
    // Test empty collections
    std::vector<Value> empty_data;
    Value empty_list(empty_data);
    Value empty_vector = Value::vector(empty_data);

    REQUIRE(empty_list.is_empty());
    REQUIRE(empty_list.is_list_and_empty());
    REQUIRE(empty_vector.is_empty());
    REQUIRE_FALSE(empty_vector.is_list_and_empty()); // It's a vector, not a list

    // Test non-empty collections
    std::vector<Value> non_empty_data = { Value(1) };
    Value non_empty_list(non_empty_data);
    Value non_empty_vector = Value::vector(non_empty_data);

    REQUIRE_FALSE(non_empty_list.is_empty());
    REQUIRE_FALSE(non_empty_list.is_list_and_empty());
    REQUIRE_FALSE(non_empty_vector.is_empty());
    REQUIRE_FALSE(non_empty_vector.is_list_and_empty());

    // Test non-collections (behavior may vary by implementation)
    Value int_val(42);
    Value string_val = Value::string("test");
    // Note: Non-collection values may or may not report as empty depending on
    // implementation Just verify the methods don't crash
    int_val.is_empty();
    string_val.is_empty();
}

////////////////////////////////////////////////////////////////////////////////
/// VALUE CONVERSION API TESTS
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("Value as conversions", "[value][conversions][as]")
{
    // Test integer conversions
    Value int_val(42);
    REQUIRE(int_val.as_int() == 42);
    REQUIRE(int_val.as_float() == Approx(42.0).epsilon(0.001));
    REQUIRE(int_val.as_bool()); // Non-zero is true

    Value zero_val(0);
    REQUIRE(zero_val.as_int() == 0);
    REQUIRE(zero_val.as_float() == Approx(0.0).epsilon(0.001));
    REQUIRE_FALSE(zero_val.as_bool()); // Zero is false

    // Test float conversions
    Value float_val(3.14);
    REQUIRE(float_val.as_int() == 3); // Should truncate
    REQUIRE(float_val.as_float() == Approx(3.14).epsilon(0.001));
    REQUIRE(float_val.as_bool()); // Non-zero is true

    // Test string conversions
    Value string_val = Value::string("hello world");
    REQUIRE(string_val.as_string() == "hello world");
    REQUIRE(string_val.as_bool()); // Non-empty string is true

    Value empty_string = Value::string("");
    REQUIRE(empty_string.as_string() == "");
    // Note: as_bool() behavior for empty strings may vary by implementation
    empty_string.as_bool(); // Just verify it doesn't crash

    // Test symbol conversions
    Value symbol_val = Value::atom("test-symbol");
    REQUIRE(symbol_val.as_atom() == "test-symbol");
    REQUIRE(symbol_val.as_bool()); // Symbols are truthy
}

TEST_CASE("Value collection conversions", "[value][conversions][collections]")
{
    // Test list conversions
    std::vector<Value> list_data = { Value(1), Value(2), Value(3) };
    Value list_val(list_data);

    std::vector<Value> retrieved_list = list_val.as_list();
    REQUIRE(retrieved_list.size() == 3);
    REQUIRE(retrieved_list[0].as_int() == 1);
    REQUIRE(retrieved_list[1].as_int() == 2);
    REQUIRE(retrieved_list[2].as_int() == 3);

    std::vector<Value> retrieved_sequential = list_val.as_sequential();
    REQUIRE(retrieved_sequential.size() == 3);
    REQUIRE(retrieved_sequential[0].as_int() == 1);

    // Test vector conversions
    std::vector<Value> vector_data = { Value(10), Value(20), Value(30) };
    Value vector_val               = Value::vector(vector_data);

    std::vector<Value> retrieved_vector = vector_val.as_vector();
    REQUIRE(retrieved_vector.size() == 3);
    REQUIRE(retrieved_vector[0].as_int() == 10);
    REQUIRE(retrieved_vector[1].as_int() == 20);
    REQUIRE(retrieved_vector[2].as_int() == 30);

    std::vector<Value> retrieved_sequential2 = vector_val.as_sequential();
    REQUIRE(retrieved_sequential2.size() == 3);
    REQUIRE(retrieved_sequential2[0].as_int() == 10);
}

TEST_CASE("Value explicit casting", "[value][conversions][casting]")
{
    // Test cast_to_int
    Value float_val(3.7);
    Value int_cast = float_val.cast_to_int();
    REQUIRE(int_cast.is_int());
    REQUIRE(int_cast.as_int() == 3);

    Value int_val(42);
    Value int_cast2 = int_val.cast_to_int();
    REQUIRE(int_cast2.is_int());
    REQUIRE(int_cast2.as_int() == 42);

    // Test cast_to_float
    Value int_val2(15);
    Value float_cast = int_val2.cast_to_float();
    REQUIRE(float_cast.is_float());
    REQUIRE(float_cast.as_float() == Approx(15.0).epsilon(0.001));

    Value float_val2(2.71);
    Value float_cast2 = float_val2.cast_to_float();
    REQUIRE(float_cast2.is_float());
    REQUIRE(float_cast2.as_float() == Approx(2.71).epsilon(0.001));
}

////////////////////////////////////////////////////////////////////////////////
/// VALUE COMPARISON API TESTS
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("Value equality comparison", "[value][comparison][equality]")
{
    // Test integer equality
    Value int1(42);
    Value int2(42);
    Value int3(17);
    REQUIRE(int1 == int2);
    REQUIRE_FALSE(int1 == int3);
    REQUIRE_FALSE(int1 != int2);
    REQUIRE(int1 != int3);

    // Test float equality
    Value float1(3.14);
    Value float2(3.14);
    Value float3(2.71);
    REQUIRE(float1 == float2);
    REQUIRE_FALSE(float1 == float3);

    // Test string equality
    Value str1 = Value::string("hello");
    Value str2 = Value::string("hello");
    Value str3 = Value::string("world");
    REQUIRE(str1 == str2);
    REQUIRE_FALSE(str1 == str3);

    // Test mixed type inequality
    Value int_val(42);
    Value float_val(42.0);
    Value string_val = Value::string("42");
    // Different types should not be equal even if values look similar
    REQUIRE_FALSE(int_val == string_val);
}

TEST_CASE("Value ordering comparison", "[value][comparison][ordering]")
{
    // Test integer ordering
    Value int1(10);
    Value int2(20);
    Value int3(10);

    REQUIRE(int1 < int2);
    REQUIRE_FALSE(int2 < int1);
    REQUIRE_FALSE(int1 < int3); // Equal values

    REQUIRE(int2 > int1);
    REQUIRE_FALSE(int1 > int2);
    REQUIRE_FALSE(int1 > int3); // Equal values

    REQUIRE(int1 <= int2);
    REQUIRE(int1 <= int3); // Equal values
    REQUIRE_FALSE(int2 <= int1);

    REQUIRE(int2 >= int1);
    REQUIRE(int1 >= int3); // Equal values
    REQUIRE_FALSE(int1 >= int2);

    // Test float ordering
    Value float1(1.5);
    Value float2(2.5);

    REQUIRE(float1 < float2);
    REQUIRE_FALSE(float2 < float1);
    REQUIRE(float2 > float1);
    REQUIRE_FALSE(float1 > float2);
}

TEST_CASE("Value string comparison", "[value][comparison][string]")
{
    // Test string comparison with == operator overload for String
    Value str_val = Value::string("test");
    REQUIRE(str_val == String("test"));
    REQUIRE_FALSE(str_val == String("other"));
}

////////////////////////////////////////////////////////////////////////////////
/// VALUE ARITHMETIC API TESTS
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("Value arithmetic operations", "[value][arithmetic]")
{
    // Test integer arithmetic
    Value int1(10);
    Value int2(3);

    Value sum = int1 + int2;
    REQUIRE(sum.is_int());
    REQUIRE(sum.as_int() == 13);

    Value diff = int1 - int2;
    REQUIRE(diff.is_int());
    REQUIRE(diff.as_int() == 7);

    Value product = int1 * int2;
    REQUIRE(product.is_int());
    REQUIRE(product.as_int() == 30);

    Value quotient = int1 / int2;
    // Division may promote to float
    REQUIRE(quotient.is_number());
    REQUIRE(quotient.as_float() == Approx(3.333).epsilon(0.01));

    Value remainder = int1 % int2;
    REQUIRE(remainder.is_int());
    REQUIRE(remainder.as_int() == 1);

    // Test float arithmetic
    Value float1(10.5);
    Value float2(2.5);

    Value float_sum = float1 + float2;
    REQUIRE(float_sum.is_float());
    REQUIRE(float_sum.as_float() == Approx(13.0).epsilon(0.001));

    Value float_diff = float1 - float2;
    REQUIRE(float_diff.is_float());
    REQUIRE(float_diff.as_float() == Approx(8.0).epsilon(0.001));

    Value float_product = float1 * float2;
    REQUIRE(float_product.is_float());
    REQUIRE(float_product.as_float() == Approx(26.25).epsilon(0.001));

    Value float_quotient = float1 / float2;
    REQUIRE(float_quotient.is_float());
    REQUIRE(float_quotient.as_float() == Approx(4.2).epsilon(0.001));
}

TEST_CASE("Value mixed arithmetic", "[value][arithmetic][mixed]")
{
    // Test mixed int/float arithmetic
    Value int_val(10);
    Value float_val(2.5);

    Value mixed_sum = int_val + float_val;
    REQUIRE(mixed_sum.is_float()); // Should promote to float
    REQUIRE(mixed_sum.as_float() == Approx(12.5).epsilon(0.001));

    Value mixed_product = int_val * float_val;
    REQUIRE(mixed_product.is_float());
    REQUIRE(mixed_product.as_float() == Approx(25.0).epsilon(0.001));
}

////////////////////////////////////////////////////////////////////////////////
/// VALUE COLLECTION MANIPULATION API TESTS
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("Value list manipulation", "[value][lists][manipulation]")
{
    // Test push operation
    std::vector<Value> initial_data = { Value(1), Value(2) };
    Value list_val(initial_data);

    list_val.push(Value(3));
    std::vector<Value> after_push = list_val.as_list();
    REQUIRE(after_push.size() == 3);
    REQUIRE(after_push[0].as_int() == 1);
    REQUIRE(after_push[1].as_int() == 2);
    REQUIRE(after_push[2].as_int() == 3);

    // Test pop operation
    Value popped = list_val.pop();
    REQUIRE(popped.is_int());
    REQUIRE(popped.as_int() == 3);

    std::vector<Value> after_pop = list_val.as_list();
    REQUIRE(after_pop.size() == 2);
    REQUIRE(after_pop[0].as_int() == 1);
    REQUIRE(after_pop[1].as_int() == 2);
}

////////////////////////////////////////////////////////////////////////////////
/// VALUE METADATA API TESTS
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("Value type metadata", "[value][metadata][types]")
{
    // Test type name reporting
    Value int_val(42);
    Value float_val(3.14);
    Value string_val             = Value::string("test");
    Value symbol_val             = Value::atom("symbol");
    std::vector<Value> list_data = { Value(1) };
    Value list_val(list_data);
    Value vector_val = Value::vector(list_data);
    Value nil_val    = Value::nil();
    Value error_val  = Value::error();

    // Test that type names are non-empty strings
    REQUIRE(int_val.get_type_name().length() > 0);
    REQUIRE(float_val.get_type_name().length() > 0);
    REQUIRE(string_val.get_type_name().length() > 0);
    REQUIRE(symbol_val.get_type_name().length() > 0);
    REQUIRE(list_val.get_type_name().length() > 0);
    REQUIRE(vector_val.get_type_name().length() > 0);
    REQUIRE(nil_val.get_type_name().length() > 0);
    REQUIRE(error_val.get_type_name().length() > 0);

    // Test type enum reporting
    REQUIRE(int_val.get_type_enum() >= 0);
    REQUIRE(float_val.get_type_enum() >= 0);
    REQUIRE(string_val.get_type_enum() >= 0);

    // Different types should have different enums
    REQUIRE(int_val.get_type_enum() != float_val.get_type_enum());
    REQUIRE(string_val.get_type_enum() != symbol_val.get_type_enum());
}

TEST_CASE("Value display API", "[value][display]")
{
    // Test display method produces readable output
    Value int_val(42);
    Value float_val(3.14);
    Value string_val = Value::string("hello");
    Value symbol_val = Value::atom("test");

    String int_display    = int_val.display();
    String float_display  = float_val.display();
    String string_display = string_val.display();
    String symbol_display = symbol_val.display();

    // Displays should be non-empty
    REQUIRE(int_display.length() > 0);
    REQUIRE(float_display.length() > 0);
    REQUIRE(string_display.length() > 0);
    REQUIRE(symbol_display.length() > 0);

    // Integer display should contain the number
    REQUIRE(int_display.indexOf("42") >= 0);
}

TEST_CASE("Value LISP source API", "[value][lisp][source]")
{
    // Test to_lisp_src method produces valid LISP syntax
    Value int_val(42);
    Value float_val(3.14);
    Value string_val = Value::string("hello");
    Value symbol_val = Value::atom("test");

    String int_src    = int_val.to_lisp_src();
    String float_src  = float_val.to_lisp_src();
    String string_src = string_val.to_lisp_src();
    String symbol_src = symbol_val.to_lisp_src();

    // Source representations should be non-empty
    REQUIRE(int_src.length() > 0);
    REQUIRE(float_src.length() > 0);
    REQUIRE(string_src.length() > 0);
    REQUIRE(symbol_src.length() > 0);

    // String should be quoted in LISP source
    REQUIRE(string_src.indexOf("\"") >= 0);

    // Symbol should not be quoted
    REQUIRE(symbol_src == "test");
}

////////////////////////////////////////////////////////////////////////////////
/// VALUE EDGE CASES AND ERROR HANDLING
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("Value edge cases", "[value][edge_cases]")
{
    // Test very large numbers
    Value large_int(2147483647); // Max int
    REQUIRE(large_int.is_int());
    REQUIRE(large_int.as_int() == 2147483647);

    Value small_int(static_cast<int>(-2147483648LL)); // Min int
    REQUIRE(small_int.is_int());
    REQUIRE(small_int.as_int() == static_cast<int>(-2147483648LL));

    // Test very small/large floats
    Value tiny_float(0.000001);
    REQUIRE(tiny_float.is_float());
    REQUIRE(tiny_float.as_float() == Approx(0.000001).epsilon(0.0000001));

    Value huge_float(1e10);
    REQUIRE(huge_float.is_float());
    REQUIRE(huge_float.as_float() == Approx(1e10).epsilon(0.1));

    // Test special float values
    Value zero_float(0.0);
    Value neg_zero_float(-0.0);
    REQUIRE(zero_float.is_float());
    REQUIRE(neg_zero_float.is_float());
}

TEST_CASE("Value collection edge cases", "[value][collections][edge_cases]")
{
    // Test deeply nested structures
    std::vector<Value> inner_list = { Value(1), Value(2) };
    std::vector<Value> outer_list = { Value(inner_list), Value(3) };
    Value nested_list(outer_list);

    REQUIRE(nested_list.is_list());
    REQUIRE(nested_list.as_list().size() == 2);
    REQUIRE(nested_list.as_list()[0].is_list());
    REQUIRE(nested_list.as_list()[1].is_int());

    // Test mixed list/vector nesting
    std::vector<Value> mixed_data = { Value(inner_list), Value::vector(inner_list) };
    Value mixed_nested(mixed_data);

    REQUIRE(mixed_nested.is_list());
    REQUIRE(mixed_nested.as_list().size() == 2);
    REQUIRE(mixed_nested.as_list()[0].is_list());
    REQUIRE(mixed_nested.as_list()[1].is_vector());
}
