#include "state_registry.h"
#include <cstring>

namespace sig {

uint16_t StateResourceRegistry::resolve(const StateResourceKey& key,
                                        double init_value,
                                        double* state_values,
                                        uint16_t& state_slot_count) {
    // Search for existing entry
    for (uint16_t i = 0; i < entry_count; i++) {
        if (entries[i].key == key) {
            entries[i].active = true;
            return entries[i].slot_index;
        }
    }

    // New entry — allocate a dense slot
    if (state_slot_count >= MAX_STATE_SLOTS) return NODE_NONE;
    if (entry_count >= MAX_STATE_SLOTS) return NODE_NONE;

    uint16_t slot = state_slot_count++;
    state_values[slot] = init_value;

    entries[entry_count].key = key;
    entries[entry_count].slot_index = slot;
    entries[entry_count].init_value = init_value;
    entries[entry_count].active = true;
    entry_count++;

    return slot;
}

void StateResourceRegistry::mark_all_inactive() {
    for (uint16_t i = 0; i < entry_count; i++) {
        entries[i].active = false;
    }
}

void StateResourceRegistry::clear() {
    entry_count = 0;
}

} // namespace sig
