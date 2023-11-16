from  midiIO import midiIO
import mido


MIDICONTINUOUS = 0


class SerialStreamMap:
    map = []
    MIDICONTINUOUS=0
    MIDITRIG=1
    OSC=2
    MIDINOTE=3
    lastvals = []

    @classmethod
    def init(cls):
        cls.map = [None for x in range(8)]
        cls.lastvals = [0 for x in cls.map]

    @classmethod
    def makeMIDIContinuousMap(cls, port, channel, controller):
        return [cls.MIDICONTINUOUS, port, channel, controller]

    @classmethod
    def makeMIDITrigMap(cls, port, channel, note):
        return [cls.MIDITRIG, port, channel, note]

    @classmethod
    def makeMIDINoteMap(cls, port, channel):
        return [cls.MIDINOTE, port, channel]

    @classmethod
    def makeOSCMap(cls, dest, addr):
        return [cls.OSC, dest, addr]

    @classmethod
    def set(cls, index, mapping):
        if index < len(cls.map):
            cls.map[index] = mapping

    @classmethod
    def mapSerial(cls, ch, val, log):
        if ch>=0 and ch <8 and cls.map[ch] != None:
            mapping = cls.map[ch]
            # log(f"{ch} {mapping}")
            if mapping[0]==cls.MIDICONTINUOUS:
                msg = mido.Message('control_change', channel=mapping[2], control=mapping[3],value=max(0,int(val * 127)))
                midiIO.outports[mapping[1]].send(msg)
            elif mapping[0] == cls.MIDITRIG:
                if cls.lastvals[ch] < 1 / 127.0 and val > 1 / 127.0:
                    msg = mido.Message('note_on', channel=mapping[2], note=mapping[3], velocity=int(val * 127))
                    midiIO.outports[mapping[1]].send(msg)
                elif cls.lastvals[ch] > 1 / 127.0 and val < 1 / 127.0:
                    msg = mido.Message('note_off', channel=mapping[2], note=mapping[3])
                    midiIO.outports[mapping[1]].send(msg)
            elif mapping[0]==cls.MIDINOTE:
                if cls.lastvals[ch] < 1/127.0 and val > 1/127.0:
                    msg = mido.Message('note_on', channel=mapping[2], note=int(val * 127), velocity=127)
                    midiIO.outports[mapping[1]].send(msg)
                elif cls.lastvals[ch] > 1/127.0 and val < 1/127.0:
                    msg = mido.Message('note_off', channel=mapping[2], note=int(cls.lastvals[ch] * 127))
                    midiIO.outports[mapping[1]].send(msg)

            cls.lastvals[ch] = val

    @classmethod
    def loadConfig(cls, logFunc):
        pass

    @classmethod
    def loadJSON(cls, data, log):
        error = False
        for d in data:
            if "type" in d and 'serial' in d:
                if d['type'] == "MIDITRIG":
                    if 'port' in d and 'channel' in d and 'note' in d:
                        cls.set(d['serial']-1, SerialStreamMap.makeMIDITrigMap(d['port'], d['channel'], d['note']))
                    else:
                        log("Error, key missing in: ")
                        log(d)
                        error=True
                elif d['type'] == "MIDINOTE":
                    if 'port' in d and 'channel' in d and 'note' in d:
                        cls.set(d['serial'] - 1,
                                SerialStreamMap.makeMIDINoteMap(d['port'], d['channel']))
                    else:
                        log("Error, key missing in: ")
                        log(d)
                        error = True
                elif d['type'] == "MIDICTL":
                    if 'port' in d and 'channel' in d and 'ctl' in d:
                        cls.set(d['serial']-1, SerialStreamMap.makeMIDIContinuousMap(d['port'], d['channel'], d['ctl']))
                    else:
                        log("Error, key missing in: ")
                        log(d)
                        error = True
                else:
                    log(f"Error: unrecognised configuration type: {d['type']}")
            else:
                log("Error, 'type' missing in: ")
                log(d)
                error=True
        return error



    

