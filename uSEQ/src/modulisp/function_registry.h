#ifndef FUNCTION_REGISTRY_H_
#define FUNCTION_REGISTRY_H_

#include "../utils/string.h"
#include "function_module.h"
#include "lisp/value.h"
#include <map>
#include <memory>
#include <vector>

/**
 * Central registry for all LISP functions
 * Replaces the static Environment::builtindefs() with a more flexible system
 */
class FunctionRegistry
{
public:
    FunctionRegistry()  = default;
    ~FunctionRegistry() = default;

    // Singleton instance (to replace Environment::builtindefs())
    static FunctionRegistry& getInstance()
    {
        static FunctionRegistry instance;
        return instance;
    }

    // Function registration
    void registerFunction(const FunctionDescriptor& desc);
    void
    registerFunction(const String& name, const String& module,
                     std::function<Value(std::vector<Value>&, Environment&)> func,
                     bool is_special_form = false);

    // Direct registration methods that match existing Value constructors
    void registerBuiltinFunction(const String& name, const String& module,
                                 BuiltinFuncRawPtr func);
    void registerPluginFunction(const String& name, const String& module,
                                PluginBuiltinFunc func, void* ctx);
    void registerModuLispMethod(const String& name, const String& module,
                                PluginBuiltinFunc trampoline,
                                void* instance);

    // Module management
    void registerModule(std::unique_ptr<FunctionModule> module);
    void enableModule(const String& module_name);
    void disableModule(const String& module_name);
    bool isModuleEnabled(const String& module_name) const;
    std::vector<String> getModuleNames() const;

    // Function lookup
    Value* lookup(const String& name);
    const Value* lookup(const String& name) const;
    bool hasFunction(const String& name) const;

    // Get all registered functions (for compatibility with existing code)
    std::map<String, Value>& getAllFunctions() { return functions_; }
    const std::map<String, Value>& getAllFunctions() const { return functions_; }

    // Clear all registrations (useful for testing)
    void clear();

    // Statistics and debugging
    size_t getFunctionCount() const { return functions_.size(); }
    size_t getModuleCount() const { return modules_.size(); }
    std::vector<String> getFunctionNames() const;
    std::vector<String> getFunctionNamesByModule(const String& module) const;

private:
    // Storage for all registered functions
    std::map<String, Value> functions_;

    // Storage for function metadata (module association, etc.)
    std::map<String, FunctionDescriptor> descriptors_;

    // Registered modules
    std::map<String, std::unique_ptr<FunctionModule>> modules_;

    // Track which functions belong to which modules
    std::map<String, std::vector<String>> module_functions_;

    // Helper to create Value from FunctionDescriptor
    Value createValueFromDescriptor(const FunctionDescriptor& desc);
};

// Compatibility macro to ease transition from Environment::builtindefs()
#define BUILTIN_REGISTRY FunctionRegistry::getInstance().getAllFunctions()

#endif // FUNCTION_REGISTRY_H_