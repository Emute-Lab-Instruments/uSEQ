// Dedicated main for this test executable
#define CATCH_CONFIG_MAIN
#include "catch.hpp"
#include <memory>

#include "../uSEQ/src/uSEQ/output_manager.h"

TEST_CASE("OutputManager configuration table", "[output_manager][config]") {
    SECTION("Static configuration data is correct") {
        const auto& configs = OutputManager::get_all_configs();
        
        // Should have 8 + 8 + 8 = 24 total configurations
        REQUIRE(configs.size() == 24);
        
        // Check some specific configurations
        const auto* a1_config = OutputManager::get_config(1, OutputManager::OutputType::CONTINUOUS);
        REQUIRE(a1_config != nullptr);
        REQUIRE(a1_config->id == 1);
        REQUIRE(a1_config->type == OutputManager::OutputType::CONTINUOUS);
        REQUIRE(a1_config->pin_index == 0);
        REQUIRE(std::string(a1_config->lisp_name) == "a1");
        REQUIRE(std::string(a1_config->getter_name) == "get-a1");
        
        const auto* d3_config = OutputManager::get_config(3, OutputManager::OutputType::BINARY);
        REQUIRE(d3_config != nullptr);
        REQUIRE(d3_config->id == 3);
        REQUIRE(d3_config->type == OutputManager::OutputType::BINARY);
        REQUIRE(d3_config->pin_index == 2);
        REQUIRE(std::string(d3_config->lisp_name) == "d3");
        REQUIRE(std::string(d3_config->getter_name) == "get-d3");
        
        const auto* s5_config = OutputManager::get_config(5, OutputManager::OutputType::SERIAL);
        REQUIRE(s5_config != nullptr);
        REQUIRE(s5_config->id == 5);
        REQUIRE(s5_config->type == OutputManager::OutputType::SERIAL);
        REQUIRE(s5_config->pin_index == 4);
        REQUIRE(std::string(s5_config->lisp_name) == "s5");
        REQUIRE(std::string(s5_config->getter_name) == "get-s5");
    }
    
    SECTION("Non-existent configurations return null") {
        const auto* invalid_config = OutputManager::get_config(99, OutputManager::OutputType::CONTINUOUS);
        REQUIRE(invalid_config == nullptr);
    }
}

TEST_CASE("OutputManager availability checks", "[output_manager][availability]") {
    SECTION("Valid outputs are available") {
        // Desktop defaults: 3 continuous, 3 binary, 8 serial
        REQUIRE(OutputManager::is_output_available(1, OutputManager::OutputType::CONTINUOUS) == true);
        REQUIRE(OutputManager::is_output_available(3, OutputManager::OutputType::CONTINUOUS) == true);
        REQUIRE(OutputManager::is_output_available(1, OutputManager::OutputType::BINARY) == true);
        REQUIRE(OutputManager::is_output_available(3, OutputManager::OutputType::BINARY) == true);
        REQUIRE(OutputManager::is_output_available(1, OutputManager::OutputType::SERIAL) == true);
        REQUIRE(OutputManager::is_output_available(8, OutputManager::OutputType::SERIAL) == true);
    }
    
    SECTION("Out-of-bounds outputs are not available") {
        // Test bounds for desktop config
        REQUIRE(OutputManager::is_output_available(4, OutputManager::OutputType::CONTINUOUS) == false);
        REQUIRE(OutputManager::is_output_available(4, OutputManager::OutputType::BINARY) == false);
        REQUIRE(OutputManager::is_output_available(9, OutputManager::OutputType::SERIAL) == false);
        REQUIRE(OutputManager::is_output_available(0, OutputManager::OutputType::CONTINUOUS) == false);
        REQUIRE(OutputManager::is_output_available(-1, OutputManager::OutputType::CONTINUOUS) == false);
    }
}

TEST_CASE("OutputManager name utilities", "[output_manager][names]") {
    SECTION("get_lisp_name returns correct names") {
        REQUIRE(std::string(OutputManager::get_lisp_name(1, OutputManager::OutputType::CONTINUOUS)) == "a1");
        REQUIRE(std::string(OutputManager::get_lisp_name(2, OutputManager::OutputType::BINARY)) == "d2");
        REQUIRE(std::string(OutputManager::get_lisp_name(3, OutputManager::OutputType::SERIAL)) == "s3");
        
        // Invalid IDs return null
        REQUIRE(OutputManager::get_lisp_name(99, OutputManager::OutputType::CONTINUOUS) == nullptr);
    }
    
    SECTION("get_getter_name returns correct names") {
        REQUIRE(std::string(OutputManager::get_getter_name(1, OutputManager::OutputType::CONTINUOUS)) == "get-a1");
        REQUIRE(std::string(OutputManager::get_getter_name(2, OutputManager::OutputType::BINARY)) == "get-d2");
        REQUIRE(std::string(OutputManager::get_getter_name(3, OutputManager::OutputType::SERIAL)) == "get-s3");
        
        // Invalid IDs return null
        REQUIRE(OutputManager::get_getter_name(99, OutputManager::OutputType::CONTINUOUS) == nullptr);
    }
}

TEST_CASE("OutputManager configuration completeness", "[output_manager][completeness]") {
    const auto& configs = OutputManager::get_all_configs();
    
    SECTION("All continuous outputs are configured") {
        for (int i = 1; i <= 8; i++) {
            const auto* config = OutputManager::get_config(i, OutputManager::OutputType::CONTINUOUS);
            REQUIRE(config != nullptr);
            REQUIRE(config->id == i);
            REQUIRE(config->pin_index == i - 1);  // 0-based indexing
            REQUIRE(std::string(config->lisp_name) == ("a" + std::to_string(i)));
            REQUIRE(std::string(config->getter_name) == ("get-a" + std::to_string(i)));
        }
    }
    
    SECTION("All binary outputs are configured") {
        for (int i = 1; i <= 8; i++) {
            const auto* config = OutputManager::get_config(i, OutputManager::OutputType::BINARY);
            REQUIRE(config != nullptr);
            REQUIRE(config->id == i);
            REQUIRE(config->pin_index == i - 1);  // 0-based indexing
            REQUIRE(std::string(config->lisp_name) == ("d" + std::to_string(i)));
            REQUIRE(std::string(config->getter_name) == ("get-d" + std::to_string(i)));
        }
    }
    
    SECTION("All serial outputs are configured") {
        for (int i = 1; i <= 8; i++) {
            const auto* config = OutputManager::get_config(i, OutputManager::OutputType::SERIAL);
            REQUIRE(config != nullptr);
            REQUIRE(config->id == i);
            REQUIRE(config->pin_index == i - 1);  // 0-based indexing
            REQUIRE(std::string(config->lisp_name) == ("s" + std::to_string(i)));
            REQUIRE(std::string(config->getter_name) == ("get-s" + std::to_string(i)));
        }
    }
}

TEST_CASE("OutputManager boundary condition tests", "[output_manager][boundaries]") {
    SECTION("Test configuration access at boundaries") {
        // Test first and last valid IDs for each type
        REQUIRE(OutputManager::get_config(1, OutputManager::OutputType::CONTINUOUS) != nullptr);
        REQUIRE(OutputManager::get_config(8, OutputManager::OutputType::CONTINUOUS) != nullptr);
        
        REQUIRE(OutputManager::get_config(1, OutputManager::OutputType::BINARY) != nullptr);
        REQUIRE(OutputManager::get_config(8, OutputManager::OutputType::BINARY) != nullptr);
        
        REQUIRE(OutputManager::get_config(1, OutputManager::OutputType::SERIAL) != nullptr);
        REQUIRE(OutputManager::get_config(8, OutputManager::OutputType::SERIAL) != nullptr);
        
        // Test just outside valid boundaries
        REQUIRE(OutputManager::get_config(0, OutputManager::OutputType::CONTINUOUS) == nullptr);
        REQUIRE(OutputManager::get_config(9, OutputManager::OutputType::CONTINUOUS) == nullptr);
        REQUIRE(OutputManager::get_config(-1, OutputManager::OutputType::BINARY) == nullptr);
        REQUIRE(OutputManager::get_config(10, OutputManager::OutputType::SERIAL) == nullptr);
    }
    
    SECTION("Availability checks match configuration data") {
        // For desktop defaults (3 continuous, 3 binary, 8 serial), verify that
        // availability checks are consistent with the overall configuration
        for (int i = 1; i <= 8; i++) {
            bool continuous_config_exists = (OutputManager::get_config(i, OutputManager::OutputType::CONTINUOUS) != nullptr);
            bool continuous_available = OutputManager::is_output_available(i, OutputManager::OutputType::CONTINUOUS);
            
            bool binary_config_exists = (OutputManager::get_config(i, OutputManager::OutputType::BINARY) != nullptr);
            bool binary_available = OutputManager::is_output_available(i, OutputManager::OutputType::BINARY);
            
            bool serial_config_exists = (OutputManager::get_config(i, OutputManager::OutputType::SERIAL) != nullptr);
            bool serial_available = OutputManager::is_output_available(i, OutputManager::OutputType::SERIAL);
            
            // Config always exists (1-8), but availability depends on NUM_*_OUTS constants
            REQUIRE(continuous_config_exists == true);
            REQUIRE(binary_config_exists == true);
            REQUIRE(serial_config_exists == true);
            
            // For desktop: continuous/binary available 1-3, serial available 1-8
            if (i <= 3) {
                REQUIRE(continuous_available == true);
                REQUIRE(binary_available == true);
            } else {
                REQUIRE(continuous_available == false);
                REQUIRE(binary_available == false);
            }
            REQUIRE(serial_available == true);  // All 8 serial outputs available on desktop
        }
    }
}