import argparse
import curses
import sys

import serial

from Buffer import Buffer
from Cursor import Cursor
from MessageLog import MessageLog


def clamp(x, lower, upper):
    if x < lower:
        return lower
    if x > upper:
        return upper
    return x


class Window:
    def __init__(self, n_rows, n_cols, row=0, col=0):
        self.n_rows = n_rows
        self.n_cols = n_cols
        self.row = row
        self.col = col

    @property
    def bottom(self):
        return self.row + self.n_rows - 1

    def up(self, cursor):
        if cursor.row == self.row - 1 and self.row > 0:
            self.row -= 1

    def down(self, buffer, cursor):
        if cursor.row == self.bottom + 1 and self.bottom < len(buffer) - 1:
            self.row += 1

    def horizontal_scroll(self, cursor, left_margin=5, right_margin=2):
        n_pages = cursor.col // (self.n_cols - right_margin)
        self.col = max(n_pages * self.n_cols - right_margin - left_margin, 0)

    def translate(self, cursor):
        return cursor.row - self.row, cursor.col - self.col


def left(window, buffer, cursor):
    cursor.left(buffer)
    window.up(cursor)
    window.horizontal_scroll(cursor)


def right(window, buffer, cursor):
    cursor.right(buffer)
    window.down(buffer, cursor)
    window.horizontal_scroll(cursor)



def main():

    parser = argparse.ArgumentParser()
    parser.add_argument("filename", help="A file to edit")
    parser.add_argument("-cw", "--conswidth", help="console width", default=40, type=int)
    args = parser.parse_args()

    pasteBuffer=""

    stdscr = curses.initscr()
    curses.start_color()
    curses.mousemask(curses.ALL_MOUSE_EVENTS | curses.REPORT_MOUSE_POSITION)
    curses.mouseinterval(20)
    curses.init_pair(1, curses.COLOR_RED, curses.COLOR_WHITE)
    curses.init_pair(2, curses.COLOR_MAGENTA, curses.COLOR_BLACK)
    curses.raw()
    consoleWidth = args.conswidth
    window = Window(curses.LINES - 1, curses.COLS - 1 - consoleWidth)
    editor = curses.newwin(curses.LINES - 1, curses.COLS - 1 - consoleWidth)
    console = curses.newwin(curses.LINES-1, consoleWidth-1, 0, curses.COLS-consoleWidth)

    cursor = Cursor()
    editor.keypad(True)

    msglog = MessageLog(curses.LINES - 3, consoleWidth - 4)

    def updateConsole(msg=None):
        if (msg):
            msglog.addMessage(msg)
        console.erase()
        console.border()
        for i, msg in enumerate(msglog):
            for j, ch in enumerate(msg):
                console.addch(i + 1, 2+j, ch, curses.color_pair(1))
        console.refresh()

    def findMatchingLeftParenthesis(buffer, cursor):
        searchCursor = Cursor.createFromCursor(cursor)
        stack = 0
        found = False
        while searchCursor.left(buffer):
            searchChar = buffer.getch(searchCursor)
            if (searchChar == ')'):
                stack = stack + 1
            elif (searchChar == '('):
                if stack == 0:
                    found = True
                    break
                else:
                    stack = stack - 1
        return (searchCursor if found else None)

    def findMatchingRightParenthesis(buffer, cursor, stack=0):
        searchCursor = Cursor.createFromCursor(cursor)
        found = False
        while searchCursor.right(buffer):
            searchChar = buffer.getch(searchCursor)
            if (searchChar == '('):
                stack = stack + 1
            elif (searchChar == ')'):
                stack = stack - 1
                if stack == 0:
                    found = True
                    break
        return (searchCursor if found else None)

    #serial setup
    incoming = ''
    port = '/dev/ttyACM0'
    cx = trySerialConnection(port, updateConsole)
    if not cx:
        updateConsole("Error connecting to uSEQ")

    with open(args.filename) as f:
        buffer = Buffer(f.read().splitlines())

    editor.nodelay(True) #nonblocking getch

    codeQueue = []
    undoList = []

    while True:
        stdscr.erase()
        # console.erase()
        editor.erase();

        # console.border(1)
        updateConsole()


        for row, line in enumerate(buffer[window.row:window.row + window.n_rows-1]):
            if row == cursor.row - window.row and window.col > 0:
                line = "«" + line[window.col + 1:]
            if len(line) > window.n_cols:
                line = line[:window.n_cols - 1] + "»"
            editor.addstr(row, 0, line)
        editor.move(*window.translate(cursor))

        outerBrackets = None
        #do highlighting
        leftParenCursor = cursor if buffer.getch(cursor) == '(' else None
        #     editor.chgat(*window.translate(cursor),1,curses.A_BOLD | curses.color_pair(1))
        #find the matching bracket
        if (leftParenCursor == None):
            leftParenCursor = findMatchingLeftParenthesis(buffer, cursor)
        if leftParenCursor:
            editor.chgat(*window.translate(leftParenCursor), 1, curses.A_BOLD | curses.color_pair(1))
            # leftParenCursor.right(buffer)
            rightParen = findMatchingRightParenthesis(buffer, leftParenCursor, 1)
            if rightParen:
                editor.chgat(*window.translate(rightParen), 1, curses.A_BOLD | curses.color_pair(1))
                #find outer statement
                highParen = findMatchingRightParenthesis(buffer, cursor, 1)
                while highParen != None:
                    # updateConsole(f"hp {highParen.row} {highParen.col}")
                    nextHighParen = findMatchingRightParenthesis(buffer, highParen, 1)
                    if not nextHighParen:
                        editor.chgat(*window.translate(highParen), 1, curses.A_BOLD | curses.color_pair(2))
                        leftParenCursor = findMatchingLeftParenthesis(buffer, highParen)
                        if leftParenCursor:
                            editor.chgat(*window.translate(leftParenCursor), 1, curses.A_BOLD | curses.color_pair(2))
                            outerBrackets = (leftParenCursor, highParen)
                    highParen = nextHighParen

        editor.move(*window.translate(cursor))
        def sendTouSEQ(statement):
            # send to terminal
            if cx:
                cx.write(statement.encode('ascii'))
                updateConsole(f">> {statement}")
            else:
                updateConsole("Serial disconnected")

        actionReceived=False
        while not actionReceived:
            k = editor.getch()

            if (k!=-1):
                actionReceived = True
                # updateConsole(k)
                if (k == curses.KEY_MOUSE):
                    _, mx, my, _, bstate = curses.getmouse()
                    if (my < window.n_rows and mx < window.n_cols):
                        cursor.move(my, mx, buffer)
                else:
                    updateConsole(f"input {k}")
                    if k == 23: #ctrl-w
                        if cx:
                            cx.close()
                        curses.endwin()
                        sys.exit(0)
                    elif k == 260: #left arrow
                        left(window, buffer, cursor)
                    elif k == 258: #down arrow
                        cursor.down(buffer)
                        window.down(buffer, cursor)
                        window.horizontal_scroll(cursor)
                    elif k == 259: #up arrow
                        cursor.up(buffer)
                        window.up(cursor)
                        window.horizontal_scroll(cursor)
                    elif k == 261: #right arrow
                        right(window, buffer, cursor)
                        undoList.append(lambda window, buffer, cursor: left(window, buffer, cursor))
                    elif k == 10: #enter
                        buffer.split(cursor)
                        right(window, buffer, cursor)
                    elif k == 330: #delete
                        ch = buffer.delete(cursor)
                    elif k == 127: #backspace
                        if (cursor.row, cursor.col) > (0, 0):
                            left(window, buffer, cursor)
                            buffer.delete(cursor)
                    elif k == 12: #ctrl-l
                        if outerBrackets:
                            code = buffer.copy(outerBrackets[0], outerBrackets[1])
                            sendTouSEQ(code)
                        else:
                            updateConsole("missing a bracket?")
                    elif k == 3:  # ctrl-c - copy
                        if outerBrackets:
                            code = buffer.copy(outerBrackets[0], outerBrackets[1])
                            pasteBuffer = code
                            updateConsole(f"pb << {code}")
                    elif k == 24:  # ctrl-X - cut
                        if outerBrackets:
                            code = buffer.copy(outerBrackets[0], outerBrackets[1])
                            pasteBuffer = code
                            updateConsole(f"pbx << {code}")
                            buffer.deleteSection(outerBrackets[0], outerBrackets[1])
                            cursor = outerBrackets[0]
                    elif k == 22:  # ctrl-v - paste
                        buffer.insert(cursor, pasteBuffer)
                        for i in range(len(pasteBuffer)):
                            right(window, buffer, cursor)
                    elif k == 9: #ctrl-i
                        #add statement to queue
                        if outerBrackets:
                            code = buffer.copy(outerBrackets[0], outerBrackets[1])
                            codeQueue.append(code)
                            updateConsole(f"Qd: {code}")
                    elif k == 15:
                        updateConsole(f"Sending Q: {len(codeQueue)}")
                        for i, statement in enumerate(codeQueue):
                            updateConsole(f"{i} {statement}")
                            sendTouSEQ(statement)
                        codeQueue = []
                    elif k == 21: #ctrl-u - undo
                        if len(undoList) > 0:
                            for u in undoList:
                                updateConsole(u)
                                u(window, buffer, cursor)
                            undoList = []
                            None
                    else:
                        kchar = chr(k)
                        if (kchar.isascii()):
                            buffer.insert(cursor, kchar)
                            right(window, buffer, cursor)
                    editor.refresh()

                    #save the buffer
                    with open(args.filename, "w") as f:
                        [f.write(x + '\n') for x in buffer]

            ##read serial if available
            if cx:
                try:
                    if (cx.in_waiting > 0):
                        byteCount = cx.in_waiting
                        actionReceived = True
                        # updateConsole(f"reading serial {cx.in_waiting}")
                        for i in range(byteCount):
                            inchar = cx.read()
                            if (inchar != b'\n' and inchar != b'\r'):
                                incoming = incoming + str(chr(inchar[0]))
                            if (inchar == b'\n' or inchar == b'\r'):
                                if (incoming != ''):
                                    updateConsole(incoming)
                                incoming = ''
                except:
                    cx = None
                    updateConsole("uSEQ disconnected")
            else:
                cx = trySerialConnection(port, updateConsole)

            #save some cpu
            curses.napms(2)



def trySerialConnection(port, updateConsole):
    try:
        cx = serial.Serial(port, baudrate=115200)
        updateConsole(f"Connected to uSEQ on {port}")
    except serial.SerialException:
        cx = None
    return cx


if __name__ == "__main__":
    main()
