#include "uSEQ.h"



void uSEQ::serial_write(int out, double val)
{
    DBG("uSEQ::serial_write");

    Serial.write(SerialMsg::message_begin_marker);
    Serial.write((u_int8_t)SerialMsg::serial_message_types::STREAM);
    Serial.write((u_int8_t)(out + 1));
    u_int8_t* byteArray = reinterpret_cast<u_int8_t*>(&val);
    for (size_t b = 0; b < 8; b++)
    {
        Serial.write(byteArray[b]);
    }
}

bool is_new_code_waiting() { return Serial.available(); }

String get_code_waiting() { return Serial.readStringUntil('\n'); }

void uSEQ::check_and_handle_user_input()
{
    DBG("uSEQ::check_and_handle_user_input");
    // m_repl.check_and_handle_input();

    if (is_new_code_waiting())
    {
        m_manual_evaluation = true;

        int first_byte = Serial.read();
        // SERIAL
        if (first_byte == SerialMsg::message_begin_marker /*31*/)
        {
            // incoming serial stream
            size_t channel = Serial.read();
            char buffer[8];
            Serial.readBytes(buffer, 8);
            if (channel > 0 && channel <= m_num_serial_ins)
            {
                double v = 0;
                memcpy(&v, buffer, 8);
                m_serial_input_streams[(channel - 1)] = v;
            }
        }
        else
        {
            // Read code
            m_last_received_code = get_code_waiting();

            if (m_last_received_code == exit_command)
            {
                m_should_quit = true;
            }
            // EXECUTE NOW
            if (first_byte == SerialMsg::execute_now_marker /*'@'*/)
            {
                // Clear error queue
                error_msg_q.clear();

                String result = eval(m_last_received_code);

                if (error_msg_q.size() > 0)
                {
                    println(error_msg_q[0]);
                }

                println(result);
            }
            // SCHEDULE FOR LATER
            else
            {
                m_last_received_code =
                    String((char)first_byte) + m_last_received_code;
                println(m_last_received_code);
                Value expr = parse(m_last_received_code);
                m_runQueue.push_back(expr);
            }
        }

        m_manual_evaluation = false;
        // flush_print_jobs();
    }
}