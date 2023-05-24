class Buffer:
    def __init__(self, lines):
        self.lines = lines

    def __len__(self):
        return len(self.lines)

    def __getitem__(self, index):
        return self.lines[index]

    @property
    def bottom(self):
        return len(self) - 1

    def getch(self, cursor):
        ch=''
        if cursor.row < len(self.lines) and cursor.col < len(self.lines[cursor.row]):
            ch = self.lines[cursor.row][cursor.col]
        return ch

    def getLine(self, cursor):
        line=""
        if cursor.row < len(self.lines) and cursor.col < len(self.lines[cursor.row]):
            line = self.lines[cursor.row]
        return line

    def deleteLine(self, cursor):
        line=""
        if cursor.row < len(self.lines) and cursor.col < len(self.lines[cursor.row]):
            line = self.lines[cursor.row]
            del self.lines[cursor.row]
        return line

    #todo: multiline insert
    def insert(self, cursor, string):
        row, col = cursor.row, cursor.col
        linesToInsert = string.split('\n')
        current = self.lines.pop(row)
        new = current[:col] + string + current[col:]
        self.lines.insert(row, new)

    def split(self, cursor):
        row, col = cursor.row, cursor.col
        current = self.lines.pop(row)
        self.lines.insert(row, current[:col])
        self.lines.insert(row + 1, current[col:])

    def delete(self, cursor):
        row, col = cursor.row, cursor.col
        ch = self[row][col]
        if (row, col) < (self.bottom, len(self[row])):
            current = self.lines.pop(row)
            if col < len(current):
                new = current[:col] + current[(col + 1):]
                self.lines.insert(row, new)
            else:
                next = self.lines.pop(row)
                new = current + next
                self.lines.insert(row, new)
        return ch

    def copy(self, leftCursor, rightCursor):
        copyText = ""
        if leftCursor.row == rightCursor.row:
            copyText = self.lines[leftCursor.row][leftCursor.col:rightCursor.col+1]
        else:
            copyText = self.lines[leftCursor.row][leftCursor.col:]
            for line in range(leftCursor.row+1, rightCursor.row):
                copyText = copyText + self.lines[line]
            copyText = copyText + self.lines[rightCursor.row][:rightCursor.col+1]
        return copyText

    def deleteSection(self, leftCursor, rightCursor):
        if leftCursor.row == rightCursor.row:
            self.lines[leftCursor.row] = self.lines[leftCursor.row][:leftCursor.col] + self.lines[leftCursor.row][rightCursor.col+1:]
        else:
            self.lines[leftCursor.row] = self.lines[leftCursor.row][:leftCursor.col]
            self.lines[rightCursor.row] = self.lines[rightCursor.row][rightCursor.col+1:]
            self.lines = self.lines[:leftCursor.row+1] + self.lines[rightCursor.row:]

