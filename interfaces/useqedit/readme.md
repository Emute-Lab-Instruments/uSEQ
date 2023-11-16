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
| -p, --port | Specify the USB serial port to connect to e.g. /dev/ttyACM0. The app will automatically search for a port if one isn't specified.|



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


## Serial USB Mappings

uSEQ will send a waveform back to the client computer over USB when you use the outputs s1-8. For example

```
(s1 (euclid bar 13 4))
```
sends a euclidean gate sequence on serial channel 1

or

```
(s7 bar)
```
sends the bar phasor over channel 7

uSEQ edit receives these signals and can be configured to forward them over with MIDI or OSC (the OSC part is still in development)

The configuration for this is done in the file `useqedit.json`.  If this file exists, the app will load the settings from it on startup.  There is a template file `useqedit.json.template` which can be copied and used as a starting point.

The configuration file contains an entry `serialMap` which is an array of configurations for each serial channel.  There are three types of MIDI configuration:

### Midi Trigger

Use this for drums or samples

```
  {
    "serial": [serial channel number],
    "type": "MIDITRIG",
    "port": [index of the serial port],
    "channel": [midi channel],
    "note": [midi note number]
  }
```

When the waveform on the serial bus transitions from 0 to any value above zero, a note is sent out.

### MIDI Controllers

```
{
    "serial": [serial channel number],
    "type": "MIDICTL",
    "port": [index of the serial port],
    "channel": [midi channel],
    "ctl": [midi controller number]
}
```

Values between 0 and 1 in the waveform are translated to the value of the controller between 0 and 127.

### MIDI Note

```
{
    "serial": [serial channel number],
    "type": "MIDINOTE",
    "port": [index of the serial port],
    "channel": [midi channel],
}
```

When a waveform transitions from 0 to above 0 a note is sent. The pitch is determined by the amplitude of the waveform, divided by 127 and rounded to the nearest integer.


## Troubleshooting

The editor crashed and now I can't see any text in my terminal
: run '''reset'''
