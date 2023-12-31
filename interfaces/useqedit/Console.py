import curses
from MessageLog import MessageLog

class Console:
    console=None
    redraw=False
    @classmethod
    def init(cls, consoleWidth):
        cls.console = curses.newwin(curses.LINES - 1, consoleWidth - 1, 0, curses.COLS - consoleWidth)
        cls.msglog = MessageLog(curses.LINES - 3, consoleWidth - 4)

    @classmethod
    def post(cls,msg=None):
        if (msg):
            cls.msglog.addMessage(msg)
        cls.console.erase()
        cls.console.attroff(curses.color_pair(1))
        cls.console.border()
        cls.console.attron(curses.color_pair(1))
        for i, msg in enumerate(cls.msglog):
            cls.console.addstr(i+1, 2, msg)
        cls.console.refresh()
        redraw = True

    @classmethod
    def isRedraw(cls):
        return cls.redraw

    @classmethod
    def resetRedraw(cls):
        cls.redraw=False
