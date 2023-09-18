import pyperclip
class MultiClip:
    @staticmethod
    def testSystem():
        try:
            tmp = pyperclip.paste()
        except:
            MultiClip.useXClip = False
        MultiClip.buffer=""


    @staticmethod
    def copy(buffer):
        if MultiClip.useXClip:
            pyperclip.copy(buffer)
        else:
            MultiClip.buffer = buffer

    @staticmethod
    def paste():
        if (MultiClip.useXClip):
            return pyperclip.paste()
        else:
            return MultiClip.buffer

MultiClip.useXClip = True
MultiClip.buffer = ""
MultiClip.testSystem()