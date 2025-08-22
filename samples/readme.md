# Sample Library Creation for uSEQ


## Requirements

Python 3
A terminal
[Picotool](https://github.com/raspberrypi/picotool)

## Samples

Create a folder with the samples that you'd like to have in your library.  The names of the files will be the names that you use to refer to the samples in Modulisp code.

# Compiling samples into a binary File

To get help:

```
 python3 multi_audio_converter.py -h
```


Usage:

This code creates a binary file ```mysamples.bin```, converting all the samples to 22050 KHz. It also creates a script to load the samples into flash memory

```
python3 multi_audio_converter.py [folder of samples] -o mysamples.bin -r 22050 -s loader.sh
```

# Load the samples onto the hardware

Connect the instrument, and make sure no other serial connections are open (e.g. the code editor).  Run the sample loading script that was generated in the step above. e.g.

```
./loader.sh
```



