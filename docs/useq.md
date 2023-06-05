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

## `setbpm <bpm>`

Set the speed of the sequencer in beats per minute

| Parameter | Description | Range |
| --- | --- | --- |
| bpm | beats per minute | any |

## `settimesig <numerator> <denominator>`

Set the time signature of the sequencer

| Parameter | Description | Range |
| --- | --- | --- |
| numerator | number of beats in a measure | any |
| denominator | the length of a beat | any |

# Sequencing Callback Functions

## `q0 <form>`

Specify a function to run at the start of each quantum

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

## `fromList <list> <position>`

Read an item from a list, using a normalised index.  

Items in the list are evaluated before being returned, so you can use functions and variables in the list.

| Parameter | Description | Range |
| --- | --- | --- |
| list | A list of values | any |
| position | A normalised index | 0-1 |

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

## `gates <list> <phasor> <speed> (<pulse width>)`

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

## `gatesw <list> <phasor> (<speed>)`

Output a sequence of gates, with pulse width controlled from values in the list

| Parameter | Description | Range |
| --- | --- | --- |
| list | A list of gate/pulse width values, varying from 0 (0% pulse width) to 9 (100% pulse width / tie into the next note) | 0 - 9 |
| phasor | The sequence is output once per cycle of the phasor | 0-1 |
| speed | Optional, default: 1. Modify the speed of the phasor | >= 1 |

```
(d2 (gatesw (quote 9 9 5 9 3 0 3 8) bar 2))
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

# MIDI functions

To use MIDI functions, the firmware must be compiled with the MIDI flag(s) defined.  See the hardware documentation for info on how to extend the module with MIDI inputs and outputs

# System Functions

## `perf`

Returns information on system performance and free memory

