import argparse
import curses
import struct
import sys
from art import *
from copy import deepcopy
import glob
from Clipping import MultiClip
from Lispy import Lispy

import serial

from Buffer import Buffer
from Cursor import Cursor
from MessageLog import MessageLog
from Window import Window
from midiIO import midiIO
from SerialStreamMap import SerialStreamMap
import re
import json
import os

def clamp(x, lower, upper):
    if x < lower:
        return lower
    if x > upper:
        return upper
    return x

def left(window, buffer, cursor):
    cursor.left(buffer)
    window.up(cursor)
    window.horizontal_scroll(cursor)


def right(window, buffer, cursor):
    cursor.right(buffer)
    window.down(buffer, cursor)
    window.horizontal_scroll(cursor)

serialIOMessage = []
serialIOMessageCounter=-1

def main():
    serialIOMessage = []
    serialIOMessageCounter = -1
    parser = argparse.ArgumentParser()
    parser.add_argument("filename", help="A file to edit")
    parser.add_argument("-cw", "--conswidth", help="console width", default=40, type=int)
    parser.add_argument("-p", "--port", help="serial usb port", default="")
    parser.add_argument("-lm", "--listmidi", help="show midi ports", action='store_true')
    args = parser.parse_args()

    stdscr = curses.initscr()
    curses.start_color()
    curses.mousemask(curses.ALL_MOUSE_EVENTS | curses.REPORT_MOUSE_POSITION)
    curses.mouseinterval(20)
    curses.init_pair(1, curses.COLOR_CYAN, curses.COLOR_BLACK)
    curses.init_pair(2, curses.COLOR_YELLOW, curses.COLOR_MAGENTA)
    curses.init_pair(3, curses.COLOR_GREEN, curses.COLOR_BLACK)
    curses.init_pair(4, curses.COLOR_BLUE, curses.COLOR_WHITE)
    curses.init_pair(5, curses.COLOR_CYAN, curses.COLOR_BLACK)
    curses.init_pair(6, curses.COLOR_WHITE, curses.COLOR_BLACK)
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
        #move cursor back to editor window
        editor.refresh()


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

    def highlightCurrentCodeblock(buffer, cursor, editor, updateConsole, window):
        outerBrackets = None
        innerBrackets = None
        #find the nearest bracket to the left
        leftParenCursor = cursor if buffer.getch(cursor) == '(' else findMatchingLeftParenthesis(buffer, cursor)
        if leftParenCursor:
            #find the matching cursor to the right
            rightParen = findMatchingRightParenthesis(buffer, leftParenCursor, 1)
            if rightParen:
                #we've found at least one set of brackets
                innerBrackets = (leftParenCursor, rightParen)

                #search for outermost brackets
                outerBrackets = innerBrackets
                searching=True
                while searching:
                    testLeft = findMatchingLeftParenthesis(buffer, outerBrackets[0])
                    if (testLeft):
                        # updateConsole(f"{testLeft.row}, {testLeft.col}")
                        testRight = findMatchingRightParenthesis(buffer, testLeft,1)
                        if (testRight):
                            # updateConsole(f"{testRight.row}, {testRight.col}")
                            outerBrackets = [testLeft, testRight]

                    if testLeft==None or testRight==None:
                        searching=False

                if innerBrackets:
                    leftInnerBracketPos = window.translateCursorToScreenCoords(innerBrackets[0])
                    if window.isInWindow(leftInnerBracketPos):
                        editor.chgat(*leftInnerBracketPos, 1, curses.A_BOLD | curses.color_pair(1))
                    rightInnerBracketPos = window.translateCursorToScreenCoords(innerBrackets[1])
                    if window.isInWindow(rightInnerBracketPos):
                        editor.chgat(*rightInnerBracketPos, 1, curses.A_BOLD | curses.color_pair(1))
                    if outerBrackets != innerBrackets:
                        leftBracketPos = window.translateCursorToScreenCoords(outerBrackets[0])
                        if window.isInWindow(leftBracketPos):
                            editor.chgat(*leftBracketPos, 1, curses.A_BOLD | curses.color_pair(2))
                        # updateConsole(window.translateCursorToScreenCoords(outerBrackets[1]))
                        rightBracketPos = window.translateCursorToScreenCoords(outerBrackets[1])
                        if window.isInWindow(rightBracketPos):
                            editor.chgat(*rightBracketPos, 1, curses.A_BOLD | curses.color_pair(2))

            else:
                None
        return outerBrackets, innerBrackets

    #serial setup
    incoming = ''
    serialPortName = args.port
    if serialPortName=="":
        #auto detect port
        devlist = sorted(glob.glob("/dev/ttyACM*"))  #what happens on windows?
        if len(devlist) > 0:
            serialPortName = devlist[0]
        else:
            devlist = sorted(glob.glob("/dev/tty.usbmodem147*"))  # connect to virtual uSEQ?
            if len(devlist) > 0:
                serialPortName = devlist[0]
            else:
                devlist = sorted(glob.glob("/tmp/ttyUSEQVirtual"))  # connect to virtual uSEQ?
                if len(devlist) > 0:
                    serialPortName = devlist[0]

    cx = trySerialConnection(serialPortName, updateConsole)
    if not cx:
        updateConsole(f"Error connecting to uSEQ at {serialPortName}")
    try:
        with open(args.filename) as f:
            buffer = Buffer(f.read().splitlines())
    except:
        buffer = Buffer([""])

    editor.nodelay(True) #nonblocking getch

    codeQueue = []
    undoList = []
    startMarker=None
    endMarker=None


    #midi setup
    midiIO.init()

    if args.listmidi:
        updateConsole("MIDI Input Ports:")
        for i, midiport in enumerate(midiIO.listMIDIInputs()):
            updateConsole(f"[{i}] {midiport}")

        updateConsole("MIDI Output Ports:")
        for i, midiport in enumerate(midiIO.listMIDIOutputs()):
            updateConsole(f"[{i}] {midiport}")

    # midi output mappings
    SerialStreamMap.init()
    configFile='useqedit.json'
    if os.path.exists(configFile):
        try:
            with open(configFile, 'r') as f:
                data = json.load(f)
                if "serialMap" in data:
                    error = SerialStreamMap.loadJSON(data["serialMap"], updateConsole)
                    if error:
                        updateConsole("There was a configuration error, some settings may not have been applied")
                    # else:
                    #     updateConsole("Serial mappings loaded")

        except Exception as e:
            updateConsole("Error loading config")
            updateConsole(e)
        # SerialStreamMap.loadConfig(updateConsole);

    SerialStreamMap.set(0, SerialStreamMap.makeMIDITrigMap(1,9,36))
    SerialStreamMap.set(1, SerialStreamMap.makeMIDITrigMap(1,9,42))
    SerialStreamMap.set(2, SerialStreamMap.makeMIDITrigMap(1,9,43))
    SerialStreamMap.set(3, SerialStreamMap.makeMIDITrigMap(1,9,38))
    SerialStreamMap.set(4, SerialStreamMap.makeMIDITrigMap(1,9,48))
    SerialStreamMap.set(5, SerialStreamMap.makeMIDITrigMap(0,9,34))
    SerialStreamMap.set(6, SerialStreamMap.makeMIDITrigMap(0,9,35))
    SerialStreamMap.set(7, SerialStreamMap.makeMIDITrigMap(0,9,36))



    def markedSection():
        return startMarker != None and endMarker != None

    def saveUndo(buffer, cursor):
        undoList.append([deepcopy(buffer), deepcopy(cursor)])
        #limit the size of the undo list
        if sys.getsizeof(undoList) > 1024 * 1024 * 128:
            undoList.pop(0)

    def clearMarkedSection():
        startMarker = None
        endMarker = None

    while True:
        stdscr.erase()
        # console.erase()
        editor.erase();

        # console.border(1)
        updateConsole()

        highlightOn = False
        for row, line in enumerate(buffer[window.row:window.row + window.n_rows-1]):
            if row == cursor.row - window.row and window.col > 0:
                line = "«" + line[window.col + 1:]
            if len(line) > window.n_cols:
                line = line[:window.n_cols - 1] + "»"
            for i, ch in enumerate(line):
                editor.addch(row,i,ch)
                if markedSection():
                    if (row == startMarker.row and i == startMarker.col):
                        highlightOn = True
                    if  (row == endMarker.row and i == endMarker.col):
                        highlightOn = False
                    if (highlightOn):
                        editor.chgat(row, i, curses.color_pair(1))

            # editor.addstr(row, 0, line)
        editor.move(*window.translateCursorToScreenCoords(cursor))

        outerBrackets = None
        innerBrackets = None

        #do highlighting
        outerBrackets, innerBrackets = highlightCurrentCodeblock(buffer, cursor, editor, updateConsole, window)
        # if still didn't find outerbrackets, we need to move leftwards because we might be between lisp statements
        if outerBrackets==None:
            searchCursor = Cursor.createFromCursor(cursor)
            while searchCursor.left(buffer):
                searchChar = buffer.getch(searchCursor)
                if (searchChar == ')'):
                    outerBrackets, innerBrackets = highlightCurrentCodeblock(buffer, searchCursor, editor, updateConsole, window)
                    break




        #highlight keywords
        for row in range(window.row, min(window.bottom, len(buffer))):
            # updateConsole(row)
            line = buffer.getLine(row)
            # updateConsole(line)
            if len(line) > 0 and line[0] == "#":
                for highlightPos in range(window.col, min(len(line), window.col + window.n_cols)):
                    editor.chgat(row - window.row, highlightPos, 1, curses.A_DIM | curses.color_pair(6))
            else:
                def searchAndHighlight(regexStr, colourIdx):
                    for match in re.finditer(regexStr, line):
                        for highlightPos in range(match.start(), min(match.end(), window.col + window.n_cols)):
                            if line[highlightPos] not in ['(',')']:
                                editor.chgat(row-window.row, highlightPos, 1, curses.color_pair(colourIdx))
                searchAndHighlight(r'd1|d2|d3|d4|d5|d6|a1|a2|a3|a4|a5|a6|in1|in2|swm|swt|swr|rot|q0|s1|s2|s3|s4|s5|s6|s7|s8',3)
                searchAndHighlight(r'fast|fromList|sqr|gatesw|trigs|\+|\-|\*|\/|perf|pm|dw|dr|useqaw|useqdw|delay|delaym|millis|micros|pulse|slow|flatIdx|flat|looph|dm|gates|setbpm|settimesig|interp|mdo|sin|cos|tan|abs|min|max|pow|sqrt|scale|seq|step|euclid',4)
                searchAndHighlight(r'(\s|\()(bar|phrase|beat|section|time|t)(\s|\))',5)

        editor.move(*window.translateCursorToScreenCoords(cursor))

        def sendTouSEQ(statement):
            # send to terminal
            if cx:
                asciiCode = statement.encode('ascii')
                cx.write(asciiCode)
                # updateConsole(f">> {statement}")
            else:
                updateConsole("Serial disconnected")

        def cutSection(st, en):
            code = buffer.copy(st, en)
            buffer.deleteSection(st, en)
            return code

        actionReceived=False
        while not actionReceived:
            k = editor.getch()

            if (k!=-1):
                actionReceived = True
                # updateConsole(f"key {k}")
                if (k == curses.KEY_MOUSE):
                    _, mx, my, _, bstate = curses.getmouse()
                    # updateConsole(f"mouse {bstate}")
                    if bstate==1:
                        # if (my < window.n_rows and mx < window.n_cols):
                        my = max(0,my)
                        mx = max(0,mx)
                        newCursor = window.translateScreenCoordsToCursor(my, mx)
                        updateConsole(f"{newCursor.row} {newCursor._col} {my} {mx}")
                        # newCursor._clamp_row(buffer)
                        # updateConsole(f"{newCursor.row} {newCursor._col}")
                        # newCursor._clamp_col(buffer)
                        # updateConsole(f"{newCursor.row} {newCursor._col}")
                        newCursor.clamp(buffer)
                        cursor.move(newCursor.row, newCursor.col, buffer)
                    elif bstate==65536:
                        cursor.down(buffer)
                        window.down(buffer, cursor)
                        window.horizontal_scroll(cursor)
                    elif bstate==2097152:
                        cursor.up(buffer)
                        window.up(cursor)
                        window.horizontal_scroll(cursor)
                else:
                    # updateConsole(f"input {k}")
                    if k == 23: #ctrl-w
                        if cx:
                            cx.close()
                        curses.endwin()
                        sys.exit(0)
                    elif k == 260: #left arrow
                        left(window, buffer, cursor)
                    # elif k == 393:  # shift-left arrow
                    #     None
                    # elif k == 550:  # left arrow
                    #     None
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
                    # elif k == 402:  # shift-right arrow
                    #     None
                    # elif k == 565:  # ctrl-right arrow
                    #     None
                    # elif k == 566:  # ctrl-shift-right arrow
                    #     None
                    elif k == 10: #enter
                        saveUndo(buffer, cursor)
                        buffer.split(cursor)
                        right(window, buffer, cursor)
                    elif k in [330]: #delete
                        saveUndo(buffer, cursor)
                        ch = buffer.delete(cursor)
                    elif k in [127, 263]: #backspace
                        if (cursor.row, cursor.col) > (0, 0):
                            saveUndo(buffer, cursor)
                            left(window, buffer, cursor)
                            buffer.delete(cursor)
                    elif k == 12: #ctrl-l - run, quantised
                        if outerBrackets:
                            code = buffer.copy(outerBrackets[0], outerBrackets[1])
                            sendTouSEQ(code)
                        else:
                            updateConsole("missing a bracket?")
                    elif k == 11:  # ctrl-k - run immediately
                            if outerBrackets:
                                code = buffer.copy(outerBrackets[0], outerBrackets[1])
                                sendTouSEQ('@' + code)
                            else:
                                updateConsole("missing a bracket?")
                    elif k == 3:  # ctrl-c - copy
                        def copySection(st, en):
                            code = buffer.copy(st, en)
                            return code
                        if markedSection():
                            MultiClip.copy(copySection(startMarker, endMarker))
                            clearMarkedSection()
                        elif outerBrackets:
                            sect = copySection(outerBrackets[0], outerBrackets[1])
                            updateConsole(sect)
                            MultiClip.copy(sect)
                        updateConsole(f"pb << {MultiClip.paste()}")
                    elif k == 24:  # ctrl-X - cut
                        if markedSection():
                            saveUndo(buffer, cursor)
                            MultiClip.copy(cutSection(startMarker, endMarker))
                            clearMarkedSection()
                            cursor = startMarker
                        elif outerBrackets:
                            saveUndo(buffer, cursor)
                            MultiClip.copy(cutSection(outerBrackets[0], outerBrackets[1]))
                            cursor = outerBrackets[0]
                        updateConsole(f"pbx << {MultiClip.paste()}")
                    elif k == 22:  # ctrl-v - paste
                        saveUndo(buffer, cursor)
                        buffer.insert(cursor, MultiClip.paste())
                        for i in range(len(MultiClip.paste())):
                            right(window, buffer, cursor)
                    elif k == 9: #ctrl-i
                        #add statement to queue
                        if outerBrackets:
                            code = buffer.copy(outerBrackets[0], outerBrackets[1])
                            codeQueue.append(code)
                            updateConsole(f"Qd: {code}")
                    elif k == 15: #ctrl-o, send Q
                        updateConsole(f"Sending Q: {len(codeQueue)}")
                        for i, statement in enumerate(codeQueue):
                            updateConsole(f"{i} {statement}")
                            sendTouSEQ(statement)
                        codeQueue = []
                    elif k == 26: #ctrl-z - undo
                        if len(undoList) > 0:
                            newState = undoList.pop()
                            buffer = deepcopy(newState[0])
                            cursor = deepcopy(newState[1])
                    elif k == 28: #ctrl-\, asciiart the current line as a  comment
                        currentLine = buffer.deleteLine(cursor)
                        updateConsole(currentLine)
                        s = text2art(currentLine)
                        #add ;comment symbols to the text
                        s = ";" + s
                        s = s.replace('\n', '\n;')
                        s = s + '\n'
                        # s = "11111\n2222222\n333\n4444\n"
                        updateConsole(s)
                        buffer.insert(cursor, s)
                    elif k == 2: #ctrl-b - begin
                        startMarker = Cursor.createFromCursor(cursor)
                    elif k == 7: #ctrl-g
                        endMarker = Cursor.createFromCursor(cursor)
                    elif k == 6:  # ctrl-f - format statement
                        if (outerBrackets):
                            saveUndo(buffer,cursor)
                            cursor = outerBrackets[0]
                            code = cutSection(outerBrackets[0], outerBrackets[1])
                            tokens = Lispy.tokenize_lisp(code)
                            ast = Lispy.get_ast(tokens.copy())
                            codestr = Lispy.astToFormattedCode(ast)
                            buffer.insert(cursor, codestr)

                    else:
                        kchar = chr(k)
                        if (kchar.isascii()):
                            saveUndo(buffer,cursor)
                            if (kchar=='('):
                                buffer.insert(cursor, ')')
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
                        # actionReceived = True
                        # updateConsole(f"reading serial {cx.in_waiting}")
                        for i in range(byteCount):
                            inchar = cx.read()
                            #serial IO message?
                            # updateConsole(inchar)

                            if (inchar == b'\x1f'):
                                serialIOMessageCounter=9
                                serialIOMessage=[]
                                # updateConsole(f"SIO31  {serialIOMessageCounter}")
                            else:
                                if serialIOMessageCounter > 0:
                                    serialIOMessage.append(inchar)
                                    serialIOMessageCounter = serialIOMessageCounter - 1
                                    if serialIOMessageCounter==0:
                                        try:
                                            # updateConsole(serialIOMessage)
                                            #convert bytes to double
                                            dblbytes = b''.join(serialIOMessage[1:])
                                            val = struct.unpack('d',dblbytes)[0]
                                            ch = serialIOMessage[0][0]-1
                                            SerialStreamMap.mapSerial(ch, val)
                                            # updateConsole(str(val))
                                        except Exception as e:
                                            updateConsole(e)
                                else:
                                    if (inchar != b'\n' and inchar != b'\r'):
                                        incoming = incoming + str(chr(inchar[0]))
                                    if (inchar == b'\n' or inchar == b'\r'):
                                        if (incoming != ''):
                                            updateConsole(incoming)
                                        incoming = ''
                except Exception as e:
                    cx = None
                    updateConsole(e)
                    updateConsole("uSEQ disconnected")
            else:
                cx = trySerialConnection(serialPortName, updateConsole)

            #save some cpu
            curses.napms(1)




def trySerialConnection(port, updateConsole):
    try:
        cx = serial.Serial(port, baudrate=115200)
        updateConsole(f"Connected to uSEQ on {port}")
    except serial.SerialException:
        cx = None
    return cx


if __name__ == "__main__":
    main()
