#define CATCH_CONFIG_MAIN
#include "catch.hpp"
#include "../uSEQ/src/uSEQ.h"
#include "../uSEQ/src/ports/mocks/MockStorage.h"
#include "../uSEQ/src/ports/mocks/MockClock.h"
#include "../uSEQ/src/ports/mocks/MockLogger.h"

TEST_CASE("Storage Environment - Basic save and load", "[storage][environment]") {
    // Set up mock components
    MockStorage mock_storage(16 * 1024); // 16KB storage
    MockClock clock;
    MockLogger logger;
    
    uSEQ useq_instance(&clock, &logger, nullptr, nullptr, &mock_storage);
    useq_instance.init();
    
    // Clear any built-in definitions loaded by init() for clean testing
    useq_instance.__test_get_defs().clear();
    useq_instance.__test_get_def_exprs().clear();
    
    SECTION("Save and load simple variables") {
        // Clear any previous test state
        useq_instance.__test_get_defs().clear();
        useq_instance.__test_get_def_exprs().clear();
        
        // Add some simple variable definitions to the environment
        useq_instance.__test_get_defs()[String("x")] = Value(42);
        useq_instance.__test_get_defs()[String("y")] = Value::string(String("hello"));
        useq_instance.__test_get_defs()[String("z")] = Value(3.14);
        
        // Save environment to storage
        REQUIRE(useq_instance.__test_save_env_to_storage(mock_storage));
        
        // Clear the environment
        useq_instance.__test_get_defs().clear();
        useq_instance.__test_get_def_exprs().clear();
        
        REQUIRE(useq_instance.__test_get_defs().empty());
        REQUIRE(useq_instance.__test_get_def_exprs().empty());
        
        // Load environment from storage
        REQUIRE(useq_instance.__test_load_env_from_storage(mock_storage));
        
        // Verify variables were restored
        REQUIRE(useq_instance.__test_get_defs().size() == 3);
        
        auto it_x = useq_instance.__test_get_defs().find(String("x"));
        REQUIRE(it_x != useq_instance.__test_get_defs().end());
        REQUIRE(it_x->second.is_number());
        REQUIRE(it_x->second.as_int() == 42);
        
        auto it_y = useq_instance.__test_get_defs().find(String("y"));
        REQUIRE(it_y != useq_instance.__test_get_defs().end());
        REQUIRE(it_y->second.is_string());
        REQUIRE(it_y->second.as_string() == String("hello"));
        
        auto it_z = useq_instance.__test_get_defs().find(String("z"));
        REQUIRE(it_z != useq_instance.__test_get_defs().end());
        REQUIRE(it_z->second.is_number());
        REQUIRE(it_z->second.as_float() == Approx(3.14));
    }
    
    SECTION("Save and load expressions") {
        // Clear any previous test state
        useq_instance.__test_get_defs().clear();
        useq_instance.__test_get_def_exprs().clear();
        
        // Add some expression definitions (defexpr)
        Value lambda_expr = Value({
            Value::atom(String("lambda")),
            Value({Value::atom(String("x"))}),
            Value({Value::atom(String("+")), Value::atom(String("x")), Value(1)})
        });
        
        Value addition_expr = Value({
            Value::atom(String("+")),
            Value(10),
            Value(20)
        });
        
        useq_instance.__test_get_def_exprs()[String("func")] = lambda_expr;
        useq_instance.__test_get_def_exprs()[String("sum")] = addition_expr;
        
        // Save environment to storage
        REQUIRE(useq_instance.__test_save_env_to_storage(mock_storage));
        
        // Clear the environment
        useq_instance.__test_get_defs().clear();
        useq_instance.__test_get_def_exprs().clear();
        
        // Load environment from storage
        REQUIRE(useq_instance.__test_load_env_from_storage(mock_storage));
        
        // Verify expressions were restored
        REQUIRE(useq_instance.__test_get_def_exprs().size() == 2);
        
        auto it_func = useq_instance.__test_get_def_exprs().find(String("func"));
        REQUIRE(it_func != useq_instance.__test_get_def_exprs().end());
        REQUIRE(it_func->second.is_list());
        
        auto it_sum = useq_instance.__test_get_def_exprs().find(String("sum"));
        REQUIRE(it_sum != useq_instance.__test_get_def_exprs().end());
        REQUIRE(it_sum->second.is_list());
    }
    
    SECTION("Save and load mixed definitions") {
        // Clear any previous test state
        useq_instance.__test_get_defs().clear();
        useq_instance.__test_get_def_exprs().clear();
        
        // Add regular definitions
        useq_instance.__test_get_defs()[String("count")] = Value(100);
        useq_instance.__test_get_defs()[String("name")] = Value::string(String("test_module"));
        
        // Add expression definitions
        useq_instance.__test_get_def_exprs()[String("doubler")] = Value({
            Value::atom(String("lambda")),
            Value({Value::atom(String("n"))}),
            Value({Value::atom(String("*")), Value::atom(String("n")), Value(2)})
        });
        
        // Save and verify round-trip
        REQUIRE(useq_instance.__test_save_env_to_storage(mock_storage));
        
        // Clear and reload
        useq_instance.__test_get_defs().clear();
        useq_instance.__test_get_def_exprs().clear();
        
        REQUIRE(useq_instance.__test_load_env_from_storage(mock_storage));
        
        // Verify all definitions were restored
        REQUIRE(useq_instance.__test_get_defs().size() == 2);
        REQUIRE(useq_instance.__test_get_def_exprs().size() == 1);
        
        // Check specific values
        REQUIRE(useq_instance.__test_get_defs()[String("count")].as_int() == 100);
        REQUIRE(useq_instance.__test_get_defs()[String("name")].as_string() == String("test_module"));
        
        auto doubler_it = useq_instance.__test_get_def_exprs().find(String("doubler"));
        REQUIRE(doubler_it != useq_instance.__test_get_def_exprs().end());
        REQUIRE(doubler_it->second.is_list());
    }
}

TEST_CASE("Storage Environment - Edge cases", "[storage][edge_cases]") {
    MockStorage mock_storage(16 * 1024);
    MockClock clock;
    MockLogger logger;
    
    uSEQ useq_instance(&clock, &logger, nullptr, nullptr, &mock_storage);
    useq_instance.init();
    
    // Clear any built-in definitions loaded by init() for clean testing
    useq_instance.__test_get_defs().clear();
    useq_instance.__test_get_def_exprs().clear();
    
    SECTION("Empty environment") {
        // Ensure environment is empty
        useq_instance.__test_get_defs().clear();
        useq_instance.__test_get_def_exprs().clear();
        
        // Save empty environment
        REQUIRE(useq_instance.__test_save_env_to_storage(mock_storage));
        
        // Add some data to make sure loading actually clears it
        useq_instance.__test_get_defs()[String("temp")] = Value(999);
        REQUIRE(useq_instance.__test_get_defs().size() == 1);
        
        // Load empty environment
        REQUIRE(useq_instance.__test_load_env_from_storage(mock_storage));
        
        // Should be empty again
        REQUIRE(useq_instance.__test_get_defs().empty());
        REQUIRE(useq_instance.__test_get_def_exprs().empty());
    }
    
    SECTION("Large environment") {
        // Clear any previous test state
        useq_instance.__test_get_defs().clear();
        useq_instance.__test_get_def_exprs().clear();
        
        // Add many variables
        for (int i = 0; i < 20; i++) {
            String var_name = String("var") + String(std::to_string(i).c_str());
            useq_instance.__test_get_defs()[var_name] = Value(i * 2);
            
            String expr_name = String("expr") + String(std::to_string(i).c_str());
            useq_instance.__test_get_def_exprs()[expr_name] = Value({
                Value::atom(String("+")), Value(i), Value(i + 1)
            });
        }
        
        // Save large environment
        REQUIRE(useq_instance.__test_save_env_to_storage(mock_storage));
        
        // Clear and reload
        useq_instance.__test_get_defs().clear();
        useq_instance.__test_get_def_exprs().clear();
        
        REQUIRE(useq_instance.__test_load_env_from_storage(mock_storage));
        
        // Verify all data was restored
        REQUIRE(useq_instance.__test_get_defs().size() == 20);
        REQUIRE(useq_instance.__test_get_def_exprs().size() == 20);
        
        // Spot check a few values
        REQUIRE(useq_instance.__test_get_defs()[String("var10")].as_int() == 20);
        
        auto expr_it = useq_instance.__test_get_def_exprs().find(String("expr5"));
        REQUIRE(expr_it != useq_instance.__test_get_def_exprs().end());
        REQUIRE(expr_it->second.is_list());
    }
}

TEST_CASE("Storage Environment - Error handling", "[storage][error]") {
    MockClock clock;
    MockLogger logger;
    
    SECTION("Storage operations fail") {
        // Create a storage that always fails
        class FailingStorage : public IStorage {
        public:
            bool write(uint32_t, const uint8_t*, size_t) override { return false; }
            bool read(uint32_t, uint8_t*, size_t) override { return false; }
            bool erase(uint32_t, size_t) override { return false; }
        };
        
        FailingStorage failing_storage;
        uSEQ useq_instance(&clock, &logger, nullptr, nullptr, &failing_storage);
        useq_instance.init();
        
        // Add some data
        useq_instance.__test_get_defs()[String("test")] = Value(123);
        
        // Save should fail
        REQUIRE_FALSE(useq_instance.__test_save_env_to_storage(failing_storage));
        
        // Load should fail
        REQUIRE_FALSE(useq_instance.__test_load_env_from_storage(failing_storage));
    }
    
    SECTION("No storage port injected") {
        // Create uSEQ without storage port
        uSEQ useq_instance(&clock, &logger, nullptr, nullptr, nullptr);
        useq_instance.init();
        useq_instance.__test_get_defs().clear();
        useq_instance.__test_get_def_exprs().clear();
        
        MockStorage mock_storage;
        
        // Desktop functions should still work with direct storage parameter
        // even when no storage port is injected
        REQUIRE(useq_instance.__test_save_env_to_storage(mock_storage));
        REQUIRE(useq_instance.__test_load_env_from_storage(mock_storage));
    }
}

TEST_CASE("Mock Storage - Basic functionality", "[storage][mock]") {
    SECTION("Basic read/write operations") {
        MockStorage storage(1024);
        
        // Write some data
        uint8_t write_data[] = {0x01, 0x02, 0x03, 0x04};
        REQUIRE(storage.write(0, write_data, sizeof(write_data)));
        
        // Read it back
        uint8_t read_data[4];
        REQUIRE(storage.read(0, read_data, sizeof(read_data)));
        
        // Verify data matches
        for (size_t i = 0; i < sizeof(write_data); i++) {
            REQUIRE(read_data[i] == write_data[i]);
        }
    }
    
    SECTION("Bounds checking") {
        MockStorage small_storage(10); // Only 10 bytes
        
        uint8_t data[20] = {0};
        
        // Write beyond bounds should fail
        REQUIRE_FALSE(small_storage.write(5, data, 20));
        
        // Read beyond bounds should fail
        REQUIRE_FALSE(small_storage.read(5, data, 20));
        
        // Erase beyond bounds should fail
        REQUIRE_FALSE(small_storage.erase(5, 20));
    }
    
    SECTION("Erase functionality") {
        MockStorage storage(100);
        
        // Fill with non-zero data
        uint8_t fill_data[50];
        for (size_t i = 0; i < sizeof(fill_data); i++) {
            fill_data[i] = 0xFF;
        }
        REQUIRE(storage.write(0, fill_data, sizeof(fill_data)));
        
        // Erase part of it
        REQUIRE(storage.erase(10, 20));
        
        // Verify erased portion is zero
        uint8_t read_data[50];
        REQUIRE(storage.read(0, read_data, sizeof(read_data)));
        
        // First 10 bytes should still be 0xFF
        for (int i = 0; i < 10; i++) {
            REQUIRE(read_data[i] == 0xFF);
        }
        
        // Next 20 bytes should be 0 (erased)
        for (int i = 10; i < 30; i++) {
            REQUIRE(read_data[i] == 0);
        }
        
        // Remaining bytes should still be 0xFF
        for (int i = 30; i < 50; i++) {
            REQUIRE(read_data[i] == 0xFF);
        }
    }
}