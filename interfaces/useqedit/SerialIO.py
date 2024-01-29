import serial
import glob
from SerialStreamMap import SerialStreamMap
from Console import Console
import struct


class SerialIO:
    serialIOMessage = []
    serialIOMessageCounter = -1
    incoming = ''
    cx = None
    serialPortName=''

    @classmethod
    def openSerialCx(cls, serialPortName):
        cls.serialPortName = serialPortName
        if cls.serialPortName == "":
            cls.serialPortName = SerialIO.autoDetectSerialPort()

        connected = SerialIO.trySerialConnection(cls.serialPortName)
        if not connected:
            Console.post(f"Error connecting to uSEQ")

    @classmethod
    def autoDetectSerialPort(cls):
        serialPortName=''
        devlist = sorted(glob.glob("/dev/ttyACM*"))  #linux // what happens on windows?
        if len(devlist) > 0:
            serialPortName = devlist[0]
        else:
            devlist = sorted(glob.glob("/dev/tty.usbmodem*"))  # connect on OSX
            if len(devlist) > 0:
                serialPortName = devlist[0]
            else:
                devlist = sorted(glob.glob("/tmp/ttyUSEQVirtual"))  # connect to virtual uSEQ
                if len(devlist) > 0:
                    serialPortName = devlist[0]
        return serialPortName

    @classmethod
    def trySerialConnection(cls,port):
        ok=False
        try:
            cls.cx = serial.Serial(port, baudrate=115200)
            Console.post(f"Connected to uSEQ on {port}")
            ok=True
        except serial.SerialException:
            cls.cx = None
        return ok

    @classmethod
    def sendTouSEQ(cls,statement):
        # send to terminal
        ok=False
        if cls.cx:
            asciiCode = (statement + '\n').encode('ascii')
            cls.cx.write(asciiCode)
            ok=True
        return ok

    @classmethod
    def sendBytes(cls, data):
        if cls.cx:
            cls.cx.write(data)


    @classmethod
    def close(cls):
        if cls.cx:
            cls.cx.close()


    @classmethod
    def readSerial(cls):
        ##read serial if available
        if cls.cx:
            try:
                if (cls.cx.in_waiting > 0):
                    byteCount = cls.cx.in_waiting
                    # actionReceived = True
                    # updateConsole(f"reading serial {cx.in_waiting}")
                    for i in range(byteCount):
                        inchar = cls.cx.read()

                        if (inchar == b'\x1f'):
                            cls.serialIOMessageCounter = 9
                            cls.serialIOMessage = []
                            # updateConsole(f"SIO31  {serialIOMessageCounter}")
                        else:
                            if cls.serialIOMessageCounter > 0:
                                cls.serialIOMessage.append(inchar)
                                cls.serialIOMessageCounter = cls.serialIOMessageCounter - 1
                                if cls.serialIOMessageCounter == 0:
                                    try:
                                        # updateConsole(serialIOMessage)
                                        # convert bytes to double
                                        dblbytes = b''.join(cls.serialIOMessage[1:])
                                        val = struct.unpack('d', dblbytes)[0]
                                        ch = cls.serialIOMessage[0][0] - 1
                                        SerialStreamMap.mapSerial(ch, val)
                                        # updateConsole(str(val))
                                    except Exception as e:
                                        Console.post(e)
                            else:
                                if (inchar != b'\n' and inchar != b'\r'):
                                    cls.incoming = cls.incoming + str(chr(inchar[0]))
                                if (inchar == b'\n' or inchar == b'\r'):
                                    if (cls.incoming != ''):
                                        Console.post(cls.incoming)
                                    cls.incoming = ''
            except Exception as e:
                cx = None
                Console.post(e)
                Console.post("uSEQ disconnected")
        else:
            SerialIO.trySerialConnection(cls.serialPortName)

