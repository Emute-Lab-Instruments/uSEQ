#ifndef FUNCTION_MODULE_H_
#define FUNCTION_MODULE_H_

#include "lisp/value.h"
#include "../utils/string.h"
#include <vector>
#include <functional>

// Forward declaration
class FunctionRegistry;
class Environment;

/**
 * Descriptor for a single LISP function
 * Contains all metadata needed to register and invoke a function
 */
struct FunctionDescriptor {
    String name;                    // Function name in LISP (e.g., "a1", "set-bpm")
    String module;                  // Module name for grouping (e.g., "output", "timing")
    bool is_special_form = false;  // Whether args should be evaluated before calling
    
    // Function pointer types for different calling conventions
    using BuiltinFunc = std::function<Value(std::vector<Value>&, Environment&)>;
    using MethodFunc = Value(void*, std::vector<Value>&, Environment&);
    
    BuiltinFunc builtin_func;      // For standalone functions
    MethodFunc* method_func = nullptr;  // For class methods
    void* instance = nullptr;       // Instance pointer for methods
    
    // Constructor for standalone functions
    FunctionDescriptor(const String& n, const String& m, BuiltinFunc f, bool special = false)
        : name(n), module(m), is_special_form(special), builtin_func(f) {}
    
    // Constructor for class methods
    FunctionDescriptor(const String& n, const String& m, MethodFunc* f, void* inst, bool special = false)
        : name(n), module(m), is_special_form(special), method_func(f), instance(inst) {}
};

/**
 * Base class for modules that provide LISP functions
 * Allows grouping related functions and conditional compilation/enabling
 */
class FunctionModule {
public:
    explicit FunctionModule(const String& name) : module_name_(name), enabled_(true) {}
    virtual ~FunctionModule() = default;
    
    // Called to register all functions this module provides
    virtual void registerFunctions(FunctionRegistry& registry) = 0;
    
    // Module management
    virtual bool isEnabled() const { return enabled_; }
    virtual void setEnabled(bool enabled) { enabled_ = enabled; }
    virtual const String& getName() const { return module_name_; }
    
    // Optional: Called when module is enabled/disabled
    virtual void onEnable() {}
    virtual void onDisable() {}
    
protected:
    String module_name_;
    bool enabled_;
};

#endif // FUNCTION_MODULE_H_