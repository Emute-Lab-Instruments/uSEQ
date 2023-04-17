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


Control the speed of a square wave with toggle switch 1
```
(d2 (sqr (fast (+ 1 (swt 1)) beat)))
```

## `swr`

Read the value of the switch rotary encoder switch

```
(q0 (print (swr)))
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


# Sequencing Functions

## `q0 <form>`

Specify a function to run at the start of each quantum

```
(q0 (print bar))
```
```
(q0 0)
```


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

# MIDI functions

To use MIDI functions, the firmware must be compiled with the MIDI flag(s) defined.  See the hardware documentation for info on how to extend the module with MIDI inputs and outputs

# System Functions

## `perf`

Returns information on system performance and free memory

