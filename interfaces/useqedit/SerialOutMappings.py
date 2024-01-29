from Console import Console
from midiIO import midiIO
from SerialIO import SerialIO
import struct

# double
# val = result.as_float();
# Serial.write((u_int8_t)
# 31);
# Serial.write((u_int8_t)(i + 1));
# char * byteArray = reinterpret_cast < char * > (& val);
# for (size_t b = 0; b < 8; b++) {
#     Serial.write(byteArray[b]);
# }


class SerialOutMappings:
    @classmethod
    def process(cls):
        for port in midiIO.inports:
            for msg in port.iter_pending():
                Console.post(msg)
                msgBegin = [31, 1,]
                SerialIO.sendBytes(bytearray(msgBegin))
                v = 1.238421
                valueBytes = struct.pack('d',v)
                SerialIO.sendBytes(bytearray(valueBytes))
