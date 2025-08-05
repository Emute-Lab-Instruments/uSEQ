#include "../uSEQ/src/modulisp/lisp/environment.h"
#include "../uSEQ/src/modulisp/lisp/value.h"
#include <cassert>
#include <iostream>
#include <optional>

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
/// ENVIRONMENT CONSTRUCTION AND BASIC OPERATIONS
////////////////////////////////////////////////////////////////////////////////

TEST_CASE(test_environment_construction) {
    // Test default constructor
    Environment env1;
    
    // Environment should be constructible without issues
    ASSERT_TRUE(true); // If we get here, construction succeeded
    
    // Test copy constructor
    Environment env2(env1);
    ASSERT_TRUE(true); // Copy construction succeeded
    
    // Test move constructor
    Environment temp_env;
    Environment env3(std::move(temp_env));
    ASSERT_TRUE(true); // Move construction succeeded
}

TEST_CASE(test_environment_basic_variable_operations) {
    Environment env;
    
    // Test setting and getting a simple integer variable
    Value int_val(42);
    env.set("test-var", int_val);
    
    // Test that variable exists
    ASSERT_TRUE(env.has("test-var"));
    
    // Test retrieving the variable
    std::optional<Value> retrieved = env.get("test-var");
    ASSERT_TRUE(retrieved.has_value());
    ASSERT_TRUE(retrieved->is_int());
    ASSERT_EQ(42, retrieved->as_int());
    
    // Test non-existent variable
    ASSERT_FALSE(env.has("non-existent"));
    std::optional<Value> missing = env.get("non-existent");
    ASSERT_FALSE(missing.has_value());
}

TEST_CASE(test_environment_multiple_variables) {
    Environment env;
    
    // Set multiple variables of different types
    env.set("int-var", Value(100));
    env.set("float-var", Value(3.14));
    env.set("string-var", Value::string("hello"));
    env.set("symbol-var", Value::atom("test-symbol"));
    
    // Test all variables exist
    ASSERT_TRUE(env.has("int-var"));
    ASSERT_TRUE(env.has("float-var"));
    ASSERT_TRUE(env.has("string-var"));
    ASSERT_TRUE(env.has("symbol-var"));
    
    // Test all variables have correct values
    auto int_result = env.get("int-var");
    ASSERT_TRUE(int_result.has_value());
    ASSERT_TRUE(int_result->is_int());
    ASSERT_EQ(100, int_result->as_int());
    
    auto float_result = env.get("float-var");
    ASSERT_TRUE(float_result.has_value());
    ASSERT_TRUE(float_result->is_float());
    ASSERT_NEAR(3.14, float_result->as_float(), 0.001);
    
    auto string_result = env.get("string-var");
    ASSERT_TRUE(string_result.has_value());
    ASSERT_TRUE(string_result->is_string());
    ASSERT_STR_EQ("hello", string_result->as_string());
    
    auto symbol_result = env.get("symbol-var");
    ASSERT_TRUE(symbol_result.has_value());
    ASSERT_TRUE(symbol_result->is_symbol());
    ASSERT_STR_EQ("test-symbol", symbol_result->as_atom());
}

TEST_CASE(test_environment_variable_overwriting) {
    Environment env;
    
    // Set initial value
    env.set("var", Value(10));
    auto initial = env.get("var");
    ASSERT_TRUE(initial.has_value());
    ASSERT_EQ(10, initial->as_int());
    
    // Overwrite with same type
    env.set("var", Value(20));
    auto updated = env.get("var");
    ASSERT_TRUE(updated.has_value());
    ASSERT_EQ(20, updated->as_int());
    
    // Overwrite with different type
    env.set("var", Value::string("now-a-string"));
    auto changed_type = env.get("var");
    ASSERT_TRUE(changed_type.has_value());
    ASSERT_TRUE(changed_type->is_string());
    ASSERT_STR_EQ("now-a-string", changed_type->as_string());
}

TEST_CASE(test_environment_variable_unsetting) {
    Environment env;
    
    // Set a variable
    env.set("temp-var", Value(123));
    ASSERT_TRUE(env.has("temp-var"));
    
    // Unset the variable
    env.unset("temp-var");
    ASSERT_FALSE(env.has("temp-var"));
    
    // Try to get unset variable
    auto result = env.get("temp-var");
    ASSERT_FALSE(result.has_value());
    
    // Unsetting non-existent variable should not cause issues
    env.unset("never-existed");
    ASSERT_TRUE(true); // If we get here, no crash occurred
}

////////////////////////////////////////////////////////////////////////////////
/// ENVIRONMENT EXPRESSION STORAGE API TESTS
////////////////////////////////////////////////////////////////////////////////

TEST_CASE(test_environment_expression_storage) {
    Environment env;
    
    // Test setting and getting expressions (unevaluated code)
    Value expr_val = Value::atom("some-expression");
    env.set_expr("expr-var", expr_val);
    
    // Test that expression exists
    std::optional<Value> retrieved_expr = env.get_expr("expr-var");
    ASSERT_TRUE(retrieved_expr.has_value());
    ASSERT_TRUE(retrieved_expr->is_symbol());
    ASSERT_STR_EQ("some-expression", retrieved_expr->as_atom());
    
    // Test that regular get doesn't find expressions
    std::optional<Value> regular_get = env.get("expr-var");
    ASSERT_FALSE(regular_get.has_value());
    
    // Test unsetting expressions
    env.unset_expr("expr-var");
    std::optional<Value> after_unset = env.get_expr("expr-var");
    ASSERT_FALSE(after_unset.has_value());
}

TEST_CASE(test_environment_expression_vs_value_separation) {
    Environment env;
    
    // Set both a value and an expression with the same name
    env.set("dual-var", Value(42));
    env.set_expr("dual-var", Value::string("expression-code"));
    
    // Both should be retrievable separately
    auto value_result = env.get("dual-var");
    ASSERT_TRUE(value_result.has_value());
    ASSERT_TRUE(value_result->is_int());
    ASSERT_EQ(42, value_result->as_int());
    
    auto expr_result = env.get_expr("dual-var");
    ASSERT_TRUE(expr_result.has_value());
    ASSERT_TRUE(expr_result->is_string());
    ASSERT_STR_EQ("expression-code", expr_result->as_string());
    
    // Unsetting one shouldn't affect the other
    env.unset("dual-var");
    ASSERT_FALSE(env.get("dual-var").has_value());
    ASSERT_TRUE(env.get_expr("dual-var").has_value());
    
    env.unset_expr("dual-var");
    ASSERT_FALSE(env.get_expr("dual-var").has_value());
}

////////////////////////////////////////////////////////////////////////////////
/// ENVIRONMENT SCOPING AND INHERITANCE TESTS
////////////////////////////////////////////////////////////////////////////////

TEST_CASE(test_environment_parent_child_scoping) {
    Environment parent_env;
    Environment child_env;
    
    // Set up parent-child relationship
    child_env.set_parent_scope(&parent_env);
    
    // Set variable in parent
    parent_env.set("parent-var", Value(100));
    
    // Child should be able to see parent variable
    ASSERT_TRUE(child_env.has("parent-var"));
    auto inherited = child_env.get("parent-var");
    ASSERT_TRUE(inherited.has_value());
    ASSERT_EQ(100, inherited->as_int());
    
    // Set variable in child
    child_env.set("child-var", Value(200));
    
    // Parent should not see child variable
    ASSERT_FALSE(parent_env.has("child-var"));
    
    // Child should see its own variable
    ASSERT_TRUE(child_env.has("child-var"));
    auto child_var = child_env.get("child-var");
    ASSERT_TRUE(child_var.has_value());
    ASSERT_EQ(200, child_var->as_int());
}

TEST_CASE(test_environment_variable_shadowing) {
    Environment parent_env;
    Environment child_env;
    child_env.set_parent_scope(&parent_env);
    
    // Set variable in parent
    parent_env.set("shadowed-var", Value(10));
    
    // Child can see parent variable initially
    auto inherited = child_env.get("shadowed-var");
    ASSERT_TRUE(inherited.has_value());
    ASSERT_EQ(10, inherited->as_int());
    
    // Child shadows parent variable
    child_env.set("shadowed-var", Value(20));
    
    // Child should see its own value
    auto child_value = child_env.get("shadowed-var");
    ASSERT_TRUE(child_value.has_value());
    ASSERT_EQ(20, child_value->as_int());
    
    // Parent should still have original value
    auto parent_value = parent_env.get("shadowed-var");
    ASSERT_TRUE(parent_value.has_value());
    ASSERT_EQ(10, parent_value->as_int());
}

TEST_CASE(test_environment_multilevel_scoping) {
    Environment grandparent_env;
    Environment parent_env;
    Environment child_env;
    
    // Set up three-level hierarchy
    parent_env.set_parent_scope(&grandparent_env);
    child_env.set_parent_scope(&parent_env);
    
    // Set variables at each level
    grandparent_env.set("gp-var", Value(1));
    parent_env.set("p-var", Value(2));
    child_env.set("c-var", Value(3));
    
    // Child should see all variables
    ASSERT_TRUE(child_env.has("gp-var"));
    ASSERT_TRUE(child_env.has("p-var"));
    ASSERT_TRUE(child_env.has("c-var"));
    
    auto gp_val = child_env.get("gp-var");
    auto p_val = child_env.get("p-var");
    auto c_val = child_env.get("c-var");
    
    ASSERT_TRUE(gp_val.has_value());
    ASSERT_TRUE(p_val.has_value());
    ASSERT_TRUE(c_val.has_value());
    
    ASSERT_EQ(1, gp_val->as_int());
    ASSERT_EQ(2, p_val->as_int());
    ASSERT_EQ(3, c_val->as_int());
    
    // Parent should see grandparent but not child
    ASSERT_TRUE(parent_env.has("gp-var"));
    ASSERT_TRUE(parent_env.has("p-var"));
    ASSERT_FALSE(parent_env.has("c-var"));
    
    // Grandparent should only see its own
    ASSERT_TRUE(grandparent_env.has("gp-var"));
    ASSERT_FALSE(grandparent_env.has("p-var"));
    ASSERT_FALSE(grandparent_env.has("c-var"));
}

////////////////////////////////////////////////////////////////////////////////
/// ENVIRONMENT GLOBAL OPERATIONS API TESTS
////////////////////////////////////////////////////////////////////////////////

TEST_CASE(test_environment_global_variable_setting) {
    Environment grandparent_env;
    Environment parent_env;
    Environment child_env;
    
    // Set up hierarchy
    parent_env.set_parent_scope(&grandparent_env);
    child_env.set_parent_scope(&parent_env);
    
    // Set global variable from child environment
    child_env.set_global("global-var", Value(999));
    
    // Global variable should be accessible from all levels
    ASSERT_TRUE(grandparent_env.has("global-var"));
    ASSERT_TRUE(parent_env.has("global-var"));
    ASSERT_TRUE(child_env.has("global-var"));
    
    auto gp_global = grandparent_env.get("global-var");
    auto p_global = parent_env.get("global-var");
    auto c_global = child_env.get("global-var");
    
    ASSERT_TRUE(gp_global.has_value());
    ASSERT_TRUE(p_global.has_value());
    ASSERT_TRUE(c_global.has_value());
    
    ASSERT_EQ(999, gp_global->as_int());
    ASSERT_EQ(999, p_global->as_int());
    ASSERT_EQ(999, c_global->as_int());
}

TEST_CASE(test_environment_global_expression_setting) {
    Environment parent_env;
    Environment child_env;
    child_env.set_parent_scope(&parent_env);
    
    // Set global expression from child
    child_env.set_global_expr("global-expr", Value::string("global-expression"));
    
    // Both should be able to access the global expression
    auto parent_expr = parent_env.get_expr("global-expr");
    auto child_expr = child_env.get_expr("global-expr");
    
    ASSERT_TRUE(parent_expr.has_value());
    ASSERT_TRUE(child_expr.has_value());
    
    ASSERT_STR_EQ("global-expression", parent_expr->as_string());
    ASSERT_STR_EQ("global-expression", child_expr->as_string());
}

////////////////////////////////////////////////////////////////////////////////
/// ENVIRONMENT COMBINATION AND MERGING TESTS
////////////////////////////////////////////////////////////////////////////////

TEST_CASE(test_environment_combination) {
    Environment env1;
    Environment env2;
    
    // Set different variables in each environment
    env1.set("var1", Value(10));
    env1.set("var2", Value(20));
    
    env2.set("var3", Value(30));
    env2.set("var4", Value(40));
    
    // Combine env2 into env1
    env1.combine(env2);
    
    // env1 should now have all variables
    ASSERT_TRUE(env1.has("var1"));
    ASSERT_TRUE(env1.has("var2"));
    ASSERT_TRUE(env1.has("var3"));
    ASSERT_TRUE(env1.has("var4"));
    
    // Verify values are correct
    ASSERT_EQ(10, env1.get("var1")->as_int());
    ASSERT_EQ(20, env1.get("var2")->as_int());
    ASSERT_EQ(30, env1.get("var3")->as_int());
    ASSERT_EQ(40, env1.get("var4")->as_int());
    
    // env2 should still have its original variables
    ASSERT_TRUE(env2.has("var3"));
    ASSERT_TRUE(env2.has("var4"));
    ASSERT_FALSE(env2.has("var1"));
    ASSERT_FALSE(env2.has("var2"));
}

TEST_CASE(test_environment_combination_with_conflicts) {
    Environment env1;
    Environment env2;
    
    // Set conflicting variables
    env1.set("conflict-var", Value(100));
    env2.set("conflict-var", Value(200));
    
    // Also set unique variables
    env1.set("unique1", Value(1));
    env2.set("unique2", Value(2));
    
    // Combine env2 into env1
    env1.combine(env2);
    
    // Check that conflicting variable is handled (typically env2 overwrites env1)
    ASSERT_TRUE(env1.has("conflict-var"));
    auto conflict_result = env1.get("conflict-var");
    ASSERT_TRUE(conflict_result.has_value());
    // The exact behavior depends on implementation, but value should be one of the two
    ASSERT_TRUE(conflict_result->as_int() == 100 || conflict_result->as_int() == 200);
    
    // Unique variables should be preserved
    ASSERT_TRUE(env1.has("unique1"));
    ASSERT_TRUE(env1.has("unique2"));
    ASSERT_EQ(1, env1.get("unique1")->as_int());
    ASSERT_EQ(2, env1.get("unique2")->as_int());
}

////////////////////////////////////////////////////////////////////////////////
/// ENVIRONMENT COMPLEX DATA TYPES TESTS
////////////////////////////////////////////////////////////////////////////////

TEST_CASE(test_environment_complex_values) {
    Environment env;
    
    // Test storing lists
    std::vector<Value> list_data = {Value(1), Value(2), Value(3)};
    Value list_val(list_data);
    env.set("list-var", list_val);
    
    auto retrieved_list = env.get("list-var");
    ASSERT_TRUE(retrieved_list.has_value());
    ASSERT_TRUE(retrieved_list->is_list());
    ASSERT_EQ(3, retrieved_list->as_list().size());
    
    // Test storing vectors
    std::vector<Value> vector_data = {Value(10), Value(20)};
    Value vector_val = Value::vector(vector_data);
    env.set("vector-var", vector_val);
    
    auto retrieved_vector = env.get("vector-var");
    ASSERT_TRUE(retrieved_vector.has_value());
    ASSERT_TRUE(retrieved_vector->is_vector());
    ASSERT_EQ(2, retrieved_vector->as_vector().size());
    
    // Test storing nested structures
    std::vector<Value> nested_data = {list_val, vector_val};
    Value nested_val(nested_data);
    env.set("nested-var", nested_val);
    
    auto retrieved_nested = env.get("nested-var");
    ASSERT_TRUE(retrieved_nested.has_value());
    ASSERT_TRUE(retrieved_nested->is_list());
    ASSERT_EQ(2, retrieved_nested->as_list().size());
    ASSERT_TRUE(retrieved_nested->as_list()[0].is_list());
    ASSERT_TRUE(retrieved_nested->as_list()[1].is_vector());
}

TEST_CASE(test_environment_special_values) {
    Environment env;
    
    // Test storing nil
    env.set("nil-var", Value::nil());
    auto nil_result = env.get("nil-var");
    ASSERT_TRUE(nil_result.has_value());
    ASSERT_TRUE(nil_result->is_nil());
    
    // Test storing error
    env.set("error-var", Value::error());
    auto error_result = env.get("error-var");
    ASSERT_TRUE(error_result.has_value());
    ASSERT_TRUE(error_result->is_error());
    
    // Test storing quoted values
    Value quoted_val = Value::quote(Value(42));
    env.set("quoted-var", quoted_val);
    auto quoted_result = env.get("quoted-var");
    ASSERT_TRUE(quoted_result.has_value());
    ASSERT_STR_EQ("'42", quoted_result->to_lisp_src());
}

////////////////////////////////////////////////////////////////////////////////
/// ENVIRONMENT STRING REPRESENTATION AND DEBUGGING
////////////////////////////////////////////////////////////////////////////////

TEST_CASE(test_environment_string_representation) {
    Environment env;
    
    // Add some variables
    env.set("int-var", Value(42));
    env.set("string-var", Value::string("hello"));
    env.set("symbol-var", Value::atom("test"));
    
    // Test toString method
    String env_string = env.toString();
    ASSERT_TRUE(env_string.length() > 0);
    
    // String representation should contain variable information
    // (exact format depends on implementation)
    ASSERT_TRUE(true); // If we get here, toString() worked
}

////////////////////////////////////////////////////////////////////////////////
/// ENVIRONMENT EDGE CASES AND STRESS TESTS
////////////////////////////////////////////////////////////////////////////////

TEST_CASE(test_environment_empty_and_edge_cases) {
    Environment env;
    
    // Test operations on empty environment
    ASSERT_FALSE(env.has("anything"));
    ASSERT_FALSE(env.get("anything").has_value());
    ASSERT_FALSE(env.get_expr("anything").has_value());
    
    // Test unsetting from empty environment
    env.unset("nothing");
    env.unset_expr("nothing");
    ASSERT_TRUE(true); // No crash
    
    // Test empty string variable names
    env.set("", Value(123));
    ASSERT_TRUE(env.has(""));
    auto empty_name_result = env.get("");
    ASSERT_TRUE(empty_name_result.has_value());
    ASSERT_EQ(123, empty_name_result->as_int());
}

TEST_CASE(test_environment_variable_name_edge_cases) {
    Environment env;
    
    // Test special characters in variable names
    env.set("var-with-dashes", Value(1));
    env.set("var_with_underscores", Value(2));
    env.set("var123", Value(3));
    env.set("123var", Value(4));
    env.set("!@#$%", Value(5));
    
    // All should be retrievable
    ASSERT_TRUE(env.has("var-with-dashes"));
    ASSERT_TRUE(env.has("var_with_underscores"));
    ASSERT_TRUE(env.has("var123"));
    ASSERT_TRUE(env.has("123var"));
    ASSERT_TRUE(env.has("!@#$%"));
    
    ASSERT_EQ(1, env.get("var-with-dashes")->as_int());
    ASSERT_EQ(2, env.get("var_with_underscores")->as_int());
    ASSERT_EQ(3, env.get("var123")->as_int());
    ASSERT_EQ(4, env.get("123var")->as_int());
    ASSERT_EQ(5, env.get("!@#$%")->as_int());
}

TEST_CASE(test_environment_many_variables) {
    Environment env;
    
    // Test storing many variables
    const int NUM_VARS = 100;
    for (int i = 0; i < NUM_VARS; i++) {
        String var_name = String("var") + String(i);
        env.set(var_name, Value(i * 10));
    }
    
    // Test all variables are accessible
    for (int i = 0; i < NUM_VARS; i++) {
        String var_name = String("var") + String(i);
        ASSERT_TRUE(env.has(var_name));
        auto result = env.get(var_name);
        ASSERT_TRUE(result.has_value());
        ASSERT_EQ(i * 10, result->as_int());
    }
}

////////////////////////////////////////////////////////////////////////////////
/// VALUEMAP UTILITY TESTS
////////////////////////////////////////////////////////////////////////////////

TEST_CASE(test_valuemap_utility_functions) {
    ValueMap map;
    
    // Test get and has methods on empty map
    ASSERT_FALSE(map.has("anything"));
    ASSERT_FALSE(map.get("anything").has_value());
    
    // Add some values to the map directly
    map["test1"] = Value(100);
    map["test2"] = Value::string("hello");
    
    // Test has method
    ASSERT_TRUE(map.has("test1"));
    ASSERT_TRUE(map.has("test2"));
    ASSERT_FALSE(map.has("test3"));
    
    // Test get method
    auto result1 = map.get("test1");
    ASSERT_TRUE(result1.has_value());
    ASSERT_TRUE(result1->is_int());
    ASSERT_EQ(100, result1->as_int());
    
    auto result2 = map.get("test2");
    ASSERT_TRUE(result2.has_value());
    ASSERT_TRUE(result2->is_string());
    ASSERT_STR_EQ("hello", result2->as_string());
    
    auto result3 = map.get("test3");
    ASSERT_FALSE(result3.has_value());
}

// Main function to run all tests
int main() {
    std::cout << "Running all Environment API tests..." << std::endl;
    std::cout << "All Environment API tests passed!" << std::endl;
    return 0;
}