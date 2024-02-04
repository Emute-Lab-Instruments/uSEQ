# uSEQ timing system

Most computer music systems run with a fixed quantum, but uSEQ is a bit different. It runs as fast as possible, a bit like a game will try to run at the fastest fps possible.  Timing functions take a *functional rendering* approach; they take the current time ```t``` as an argument, and react accordingly.  Much of the sequencing in uSEQ is done using *phasors*, ramps that rise from 0 to 1 in a fixed time period.


# Code sequencing

By default, code is added into a queue which will run at the start of each bar (future: this will be more configurable). It you want to run code immediately, put an '@' sign in front of it.

For example

```
@(+ 1 1)
```
will run as soon as you send it to uSeq

```
(print bar)
```

Will run as close as possible before the start of each bar


# Variables

## `time`

The number of milliseconds since the last reset.

## `beat` 

A phasor, rising from 0-1 over the length of a beat

## `bar` 

A phasor, rising from 0-1 over the length of a bar

## `phrase` 

A phasor, rising from 0-1 over the length of a phrase

## `section` 

A phasor, rising from 0-1 over the length of a section

# Input and Output

## `in1`

Returns the value of digital input 1 

Example: to echo digital input 1 to digital output 1
```
(d1 (in1))
```

## `in2`

Returns the value of digital input 2

Example: to echo digital input 2 to digital output 1
```
(d1 (in2))
```

## `swm <index>`

Read the value of a momentary switch

| Parameter | Description | Range |
| --- | --- | --- |
| index | The index of the switch | 1 or 2 |


Control the speed of a square wave with momentary switch 1
```
(d2 (sqr (fast (+ 1 (swm 1)) beat)))
```

## `swt <index>`

Read the value of a toggle switch

| Parameter | Description | Range |
| --- | --- | --- |
| index | The index of the switch | 1 or 2 |


Control the speed of a square wave with toggle switch 2
```
(d4 (sqr (fast (scale (swt 2) 0 1 3 8) beat)))
```

## `swr`

Read the value of the rotary encoder switch

```
(q0 (print (swr)))
```


## `rot`

Read the value of the rotary encoder

```
(q0 (print (rot)))
```
## `print <value>`

Print to the serial terminal

| Parameter | Description | Range |
| --- | --- | --- |
| value | A value to print | any |


```
(print (in1))
```


# Timing functions

## `setbpm <bpm> (<change threshold> = 0)`

Set the speed of the sequencer in beats per minute

| Parameter | Description | Range |
| --- | --- | --- |
| bpm | beats per minute | any |
| change threshold | the bpm will only change if the difference between the current bpm and the new bpm is more than this threshold. Use this to help stabilise the bpm when it is tracked from an external source using getbpm, as small changes in bpm get disrupt phasors and patterns. | >=0 |

## `getbpm <input>`

Set the speed of the sequencer in beats per minute

| Parameter | Description | Range |
| --- | --- | --- |
| input | get the estimated BPM of the signal on either input 1 or 2, based on the average time between pulses | 1 or 2 |

Use this to get the bpm of the uSEQs sequencing engine:

```
(q0 (setbpm (getbpm 1)))
```

## `settimesig <numerator> <denominator>`

Set the time signature of the sequencer

| Parameter | Description | Range |
| --- | --- | --- |
| numerator | number of beats in a measure | any |
| denominator | the length of a beat | any |


# Sequencing Callback Functions

## `q0 <form>`

Specify a function to run at the start of each quantisation period (by default, each bar)

```
(q0 (print bar))
```
```
(q0 0)
```

## `a1 <form>`

Specify a function to calculate the value of analog output 1, calculated every quantum

```
(a1 (* (fromList '(0.4 0.1) bar) 3))
```
```
(a1 0)
```

## `a2 <form>`

Specify a function to calculate the value of analog output 2, calculated every quantum

```
(a2 (* (fromList '(0.4 0.1) bar) 3))
```
```
(a2 0)
```

## `d1 <form>`

Specify a function to calculate the value of digital output 1, calculated every quantum

```
(d1 (sqr beat))
```
```
(d1 0)
```

## `d2 <form>`

Specify a function to calculate the value of digital output 2, calculated every quantum

```
(d2 (sqr beat))
```
```
(d2 0)
```

## `d3 <form>`

Specify a function to calculate the value of digital output 3, calculated every quantum

```
(d3 (sqr beat))
```
```
(d3 0)
```

## `d4 <form>`

Specify a function to calculate the value of digital output 4, calculated every quantum

```
(d4 (sqr beat))
```
```
(d4 0)
```

## `s[x] <form>`

(where x is a number between 1 and 8)

Send a sequence over the USB serial connection.  This sequence can be decoded by your USB client software.  To differentiate from other text send by the module, serial data is send in a 10 byte message in the following format:
[31][index of serial stream][8 bytes representing a double]

The useqedit app can decode these messages and forward them as MIDI or OSC.

# Sequence Generation and Manipulation

## `sqr <phasor>`

Turns a phasor into a square wave.

| Parameter | Description | Range |
| --- | --- | --- |
| phasor | A phasor (e.g. bar, beat) | 0-1 |


## `pulse <phasor> <pulse width>`

Outputs a pulse wave.

| Parameter | Description | Range |
| --- | --- | --- |
| phasor | A phasor (e.g. bar, beat) | 0-1 |
| pulse width | The relative width of each pulse | 0-1 |


## `dm <input value> <value 1> <value 2>`

Digital mapping.

| Parameter | Description | Range |
| --- | --- | --- |
| input value | A binary input | 0 or 1 |
| value 1 | This value is returned if the input is 0 | any |
| value 2 | This value is returned if the input is 1 | any |

```
(d2 (gates (quote 0 1 1 0) bar (dm (swt 1) 2 6) 0.5)))
```

## `fromList <list> <position> (<scale>)`  (alias: `seq`)

Read an item from a list, using a normalised index.  

Items in the list are evaluated before being returned, so you can use functions and variables in the list.

| Parameter | Description | Range |
| --- | --- | --- |
| list | A list of values | any |
| position | A normalised index | 0-1 |
| scale | Divide all numbers by this value | !=0 |

Examples:

```
(fromList (quote 1 0 0 1) 0.2))
```

```
(fromList '(1 2 3 4) bar))
```

```
(fromList '(1 phrase) bar))
```

```
(fromList '(1 phrase) bar))
```

```
(fromList (quote 1 2 (fromList (quote 1 2) bar)) 0.9)

```

## `flat <list>`

Take a list that might contain other lists or functions, evaluate them all in turn and collect them in a one dimensional list.

| Parameter | Description | Range |
| --- | --- | --- |
| list | A list of values | any |

Examples:

```
(flat '(1 '(2 3) bar))
```


```
(define part1 '(1 2 3))
(define part2 '(4 5))
(flat '(part1 part2 part1))
```

## `flatIdx <list> <position>`

Read an item from a list, using a normalised index, but flatten then list first (using `flat`)



| Parameter | Description | Range |
| --- | --- | --- |
| list | A list of values | any |
| position | A normalised index | 0-1 |

This is a shortcut, equivelant to

```
(fromList (flat list), position)
```

## `gates <list> <phasor> <speed> (<pulse width> = 0.5)`

Output a sequence of gates, with variable pulse width.

| Parameter | Description | Range |
| --- | --- | --- |
| list | A list of gate values | 0 or 1 |
| phasor | The sequence is output once per cycle of the phasor | 0-1 |
| speed | Modify the speed of the phasor | >= 1 |
| pulse width | Optional, default: 0.5. The pulse width of the gates | 0-1 |

```
(d2 (gates (quote 0 1 1 0  1 1 1 0  1 1 0 1  1 0 0 1) bar 1 (+ (swm 1) 0.3)))

```

```
(d2 (gates (quote 0 1 1 0 1 0 0 (swt 1) ) bar 2 0.5)))
```

## `gatesw <list> <phasor> (<speed> = 1)`

Output a sequence of gates, with pulse width controlled from values in the list

| Parameter | Description | Range |
| --- | --- | --- |
| list | A list of gate/pulse width values, varying from 0 (0% pulse width) to 9 (100% pulse width / tie into the next note) | 0 - 9 |
| phasor | The sequence is output once per cycle of the phasor | 0-1 |
| speed | Optional, default: 1. Modify the speed of the phasor | >= 1 |

```
(d2 (gatesw (quote 9 9 5 9 3 0 3 8) bar 2))
```

## `trigs <list> <phasor> (<speed>) (<pulsewidth>)`

Output a sequence of gates, with pulse width controlled from values in the list

| Parameter | Description | Range |
| --- | --- | --- |
| list | A list of trigger values, varying from 0 (0% amplitude) to 9 (100% amplitude) | 0 - 9 |
| phasor | The sequence is output once per cycle of the phasor | 0-1 |
| speed | Optional, default: 1. Modify the speed of the phasor | >= 1 |
| pulseWidth | Optional, default: 0.1. Modify the pulse width of the trigger | 0 - 1 |

```
(s3 (trigs (quote 0 1 9 0 1) bar 2))
```


## `looph <phasor> <looppoint>`

Take a phasor, and make it wrap/loop around at a certain point

| Parameter | Description | Range |
| --- | --- | --- |
| phasor | A phasor | 0 - 1 |
| looppoint | The point at which the phasor should wrap/loop | 0-1 |

E.g. the code below will play the first half of the sequence repeatedly

```
(d2 (gatesw (quote 9 9 5 9 3 0 3 8) (looph bar 0.5) 2))
```

## `interp <list> <phasor>`

Interpolate across a list, using a phasor.  This function acts as if the list of values describes a continuous envelope, and returns the value at a position in that envelope.  e.g.

| Parameter | Description | Range |
| --- | --- | --- |
| values | A list of values | any list |
| phasor | A phasor | 0 - 1 |

```
(interp '(0 0.5 0) 0.75)
```

describes a triangle shape, and returns the value that it 75% along the triangle (0.25).

```
(a1 (interp '(1 0.5 0 0.6 1) bar))
```

makes a roughly inverted triangle, and plays it once per bar on PWM output 1

```
(a2 (interp '(0 (sin phrase) 1) section))
```

creates a slowly changing envelope that loops every section, sent to PWM output 2

## `step <phasor> <count> (<offset> = 0)`

Turn a phasor into an integer counter

| Parameter | Description | Range |
| --- | --- | --- |
| phasor | A phasor | 0 - 1 |
| count | the number of divisions to divide the phasor into | >0 |
| offset | Optional. the point to start the counter from | any |


## `euclid <phasor> <n> <k> (<offset> = 0) (<pulseWidth> = 0.5)`

Generate a sequence of gates based on euclidean sequencing.

For more info: https://erikdemaine.org/papers/DeepRhythms_CGTA/paper.pdf

Demaine, E.D., Gomez-Martin, F., Meijer, H., Rappaport, D., Taslakian, P., Toussaint, G.T., Winograd, T. and Wood, D.R., 2009. The distance geometry of music. Computational geometry, 42(5), pp.429-454.

| Parameter | Description | Range |
| --- | --- | --- |
| phasor | A phasor | 0 - 1 |
| n | the number of beats to fit into the period of the phasor | >0 |
| k | the number of beats to fit, equally spaced, into n beats | >0 |
| offset | Optional. A phase offset, measured in beats | >0 |
| pulseWidth | Optional. Width of the gates | >0 and <1 |

```
(d1 (euclid bar 16 (step phrase 4 4) 2))
(d2 (euclid bar 32 8 4 0.1))
(d3 (euclid bar 16 6 (step (fast 4 phrase) 4)))
```
# Scheduling

The scheduler runs a statement periodically.  This might be needed for stateful functions that require regular periodic updates

## `schedule <name> <statement> <period>`

Run the code in `statement` periodically, at the frequency of `period` times per bar

| Parameter | Description | Range |
| --- | --- | --- |
| name | An identifier | Any string |
| statement | A function | Any function |
| period | the number of times per bar to run the code | >0 |

To print "hi" 3 times per bar:
```
(schedule "test" (print "hi") 3)
```

## `unschedule <name>`

Remove a function from the scheduler

| Parameter | Description | Range |
| --- | --- | --- |
| name | An identifier | Any string |

```
(unschedule "test")
```

# MIDI functions

To use MIDI functions, the firmware must be compiled with the MIDI flag(s) defined.  See the hardware documentation for info on how to extend the module with MIDI inputs and outputs

## `mdo <note> <function>`

MIDI Drum Out

| Parameter | Description | Range |
| --- | --- | --- |
| note | The MIDI note of the drum to be triggered | 0 - 127 |
| function | A lambda function that describes a pattern of triggers, with time as an argument | n/a |



# System Functions

## `perf`

Returns information on system performance and free memory


## `timeit <statement>`

Returns the amount if time it took run evaluate <statement> in microseconds

| Parameter | Description | Range |
| --- | --- | --- |
| statement | Any LISP statement | - |


```
(timeit (+ 1 1))
```