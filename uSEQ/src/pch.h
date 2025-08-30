// Precompiled header for uSEQ project
// Contains stable system includes to speed up compilation

#ifndef USEQ_PCH_H
#define USEQ_PCH_H

// Standard Library - Containers
#include <vector>
#include <array>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <deque>
#include <queue>
#include <stack>

// Standard Library - Memory Management
#include <memory>

// Standard Library - Algorithms & Utilities
#include <algorithm>
#include <functional>
#include <utility>
#include <optional>
#include <variant>
#include <tuple>

// Standard Library - Numeric
#include <cmath>
#include <numeric>
#include <limits>
#include <cstdint>

// Standard Library - I/O
#include <iostream>
#include <sstream>
#include <fstream>
#include <iomanip>

// Standard Library - Time & Chrono
#include <chrono>
#include <ctime>

// Standard Library - Other
#include <exception>
#include <stdexcept>
#include <typeinfo>
#include <type_traits>
#include <cassert>
#include <cstring>
#include <cstdlib>
#include <cstdio>

// Conditional Arduino headers (if building for Arduino)
#ifdef ARDUINO
#include <Arduino.h>
#endif

#endif // USEQ_PCH_H