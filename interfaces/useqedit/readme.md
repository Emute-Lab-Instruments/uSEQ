# uSEQ Edit

uSEQ editor is a text editor specially designed for livecoding uSEQ modules.  You can talk to uSEQ in many ways (anything with a serial connection), but this editor provides controls that are specialised for making music.

To run the editor:

```
python useqedit.py [name of the file]
```

The app runs in a linux/unix terminal, and requires Python 3.

uSEQEdit saves (overwrites) the file every time you make a change.

## Command Line Options

| Key | Description |
| --- | --- |
| -cw, --conswidth | Specify the width of the console in characters |



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

### Troubleshooting



## Troubleshooting

The editor crashed and now I can't see any text in my terminal
: run '''reset'''
