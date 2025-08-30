#define CATCH_CONFIG_MAIN
#include "catch.hpp"
#include "../uSEQ/src/uSEQ.h"
#include "../uSEQ/src/ports/mocks/MockI2CBus.h"
#include "../uSEQ/src/ports/mocks/MockClock.h"
#include "../uSEQ/src/ports/mocks/MockLogger.h"

TEST_CASE("I2C Bus - Basic message sending and receiving", "[i2c][ports]") {
    // Set up shared mock components
    MockI2CBus mock_bus;
    MockClock clock;
    MockLogger logger;
    
    SECTION("Send and receive simple message") {
        // Create and send a message
        I2CMessage msg;
        msg.src = 1;
        msg.dst = 5; 
        msg.payload = {0x01, 0x02, 0x03, 0x04};
        
        REQUIRE(mock_bus.send(msg));
        
        // Check that the message is available at destination address
        REQUIRE(mock_bus.available(5));
        REQUIRE_FALSE(mock_bus.available(6)); // Other addresses should be empty
        
        // Receive the message
        I2CMessage received_msg;
        REQUIRE(mock_bus.receive(5, received_msg));
        
        // Verify message contents
        REQUIRE(received_msg.src == 1);
        REQUIRE(received_msg.dst == 5);
        REQUIRE(received_msg.payload.size() == 4);
        REQUIRE(received_msg.payload[0] == 0x01);
        REQUIRE(received_msg.payload[1] == 0x02);
        REQUIRE(received_msg.payload[2] == 0x03);
        REQUIRE(received_msg.payload[3] == 0x04);
        
        // After receiving, queue should be empty
        REQUIRE_FALSE(mock_bus.available(5));
    }
    
    SECTION("Multiple messages to same address") {
        // Send multiple messages to the same address
        for (int i = 0; i < 5; i++) {
            I2CMessage msg;
            msg.src = 1;
            msg.dst = 10;
            msg.payload = {static_cast<uint8_t>(i)};
            
            REQUIRE(mock_bus.send(msg));
        }
        
        // Verify all messages are queued
        REQUIRE(mock_bus.pending_messages(10) == 5);
        
        // Receive all messages in order
        for (int i = 0; i < 5; i++) {
            REQUIRE(mock_bus.available(10));
            
            I2CMessage received;
            REQUIRE(mock_bus.receive(10, received));
            
            REQUIRE(received.payload[0] == i);
        }
        
        // Queue should be empty now
        REQUIRE_FALSE(mock_bus.available(10));
        REQUIRE(mock_bus.pending_messages(10) == 0);
    }
}

TEST_CASE("uSEQ I2C Integration - Sync trigger via injected port", "[useq][i2c][integration]") {
    // Set up components
    MockI2CBus shared_bus;
    MockClock clock;
    MockLogger logger;
    
    // Create uSEQ instance with injected I2C bus
    uSEQ useq_instance(&clock, &logger, nullptr, &shared_bus, nullptr);
    useq_instance.init();
    
    SECTION("Send sync trigger via I2C port") {
        // Clear any existing messages
        shared_bus.clear_all();
        
        // Execute sync trigger using test helper
        Value result = useq_instance.__test_send_sync_trigger_i2c();
        
        REQUIRE(result.is_nil()); // Should return nil on success
        
        // Check that sync messages were sent to address 1
        REQUIRE(shared_bus.available(1));
        REQUIRE(shared_bus.pending_messages(1) >= 1); // At least one message
        
        // Receive the first sync message (high trigger)
        I2CMessage sync_msg;
        REQUIRE(shared_bus.receive(1, sync_msg));
        
        // Verify it's a sync message with 8 floats (32 bytes)
        REQUIRE(sync_msg.src == 0); // Source address
        REQUIRE(sync_msg.dst == 1); // Destination address
        REQUIRE(sync_msg.payload.size() == 32); // 8 floats * 4 bytes each
        
        // Verify the first few bytes represent high values (1.0f)
        float* float_data = reinterpret_cast<float*>(sync_msg.payload.data());
        REQUIRE(float_data[0] == Approx(1.0f));
        REQUIRE(float_data[1] == Approx(1.0f));
        REQUIRE(float_data[2] == Approx(1.0f));
    }
}

TEST_CASE("uSEQ I2C Integration - Send-to LISP function", "[useq][i2c][lisp]") {
    // Set up components
    MockI2CBus shared_bus;
    MockClock clock;
    MockLogger logger;
    
    uSEQ useq_instance(&clock, &logger, nullptr, &shared_bus, nullptr);
    useq_instance.init();
    
    SECTION("Send LISP expression via I2C") {
        shared_bus.clear_all();
        
        // Use test helper to send LISP expression to address 3
        Value result = useq_instance.__test_i2c_send_to(3, "(+ 1 2)");
        
        REQUIRE(result.is_nil()); // Should return nil on success
        
        // Check that message was sent to address 3
        REQUIRE(shared_bus.available(3));
        
        // Receive and verify the message
        I2CMessage lisp_msg;
        REQUIRE(shared_bus.receive(3, lisp_msg));
        
        REQUIRE(lisp_msg.src == 0);
        REQUIRE(lisp_msg.dst == 3);
        
        // Convert payload back to string and verify it contains the expression
        String payload_str(reinterpret_cast<const char*>(lisp_msg.payload.data()));
        REQUIRE(payload_str.indexOf(String("@\"(+ 1 2)\"")) != -1);
    }
}

TEST_CASE("I2C Bus - Error handling", "[i2c][error]") {
    MockI2CBus mock_bus;
    
    SECTION("Receive from empty address") {
        // Try to receive from empty address
        I2CMessage msg;
        REQUIRE_FALSE(mock_bus.receive(99, msg));
        
        // Available should return false for non-existent addresses
        REQUIRE_FALSE(mock_bus.available(99));
        
        // Pending messages should return 0 for non-existent addresses
        REQUIRE(mock_bus.pending_messages(99) == 0);
    }
}