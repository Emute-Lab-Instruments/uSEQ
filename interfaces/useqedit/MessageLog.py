from datetime import datetime


class MessageLog:
    def __init__(self, maxlines, width):
        self.lines = []
        self.maxlines = maxlines
        self.width = width
        self.startTime = datetime.now()


    def addMessage(self, msg):
        def seconds_to_dhms(seconds):
            days = seconds // (3600 * 24)
            hours = (seconds // 3600) % 24
            minutes = (seconds // 60) % 60
            seconds = seconds % 60
            return days, hours, minutes, seconds
        dtime = datetime.now() - self.startTime
        d,h,m,s = seconds_to_dhms(dtime.seconds)
        msg = f"{h:02d}:{m:02d}:{s:02d}:{dtime.microseconds//100000} {msg}"
        parts = [msg[i:i + self.width] for i in range(0, len(msg), self.width)]
        [self.lines.append(x) for x in parts]
        [self.lines.pop(0) for x in range(len(self.lines) - self.maxlines)]

    def __getitem__(self, index):
        return self.lines[index]
    def __len__(self):
        return len(self.lines)
