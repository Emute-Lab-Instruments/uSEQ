import mido
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
        except:
            None

    @classmethod
    def listMIDIOutputs(cls):
        return mido.get_output_names()
    @classmethod
    def listMIDIInputs(cls):
        return mido.get_input_names()


