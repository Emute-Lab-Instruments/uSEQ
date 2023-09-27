from Cursor import Cursor


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

    def translateCursorToScreenCoords(self, cursor):
        return cursor.row - self.row, cursor.col - self.col

    def translateScreenCoordsToCursor(self, row, col):
        return Cursor(row + self.row, col + self.col)

    def isInWindow(self, coords):
        return coords[0] >= 0 and coords[0] <= self.n_rows - 1 and coords[1] >=0 and coords[1] < self.n_cols - 1
