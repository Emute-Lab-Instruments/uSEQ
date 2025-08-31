#include "function_registry.h"
#include "../utils/log.h"

// Define DBG macro if not already defined
#ifndef DBG
#ifdef DEBUG_BUILD
#define DBG(x) ::println(x)
#else
#define DBG(x)
#endif
#endif

void FunctionRegistry::registerFunction(const FunctionDescriptor& desc) {
    // Store the descriptor
    descriptors_[desc.name] = desc;
    
    // Create and store the Value
    Value func_value = createValueFromDescriptor(desc);
    functions_[desc.name] = func_value;
    
    // Track module association
    module_functions_[desc.module].push_back(desc.name);
    
    DBG("Registered function: " + desc.name + " in module: " + desc.module);
}

void FunctionRegistry::registerFunction(const String& name, const String& module,
                                       std::function<Value(std::vector<Value>&, Environment&)> func,
                                       bool is_special_form) {
    FunctionDescriptor desc(name, module, func, is_special_form);
    registerFunction(desc);
}

void FunctionRegistry::registerModule(std::unique_ptr<FunctionModule> module) {
    if (!module) return;
    
    String module_name = module->getName();
    
    // Store the module
    modules_[module_name] = std::move(module);
    
    // Register all functions from this module
    modules_[module_name]->registerFunctions(*this);
    
    DBG("Registered module: " + module_name);
}

void FunctionRegistry::enableModule(const String& module_name) {
    auto it = modules_.find(module_name);
    if (it != modules_.end()) {
        it->second->setEnabled(true);
        it->second->onEnable();
        
        // Re-enable all functions from this module
        auto func_it = module_functions_.find(module_name);
        if (func_it != module_functions_.end()) {
            for (const auto& func_name : func_it->second) {
                // Functions are enabled by being present in the registry
                // We might want to add an "enabled" flag to descriptors later
            }
        }
        
        DBG("Enabled module: " + module_name);
    }
}

void FunctionRegistry::disableModule(const String& module_name) {
    auto it = modules_.find(module_name);
    if (it != modules_.end()) {
        it->second->setEnabled(false);
        it->second->onDisable();
        
        // Note: We don't remove functions from the registry when disabling
        // This allows re-enabling without re-registration
        // We could add an "enabled" flag to descriptors if needed
        
        DBG("Disabled module: " + module_name);
    }
}

bool FunctionRegistry::isModuleEnabled(const String& module_name) const {
    auto it = modules_.find(module_name);
    if (it != modules_.end()) {
        return it->second->isEnabled();
    }
    return false;
}

std::vector<String> FunctionRegistry::getModuleNames() const {
    std::vector<String> names;
    for (const auto& pair : modules_) {
        names.push_back(pair.first);
    }
    return names;
}

Value* FunctionRegistry::lookup(const String& name) {
    auto it = functions_.find(name);
    if (it != functions_.end()) {
        // Check if the module is enabled
        auto desc_it = descriptors_.find(name);
        if (desc_it != descriptors_.end()) {
            if (isModuleEnabled(desc_it->second.module) || 
                modules_.find(desc_it->second.module) == modules_.end()) {
                // Return if module is enabled or if function has no module (core functions)
                return &it->second;
            }
        } else {
            // No descriptor means it's a legacy registration, always return it
            return &it->second;
        }
    }
    return nullptr;
}

const Value* FunctionRegistry::lookup(const String& name) const {
    auto it = functions_.find(name);
    if (it != functions_.end()) {
        // Check if the module is enabled
        auto desc_it = descriptors_.find(name);
        if (desc_it != descriptors_.end()) {
            if (isModuleEnabled(desc_it->second.module) || 
                modules_.find(desc_it->second.module) == modules_.end()) {
                return &it->second;
            }
        } else {
            return &it->second;
        }
    }
    return nullptr;
}

bool FunctionRegistry::hasFunction(const String& name) const {
    return lookup(name) != nullptr;
}

void FunctionRegistry::clear() {
    functions_.clear();
    descriptors_.clear();
    modules_.clear();
    module_functions_.clear();
}

std::vector<String> FunctionRegistry::getFunctionNames() const {
    std::vector<String> names;
    for (const auto& pair : functions_) {
        if (lookup(pair.first)) {  // Only include enabled functions
            names.push_back(pair.first);
        }
    }
    return names;
}

std::vector<String> FunctionRegistry::getFunctionNamesByModule(const String& module) const {
    auto it = module_functions_.find(module);
    if (it != module_functions_.end()) {
        return it->second;
    }
    return std::vector<String>();
}

Value FunctionRegistry::createValueFromDescriptor(const FunctionDescriptor& desc) {
    // Since we can't store lambdas in Value directly, we need to handle this differently
    // For now, we'll store the function pointer directly in the descriptor
    // and create a simple wrapper function
    
    // Note: This is a simplified implementation. In a real system, we'd need
    // to properly handle the function storage and dispatch mechanism.
    // For now, we'll just return nil and handle the actual function storage
    // in a different way through the existing Environment::builtindefs() mechanism
    
    return Value::nil();
}