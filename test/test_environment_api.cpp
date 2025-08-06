#define CATCH_CONFIG_MAIN  // This tells Catch to provide a main() - only do this in one cpp file
#include "catch.hpp"

#include "../uSEQ/src/modulisp/lisp/environment.h"
#include "../uSEQ/src/modulisp/lisp/value.h"
#include <optional>

////////////////////////////////////////////////////////////////////////////////
/// ENVIRONMENT CONSTRUCTION AND BASIC OPERATIONS
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("Environment construction and basic initialization", "[environment][construction]") {
    SECTION("default constructor") {
        Environment env1;
        
        // Environment should be constructible without issues
        REQUIRE(true); // If we get here, construction succeeded
    }
    
    SECTION("copy constructor") {
        Environment env1;
        Environment env2(env1);
        REQUIRE(true); // Copy construction succeeded
    }
    
    SECTION("move constructor") {
        Environment temp_env;
        Environment env3(std::move(temp_env));
        REQUIRE(true); // Move construction succeeded
    }
}

TEST_CASE("Basic environment variable operations", "[environment][variables]") {
    Environment env;
    
    SECTION("setting and getting integer variable") {
        Value int_val(42);
        env.set("test-var", int_val);
        
        // Test that variable exists
        REQUIRE(env.has("test-var"));
        
        // Test retrieving the variable
        std::optional<Value> retrieved = env.get("test-var");
        REQUIRE(retrieved.has_value());
        REQUIRE(retrieved->is_int());
        REQUIRE(retrieved->as_int() == 42);
    }
    
    SECTION("non-existent variable") {
        REQUIRE_FALSE(env.has("non-existent"));
        std::optional<Value> missing = env.get("non-existent");
        REQUIRE_FALSE(missing.has_value());
    }
}

TEST_CASE("Multiple variables of different types", "[environment][variables][types]") {
    Environment env;
    
    // Set multiple variables of different types
    env.set("int-var", Value(100));
    env.set("float-var", Value(3.14));
    env.set("string-var", Value::string("hello"));
    env.set("symbol-var", Value::atom("test-symbol"));
    
    SECTION("all variables exist") {
        REQUIRE(env.has("int-var"));
        REQUIRE(env.has("float-var"));
        REQUIRE(env.has("string-var"));
        REQUIRE(env.has("symbol-var"));
    }
    
    SECTION("integer variable") {
        auto int_result = env.get("int-var");
        REQUIRE(int_result.has_value());
        REQUIRE(int_result->is_int());
        REQUIRE(int_result->as_int() == 100);
    }
    
    SECTION("float variable") {
        auto float_result = env.get("float-var");
        REQUIRE(float_result.has_value());
        REQUIRE(float_result->is_float());
        REQUIRE(float_result->as_float() == Approx(3.14).epsilon(0.001));
    }
    
    SECTION("string variable") {
        auto string_result = env.get("string-var");
        REQUIRE(string_result.has_value());
        REQUIRE(string_result->is_string());
        REQUIRE(string_result->as_string() == "hello");
    }
    
    SECTION("symbol variable") {
        auto symbol_result = env.get("symbol-var");
        REQUIRE(symbol_result.has_value());
        REQUIRE(symbol_result->is_symbol());
        REQUIRE(symbol_result->as_atom() == "test-symbol");
    }
}

TEST_CASE("Variable overwriting behavior", "[environment][variables][overwrite]") {
    Environment env;
    
    SECTION("initial value setting") {
        env.set("var", Value(10));
        auto initial = env.get("var");
        REQUIRE(initial.has_value());
        REQUIRE(initial->as_int() == 10);
    }
    
    SECTION("overwrite with same type") {
        env.set("var", Value(10));
        env.set("var", Value(20));
        auto updated = env.get("var");
        REQUIRE(updated.has_value());
        REQUIRE(updated->as_int() == 20);
    }
    
    SECTION("overwrite with different type") {
        env.set("var", Value(10));
        env.set("var", Value::string("now-a-string"));
        auto changed_type = env.get("var");
        REQUIRE(changed_type.has_value());
        REQUIRE(changed_type->is_string());
        REQUIRE(changed_type->as_string() == "now-a-string");
    }
}

TEST_CASE("Variable unsetting operations", "[environment][variables][unset]") {
    Environment env;
    
    SECTION("unset existing variable") {
        env.set("temp-var", Value(123));
        REQUIRE(env.has("temp-var"));
        
        env.unset("temp-var");
        REQUIRE_FALSE(env.has("temp-var"));
        
        auto result = env.get("temp-var");
        REQUIRE_FALSE(result.has_value());
    }
    
    SECTION("unset non-existent variable") {
        env.unset("never-existed");
        REQUIRE(true); // If we get here, no crash occurred
    }
}

////////////////////////////////////////////////////////////////////////////////
/// ENVIRONMENT EXPRESSION STORAGE API TESTS
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("Expression storage in environment", "[environment][expressions]") {
    Environment env;
    
    SECTION("setting and getting expressions") {
        Value expr_val = Value::atom("some-expression");
        env.set_expr("expr-var", expr_val);
        
        std::optional<Value> retrieved_expr = env.get_expr("expr-var");
        REQUIRE(retrieved_expr.has_value());
        REQUIRE(retrieved_expr->is_symbol());
        REQUIRE(retrieved_expr->as_atom() == "some-expression");
    }
    
    SECTION("expressions separate from regular variables") {
        Value expr_val = Value::atom("some-expression");
        env.set_expr("expr-var", expr_val);
        
        // Regular get doesn't find expressions
        std::optional<Value> regular_get = env.get("expr-var");
        REQUIRE_FALSE(regular_get.has_value());
    }
    
    SECTION("unsetting expressions") {
        Value expr_val = Value::atom("some-expression");
        env.set_expr("expr-var", expr_val);
        
        env.unset_expr("expr-var");
        std::optional<Value> after_unset = env.get_expr("expr-var");
        REQUIRE_FALSE(after_unset.has_value());
    }
}

TEST_CASE("Expression and value namespace separation", "[environment][expressions][separation]") {
    Environment env;
    
    // Set both a value and an expression with the same name
    env.set("dual-var", Value(42));
    env.set_expr("dual-var", Value::string("expression-code"));
    
    SECTION("both namespaces coexist") {
        auto value_result = env.get("dual-var");
        REQUIRE(value_result.has_value());
        REQUIRE(value_result->is_int());
        REQUIRE(value_result->as_int() == 42);
        
        auto expr_result = env.get_expr("dual-var");
        REQUIRE(expr_result.has_value());
        REQUIRE(expr_result->is_string());
        REQUIRE(expr_result->as_string() == "expression-code");
    }
    
    SECTION("unsetting operates independently") {
        env.unset("dual-var");
        REQUIRE_FALSE(env.get("dual-var").has_value());
        REQUIRE(env.get_expr("dual-var").has_value());
        
        env.unset_expr("dual-var");
        REQUIRE_FALSE(env.get_expr("dual-var").has_value());
    }
}

////////////////////////////////////////////////////////////////////////////////
/// ENVIRONMENT SCOPING AND INHERITANCE TESTS
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("Parent-child environment scoping", "[environment][scoping]") {
    Environment parent_env;
    Environment child_env;
    
    // Set up parent-child relationship
    child_env.set_parent_scope(&parent_env);
    
    SECTION("child inherits from parent") {
        parent_env.set("parent-var", Value(100));
        
        REQUIRE(child_env.has("parent-var"));
        auto inherited = child_env.get("parent-var");
        REQUIRE(inherited.has_value());
        REQUIRE(inherited->as_int() == 100);
    }
    
    SECTION("parent cannot see child variables") {
        child_env.set("child-var", Value(200));
        
        REQUIRE_FALSE(parent_env.has("child-var"));
        REQUIRE(child_env.has("child-var"));
        
        auto child_var = child_env.get("child-var");
        REQUIRE(child_var.has_value());
        REQUIRE(child_var->as_int() == 200);
    }
}

TEST_CASE("Variable shadowing in environment scopes", "[environment][scoping][shadowing]") {
    Environment parent_env;
    Environment child_env;
    child_env.set_parent_scope(&parent_env);
    
    parent_env.set("shadowed-var", Value(10));
    
    SECTION("child initially sees parent variable") {
        auto inherited = child_env.get("shadowed-var");
        REQUIRE(inherited.has_value());
        REQUIRE(inherited->as_int() == 10);
    }
    
    SECTION("child can shadow parent variable") {
        child_env.set("shadowed-var", Value(20));
        
        // Child should see its own value
        auto child_value = child_env.get("shadowed-var");
        REQUIRE(child_value.has_value());
        REQUIRE(child_value->as_int() == 20);
        
        // Parent should still have original value
        auto parent_value = parent_env.get("shadowed-var");
        REQUIRE(parent_value.has_value());
        REQUIRE(parent_value->as_int() == 10);
    }
}

TEST_CASE("Multi-level environment scoping", "[environment][scoping][multilevel]") {
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
    
    SECTION("child sees all variables") {
        REQUIRE(child_env.has("gp-var"));
        REQUIRE(child_env.has("p-var"));
        REQUIRE(child_env.has("c-var"));
        
        auto gp_val = child_env.get("gp-var");
        auto p_val = child_env.get("p-var");
        auto c_val = child_env.get("c-var");
        
        REQUIRE(gp_val.has_value());
        REQUIRE(p_val.has_value());
        REQUIRE(c_val.has_value());
        
        REQUIRE(gp_val->as_int() == 1);
        REQUIRE(p_val->as_int() == 2);
        REQUIRE(c_val->as_int() == 3);
    }
    
    SECTION("parent sees grandparent but not child") {
        REQUIRE(parent_env.has("gp-var"));
        REQUIRE(parent_env.has("p-var"));
        REQUIRE_FALSE(parent_env.has("c-var"));
    }
    
    SECTION("grandparent only sees its own variables") {
        REQUIRE(grandparent_env.has("gp-var"));
        REQUIRE_FALSE(grandparent_env.has("p-var"));
        REQUIRE_FALSE(grandparent_env.has("c-var"));
    }
}

////////////////////////////////////////////////////////////////////////////////
/// ENVIRONMENT GLOBAL OPERATIONS API TESTS
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("Global variable setting in environment hierarchy", "[environment][global][variables]") {
    Environment grandparent_env;
    Environment parent_env;
    Environment child_env;
    
    // Set up hierarchy
    parent_env.set_parent_scope(&grandparent_env);
    child_env.set_parent_scope(&parent_env);
    
    // Set global variable from child environment
    child_env.set_global("global-var", Value(999));
    
    SECTION("global variable accessible from all levels") {
        REQUIRE(grandparent_env.has("global-var"));
        REQUIRE(parent_env.has("global-var"));
        REQUIRE(child_env.has("global-var"));
    }
    
    SECTION("global variable has same value at all levels") {
        auto gp_global = grandparent_env.get("global-var");
        auto p_global = parent_env.get("global-var");
        auto c_global = child_env.get("global-var");
        
        REQUIRE(gp_global.has_value());
        REQUIRE(p_global.has_value());
        REQUIRE(c_global.has_value());
        
        REQUIRE(gp_global->as_int() == 999);
        REQUIRE(p_global->as_int() == 999);
        REQUIRE(c_global->as_int() == 999);
    }
}

TEST_CASE("Global expression setting in environment hierarchy", "[environment][global][expressions]") {
    Environment parent_env;
    Environment child_env;
    child_env.set_parent_scope(&parent_env);
    
    // Set global expression from child
    child_env.set_global_expr("global-expr", Value::string("global-expression"));
    
    SECTION("global expression accessible from both levels") {
        auto parent_expr = parent_env.get_expr("global-expr");
        auto child_expr = child_env.get_expr("global-expr");
        
        REQUIRE(parent_expr.has_value());
        REQUIRE(child_expr.has_value());
        
        REQUIRE(parent_expr->as_string() == "global-expression");
        REQUIRE(child_expr->as_string() == "global-expression");
    }
}

////////////////////////////////////////////////////////////////////////////////
/// ENVIRONMENT COMBINATION AND MERGING TESTS
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("Environment combination without conflicts", "[environment][combination]") {
    Environment env1;
    Environment env2;
    
    // Set different variables in each environment
    env1.set("var1", Value(10));
    env1.set("var2", Value(20));
    
    env2.set("var3", Value(30));
    env2.set("var4", Value(40));
    
    // Combine env2 into env1
    env1.combine(env2);
    
    SECTION("target environment has all variables") {
        REQUIRE(env1.has("var1"));
        REQUIRE(env1.has("var2"));
        REQUIRE(env1.has("var3"));
        REQUIRE(env1.has("var4"));
        
        // Verify values are correct
        REQUIRE(env1.get("var1")->as_int() == 10);
        REQUIRE(env1.get("var2")->as_int() == 20);
        REQUIRE(env1.get("var3")->as_int() == 30);
        REQUIRE(env1.get("var4")->as_int() == 40);
    }
    
    SECTION("source environment unchanged") {
        REQUIRE(env2.has("var3"));
        REQUIRE(env2.has("var4"));
        REQUIRE_FALSE(env2.has("var1"));
        REQUIRE_FALSE(env2.has("var2"));
    }
}

TEST_CASE("Environment combination with conflicts", "[environment][combination][conflicts]") {
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
    
    SECTION("conflicting variable handled") {
        REQUIRE(env1.has("conflict-var"));
        auto conflict_result = env1.get("conflict-var");
        REQUIRE(conflict_result.has_value());
        // The exact behavior depends on implementation, but value should be one of the two
        REQUIRE((conflict_result->as_int() == 100 || conflict_result->as_int() == 200));
    }
    
    SECTION("unique variables preserved") {
        REQUIRE(env1.has("unique1"));
        REQUIRE(env1.has("unique2"));
        REQUIRE(env1.get("unique1")->as_int() == 1);
        REQUIRE(env1.get("unique2")->as_int() == 2);
    }
}

////////////////////////////////////////////////////////////////////////////////
/// ENVIRONMENT COMPLEX DATA TYPES TESTS
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("Complex data types in environment", "[environment][complex_types]") {
    Environment env;
    
    SECTION("storing and retrieving lists") {
        std::vector<Value> list_data = {Value(1), Value(2), Value(3)};
        Value list_val(list_data);
        env.set("list-var", list_val);
        
        auto retrieved_list = env.get("list-var");
        REQUIRE(retrieved_list.has_value());
        REQUIRE(retrieved_list->is_list());
        REQUIRE(retrieved_list->as_list().size() == 3);
    }
    
    SECTION("storing and retrieving vectors") {
        std::vector<Value> vector_data = {Value(10), Value(20)};
        Value vector_val = Value::vector(vector_data);
        env.set("vector-var", vector_val);
        
        auto retrieved_vector = env.get("vector-var");
        REQUIRE(retrieved_vector.has_value());
        REQUIRE(retrieved_vector->is_vector());
        REQUIRE(retrieved_vector->as_vector().size() == 2);
    }
    
    SECTION("storing nested structures") {
        std::vector<Value> list_data = {Value(1), Value(2), Value(3)};
        Value list_val(list_data);
        
        std::vector<Value> vector_data = {Value(10), Value(20)};
        Value vector_val = Value::vector(vector_data);
        
        std::vector<Value> nested_data = {list_val, vector_val};
        Value nested_val(nested_data);
        env.set("nested-var", nested_val);
        
        auto retrieved_nested = env.get("nested-var");
        REQUIRE(retrieved_nested.has_value());
        REQUIRE(retrieved_nested->is_list());
        REQUIRE(retrieved_nested->as_list().size() == 2);
        REQUIRE(retrieved_nested->as_list()[0].is_list());
        REQUIRE(retrieved_nested->as_list()[1].is_vector());
    }
}

TEST_CASE("Special value types in environment", "[environment][special_values]") {
    Environment env;
    
    SECTION("storing nil values") {
        env.set("nil-var", Value::nil());
        auto nil_result = env.get("nil-var");
        REQUIRE(nil_result.has_value());
        REQUIRE(nil_result->is_nil());
    }
    
    SECTION("storing error values") {
        env.set("error-var", Value::error());
        auto error_result = env.get("error-var");
        REQUIRE(error_result.has_value());
        REQUIRE(error_result->is_error());
    }
    
    SECTION("storing quoted values") {
        Value quoted_val = Value::quote(Value(42));
        env.set("quoted-var", quoted_val);
        auto quoted_result = env.get("quoted-var");
        REQUIRE(quoted_result.has_value());
        REQUIRE(quoted_result->to_lisp_src() == "'42");
    }
}

////////////////////////////////////////////////////////////////////////////////
/// ENVIRONMENT STRING REPRESENTATION AND DEBUGGING
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("Environment string representation", "[environment][toString]") {
    Environment env;
    
    // Add some variables
    env.set("int-var", Value(42));
    env.set("string-var", Value::string("hello"));
    env.set("symbol-var", Value::atom("test"));
    
    SECTION("toString produces non-empty output") {
        String env_string = env.toString();
        REQUIRE(env_string.length() > 0);
        
        // String representation should contain variable information
        // (exact format depends on implementation)
        REQUIRE(true); // If we get here, toString() worked
    }
}

////////////////////////////////////////////////////////////////////////////////
/// ENVIRONMENT EDGE CASES AND STRESS TESTS
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("Environment edge cases and empty operations", "[environment][edge_cases]") {
    Environment env;
    
    SECTION("operations on empty environment") {
        REQUIRE_FALSE(env.has("anything"));
        REQUIRE_FALSE(env.get("anything").has_value());
        REQUIRE_FALSE(env.get_expr("anything").has_value());
    }
    
    SECTION("unsetting from empty environment") {
        env.unset("nothing");
        env.unset_expr("nothing");
        REQUIRE(true); // No crash
    }
    
    SECTION("empty string variable names") {
        env.set("", Value(123));
        REQUIRE(env.has(""));
        auto empty_name_result = env.get("");
        REQUIRE(empty_name_result.has_value());
        REQUIRE(empty_name_result->as_int() == 123);
    }
}

TEST_CASE("Variable name edge cases", "[environment][variable_names][edge_cases]") {
    Environment env;
    
    // Test special characters in variable names
    env.set("var-with-dashes", Value(1));
    env.set("var_with_underscores", Value(2));
    env.set("var123", Value(3));
    env.set("123var", Value(4));
    env.set("!@#$%", Value(5));
    
    SECTION("special character names exist") {
        REQUIRE(env.has("var-with-dashes"));
        REQUIRE(env.has("var_with_underscores"));
        REQUIRE(env.has("var123"));
        REQUIRE(env.has("123var"));
        REQUIRE(env.has("!@#$%"));
    }
    
    SECTION("special character names retrievable") {
        REQUIRE(env.get("var-with-dashes")->as_int() == 1);
        REQUIRE(env.get("var_with_underscores")->as_int() == 2);
        REQUIRE(env.get("var123")->as_int() == 3);
        REQUIRE(env.get("123var")->as_int() == 4);
        REQUIRE(env.get("!@#$%")->as_int() == 5);
    }
}

TEST_CASE("Many variables stress test", "[environment][stress_test]") {
    Environment env;
    
    const int NUM_VARS = 100;
    
    SECTION("storing many variables") {
        for (int i = 0; i < NUM_VARS; i++) {
            String var_name = String("var") + String(i);
            env.set(var_name, Value(i * 10));
        }
        
        // Test all variables are accessible
        for (int i = 0; i < NUM_VARS; i++) {
            String var_name = String("var") + String(i);
            REQUIRE(env.has(var_name));
            auto result = env.get(var_name);
            REQUIRE(result.has_value());
            REQUIRE(result->as_int() == i * 10);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
/// VALUEMAP UTILITY TESTS
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("ValueMap utility functions", "[environment][valuemap]") {
    ValueMap map;
    
    SECTION("empty map operations") {
        REQUIRE_FALSE(map.has("anything"));
        REQUIRE_FALSE(map.get("anything").has_value());
    }
    
    SECTION("map with values") {
        // Add some values to the map directly
        map["test1"] = Value(100);
        map["test2"] = Value::string("hello");
        
        // Test has method
        REQUIRE(map.has("test1"));
        REQUIRE(map.has("test2"));
        REQUIRE_FALSE(map.has("test3"));
        
        // Test get method for integer
        auto result1 = map.get("test1");
        REQUIRE(result1.has_value());
        REQUIRE(result1->is_int());
        REQUIRE(result1->as_int() == 100);
        
        // Test get method for string
        auto result2 = map.get("test2");
        REQUIRE(result2.has_value());
        REQUIRE(result2->is_string());
        REQUIRE(result2->as_string() == "hello");
        
        // Test get method for non-existent key
        auto result3 = map.get("test3");
        REQUIRE_FALSE(result3.has_value());
    }
}

