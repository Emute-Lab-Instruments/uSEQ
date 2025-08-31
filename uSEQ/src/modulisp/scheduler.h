#ifndef SCHEDULER_H_
#define SCHEDULER_H_

#include <vector>
#include "lisp/value.h"
#include "../utils/string.h"

// Represents a scheduled item that runs periodically
struct ScheduledItem {
    Value ast;           // AST to evaluate
    size_t period;       // Period in microseconds
    size_t last_run;     // Last time it ran
    String id;           // Unique identifier
};

// Manages scheduled items and run queue
class Scheduler {
public:
    // Add an item to the schedule
    void schedule(const String& id, const Value& ast, size_t period);
    
    // Remove an item from the schedule
    bool unschedule(const String& id);
    
    // Clear all scheduled items
    void clear_schedule() { m_scheduled_items.clear(); }
    
    // Get items that need to run at current time
    std::vector<ScheduledItem*> get_items_to_run(size_t current_time);
    
    // Run queue management
    void add_to_run_queue(const Value& item) { m_run_queue.push_back(item); }
    void clear_run_queue() { m_run_queue.clear(); }
    const std::vector<Value>& get_run_queue() const { return m_run_queue; }
    std::vector<Value>& get_run_queue() { return m_run_queue; }
    
    // Code quantization phasor management
    void set_cqp_ast(const Value& ast) { m_cqp_ast = ast; }
    const Value& get_cqp_ast() const { return m_cqp_ast; }
    
    void set_last_cqp(double value) { m_last_cqp = value; }
    double get_last_cqp() const { return m_last_cqp; }
    
    // Q0 AST management
    void set_q0_ast(const Value& ast) { m_q0_ast = ast; }
    const Value& get_q0_ast() const { return m_q0_ast; }
    
    // Get scheduled items (for inspection/debugging)
    const std::vector<ScheduledItem>& get_scheduled_items() const { return m_scheduled_items; }
    
private:
    std::vector<ScheduledItem> m_scheduled_items;
    std::vector<Value> m_run_queue;
    Value m_cqp_ast;  // Default to bar quantization - initialized in constructor
    Value m_q0_ast;
    double m_last_cqp = 0.0;
};

#endif // SCHEDULER_H_