# uSEQ Edit

uSEQ editor is a text editor specially designed for livecoding uSEQ modules.  You can talk to uSEQ in many ways (anything with a serial connection), but this editor provides controls that are specialised for making music.

To run the editor:

```
python useqedit.py [name of the file]
```

The app runs in a linux/unix terminal, and requires Python 3.

uSEQEdit saves (overwrites) the file every time you make a change.

Controls

| Key | Description |
| --- | --- |
| Ctrl - l | Send the current LISP statement to uSEQ (within the highlighted outer brackets) |
| Ctrl - w | Quit |



Some terminals will have standard key mappings that might affect adversely affect the editor. Here's what they do and how to mitigate them.
| Key | Effect | Solution |
| --- | --- | --- |
| Ctrl - z | Leaves the editor running in the background | Run the command `fg` to bring the editor back |
| Ctrl - s | Freezes the terminal | Use ctrl-q to unfreeze |
| Ctrl - c | Hard-quits the app | Just start the app again from the command line (press up to run the previous command) |