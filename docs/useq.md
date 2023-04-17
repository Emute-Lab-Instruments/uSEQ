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

