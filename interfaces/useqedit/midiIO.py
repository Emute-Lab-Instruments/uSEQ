import mido
from Console import Console

class midiIO:

    inports = []
    outports = []
    @classmethod
    def init(cls):
        try:
            None
            # cls.vmidi = mido.open_output("uSEQ", virtual=True)
            #open all the outports
            cls.outports = [mido.open_output(x) for x in mido.get_output_names()]
            cls.inports = [mido.open_input(x) for x in mido.get_input_names()]
        except:
            None

    @classmethod
    def listMIDIOutputs(cls):
        return mido.get_output_names()
    @classmethod
    def listMIDIInputs(cls):
        return mido.get_input_names()



    @classmethod
    def getOutputLike(cls, term):
        namelist = [x for x in mido.get_output_names() if x.find(term) >= 0]
        if len(namelist)>0:
            return mido.get_output_names().index(namelist[0])
        else:
            return False

    # @classmethod
    # def midiInputCallback(cls, msg):
    #     # if msg.dict()['type']=='note_on':
    #     Console.qpost(msg)




