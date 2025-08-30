#ifndef MOCKI2CBUS_H_
#define MOCKI2CBUS_H_

#include "../II2CBus.h"
#include <map>
#include <queue>

class MockI2CBus : public II2CBus {
public:
    MockI2CBus() = default;
    ~MockI2CBus() override = default;

    bool send(const I2CMessage& msg) override {
        // Enqueue message to destination address
        message_queues_[msg.dst].push(msg);
        return true;
    }

    bool available(uint8_t addr) override {
        auto it = message_queues_.find(addr);
        if (it == message_queues_.end()) {
            return false;
        }
        return !it->second.empty();
    }

    bool receive(uint8_t addr, I2CMessage& out) override {
        auto it = message_queues_.find(addr);
        if (it == message_queues_.end() || it->second.empty()) {
            return false;
        }
        
        out = it->second.front();
        it->second.pop();
        return true;
    }

    // Test helpers
    size_t pending_messages(uint8_t addr) const {
        auto it = message_queues_.find(addr);
        if (it == message_queues_.end()) {
            return 0;
        }
        return it->second.size();
    }

    void clear_all() {
        message_queues_.clear();
    }

private:
    std::map<uint8_t, std::queue<I2CMessage>> message_queues_;
};

#endif // MOCKI2CBUS_H_