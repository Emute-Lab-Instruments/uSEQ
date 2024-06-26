import argparse
import curses
import sys
from art import *
from copy import deepcopy
from Clipping import MultiClip
from Lispy import Lispy
from Buffer import Buffer
from Cursor import Cursor
from Window import Window
from midiIO import midiIO
from SerialStreamMap import SerialStreamMap
import re
import json
import os
from SerialIO import SerialIO
from Console import Console
from SerialOutMappings import SerialOutMappings

import logging
logger = logging.getLogger(__name__)



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


redrawFlag = True


def main():

    global redrawFlag

    logging.basicConfig(filename='useqedit.log', level=logging.DEBUG)
    logger.info("Editor started")

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
    Console.init(consoleWidth)

    cursor = Cursor()
    editor.keypad(True)


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

    def highlightCurrentCodeblock(buffer, cursor, editor, window):
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
                        testRight = findMatchingRightParenthesis(buffer, testLeft,1)
                        if (testRight):
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
                        rightBracketPos = window.translateCursorToScreenCoords(outerBrackets[1])
                        if window.isInWindow(rightBracketPos):
                            editor.chgat(*rightBracketPos, 1, curses.A_BOLD | curses.color_pair(2))
                        codestr = buffer.copy(outerBrackets[0], outerBrackets[1])
                        tokens = Lispy.tokenize_lisp(codestr)
                        ast = Lispy.get_ast(tokens.copy())
                        def doHighlights(v):
                            Console.post(v)
                        # Lispy.traverseAST(tokens, doHighlights)

            else:
                pass
        return outerBrackets, innerBrackets

    #serial setup
    SerialIO.openSerialCx(args.port)

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
        Console.post("MIDI Input Ports:")
        for i, midiport in enumerate(midiIO.listMIDIInputs()):
            Console.post(f"[{i}] {midiport}")

        Console.post("MIDI Output Ports:")
        for i, midiport in enumerate(midiIO.listMIDIOutputs()):
            Console.post(f"[{i}] {midiport}")

    # midi output mappings
    SerialStreamMap.init()
    configFile='useqedit.json'
    if os.path.exists(configFile):
        try:
            with open(configFile, 'r') as f:
                data = json.load(f)
                if "serialMap" in data:
                    error = SerialStreamMap.loadJSON(data["serialMap"])
                    if error:
                        Console.post("There was a configuration error, some settings may not have been applied")

        except Exception as e:
            Console.post("Error loading config")
            Console.post(e)

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

    redrawFlag = True


    # Console.post(curses.has_colors());
    # Console.post(curses.COLORS);
    #make gradiented colors for brackets
    #todo: check if terminal has enough colors
    import colorsys
    for i in range(25):
        rgb = colorsys.hsv_to_rgb(0.3, i/25, 1.0)
        curses.init_color(200+i,  int(rgb[0]*1000), int(rgb[1]*1000), int(rgb[2]*1000))

    while True:
        editor.refresh()
        if redrawFlag:
            # stdscr.erase()
            # console.erase()
            editor.erase()
            # console.border(1)
            # Console.post()

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
            outerBrackets, innerBrackets = highlightCurrentCodeblock(buffer, cursor, editor, window)
            # if still didn't find outerbrackets, we need to move leftwards because we might be between lisp statements
            if outerBrackets==None:
                searchCursor = Cursor.createFromCursor(cursor)
                while searchCursor.left(buffer):
                    searchChar = buffer.getch(searchCursor)
                    if (searchChar == ')'):
                        outerBrackets, innerBrackets = highlightCurrentCodeblock(buffer, searchCursor, editor, window)
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
                    searchAndHighlight(r'fast|fromList|sqr|gatesw|trigs|\+|\-|\*|\/|perf|pm|dw|dr|useqaw|useqdw|delay|delaym|millis|micros|pulse|slow|flatIdx|flat|looph|dm|gates|setbpm|settimesig|interp|mdo|sin|cos|tan|abs|min|max|pow|sqrt|scale|seq|step|euclid|defun',4)
                    searchAndHighlight(r'(\s|\()(bar|phrase|beat|section|time|t)(\s|\))',5)

            editor.move(*window.translateCursorToScreenCoords(cursor))
            redrawFlag=False

        def sendTouSEQ(codestr, prefix=''):
            #reformat to single line
            tokens = Lispy.tokenize_lisp(codestr)
            ast = Lispy.get_ast(tokens.copy())
            codestr = Lispy.astToOneLineCode(ast)
            if not SerialIO.sendTouSEQ(prefix + codestr):
                Console.post("uSEQ disconnected")
                SerialIO.openSerialCx(args.port)
        def cutSection(st, en):
            code = buffer.copy(st, en)
            buffer.deleteSection(st, en)
            return code

        Console.postQueuedMessages()

        k = editor.getch()

        if (k!=-1):
            redrawFlag=True
            # Console.post(f"key {k}")
            if (k == curses.KEY_MOUSE):
                _, mx, my, _, bstate = curses.getmouse()
                # Console.post(f"mouse {bstate}")
                if bstate==1:
                    # if (my < window.n_rows and mx < window.n_cols):
                    my = max(0,my)
                    mx = max(0,mx)
                    newCursor = window.translateScreenCoordsToCursor(my, mx)
                    Console.post(f"{newCursor.row} {newCursor._col} {my} {mx}")
                    # newCursor._clamp_row(buffer)
                    # Console.post(f"{newCursor.row} {newCursor._col}")
                    # newCursor._clamp_col(buffer)
                    # Console.post(f"{newCursor.row} {newCursor._col}")
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
                # Console.post(f"input {k}")
                if k == 23: #ctrl-w
                    SerialIO.close()
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
                        Console.post("missing a bracket?")
                elif k == 11:  # ctrl-k - run immediately
                        if outerBrackets:
                            code = buffer.copy(outerBrackets[0], outerBrackets[1])
                            sendTouSEQ(code, '@')
                        else:
                            Console.post("missing a bracket?")
                elif k == 3:  # ctrl-c - copy
                    def copySection(st, en):
                        code = buffer.copy(st, en)
                        return code
                    if markedSection():
                        MultiClip.copy(copySection(startMarker, endMarker))
                        clearMarkedSection()
                    elif outerBrackets:
                        sect = copySection(outerBrackets[0], outerBrackets[1])
                        Console.post(sect)
                        MultiClip.copy(sect)
                    Console.post(f"pb << {MultiClip.paste()}")
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
                    Console.post(f"pbx << {MultiClip.paste()}")
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
                        Console.post(f"Qd: {code}")
                elif k == 15: #ctrl-o, send Q
                    Console.post(f"Sending Q: {len(codeQueue)}")
                    for i, statement in enumerate(codeQueue):
                        Console.post(f"{i} {statement}")
                        sendTouSEQ(statement)
                    codeQueue = []
                elif k == 26: #ctrl-z - undo
                    if len(undoList) > 0:
                        newState = undoList.pop()
                        buffer = deepcopy(newState[0])
                        cursor = deepcopy(newState[1])
                elif k == 28: #ctrl-\, asciiart the current line as a  comment
                    currentLine = buffer.deleteLine(cursor)
                    Console.post(currentLine)
                    s = text2art(currentLine)
                    #add ;comment symbols to the text
                    s = ";" + s
                    s = s.replace('\n', '\n;')
                    s = s + '\n'
                    # s = "11111\n2222222\n333\n4444\n"
                    Console.post(s)
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
                elif k==27: #alt-...
                    esck = editor.getch()
                    Console.post(f"esc {esck} ")
                    if esck==102: #alt-f, unformat
                        if (outerBrackets):
                            saveUndo(buffer, cursor)
                            cursor = outerBrackets[0]
                            code = cutSection(outerBrackets[0], outerBrackets[1])
                            tokens = Lispy.tokenize_lisp(code)
                            ast = Lispy.get_ast(tokens.copy())
                            codestr = Lispy.astToOneLineCode(ast)
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
                if buffer.hasNewChanges():
                    with open(args.filename, "w") as f:
                        [f.write(x + '\n') for x in buffer]
                    buffer.resetNewChanges()


        SerialIO.readSerial()
        SerialOutMappings.process()
        #save some cpu
        curses.napms(5)






if __name__ == "__main__":
    main()
