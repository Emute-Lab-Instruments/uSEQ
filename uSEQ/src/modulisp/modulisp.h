#ifndef MODULISP_H_
#define MODULISP_H_

#include "modulisp_interpreter.h"
#include <string>
#include <memory>
#include <cctype>

// Simple facade class for easy LISP interaction
// This is the main public API for ModuLisp
class ModuLisp {
  public:
    // Response wrapper class for LISP evaluation results
    class Response {
      public:
        Response(const Value& val) : value(val) {}
        
        // Check if the evaluation succeeded (not error or nil)
        bool okay() const {
            auto type_enum = value.get_type_enum();
            // Based on enum in value.h: NIL=13, ERROR=15
            return type_enum != 15 && type_enum != 13;
        }
        
        // Get the type name (returns capitalized strings like "Integer", "Float", "Symbol")
        std::string type() const {
            String type_name = value.get_type_name();
            std::string result(type_name.c_str());
            
            // Convert to capitalized format for consistency with test expectations
            if (result == "int") return "Integer";
            if (result == "float") return "Float";
            if (result == "atom") return "Symbol";
            if (result == "string") return "String";
            if (result == "list") return "List";
            if (result == "vector") return "Vector";
            if (result == "function") return "Function";
            if (result == "nil") return "Nil";
            if (result == "error") return "Error";
            if (result == "unit") return "Unit";
            
            // Default: capitalize first letter
            if (!result.empty()) {
                result[0] = std::toupper(result[0]);
            }
            return result;
        }
        
        // Direct mappings to Value methods
        int as_int() const { return value.as_int(); }
        double as_float() const { return value.as_float(); }
        std::string as_string() const { 
            String str = value.as_string();
            return std::string(str.c_str()); 
        }
        std::string as_symbol() const { 
            String atom = value.as_atom();
            return std::string(atom.c_str()); 
        }
        std::vector<Value> as_list() const { return value.as_list(); }
        
        // Access underlying value if needed
        const Value& get_value() const { return value; }
        
      private:
        Value value;
    };
    
    // Constructor
    ModuLisp();
    ~ModuLisp();
    
    // Main API: send LISP code and get response
    Response send(const std::string& code);
    
    // Direct access to interpreter if needed (for advanced use)
    ModuLispInterpreter* get_interpreter() { return interpreter.get(); }
    
  private:
    std::unique_ptr<ModuLispInterpreter> interpreter;
};

#endif // MODULISP_H_