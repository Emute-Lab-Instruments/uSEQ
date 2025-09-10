// Precompiled header for uSEQ project
// Contains stable system includes to speed up compilation

#ifndef USEQ_PCH_H
#define USEQ_PCH_H

// Standard Library - Containers
#include <array>
#include <deque>
#include <map>
#include <queue>
#include <set>
#include <stack>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Standard Library - Memory Management
#include <memory>

// Standard Library - Algorithms & Utilities
#include <algorithm>
#include <functional>
#include <optional>
#include <tuple>
#include <utility>
#include <variant>

// Standard Library - Numeric
#include <cmath>
#include <cstdint>
#include <limits>
#include <numeric>

// Standard Library - I/O
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

// Standard Library - Time & Chrono
#include <chrono>
#include <ctime>

// Standard Library - Other
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <stdexcept>
#include <type_traits>
#include <typeinfo>

// Conditional Arduino headers (if building for Arduino)
#ifdef ARDUINO
#include <Arduino.h>
#endif

#endif // USEQ_PCH_H