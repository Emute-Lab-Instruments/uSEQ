// Minimal devtools header stub
#pragma once
#include <cstddef>

namespace dt {
inline void init(void*) {}
inline void tick_begin() {}
inline void tick_end() {}
inline void mark(const char*) {}
template<typename F>
inline void emit_streaming(F, size_t) {}
}
