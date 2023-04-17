# Variables

## `time`

The number of milliseconds since the last reset.

## `beat` 

A phasor, rising from 0-1 over the length of a beat

## `bar` 

A phasor, rising from 0-1 over the length of a bar

## `phrase` 

A phasor, rising from 0-1 over the length of a phrase

## `phrase` 

A phasor, rising from 0-1 over the length of a section


# Timing functions

## `setbpm <bpm>`

Set the speed of the sequencer in beats per minute

| Parameter | Description | Range |
| --- | --- | --- |
| bpm | beats per minute | any |


# Sequencing Functions

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


