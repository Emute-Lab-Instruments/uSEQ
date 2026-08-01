#include "state_registry.h"
#include <cstring>

namespace sig {

uint16_t StateResourceRegistry::resolve(const StateResourceKey& key,
                                        double init_value,
                                        double* state_values,
                                        uint16_t& state_slot_count,
                                        uint16_t owner_context) {
    last_owner_conflict = false;
    // Search for existing entry
    for (uint16_t i = 0; i < entry_count; i++) {
        if (entries[i].key == key) {
            if (owner_context != ANON_STATE_CONTEXT_NONE &&
                entries[i].owner_context != ANON_STATE_CONTEXT_NONE &&
                entries[i].owner_context != owner_context) {
                last_owner_conflict = true;
                return NODE_NONE;
            }
            if (entries[i].owner_context == ANON_STATE_CONTEXT_NONE) {
                entries[i].owner_context = owner_context;
            }
            entries[i].active = true;
            return entries[i].slot_index;
        }
    }

    // New entry — prefer a slot retired by a previously published graph.
    if (entry_count >= MAX_STATE_SLOTS) return NODE_NONE;
    uint16_t slot = take_free_slot();
    if (slot == NODE_NONE) {
        if (state_slot_count >= MAX_STATE_SLOTS) return NODE_NONE;
        slot = state_slot_count++;
    }
    state_values[slot] = init_value;

    entries[entry_count].key = key;
    entries[entry_count].slot_index = slot;
    entries[entry_count].owner_context = owner_context;
    entries[entry_count].init_value = init_value;
    entries[entry_count].active = true;
    entry_count++;

    return slot;
}

void StateResourceRegistry::begin_context(uint16_t owner_context) {
    if (owner_context == ANON_STATE_CONTEXT_NONE) return;
    for (uint16_t i = 0; i < entry_count; i++) {
        if (entries[i].owner_context == owner_context) {
            entries[i].active = false;
        }
    }
}

void StateResourceRegistry::commit_context(
    uint16_t owner_context, uint16_t* state_update_roots,
    uint16_t* state_owner_contexts) {
    if (owner_context == ANON_STATE_CONTEXT_NONE) return;

    uint16_t write = 0;
    for (uint16_t read = 0; read < entry_count; read++) {
        StateResourceEntry& entry = entries[read];
        if (entry.owner_context == owner_context && !entry.active) {
            if (entry.slot_index < MAX_STATE_SLOTS) {
                state_update_roots[entry.slot_index] = NODE_NONE;
                state_owner_contexts[entry.slot_index] =
                    ANON_STATE_CONTEXT_NONE;
                // Each live registry entry owns a unique slot. Avoid adding a
                // duplicate defensively so a malformed legacy image cannot
                // make the same slot available twice.
                bool already_free = false;
                for (uint16_t f = 0; f < free_slot_count; f++) {
                    if (free_slots[f] == entry.slot_index) {
                        already_free = true;
                        break;
                    }
                }
                if (!already_free && free_slot_count < MAX_STATE_SLOTS) {
                    free_slots[free_slot_count++] = entry.slot_index;
                }
            }
            continue;
        }
        if (write != read) entries[write] = entries[read];
        write++;
    }
    entry_count = write;
}

uint16_t StateResourceRegistry::take_free_slot() {
    if (free_slot_count == 0) return NODE_NONE;
    return free_slots[--free_slot_count];
}

void StateResourceRegistry::mark_all_inactive() {
    for (uint16_t i = 0; i < entry_count; i++) {
        entries[i].active = false;
    }
}

void StateResourceRegistry::clear() {
    for (uint16_t i = 0; i < MAX_STATE_SLOTS; i++) {
        entries[i] = StateResourceEntry{};
        free_slots[i] = NODE_NONE;
    }
    entry_count = 0;
    free_slot_count = 0;
    last_owner_conflict = false;
}

} // namespace sig
