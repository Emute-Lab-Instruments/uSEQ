# uSEQ Edit

uSEQ Edit is a text editor specially designed for livecoding uSEQ modules.  You can talk to uSEQ in many ways (anything with a serial connection), but this editor provides controls that are specialised for livecoding.

The app runs in a linux/unix terminal, and requires Python 3.

To install the relevant packages, run 

```
pip3 install pyperclip pyserial mido python-rtmidi art
```


To run the editor:

```
python useqedit.py [name of the file]
```


uSEQ Edit saves (overwrites) the file every time you make a change.


## Command Line Options

| Key | Description |
| --- | --- |
| -cw, --conswidth | Specify the width of the console in characters |
| -lm --listmidi | List MIDI input and output devices in the console window when starting up |
| -p, --port | specify the USB serial port to connect to e.g. /dev/ttyACM0. The app will automatically search for a port if one isn't specified.|



## Controls

| Key | Description |
| --- | --- |
| Ctrl - l | Send the current LISP statement to uSEQ (within the highlighted outer brackets) |
| Ctrl - w | Quit |
| Ctrl - i | Add a lisp statement to the queue |
| Ctrl - o | Send code in the queue to uSEQ |
| Ctrl - c | Copy |
| Ctrl - v | Paste |
| Ctrl - x | Cut |
| Ctrl - z | Undo |





## Troubleshooting

The editor crashed and now I can't see any text in my terminal
: run '''reset'''
