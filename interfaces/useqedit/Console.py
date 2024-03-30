import curses
from MessageLog import MessageLog

import logging
logger = logging.getLogger(__name__)


class Console:
    console=None
    redraw=False
    postQueue = []

    @classmethod
    def init(cls, consoleWidth):
        cls.console = curses.newwin(curses.LINES - 1, consoleWidth - 1, 0, curses.COLS - consoleWidth)
        cls.msglog = MessageLog(curses.LINES - 3, consoleWidth - 4)

    @classmethod
    def post(cls,msg=None):
        if (msg):
            cls.msglog.addMessage(msg)
        logger.info(msg)
        cls.console.erase()
        cls.console.attroff(curses.color_pair(1))
        cls.console.border()
        cls.console.attron(curses.color_pair(1))
        for i, msg in enumerate(cls.msglog):
            cls.console.addstr(i+1, 2, msg)
        cls.console.refresh()
        redraw = True

    #asynchronous post
    @classmethod
    def qpost(cls, msg=None):
        cls.postQueue.append(msg)

    @classmethod
    def postQueuedMessages(cls):
        if len(cls.postQueue) > 0:
            for msg in cls.postQueue:
                cls.post(msg)
        cls.postQueue = []

    @classmethod
    def isRedraw(cls):
        return cls.redraw

    @classmethod
    def resetRedraw(cls):
        cls.redraw=False
