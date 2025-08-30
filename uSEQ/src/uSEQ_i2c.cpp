#include "uSEQ.h"
#ifndef ARDUINO
#include "hardware_includes.h"
#endif
#include "utils.h"

Value uSEQ::useq_i2c_send_to(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "send-to";

    if (!(args.size() == 2))
    {
        report_error_wrong_num_args(user_facing_name, static_cast<int>(args.size()),
                                    NumArgsComparison::EqualTo, 2, -1);
        return Value::error();
    }

    // if (!(args[0].is_string() || args[0].is_number()))
    // {
    //     report_error_wrong_specific_pred(
    //         user_facing_name, 0, "a string (module name) or a number (I2C index)",
    //         args[0].display());
    //     return Value::error();
    // }

    if (!(args[0].is_number()))
    {
        report_error_wrong_specific_pred(user_facing_name, 0, "a number",
                                         args[0].display());
        return Value::error();
    }

    // BODY
    Value result = Value::nil();

    int i2c_idx = -1;

    // if (args[0].is_string())
    // {
    //     i2c_idx = i2c_idx_map[args[0].as_string()];
    // }
    // else
    // {
    i2c_idx = args[0].as_int();
    // }

    // the body is everything after the first arg
    // Value body = Value::vector({ args.data() + 1, args.size() - 1 });
    // NOTE: for now, the body is expected to be just one expr
    // if multiple expressions needed, use a do block
    Value body = args[1];

    String body_str = "@" + body.to_lisp_src();

    // Use injected I2C port if available
    if (i2c != nullptr) {
        I2CMessage msg;
        msg.src = 0; // Our own address (could be configurable)
        msg.dst = static_cast<uint8_t>(i2c_idx);
        
        // Convert string to byte payload
        const char* str_data = body_str.c_str();
        msg.payload.assign(str_data, str_data + strlen(str_data) + 1); // Include null terminator
        
        i2c->send(msg);
        println("String sent via injected I2C port: ");
        println(body_str);
    } else {
#ifdef ARDUINO
        i2cWriteString(i2c_idx, body_str);
        println("String being sent to i2c: ");
        println(body_str);
#endif
    }

    return result;
}

Value uSEQ::useq_i2c_host_start(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "i2c-host-start";

    if (!(args.size() == 0))
    {
        report_error_wrong_num_args(user_facing_name, static_cast<int>(args.size()),
                                    NumArgsComparison::EqualTo, 0, -1);
        return Value::error();
    }

    // BODY
    Value result = Value::nil();

    setup_i2cHOST();

    return result;
}