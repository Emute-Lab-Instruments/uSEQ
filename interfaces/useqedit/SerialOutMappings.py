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

    channels = [0]

    @classmethod
    def process(cls):
        #flip back to zero after triggering
        if (cls.channels[0] == 1.0):
            cls.channels[0] = 0.0
            cls.sendSerialValue(1, 0.0)

        for port in midiIO.inports:
            for msg in port.iter_pending():
                # Console.post(msg.dict())
                if msg.dict()['type'] == 'note_on':
                    if msg.dict()['note'] == 36:
                        cls.channels[0] = 1.0
                        cls.sendSerialValue(1, cls.channels[0])

                # v = 1
                # channel = 1
                # cls.sendSerialValue(channel, v)

    @classmethod
    def sendSerialValue(cls, channel, v):
        msgBegin = [31, channel ]
        SerialIO.sendBytes(bytearray(msgBegin))
        valueBytes = struct.pack('d', v)
        SerialIO.sendBytes(bytearray(valueBytes))
