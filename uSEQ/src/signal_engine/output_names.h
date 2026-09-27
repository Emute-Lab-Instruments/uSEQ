#ifndef SIGNAL_ENGINE_OUTPUT_NAMES_H
#define SIGNAL_ENGINE_OUTPUT_NAMES_H

#include "types.h"

namespace sig {

// Map the canonical a1-a8, d1-d8, and s1-s8 output names to their shared
// slot numbering. Return NODE_NONE for malformed or unknown names.
inline uint16_t output_name_to_index(const char* name) {
    if (!name || name[0] == '\0' || name[1] < '1' || name[1] > '8' ||
        name[2] != '\0') {
        return NODE_NONE;
    }
    const uint16_t number = static_cast<uint16_t>(name[1] - '1');
    switch (name[0]) {
        case 'a': return number;
        case 'd': return 8 + number;
        case 's': return 16 + number;
        default: return NODE_NONE;
    }
}

} // namespace sig

#endif // SIGNAL_ENGINE_OUTPUT_NAMES_H
